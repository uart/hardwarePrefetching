#!/bin/bash

#################################################################################
# Run Specific DPF Benchmarks
#
# This script runs specific benchmarks with DPF enabled
#################################################################################

show_help() {
    cat << EOF
Specific DPF Benchmark Runner

DESCRIPTION:
    Runs specific DPF benchmarks with DPF (Dynamic Prefetching Framework)
    enabled. Useful for selective testing or completing missing benchmark runs.

USAGE:
    $0 [OPTIONS]

OPTIONS:
    -h, --help            Show this help message

CONFIGURATION:
    Edit the BENCHMARKS_TO_RUN array in the script to specify which benchmarks to run.
    Default benchmarks: 641.leela, 648.exchange2, 657.xz

EXAMPLES:
    $0                    # Run configured specific benchmarks with DPF

OUTPUT:
    Results are saved to timestamped directories under results/report/
    Each benchmark creates its own subdirectory with execution logs

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

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source common setup functions
source "$SCRIPT_DIR/../utils/common_setup.sh"

# Setup environment with specific DPF logging
if ! setup_benchmark_environment "benchmark_specific_dpf"; then
    exit 1
fi

START_TIME=$(date)

# Benchmarks to run (from user request)
BENCHMARKS_TO_RUN=(
    "641.leela"
    "648.exchange2"
    "657.xz"
)

echo "================================================================================"
echo " Running Specific DPF Benchmarks"
echo "================================================================================"
echo "Start time: $START_TIME"
echo "Benchmarks to run: ${BENCHMARKS_TO_RUN[@]}"
echo "Total benchmarks: ${#BENCHMARKS_TO_RUN[@]}"
echo "Log file: $LOG_FILE"
echo "Error log: $ERROR_LOG"
echo "================================================================================"

# Initialize tracking
total_benchmarks=${#BENCHMARKS_TO_RUN[@]}
completed_benchmarks=0
failed_benchmarks=0
success_list=()
failed_list=()

# Run each benchmark
for benchmark in "${BENCHMARKS_TO_RUN[@]}"; do
    echo ""
    echo "[$(($completed_benchmarks + $failed_benchmarks + 1))/$total_benchmarks] Running $benchmark at $(date)"
    echo "Validating required files and directories..."
    
    # Run WITH DPF enabled (no --baseline flag) with extended timeout protection
    if timeout 12600 sudo "$SCRIPT_DIR/run_single_benchmark.sh" --benchmark "$benchmark" >> "$LOG_FILE" 2>&1; then
        echo "[SUCCESS] $benchmark completed" | tee -a "$LOG_FILE"
        success_list+=("$benchmark")
        ((completed_benchmarks++))
    else
        exit_code=$?
        echo " FAILED: $benchmark failed (exit code: $exit_code)" | tee -a "$LOG_FILE" | tee -a "$ERROR_LOG"
        failed_list+=("$benchmark")
        ((failed_benchmarks++))
        
        # Log specific failure details
        echo "=== FAILURE DETAILS FOR $benchmark ===" >> "$ERROR_LOG"
        echo "Timestamp: $(date)" >> "$ERROR_LOG"
        echo "Exit code: $exit_code" >> "$ERROR_LOG"
        
        # Check for timeout vs other failures
        if [ $exit_code -eq 124 ]; then
            echo "Failure type: TIMEOUT (exceeded 2 hour limit)" >> "$ERROR_LOG"
        else
            echo "Failure type: EXECUTION ERROR" >> "$ERROR_LOG"
        fi
        echo "" >> "$ERROR_LOG"
        
        echo "  Continuing with remaining benchmarks..."
    fi
    
    echo "Progress: $((completed_benchmarks + failed_benchmarks))/$total_benchmarks completed"
done

# Final summary
END_TIME=$(date)
echo ""
echo "================================================================================"
echo " DPF Benchmark Collection Summary"
echo "================================================================================"
echo "Start time: $START_TIME"
echo "End time: $END_TIME"
echo "Total benchmarks: $total_benchmarks"
echo "Successful: $completed_benchmarks"
echo "Failed: $failed_benchmarks"
echo ""

if [ ${#success_list[@]} -gt 0 ]; then
    echo " Successful benchmarks:"
    for benchmark in "${success_list[@]}"; do
        echo "   - $benchmark"
    done
fi

if [ ${#failed_list[@]} -gt 0 ]; then
    echo ""
    echo " Failed benchmarks:"
    for benchmark in "${failed_list[@]}"; do
        echo "   - $benchmark"
    done
fi

echo ""
echo "Log files:"
echo "   Main log: $LOG_FILE"
echo "   Error log: $ERROR_LOG"
echo "================================================================================"

# Exit with appropriate code but don't fail the overall pipeline
if [ $failed_benchmarks -eq $total_benchmarks ]; then
    echo "  All benchmarks failed"
    exit 1
elif [ $failed_benchmarks -gt 0 ]; then
    echo "  Some benchmarks failed but continuing pipeline"
    exit 0
else
    echo " All benchmarks completed successfully!"
    exit 0
fi
