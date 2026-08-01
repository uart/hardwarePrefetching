#!/bin/bash
# Common Setup Functions
# Shared utilities for path configuration and environment setup

setup_benchmark_environment() {
    local log_prefix="${1:-benchmark}"
    
    # Calculate paths
    local script_dir="$(cd "$(dirname "${BASH_SOURCE[1]}")" && pwd)"
    local suite_root="$(dirname "$(dirname "$script_dir")")"
    
    # Load configuration
    local config_file="$suite_root/config/benchsuite.conf"
    if [ ! -f "$config_file" ]; then
        echo "ERROR: Configuration file not found: $config_file" >&2
        return 1
    fi
    
    source "$config_file"
    
    # Derive internal directories from the simplified configuration
    export BENCHSUITE_ROOT="$suite_root"
    export LOGS_DIR="${RESULTS_DIR}/logs"
    export ANALYSIS_DIR="${RESULTS_DIR}/analysis"  # Processed analysis outputs
    export DATA_DIR="${RESULTS_DIR}/data"
    export DPF_CONFIG="$(dirname "$DPF_BINARY")/mab_config.json"
    
    # Export main configuration variables
    export SPEC_CPU_DIR RESULTS_DIR DPF_BINARY
    
    # Setup log files
    export LOG_FILE="${LOGS_DIR}/${log_prefix}_$(date +%Y%m%d_%H%M%S).log"
    export ERROR_LOG="${LOGS_DIR}/${log_prefix}_errors_$(date +%Y%m%d_%H%M%S).log"
    
    # Create logs directory if it doesn't exist
    if ! mkdir -p "$(dirname "$LOG_FILE")"; then
        echo "ERROR: Failed to create logs directory: $(dirname "$LOG_FILE")" >&2
        echo "Check permissions and disk space" >&2
        return 1
    fi
    
    return 0
}