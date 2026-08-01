#!/bin/bash

# Source utility functions for reduced complexity
source "scripts/utils/benchmark_validation.sh" 2>/dev/null || source "./scripts/utils/benchmark_validation.sh" 2>/dev/null
source "scripts/utils/dpf_management.sh" 2>/dev/null || source "./scripts/utils/dpf_management.sh" 2>/dev/null

# Global variables for cleanup
declare -a BENCHMARK_PIDS=()
DPF_PID=""
CLEANUP_REQUIRED=false
ORIGINAL_TURBO_STATE=""  # Store original turbo state for restoration

# Cleanup function to kill all processes
cleanup() {
    if [[ "$CLEANUP_REQUIRED" == true ]]; then
        echo "Starting cleanup..."
        
        # Kill benchmark processes
        for pid in "${BENCHMARK_PIDS[@]}"; do
            if kill -0 "$pid" 2>/dev/null; then
                echo "Killing benchmark process $pid"
                sudo kill -TERM "$pid" 2>/dev/null
                sleep 2
                sudo kill -KILL "$pid" 2>/dev/null
            fi
        done
        
        # Kill DPF process with proper signal handling
        if [[ -n "$DPF_PID" ]] && kill -0 "$DPF_PID" 2>/dev/null; then
            echo "Stopping DPF process $DPF_PID with SIGINT"
            sudo kill -SIGINT "$DPF_PID" 2>/dev/null
            sleep 3
            # Check if process is still running and use SIGTERM as fallback
            if kill -0 "$DPF_PID" 2>/dev/null; then
                echo "DPF process still running, using SIGTERM"
                sudo kill -TERM "$DPF_PID" 2>/dev/null
                sleep 2
            fi
        fi
        
        echo "Cleanup completed"
    fi
    
    # Restore original turbo state if it was saved
    if [[ -n "$ORIGINAL_TURBO_STATE" ]]; then
        restore_turbo_state
    fi
}

# Set up signal traps for proper cleanup
trap cleanup EXIT
trap 'echo "WARNING: Interrupted by user"; cleanup; exit 130' INT
trap 'echo "WARNING: Terminated"; cleanup; exit 143' TERM

#################################################################################
# Single Benchmark Runner with Dynamic Prefetching Framework (DPF)
#
# Purpose: Runs a single benchmark with DPF enabled
#          for performance testing and analysis
#
# Usage: ./run_single_benchmark.sh [OPTIONS]
#
# Output: Results in timestamped directory under results/report/
#################################################################################

#################################################################################

#### 1. Argument Parsing and Help ###############################################

show_help() {
    cat << EOF
Usage: $0 [OPTIONS]

Run a single Integer benchmark with or without Dynamic Prefetching Framework (DPF).

OPTIONS:
    -b, --benchmark BENCHMARK    Specify which benchmark to run (baseline)
    --dpf                        Add to a benchmark to run it with DPF
    --iterations N               Number of iterations to run (default: 5)
    -v, --verbose                Enable verbose output
    -h, --help                   Show this help message
    -l, --list                   List available benchmarks

AVAILABLE BENCHMARKS:
    600.perlbench    - Perl interpreter
    602.gcc          - GNU C Compiler  
    605.mcf          - Network flow solver
    620.omnetpp      - Network simulation
    623.xalancbmk    - XML transformation
    625.x264         - Video compression
    631.deepsjeng    - Chess engine
    641.leela        - Go game engine
    648.exchange2    - Artificial intelligence
    657.xz           - Data compression

EXAMPLES:
    $0                                 # Run default benchmark (623.xalancbmk) in baseline mode, 5 iterations
    $0 --dpf                           # Run default benchmark with DPF enabled
    $0 -b 602.gcc                      # Run GCC benchmark in baseline mode
    $0 --benchmark 625.x264 --dpf      # Run x264 with DPF enabled
    $0 -b 623.xalancbmk --iterations 3 # Run xalancbmk with 3 iterations (custom)

OUTPUT:
    Results are saved to timestamped directory under results/reports/
    Standard mode results saved with mode suffix (e.g., '_quick', '_standard')
    DPF mode results saved with '_dpf' suffix
    
EOF
}

# Parse command line arguments
BASELINE_MODE=true  # Default to baseline mode (DPF disabled)
CUSTOM_ITERATIONS=""
VERBOSE=false
SHOW_LIST=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -b|--benchmark)
            BENCHMARK_NUM="$2"
            shift 2
            ;;
        --dpf)
            BASELINE_MODE=false
            shift
            ;;
        --iterations)
            CUSTOM_ITERATIONS="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        -l|--list)
            SHOW_LIST=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

#### 2. Configuration Loading ###############################################
find_config_file() {
    local current_dir=$(pwd)
    while [[ "$current_dir" != "/" ]]; do
        # Check for benchsuite.conf in current directory
        if [ -f "$current_dir/benchsuite.conf" ]; then
            echo "$current_dir/benchsuite.conf"
            return 0
        fi
        # Check for config/benchsuite.conf in current directory
        if [ -f "$current_dir/config/benchsuite.conf" ]; then
            echo "$current_dir/config/benchsuite.conf"
            return 0
        fi
        current_dir=$(dirname "$current_dir")
    done
    
    echo "Error: benchsuite.conf not found in any parent directory."
    exit 1
}

config_file_path=$(find_config_file)
source "$config_file_path"

# Derive internal directories from the simplified configuration
BENCHSUITE_ROOT="$(dirname "$(dirname "$config_file_path")")"
LOGS_DIR="${RESULTS_DIR}/logs"
ANALYSIS_DIR="${RESULTS_DIR}/analysis"  # Processed analysis outputs
DATA_DIR="${BENCHSUITE_ROOT}/data"
DPF_CONFIG="$(dirname "$DPF_BINARY")/mab_config.json"

# Export variables for any child processes or scripts
export BENCHSUITE_ROOT SPEC_CPU_DIR RESULTS_DIR LOGS_DIR ANALYSIS_DIR DATA_DIR DPF_BINARY DPF_CONFIG

#### 3. Path Validation and Setup ###########################################
dpf_binary="$DPF_BINARY"
commands_dir="$SPEC_CPU_DIR"
# Use results directory from config
results_base_dir="$RESULTS_DIR/reports"

# Validate critical files exist (silent unless errors)
if [[ "$BASELINE_MODE" == false ]]; then
    if [[ ! -f "$DPF_BINARY" ]]; then
        echo "ERROR: DPF binary not found at: $DPF_BINARY"
        exit 1
    fi
fi

if [[ ! -d "$SPEC_CPU_DIR" ]]; then
    echo "ERROR: SPEC CPU directory not found at: $SPEC_CPU_DIR"
    exit 1
fi

# Use shared timestamp from parent process if available, otherwise generate new one
if [[ -n "$SHARED_TIMESTAMP" ]]; then
    timestamp=$(echo "$SHARED_TIMESTAMP" | tr '_' '-')  # Convert format from run_all.sh
else
    timestamp=$(date +"%Y%m%d-%H%M%S")
fi

# Use proper naming based on actual run mode, not just DPF on/off
if [[ "$BASELINE_MODE" == true ]]; then
    # Standard mode (DPF disabled) - use mode-specific suffix
    if [[ -n "$RUN_MODE" ]]; then
        base_name="${timestamp}_${RUN_MODE}"
    else
        base_name="${timestamp}_standard"
    fi
else
    # DPF enabled mode
    base_name="${timestamp}_dpf"
fi

# Add note annotation if provided
if [[ -n "$NOTE" ]]; then
    results_dir="${results_base_dir}/${base_name}_${NOTE}"
else
    results_dir="${results_base_dir}/${base_name}"
fi
if ! mkdir -p "${results_dir}"; then
    echo "ERROR: Failed to create results directory: $results_dir"
    exit 1
fi

# Source spec commands from project config (use BENCHSUITE_ROOT which was set from config file)
source "${BENCHSUITE_ROOT}/config/spec_command_lines_benchmark.sh"

# Validate that benchmark_commands array was loaded
if [[ ${#benchmark_commands[@]} -eq 0 ]]; then
    echo "ERROR: No benchmark commands loaded from spec_command_lines_benchmark.sh"
    echo "Please check the file exists at: ${BENCHSUITE_ROOT}/config/spec_command_lines_benchmark.sh"
    exit 1
fi

    [[ "$VERBOSE" == true ]] && echo " Loaded ${#benchmark_commands[@]} benchmark commands"

# Helper functions for benchmark validation (must be after benchmark_commands is loaded)
validate_benchmark() {
    local key="$1"
    [[ -n "${benchmark_commands[$key]}" ]]
}

get_benchmark_command() {
    local key="$1"
    echo "${benchmark_commands[$key]}"
}

list_benchmarks() {
    echo "Available benchmarks:"
    for key in "${!benchmark_commands[@]}"; do
        echo "  $key"
    done | sort
}

# Handle --list option now that benchmarks are loaded
if [[ "$SHOW_LIST" == true ]]; then
    list_benchmarks
    exit 0
fi

#### 4. Benchmark Selection ################################################
# Default benchmark (can be overridden with -b option)
if [[ -n "$BENCHMARK_NUM" ]]; then
    # Use the simplified format (e.g., "600.perlbench")
    BENCHMARK_SELECTION="$BENCHMARK_NUM"
else
    BENCHMARK_SELECTION="623.xalancbmk"  # Default
fi

echo "Selected benchmark: $BENCHMARK_SELECTION"

#### 5. Run Parameters ######################################################
# Set iterations based on custom parameter or default
if [[ -n "$CUSTOM_ITERATIONS" ]]; then
    iterations="$CUSTOM_ITERATIONS"
else
    iterations=5           # Default for comprehensive baseline
fi

# Set baseline based on command line argument
if [[ "$BASELINE_MODE" == true ]]; then
    baseline=1         # 1 = DPF disabled (baseline mode)
else
    baseline=0         # 0 = DPF enabled
fi

#### 6. Load Configuration Parameters #######################################
if [[ -f "$config_file_path" ]]; then
    # Configuration already sourced above, just map variables
    
    # Map config file variables to script variables
    performance=${PERFORMANCE_MODE:-1}
    turbo=${TURBO_MODE:-0}
    rdpmc=${RDPMC:-1}
    
    # Parse core IDs from config
    if [[ -n "$CORE_IDS" ]]; then
        read -ra core_ids <<< "$CORE_IDS"
    else
        core_ids=(6 7 8)  # Default fallback
    fi
else
    echo "Warning: Configuration file not found at $config_file_path"
    echo "Using default values..."
    
    # Default fallback values
    performance=1
    turbo=0
    rdpmc=1
    core_ids=(6 7 8)
fi

#### 7. Helper Functions ####################################################

# DPF execution with graceful handling
start_dpf() {
    local dpf_log="$1"
    local baseline="${2:-2}"  # Default to baseline=2 if not provided
    local config_file="$3"
    local dpf_dir="$(dirname "$dpf_binary")"
    
    # Use hardcoded DPF arguments with timeout for proper termination
    echo "Starting DPF with arguments: --core $core_range --intervall 1 --ddrbw-set 46000 -l 5 -t 2" >&2
    echo "DPF working directory: $dpf_dir" >&2
    echo "DPF log file: $dpf_log" >&2
    echo "Baseline mode: $baseline" >&2
    echo "Config file: $config_file" >&2
    
    (cd "$dpf_dir" && sudo "$dpf_binary" --core "$core_range" --intervall 1 --ddrbw-set 46000 -l 5 -t 2 > "$dpf_log" 2>&1) &
    local dpf_pid=$!
    
    # Give DPF a moment to initialize
    sleep 1
    
    # Trigger DPF to begin logging
    sudo kill -SIGUSR1 "$dpf_pid"
    
    echo "$dpf_pid"
}

#### Turbo State Management Functions #######################################

read_turbo_state() {
    # Read MSR 0x1a0 (IA32_MISC_ENABLE), check bit 38
    # Bit 38 = 1: turbo disabled, Bit 38 = 0: turbo enabled
    sudo modprobe msr 2>/dev/null
    local msr_value=$(sudo rdmsr -p 0 0x1a0 2>/dev/null)
    
    if [[ -z "$msr_value" ]]; then
        echo "WARNING: Could not read turbo state from MSR" >&2
        return 1
    fi
    
    # Check bit 38 (0x4000000000 in hex)
    local bit38=$(( (0x$msr_value >> 38) & 1 ))
    
    if [[ $bit38 -eq 1 ]]; then
        echo "disabled"
    else
        echo "enabled"
    fi
}

save_and_disable_turbo() {
    # Save current state
    ORIGINAL_TURBO_STATE=$(read_turbo_state)
    echo "Original turbo state: $ORIGINAL_TURBO_STATE"
    
    # Always disable turbo for benchmarks
    sudo modprobe msr 2>/dev/null
    sudo wrmsr -a 0x1a0 0x4000850089
    echo "Turbo disabled for benchmark execution"
}

restore_turbo_state() {
    if [[ "$ORIGINAL_TURBO_STATE" == "enabled" ]]; then
        echo "Restoring turbo to enabled state"
        sudo modprobe msr 2>/dev/null
        sudo wrmsr -a 0x1a0 0x850089
    else
        echo "Turbo was originally disabled, leaving it disabled"
    fi
}

set_governor_to_performance() {
    # More efficient: use shell globbing instead of loop
    if ls /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor >/dev/null 2>&1; then
        echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor >/dev/null
    fi
}

set_governor_to_powersave() {
    if ls /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor >/dev/null 2>&1; then
        echo powersave | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor >/dev/null
        echo "Governor set to: powersave"
    fi
}

enable_rdpmc() {
    echo 2 | sudo tee /sys/bus/event_source/devices/cpu_atom/rdpmc >/dev/null
    if [ $? -eq 0 ]; then
        echo "RDPMC enabled successfully."
    else
        echo "Warning: Failed to enable RDPMC."
    fi
}

generate_core_range() {
    local min_core=${core_ids[0]}
    local max_core=${core_ids[-1]}
    
    # Check if cores are contiguous for range format optimization
    local contiguous=true
    for ((i = 1; i < ${#core_ids[@]}; i++)); do
        if ((core_ids[i] != core_ids[i - 1] + 1)); then
            contiguous=false
            break
        fi
    done
    
    if [ "$contiguous" = true ] && [ ${#core_ids[@]} -gt 2 ]; then
        # Use range format for contiguous cores (more efficient)
        echo "$min_core-$max_core"
    else
        # Use comma-separated format for non-contiguous or small lists
        local core_list=""
        for core_id in "${core_ids[@]}"; do
            if [ -z "$core_list" ]; then
                core_list="$core_id"
            else
                core_list="$core_list,$core_id"
            fi
        done
        echo "$core_list"
    fi
}

#### 8. Main Execution Logic ################################################
core_range=$(generate_core_range) || exit 1

# Save current turbo state and disable it for benchmarks
save_and_disable_turbo

# Set CPU performance settings
if [ "$performance" -eq 1 ]; then
    set_governor_to_performance
elif [ "$performance" -eq 0 ]; then
    set_governor_to_powersave
fi

[ "$rdpmc" -eq 1 ] && enable_rdpmc

run_benchmark() {
    # Use the configured benchmark
    local benchmark_key="$BENCHMARK_SELECTION"
    
    echo "===================================================================================="
    echo "Running Single Benchmark: $benchmark_key"
    echo "Timestamp: $timestamp"
    echo "Results directory: $results_dir"
    echo "===================================================================================="
    
    # Validate that the benchmark key exists in benchmark_commands
    if ! validate_benchmark "$benchmark_key"; then
        echo "ERROR: Benchmark '$benchmark_key' not found in benchmark commands array"
        echo ""
        echo "benchmarks:"
        list_benchmarks
        exit 1
    fi
    
    [[ "$VERBOSE" == true ]] && echo " Benchmark validated: $benchmark_key"
    
    # Extract benchmark number and name for directory structure
    if [[ $benchmark_key =~ ^([0-9]{3})\.(.+)$ ]]; then
        benchmark_num="${BASH_REMATCH[1]}"
        benchmark_name="${BASH_REMATCH[2]}"
    else
        echo "ERROR: Invalid benchmark key format: $benchmark_key"
        exit 1
    fi
    
    echo "Running benchmark $benchmark_num.$benchmark_name..."
    
    # Create result directory
    dir="${results_dir}/benchmark_speed/${benchmark_num}.${benchmark_name}/ref"
    if ! mkdir -p "$dir"; then
        echo "ERROR: Failed to create results directory: $dir"
        exit 1
    fi
    
    # Get command from benchmark_commands array
    command_template=$(get_benchmark_command "$benchmark_key")
    if [[ -z "$command_template" ]]; then
        echo "ERROR: Empty command for benchmark $benchmark_key"
        exit 1
    fi
    
    # Assign command for later use
    command="$command_template"
    
    # Extract binary path from command
    full_binary_path="${command_template%% *}"
    binary_name=$(basename "$full_binary_path")

        # Verify binary exists
        if [[ ! -f "$full_binary_path" ]]; then
            echo "ERROR: Binary not found at ${full_binary_path}" | tee -a "${dir}/error.log"
            echo "Please ensure SPEC CPU2017 is properly installed and binaries are compiled"
            echo "Expected binary location: $full_binary_path"
            exit 1
        fi
        
        [[ "$VERBOSE" == true ]] && echo " Binary found: $full_binary_path"

        # Set binary name for command execution
        binary_name=$(basename "$full_binary_path")

        
        # Run benchmark iterations
        for ((i=0; i<iterations; i++)); do
            iter_timestamp=$(date +"%Y%m%d-%H%M%S")
            log_file="${dir}/${iter_timestamp}.log"
            dpf_log="${dir}/dpf_${iter_timestamp}.log"
            
            [[ "$VERBOSE" == true ]] && echo "Starting iteration $((i+1))/$iterations for $benchmark_name..."
            
            # Check memory usage before starting iteration
            local mem_percent=$(free | awk 'NR==2{printf "%.0f", $3*100/$2}')
            if (( mem_percent > 85 )); then
                echo "  WARNING: High memory usage: ${mem_percent}% - waiting for memory to free up..."
                sleep 10
                # Force garbage collection and cache cleanup
                sync && echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null
                local new_mem_percent=$(free | awk 'NR==2{printf "%.0f", $3*100/$2}')
                echo "Memory usage after cleanup: ${new_mem_percent}%"
                if (( new_mem_percent > 90 )); then
                    echo " CRITICAL: Memory usage still too high (${new_mem_percent}%) - aborting iteration"
                    cleanup
                    exit 1
                fi
            fi
            
            # Start DPF only if not in baseline mode
            if [[ "$BASELINE_MODE" == false ]]; then
                # Start DPF
                local baseline_mode=2  # baseline test with logging
                local config_file="$(dirname "$dpf_binary")/mab_config.json"
                DPF_PID=$(start_dpf "$dpf_log" "$baseline_mode" "$config_file")
                echo "DPF started with PID: $DPF_PID"
            else
                echo "Running in baseline mode - DPF disabled"
                DPF_PID=""
            fi
                 
            # Extract command arguments 
            processed_command="${command#* }"  # Remove binary path from command
            
            # Run benchmark on all cores
            BENCHMARK_PIDS=()  # Reset global array
            CLEANUP_REQUIRED=true  # Enable cleanup
            
            # Extract execution directory from the command - simple approach
            # Look for input directory pattern OR run directory pattern in the 
            # command
            input_file_path=$(echo "$command_template" | grep -oE '/[^[:space:]]*/input/[^[:space:]]*|/[^[:space:]]*/run/run_base_ref[^[:space:]]*' | head -1)
            if [[ -n "$input_file_path" ]]; then
                execution_input_dir=$(dirname "$input_file_path")
            else
                # For benchmarks with no input files (like exchange2), use benchmark root
                execution_input_dir=$(dirname "$full_binary_path")
                execution_input_dir=$(dirname "$execution_input_dir")
            fi
            
            for core_id in "${core_ids[@]}"; do
                (
                    cd "$execution_input_dir"
                    echo "Running on core $core_id..."
                    
                    # Create core-specific command for benchmark execution
                    core_command="$processed_command"
                    
                    # Handle x264 stats file naming (only benchmark that needs this)
                    if [[ "$benchmark_name" == "x264" ]]; then
                        core_command="${core_command//x264_stats.log/x264_stats_core${core_id}.log}"
                    fi
                    
                    echo "Command: $full_binary_path ${core_command}"
                    
                    # Execute with proper working directory
                    taskset -c "$core_id" /usr/bin/time -v \
                    /bin/bash -lc "cd '$execution_input_dir' && $full_binary_path ${core_command}"
                    
                    # Tell DPF we're done on this core
                    if [[ "$BASELINE_MODE" == false && -n "$DPF_PID" ]]; then
                        echo "stop $core_id" | sudo socat - UNIX-SENDTO:/tmp/dpf_socket 2>/dev/null || true
                    fi
                ) > "${log_file}.core${core_id}" 2>&1 & 
                BENCHMARK_PIDS+=($!)
            done
            
            echo "Waiting for benchmark processes to complete..."
            # Wait for all core-pinned workloads to finish
            for pid in "${BENCHMARK_PIDS[@]}"; do
                wait "$pid"
            done
            
            # Final sleep to ensure all processes have settled
            sleep 5
            
            # Kill DPF process now that all cores are done 
            if [[ "$BASELINE_MODE" == false && -n "$DPF_PID" ]]; then
                if kill -0 $DPF_PID 2>/dev/null; then
                    echo "Killing DPF process (PID: $DPF_PID)"
                    sudo kill $DPF_PID 2>/dev/null
                    # Give DPF time to terminate gracefully
                    sleep 2
                    # Force kill if still running
                    if kill -0 $DPF_PID 2>/dev/null; then
                        echo "DPF still running, forcing termination"
                        sudo kill -9 $DPF_PID 2>/dev/null
                    fi
                    echo "DPF stopped"
                fi
            fi
            
            # Final sleep to ensure all processes have settled
            sleep 10
            
            # Clear process arrays after successful completion
            BENCHMARK_PIDS=()
            DPF_PID=""
            CLEANUP_REQUIRED=false
            
            # Validate iteration results
            iteration_failed=false
            for core_id in "${core_ids[@]}"; do
                log_file_core="${log_file}.core${core_id}"
                if [[ -f "$log_file_core" ]]; then
                    # Check for successful completion (exit status 0)
                    if grep -q "Exit status: 0" "$log_file_core"; then
                        echo " Core $core_id completed successfully"
                    else
                        echo " Core $core_id failed - checking error"
                        tail -5 "$log_file_core"
                        iteration_failed=true
                    fi
                else
                    echo " Log file missing for core $core_id: $log_file_core"
                    iteration_failed=true
                fi
            done
            
            if [[ "$iteration_failed" == true ]]; then
                echo "  Iteration $((i+1)) had failures but continuing..."
            fi
            
            echo "Completed iteration $((i+1))/$iterations"
            sleep 5
        done
        
        echo "All iterations completed for $benchmark_name"
    
    # Final comprehensive validation and summary
    echo ""
    echo " Benchmark execution completed"
    echo "Results saved to: $dir"
    echo "Execution directory: $execution_input_dir"
    
    # Comprehensive result validation
    total_logs=$(find "$dir" -name "*.log.core*" | wc -l)
    successful_logs=$(find "$dir" -name "*.log.core*" -exec grep -l "Exit status: 0" {} \; | wc -l)
    failed_logs=$((total_logs - successful_logs))
    
    echo " EXECUTION SUMMARY:"
    echo "   Total log files: $total_logs"
    echo "   Successful runs: $successful_logs"
    echo "   Failed runs: $failed_logs"
    
    if [[ $total_logs -eq 0 ]]; then
        echo " CRITICAL: No log files found - benchmark may not have run"
        echo "ERROR_TYPE: SETUP_FAILURE" >> "$dir/benchmark_error.log"
        echo "ERROR_DETAIL: No execution logs generated" >> "$dir/benchmark_error.log"
        echo "TIMESTAMP: $(date)" >> "$dir/benchmark_error.log"
        return 3  # Setup failure
    elif [[ $successful_logs -eq 0 ]]; then
        echo " CRITICAL: All benchmark runs failed"
        echo "Checking first error:"
        local first_error_log=$(find "$dir" -name "*.log.core*" | head -1)
        if [[ -n "$first_error_log" ]]; then
            tail -10 "$first_error_log"
            echo "ERROR_TYPE: EXECUTION_FAILURE" >> "$dir/benchmark_error.log"
            echo "ERROR_DETAIL: All iterations failed" >> "$dir/benchmark_error.log"
            echo "SAMPLE_ERROR:" >> "$dir/benchmark_error.log"
            tail -5 "$first_error_log" >> "$dir/benchmark_error.log"
        fi
        echo "TIMESTAMP: $(date)" >> "$dir/benchmark_error.log"
        return 2  # Execution failure
    elif [[ $failed_logs -gt 0 ]]; then
        echo "  WARNING: Some runs failed but $successful_logs succeeded"
        echo "Success rate: $((successful_logs * 100 / total_logs))%"
        echo "ERROR_TYPE: PARTIAL_FAILURE" >> "$dir/benchmark_error.log"
        echo "ERROR_DETAIL: $failed_logs out of $total_logs iterations failed" >> "$dir/benchmark_error.log"
        echo "SUCCESS_RATE: $((successful_logs * 100 / total_logs))%" >> "$dir/benchmark_error.log"
        echo "TIMESTAMP: $(date)" >> "$dir/benchmark_error.log"
        # Continue with partial success - don't return error
        return 2  # Partial failure exit code
    else
        echo " SUCCESS: All benchmark runs completed successfully!"
        # Remove any existing error log on success
        rm -f "$dir/benchmark_error.log" 2>/dev/null
        return 0  # Complete success
    fi
    
    # Additional file validation
    expected_iterations=$((iterations * ${#core_ids[@]}))
    if [[ $total_logs -eq $expected_iterations ]]; then
        echo "All expected iterations completed ($expected_iterations)"
    else
        echo "WARNING: Expected $expected_iterations logs, found $total_logs"
        return 3  # Warning - unexpected iteration count
    fi
}

# Execute benchmark
echo "Running $benchmark_name ($([ "$BASELINE_MODE" == true ] && echo "baseline" || echo "DPF"), $iterations iterations)"

# Execute the benchmark and capture the result
if     run_benchmark; then
    echo "SUCCESS: $benchmark_name completed"
    if [[ "$VERBOSE" == true ]]; then
        echo "Results: $results_dir"
    fi
    exit 0
else
    exit_code=$?
    echo "FAILED: $benchmark_name (exit code: $exit_code)"
    if [[ "$VERBOSE" == true ]]; then
        echo "Results: $results_dir"
        echo "Check benchmark_error.log for details"
    fi
    exit $exit_code
fi