#!/bin/bash

#################################################################################
# Complete Benchmark DPF Runner
#
# This script runs ALL Integer benchmarks with DPF enabled
# for comprehensive performance comparison against baseline data.
#
# Designed for unattended execution - follows exact same structure as baseline runner.
#################################################################################

show_help() {
    cat << EOF
 DPF Benchmark Suite Runner

DESCRIPTION:
    Runs ALL  Integer benchmarks with DPF (Dynamic Prefetching Framework)
    enabled for comprehensive performance comparison against baseline data.

USAGE:
    $0 [OPTIONS]

OPTIONS:
    -h, --help            Show this help message

ENVIRONMENT VARIABLES:
    RUN_MODE             Controls execution mode:
                         - "quick": Run xalancbmk only (1 iteration)
                         - "dpf": Run all benchmarks, 1 iteration each

EXAMPLES:
    $0                    # Run all benchmarks with DPF in current mode
    RUN_MODE=quick $0     # Run xalancbmk with DPF only

OUTPUT:
    Results are saved to timestamped directories under results/report/
    DPF mode results saved with standard timestamp (no suffix)

REQUIREMENTS:
    - DPF kernel module must be loaded: sudo modprobe dpf
    - DPF binary must be available at configured path
    - Root privileges required for benchmark execution

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

# Setup environment with DPF-specific logging
if ! setup_benchmark_environment "benchmark_dpf"; then
    exit 1
fi

START_TIME=$(date)

# All benchmark tests (same as baseline script)
BENCHMARK_TESTS=(
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
    [[ "$VERBOSE" == true ]] && echo "QUICK mode: running xalancbmk benchmark only with DPF"
    BENCHMARK_TESTS=("623.xalancbmk")
else
    [[ "$VERBOSE" == true ]] && echo "DPF mode: running all benchmarks with DPF (1 iteration each)"
fi

echo "Starting DPF Collection ($START_TIME)"
if [[ "$VERBOSE" == true ]]; then
    echo "Total benchmarks: ${#BENCHMARK_TESTS[@]}"
    echo "Log file: $LOG_FILE"
    echo "Error log: $ERROR_LOG"
fi

# Initialize counters
total_benchmarks=${#BENCHMARK_TESTS[@]}
completed_benchmarks=0
failed_benchmarks=0
success_list=()
failure_list=()

# Run each benchmark
for benchmark in "${BENCHMARK_TESTS[@]}"; do
    echo "[$((completed_benchmarks + 1))/${#BENCHMARK_TESTS[@]}] $benchmark (DPF)" | tee -a "$LOG_FILE"
    
    # Determine command based on run mode  
    single_benchmark_cmd="$BENCHSUITE_ROOT/scripts/execution/run_single_benchmark.sh"
    if [ "$RUN_MODE" = "quick" ]; then
        # Quick mode uses 1 iteration for fast testing with DPF
        single_benchmark_cmd="$single_benchmark_cmd --benchmark $benchmark --dpf --iterations 3"
    else
        # DPF mode uses 1 iteration with DPF enabled
        single_benchmark_cmd="$single_benchmark_cmd --benchmark $benchmark --dpf --iterations 1"
    fi
    
    # Add verbose flag if enabled
    if [ "$VERBOSE" = true ]; then
        single_benchmark_cmd="$single_benchmark_cmd --verbose"
    fi    # Run WITH DPF enabled (no --baseline flag) with timeout protection
    if timeout 12600 sudo -E $single_benchmark_cmd >> "$LOG_FILE" 2>&1; then
        echo "  ✓ $benchmark (DPF)" | tee -a "$LOG_FILE"
        success_list+=("$benchmark")
        ((completed_benchmarks++))
    else
        exit_code=$?
        echo "  ✗ $benchmark (DPF, exit $exit_code)" | tee -a "$LOG_FILE" | tee -a "$ERROR_LOG"
        
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
        echo "" >> "$ERROR_LOG"
        
        failure_list+=("$benchmark")
        ((failed_benchmarks++))
        ((completed_benchmarks++))
    fi
    
    sleep 5  # Brief pause between benchmarks
done

# Final summary
END_TIME=$(date)
echo "DPF Collection Complete"
echo "Successful: ${#success_list[@]}/${#BENCHMARK_TESTS[@]}" | tee -a "$LOG_FILE"

if [[ "$VERBOSE" == true ]]; then
    echo "Start time: $START_TIME" | tee -a "$LOG_FILE"
    echo "End time: $END_TIME" | tee -a "$LOG_FILE"
fi

if [[ ${#failure_list[@]} -gt 0 ]] && [[ "$VERBOSE" == true ]]; then
    echo "Failed benchmarks: ${failure_list[*]}" | tee -a "$LOG_FILE"
    echo "Check $ERROR_LOG for details" | tee -a "$LOG_FILE"
fi

# Exit with appropriate code
if [[ ${#success_list[@]} -eq ${#BENCHMARK_TESTS[@]} ]]; then
    exit 0
elif [[ ${#success_list[@]} -gt 0 ]]; then
    exit 0
else
    echo "ERROR: All DPF benchmarks failed" | tee -a "$LOG_FILE"
    exit 1
fi

# Final summary
END_TIME=$(date)
echo ""
echo "================================================================================"
echo " Complete Benchmark DPF Collection FINISHED"
echo "================================================================================"
echo "Start time: $START_TIME" | tee -a "$LOG_FILE"
echo "End time: $END_TIME" | tee -a "$LOG_FILE"
echo "Total benchmarks: $total_benchmarks" | tee -a "$LOG_FILE"
echo "Successful: ${#success_list[@]}" | tee -a "$LOG_FILE"
echo "Failed: ${#failure_list[@]}" | tee -a "$LOG_FILE"
echo "" | tee -a "$LOG_FILE"

if [[ ${#success_list[@]} -gt 0 ]]; then
    echo "SUCCESSFUL BENCHMARKS:" | tee -a "$LOG_FILE"
    for benchmark in "${success_list[@]}"; do
        echo "   - $benchmark" | tee -a "$LOG_FILE"
    done
    echo "" | tee -a "$LOG_FILE"
fi

if [[ ${#failure_list[@]} -gt 0 ]]; then
    echo "FAILED BENCHMARKS:" | tee -a "$LOG_FILE"
    for benchmark in "${failure_list[@]}"; do
        echo "   - $benchmark" | tee -a "$LOG_FILE"
    done
    echo "" | tee -a "$LOG_FILE"
fi

# Comprehensive summary for comparison readiness
echo "BENCHMARK COLLECTION SUMMARY:" | tee -a "$LOG_FILE"
echo "=================================" | tee -a "$LOG_FILE"
echo "Mode: DPF ENABLED" | tee -a "$LOG_FILE"
echo "Success rate: $((${#success_list[@]} * 100 / total_benchmarks))%" | tee -a "$LOG_FILE"

if [[ ${#success_list[@]} -gt 0 ]]; then
    echo "" | tee -a "$LOG_FILE"
    echo "NEXT STEPS FOR PERFORMANCE COMPARISON:" | tee -a "$LOG_FILE"
    echo "1. Extract DPF metrics: python3 extract_baseline_metrics.py --output dpf_detailed.csv --summary dpf_aggregated.csv" | tee -a "$LOG_FILE"
    echo "2. Compare performance: Use baseline_detailed.csv vs dpf_detailed.csv" | tee -a "$LOG_FILE"
    echo "3. Generate comparison plots showing DPF improvements" | tee -a "$LOG_FILE"
fi

echo ""
echo "Log files created:"
echo "  Main log: $LOG_FILE"
echo "  Error log: $ERROR_LOG"
echo ""
echo "To monitor progress: tail -f $LOG_FILE"
echo "================================================================================"

# Set exit code based on results
if [[ ${#success_list[@]} -eq 0 ]]; then
    echo "CRITICAL: No benchmarks completed successfully" | tee -a "$LOG_FILE"
    exit 1
elif [[ ${#failure_list[@]} -gt 0 ]]; then
    echo "PARTIAL SUCCESS: Some benchmarks failed but continuing..." | tee -a "$LOG_FILE"
    exit 0  # Don't fail the entire pipeline for partial success
else
    echo "COMPLETE SUCCESS: All benchmarks completed" | tee -a "$LOG_FILE"
    exit 0
fi
