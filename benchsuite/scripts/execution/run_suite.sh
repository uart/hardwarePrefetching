#!/bin/bash

#################################################################################
# Complete Baseline Runner
# 
# This script runs ALL Integer benchmarks in baseline mode
#        for comprehensive performance baseline data collection.
#
#################################################################################

show_help() {
    cat << EOF
Baseline Benchmark Suite Runner

DESCRIPTION:
    Runs ALL Integer benchmarks in baseline mode
    for comprehensive performance baseline data collection.

USAGE:
    $0 [OPTIONS]

OPTIONS:
    -h, --help            Show this help message

ENVIRONMENT VARIABLES:
    RUN_MODE             Controls execution mode:
                         - "quick": Run xalancbmk only (1 iteration)
                         - "default": Run all benchmarks, 5 iterations each (default)
                         - "full": Run all benchmarks, 1 iteration each
                         - "dpf": Run all benchmarks, 1 iteration each

EXAMPLES:
    $0                    # Run all benchmarks in current mode
    RUN_MODE=quick $0     # Run xalancbmk only
    RUN_MODE=full $0      # Run 1 iteration for quick testing

OUTPUT:
    Results are saved to timestamped directories under results/reports/
    Directory names include mode suffix (e.g., '_quick', '_standard', '_baseline')

EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source common setup functions
source "$SCRIPT_DIR/../utils/common_setup.sh"

# Setup environment with mode-specific logging
log_prefix="benchmark_${RUN_MODE:-standard}"
if ! setup_benchmark_environment "$log_prefix"; then
    exit 1
fi

START_TIME=$(date)

# All benchmarks (default set)
BENCHMARK_LIST=(
    "600.perlbench"
    "602.gcc" 
    "605.mcf"
    "620.omnetpp"
    "623.xalancbmk"
    "625.x264"
    "631.deepsjeng"
    "641.leela"
    "648.exchange2"
    "657.xz"
)

# Run mode support - determine which benchmarks to run and iterations
if [ "$RUN_MODE" = "quick" ]; then
    [[ "$VERBOSE" == true ]] && echo "QUICK mode: running xalancbmk benchmark only (1 iteration)"
    BENCHMARK_LIST=("623.xalancbmk")
elif [ "$RUN_MODE" = "full" ] || [ "$RUN_MODE" = "dpf" ]; then
    [[ "$VERBOSE" == true ]] && echo "Running all benchmarks with 1 iteration"
else
    [[ "$VERBOSE" == true ]] && echo "Running all benchmarks with 5 iterations for comprehensive analysis"
fi

echo "Starting Baseline Collection ($START_TIME)"
if [[ "$VERBOSE" == true ]]; then
    echo "Total benchmarks: ${#BENCHMARK_LIST[@]}"
    echo "Log file: $LOG_FILE"
    echo "Error log: $ERROR_LOG"
fi

# Initialize counters
total_benchmarks=${#BENCHMARK_LIST[@]}
completed_benchmarks=0
failed_benchmarks=0
success_list=()
failure_list=()

# Export RUN_MODE for run_single_benchmark.sh to use in directory naming
export RUN_MODE

# Run each benchmark
for benchmark in "${BENCHMARK_LIST[@]}"; do
    echo "[$((completed_benchmarks + 1))/${#BENCHMARK_LIST[@]}] $benchmark" | tee -a "$LOG_FILE"
    
        # Determine command based on run mode  
    single_benchmark_cmd="$BENCHSUITE_ROOT/scripts/execution/run_single_benchmark.sh"
    if [ "$RUN_MODE" = "quick" ]; then
        # Quick mode uses 1 iterations for fast testing
        single_benchmark_cmd="$single_benchmark_cmd --benchmark $benchmark --iterations 1"
    elif [ "$RUN_MODE" = "baseline" ]; then
        # Baseline mode uses 5 iterations for comprehensive analysis
        single_benchmark_cmd="$single_benchmark_cmd --benchmark $benchmark --iterations 5"
    else
        # Full mode uses 1 iteration 
        single_benchmark_cmd="$single_benchmark_cmd --benchmark $benchmark --iterations 1"
    fi
    
    # Add verbose flag if enabled
    if [ "$VERBOSE" = true ]; then
        single_benchmark_cmd="$single_benchmark_cmd --verbose"
    fi
    
    if timeout 12600 sudo -E $single_benchmark_cmd >> "$LOG_FILE" 2>&1; then
        echo "  ✓ $benchmark" | tee -a "$LOG_FILE"
        success_list+=("$benchmark")
        ((completed_benchmarks++))
    else
        exit_code=$?
        echo "  ✗ $benchmark (exit $exit_code)" | tee -a "$LOG_FILE" | tee -a "$ERROR_LOG"
        
        # Log specific failure details
        echo "=== FAILURE DETAILS FOR $benchmark ===" >> "$ERROR_LOG"
        echo "Timestamp: $(date)" >> "$ERROR_LOG"
        echo "Exit code: $exit_code" >> "$ERROR_LOG"
        
        # Check for timeout vs other failures
        if [ $exit_code -eq 124 ]; then
            echo "Failure type: TIMEOUT (exceeded 3.5 hour limit)" >> "$ERROR_LOG"
        else
            echo "Failure type: EXECUTION ERROR" >> "$ERROR_LOG"
        fi
        
        # Try to capture the last few lines of error from the log
        echo "Last error lines from log:" >> "$ERROR_LOG"
        tail -10 "$LOG_FILE" >> "$ERROR_LOG" 2>/dev/null
        echo "========================================" >> "$ERROR_LOG"
        failure_list+=("$benchmark")
        ((failed_benchmarks++))
        ((completed_benchmarks++))
    fi
    
    sleep 5  # Brief pause between benchmarks
done

# Final summary
END_TIME=$(date)
echo "Baseline Collection Complete"
echo "Successful: ${#success_list[@]}/${#BENCHMARK_LIST[@]}" | tee -a "$LOG_FILE"

if [[ "$VERBOSE" == true ]]; then
    echo "Start time: $START_TIME" | tee -a "$LOG_FILE"
    echo "End time: $END_TIME" | tee -a "$LOG_FILE"
fi

if [[ ${#failure_list[@]} -gt 0 ]] && [[ "$VERBOSE" == true ]]; then
    echo "Failed benchmarks: ${failure_list[*]}" | tee -a "$LOG_FILE"
    echo "Check $ERROR_LOG for details" | tee -a "$LOG_FILE"
fi

# Exit with appropriate code
if [[ ${#success_list[@]} -eq ${#BENCHMARK_LIST[@]} ]]; then
    exit 0
elif [[ ${#success_list[@]} -gt 0 ]]; then
    exit 0
else
    echo "ERROR: All benchmarks failed" | tee -a "$LOG_FILE"
    exit 1
fi
