#!/bin/bash
# Benchmark execution workflow management

# Execute baseline workflow
execute_baseline_workflow() {
    local run_mode="$1"
    local verbose="$2"
    local log_file="$3"
    local script_dir="$4"
    
    # Export run_mode so downstream scripts can use it for directory naming
    export RUN_MODE="$run_mode"
    
    # Call the original function to preserve all progress messages
    if run_baseline_benchmarks; then
        print_success "Baseline benchmarks completed successfully"
        return 0
    else
        print_warning "Baseline suite had failures"
        return 1
    fi
}

# Execute DPF workflow
execute_dpf_workflow() {
    local run_mode="$1"
    local verbose="$2"
    local log_file="$3"
    local script_dir="$4"
    
    # Export run_mode so downstream scripts can use it for directory naming
    export RUN_MODE="$run_mode"
    
    # Call the original function to preserve all progress messages
    if run_dpf_benchmarks; then
        print_success "DPF benchmarks completed successfully"
        return 0
    else
        print_warning "DPF suite had failures"
        return 1
    fi
}

# Execute standard configuration workflow
execute_standard_workflow() {
    local run_mode="$1"
    local verbose="$2"
    local log_file="$3"
    local script_dir="$4"
    local dpf_enabled="$5"
    
    # Export run_mode so downstream scripts can use it for directory naming
    export RUN_MODE="$run_mode"
    
    # Call the original function to preserve all progress messages
    if run_standard_benchmarks; then
        print_success "Standard configuration benchmarks completed successfully"
        return 0
    else
        print_warning "Standard configuration had failures"
        return 1
    fi
}

# REMOVED: check_existing_baseline function
# Analysis and baseline checking should be handled separately by compare_performance.py
# Execution workflows should focus only on running benchmarks successfully

# Determine completion status and return exit code
determine_completion_status() {
    local run_mode="$1"
    local baseline_success="$2"
    local dpf_success="$3"
    local current_config_success="$4"
    local project_root="$5"
    local log_file="$6"
    
    # Final completion message based on what actually succeeded
    if [ "$run_mode" = "baseline" ]; then
        if [ "$baseline_success" = true ]; then
            print_success "Baseline benchmarks completed successfully!"
            print_info "Results location: $project_root/results/reports/"
            return 0
        else
            print_error "Baseline benchmarks failed!"
            print_info "Check logs for details: $log_file"
            return 1
        fi
    elif [ "$current_config_success" = true ] || [ "$dpf_success" = true ]; then
        print_success "Benchmark execution completed successfully!"
        print_info "Results location: $project_root/results/reports/"
        return 0
    elif [ "$baseline_success" = true ]; then
        print_warning "Benchmark execution completed with baseline only"
        print_warning "Current configuration testing failed - check logs for details"
        print_info "Results location: $project_root/results/reports/"
        print_info "Log file: $log_file"
        return 0
    else
        print_error "Benchmark execution failed - no benchmarks completed successfully!"
        print_info "Check logs for details: $log_file"
        return 1
    fi
}