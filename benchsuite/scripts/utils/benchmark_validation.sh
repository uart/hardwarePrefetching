#!/bin/bash
# Benchmark validation and parameter checking utilities

# Validate benchmark parameters for run_benchmark function
validate_benchmark_params() {
    local bench_name="$1"
    local input_suffix="$2"
    local benchmark_description="$3"
    local run_mode="$4"
    local current_run="$5"
    local total_runs="$6"
    
    # Check for missing parameters
    if [ -z "$bench_name" ] || [ -z "$input_suffix" ] || [ -z "$benchmark_description" ] || [ -z "$run_mode" ] || [ -z "$current_run" ] || [ -z "$total_runs" ]; then
        echo "ERROR: Insufficient parameters provided to run_benchmark"
        return 1
    fi
    
    # Validate numeric parameters
    if ! [[ "$current_run" =~ ^[0-9]+$ ]] || ! [[ "$total_runs" =~ ^[0-9]+$ ]]; then
        echo "ERROR: Invalid run number parameters"
        return 1
    fi
    
    return 0
}

# Setup memory monitoring and cleanup
setup_memory_monitoring() {
    local mem_percent=$(free | awk 'NR==2{printf "%.0f", $3*100/$2}')
    if (( mem_percent > 85 )); then
        echo "WARNING: High memory usage: ${mem_percent}% - waiting for memory to free up..."
        sleep 10
        # Force garbage collection and cache cleanup
        sync && echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null
        local new_mem_percent=$(free | awk 'NR==2{printf "%.0f", $3*100/$2}')
        echo "Memory usage after cleanup: ${new_mem_percent}%"
        if (( new_mem_percent > 90 )); then
            echo "CRITICAL: Memory usage still too high (${new_mem_percent}%) - aborting iteration"
            return 1
        fi
    fi
    return 0
}

# Validate benchmark results
validate_benchmark_results() {
    local dir="$1"
    local core_ids=("${@:2}")
    
    local iteration_failed=false
    for core_id in "${core_ids[@]}"; do
        local log_file_core="${dir}/*.log.core${core_id}"
        if [[ -f $log_file_core ]]; then
            # Check for successful completion (exit status 0)
            if grep -q "Exit status: 0" "$log_file_core"; then
                echo "Core $core_id completed successfully"
            else
                echo "Core $core_id failed - checking error"
                tail -5 "$log_file_core"
                iteration_failed=true
            fi
        else
            echo "Log file missing for core $core_id: $log_file_core"
            iteration_failed=true
        fi
    done
    
    if [[ "$iteration_failed" == true ]]; then
        return 1
    fi
    return 0
}

# Generate comprehensive result summary
generate_result_summary() {
    local dir="$1"
    local benchmark_name="$2"
    local iterations="$3"
    local core_ids=("${@:4}")
    
    echo ""
    echo "Benchmark execution completed"
    echo "Results saved to: $dir"
    
    # Comprehensive result validation
    local total_logs=$(find "$dir" -name "*.log.core*" | wc -l)
    local successful_logs=$(find "$dir" -name "*.log.core*" -exec grep -l "Exit status: 0" {} \; | wc -l)
    local failed_logs=$((total_logs - successful_logs))
    
    echo "EXECUTION SUMMARY:"
    echo "  Total log files: $total_logs"
    echo "  Successful runs: $successful_logs"
    echo "  Failed runs: $failed_logs"
    
    # Determine return code based on results
    if [[ $total_logs -eq 0 ]]; then
        echo "CRITICAL: No log files found - benchmark may not have run"
        echo "ERROR_TYPE: SETUP_FAILURE" >> "$dir/benchmark_error.log"
        echo "ERROR_DETAIL: No execution logs generated" >> "$dir/benchmark_error.log"
        echo "TIMESTAMP: $(date)" >> "$dir/benchmark_error.log"
        return 3  # Setup failure
    elif [[ $successful_logs -eq 0 ]]; then
        echo "CRITICAL: All benchmark runs failed"
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
        echo "WARNING: Some runs failed but $successful_logs succeeded"
        echo "Success rate: $((successful_logs * 100 / total_logs))%"
        echo "ERROR_TYPE: PARTIAL_FAILURE" >> "$dir/benchmark_error.log"
        echo "ERROR_DETAIL: $failed_logs out of $total_logs iterations failed" >> "$dir/benchmark_error.log"
        echo "SUCCESS_RATE: $((successful_logs * 100 / total_logs))%" >> "$dir/benchmark_error.log"
        echo "TIMESTAMP: $(date)" >> "$dir/benchmark_error.log"
        return 2  # Partial failure exit code
    else
        echo "SUCCESS: All benchmark runs completed successfully!"
        # Remove any existing error log on success
        rm -f "$dir/benchmark_error.log" 2>/dev/null
        return 0  # Complete success
    fi
}