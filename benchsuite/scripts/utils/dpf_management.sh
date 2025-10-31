#!/bin/bash
# DPF process management utilities

# Start DPF process for benchmark execution
start_dpf_process() {
    local dpf_binary="$1"
    local core_range="$2"
    local dpf_log="$3"
    local baseline_mode="$4"
    
    if [[ "$baseline_mode" == false ]]; then
        sudo "$dpf_binary" --core "$core_range" --intervall 1 --ddrbw-set 46000 -l 5 -t 2 > "$dpf_log" 2>&1 &
        local dpf_pid=$!
        sleep 1
        echo "DPF started with PID: $dpf_pid"
        echo "$dpf_pid"
        return 0
    else
        echo "Running in baseline mode - DPF disabled"
        echo ""
        return 0
    fi
}

# Stop DPF process safely
stop_dpf_process() {
    local dpf_pid="$1"
    local baseline_mode="$2"
    
    if [[ "$baseline_mode" == false && -n "$dpf_pid" ]]; then
        if kill -0 "$dpf_pid" 2>/dev/null; then
            sudo kill "$dpf_pid"
            wait "$dpf_pid" 2>/dev/null
            echo "DPF stopped"
        fi
    fi
}

# Execute benchmark on multiple cores
execute_benchmark_cores() {
    local work_dir="$1"
    local binary_name="$2"
    local processed_command="$3"
    local benchmark_name="$4"
    local log_file="$5"
    local core_ids=("${@:6}")
    
    BENCHMARK_PIDS=()  # Reset global array
    CLEANUP_REQUIRED=true  # Enable cleanup
    
    for core_id in "${core_ids[@]}"; do
        (
            cd "$work_dir"
            chmod +x "./${binary_name}"
            echo "Running on core $core_id..."
            
            # Create core-specific command
            local core_command="$processed_command"
            
            # Handle x264 stats file naming (only benchmark that needs this)
            if [[ "$benchmark_name" == "x264" ]]; then
                core_command="${core_command//x264_stats.log/x264_stats_core${core_id}.log}"
            fi
            
            echo "Command: ./${binary_name} ${core_command}"
            
            taskset -c "$core_id" /usr/bin/time -v \
            /bin/bash -lc "./${binary_name} ${core_command}"
        ) > "${log_file}.core${core_id}" 2>&1 & 
        BENCHMARK_PIDS+=($!)
    done
    
    echo "Waiting for benchmark processes to complete..."
    wait "${BENCHMARK_PIDS[@]}"
    
    # Clear process arrays after successful completion
    BENCHMARK_PIDS=()
    CLEANUP_REQUIRED=false
    
    return 0
}