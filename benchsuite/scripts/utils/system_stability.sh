#!/bin/bash

#################################################################################
# System Stability Checks and Fixes for Benchmark Runs
#
# Purpose: Prevent system crashes during long-running benchmark suites
# Issues addressed:
# - DPF kernel module instability
# - Resource monitoring and limits
# - Hardware thermal protection
# - Process cleanup and recovery
#################################################################################

# Check if DPF kernel module is properly loaded
check_dpf_module() {
    echo "Checking DPF kernel module status..."
    
    if ! lsmod | grep -q "dpf"; then
        echo "  WARNING: DPF kernel module not loaded"
        echo "Loading DPF kernel module..."
        
        if sudo modprobe dpf 2>/dev/null || sudo insmod /root/dpf/kernelmod/dpf.ko 2>/dev/null; then
            echo " DPF kernel module loaded successfully"
        else
            echo " Failed to load DPF kernel module"
            echo "Continuing without DPF - this may cause baseline-only mode"
            return 1
        fi
    else
        echo " DPF kernel module already loaded"
    fi
    return 0
}

# Monitor system resources during benchmarks
monitor_resources() {
    echo "Setting up resource monitoring..."
    
    # Set memory limits to prevent runaway processes
    ulimit -v 30000000  # 30GB virtual memory limit
    ulimit -m 25000000  # 25GB resident memory limit
    
    # Create resource monitoring log
    local monitor_log="/tmp/resource_monitor.log"
    
    # Background resource monitor
    (
        while true; do
            echo "$(date): $(free -m | grep Mem | awk '{print "Memory: " $3"/"$2 " MB used"}')" >> "$monitor_log"
            echo "$(date): $(df -h / | tail -1 | awk '{print "Disk: " $3"/"$2 " used (" $5 ")"}')" >> "$monitor_log"
            
            # Check for high memory usage
            mem_usage=$(free | grep Mem | awk '{print ($3/$2) * 100.0}')
            if (( $(echo "$mem_usage > 90" | bc -l) )); then
                echo "$(date):   HIGH MEMORY USAGE: ${mem_usage}%" >> "$monitor_log"
            fi
            
            sleep 30
        done
    ) &
    
    echo $! > /tmp/resource_monitor.pid
    echo " Resource monitoring started (PID: $(cat /tmp/resource_monitor.pid))"
}

# Clean up processes and monitoring
cleanup_system() {
    echo "Cleaning up system resources..."
    
    # Kill resource monitor
    if [ -f /tmp/resource_monitor.pid ]; then
        kill $(cat /tmp/resource_monitor.pid) 2>/dev/null
        rm -f /tmp/resource_monitor.pid
    fi
    
    # Clean up any stray benchmark processes
    sudo pkill -f "spec.*base.*" 2>/dev/null || true
    sudo pkill -f "dpf" 2>/dev/null || true
    
    # Clear any shared memory segments
    sudo ipcs -m | awk '$6 == 0 {print $2}' | xargs -r sudo ipcrm -m 2>/dev/null || true
    
    echo " System cleanup completed"
}

# Set system stability parameters
set_stability_parameters() {
    echo "Setting system stability parameters..."
    
    # Disable CPU frequency scaling during benchmarks for consistency
    echo "Setting CPU governor to performance mode..."
    # More efficient: use shell globbing and test for existence first
    if ls /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor >/dev/null 2>&1; then
        echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor >/dev/null
    else
        echo "Warning: CPU frequency scaling not available"
    fi
    
    # Enable core dumps for debugging
    ulimit -c unlimited
    echo "/tmp/core.%e.%p.%t" | sudo tee /proc/sys/kernel/core_pattern > /dev/null
    
    # Set swappiness to minimize swapping during benchmarks
    echo 1 | sudo tee /proc/sys/vm/swappiness > /dev/null
    
    echo " Stability parameters set"
}

# Main function
main() {
    case "$1" in
        "pre-run")
            echo "=== Pre-benchmark System Preparation ==="
            set_stability_parameters
            check_dpf_module
            monitor_resources
            ;;
        "post-run")
            echo "=== Post-benchmark System Cleanup ==="
            cleanup_system
            ;;
        "check")
            echo "=== System Stability Check ==="
            check_dpf_module
            free -h
            df -h /
            ;;
        *)
            echo "Usage: $0 {pre-run|post-run|check}"
            echo "  pre-run  - Prepare system before benchmark run"
            echo "  post-run - Clean up system after benchmark run"
            echo "  check    - Check current system status"
            exit 1
            ;;
    esac
}

main "$@"
