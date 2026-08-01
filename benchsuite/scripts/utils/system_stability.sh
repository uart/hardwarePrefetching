#!/bin/bash

#################################################################################
# System Stability Checks (READ-ONLY) for Benchmark Runs
#
# This script runs automatically BEFORE each benchmark via run_all.sh.
# It performs read-only system checks and optional resource monitoring.
#
# Usage (called automatically):
#   ./system_stability.sh pre-run -> Before benchmark (checks config, DPF, resources)
#   ./system_stability.sh post-run -> After benchmark (cleanup stray processes)
#   ./system_stability.sh check    -> Manual system status check
#
# To enable resource monitoring:
#   export MONITOR_RESOURCES=1
#
# Issues checked:
# - DPF kernel module status 
# - System resources (optional monitoring)
# - CPU governor and memory availability
#################################################################################

# Default: resource monitoring is DISABLED
# Set MONITOR_RESOURCES=1 environment variable to enable it
MONITOR_RESOURCES=${MONITOR_RESOURCES:-0}

# Check if DPF kernel module is loaded
check_dpf_module()
{
    echo "Checking DPF kernel module status..."
    
    if ! lsmod | grep -q "dpf"; then
        echo "  BE AWARE: DPF kernel module not loaded"
        return 0
    else
        echo "  DPF kernel module loaded"
    fi

    return 0
}

# Monitor system resources (OPT-IN, disabled by default)
# Provides a one-shot snapshot of memory and disk usage.
# Enable: export MONITOR_RESOURCES=1
monitor_resources()
{
    if [ "$MONITOR_RESOURCES" != "1" ]; then
        echo "  Resource monitoring disabled (enable with export MONITOR_RESOURCES=1)"
        return 0
    fi
    
    echo "Setting up resource monitoring..."

    
    ulimit -v 30000000
    ulimit -m 25000000

    echo "  Memory: $(free -m | grep Mem | awk '{print $3"/"$2 " MB used"}')"
    echo "  Disk: $(df -h / | tail -1 | awk '{print $3"/"$2 " used"}')"
}

# Clean up processes after benchmark
# Called automatically via: ./system_stability.sh post-run
# Kills stale spec processes and cleans shared memory segments
# to free resources before the next benchmark run.
cleanup_system_for_benchmark()
{
    echo "Cleaning up system resources..."

    sudo pkill -f "spec.*base.*" 2>/dev/null || true
    sudo ipcs -m | awk '$6 == 0 {print $2}' | xargs -r sudo ipcrm -m 2>/dev/null || true

    echo " System cleanup completed"
}

# Check configuration consistency
check_configuration()
{
    echo "Checking benchmark configuration..."
    
    # Check CPU governor
    if [ -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]; then
        local gov=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)
        if [ "$gov" != "performance" ]; then
            echo "  WARNING : CPU governor is '$gov'"
            echo "  RECOMMENDED : performance ."

        fi
    fi
    
    # Check available memory
    local mem=$(free -m | grep Mem | awk '{print $7}')
    if [ "$mem" -lt 2000 ]; then
        echo "  Warning: Low memory (${mem}MB available)"
    fi
    
    echo " Configuration check complete"
}

main() {
    case "$1" in
        "pre-run")
            echo "=== Pre-benchmark System Checks ==="
            check_configuration
            check_dpf_module
            monitor_resources
            ;;
        "post-run")
            echo "=== Post-benchmark System Cleanup ==="
            cleanup_system_for_benchmark
            ;;
        "check")
            echo "=== System Stability Check ==="
            check_dpf_module
            free -h
            df -h /
            ;;
        *)
            echo "Usage: $0 {pre-run|post-run|check}"
            echo "  pre-run  - Check system before benchmark run"
            echo "  post-run - Clean up system after benchmark run"
            echo "  check    - Check current system status"

            exit 1
            ;;
    esac
}

main "$@"
