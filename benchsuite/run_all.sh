#!/bin/bash
#
# This script automates the entire workflow:
# 1. Runs baseline benchmarks 
# 2. Runs DPF benchmarks (with DPF enabled)
# 3. Reports benchmark execution status
# Note: Performance analysis is handled separately via compare_performance.py
#################################################################################

# Source utility functions for workflow management
source "scripts/utils/workflow_management.sh" 2>/dev/null || true

# Script configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
# LOG_FILE will be set after configuration is loaded

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Configuration flags
RUN_MODE="full"       # Default to full mode (1 iteration × all benchmarks)
DPF_ENABLED=false     # Whether to run DPF analysis
VERBOSE=false         # Verbose output flag

#################################################################################
# Helper Functions
#################################################################################

print_header() {
    echo -e "${BLUE}================================================================================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}================================================================================================${NC}"
}

print_section() {
    echo -e "\n${GREEN}>>> $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}WARNING: $1${NC}"
}

print_error() {
    echo -e "${RED}ERROR: $1${NC}"
}

print_success() {
    echo -e "${GREEN}SUCCESS: $1${NC}"
}

print_info() {
    if [[ "$VERBOSE" == true ]]; then
        echo -e "${BLUE}INFO: $1${NC}"
    fi
}

log_and_print() {
    if [[ -n "$LOG_FILE" ]]; then
        echo "$1" | tee -a "$LOG_FILE"
    else
        echo "$1"
    fi
}

show_benchmark_list() {
    cat << EOF
Available Benchmarks:

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

USAGE:
    $0 --benchmark BENCHMARK_NAME [--iterations N] [--dpf] [--verbose]

EXAMPLES:
    $0 --benchmark 623.xalancbmk
    $0 --benchmark 602.gcc --iterations 3
    $0 --benchmark 641.leela --dpf --verbose

EOF
}

show_config() {
    local config_file="$PROJECT_ROOT/config/benchsuite.conf"
    cat << EOF
Current Configuration Parameters:

EOF
    if [[ -f "$config_file" ]]; then
        source "$config_file"
        cat << EOF
PATHS:
    SPEC CPU Directory:    $SPEC_CPU_DIR
    Results Directory:     $RESULTS_DIR
    DPF Binary:           $DPF_BINARY
    Reference Baseline:    ${REFERENCE_BASELINE:-"(not set)"}

CORE & CPU SETTINGS:
    Use All Cores:        $USE_ALL_CORES
    Performance Mode:     $PERFORMANCE_MODE
    Turbo Mode:          $TURBO_MODE
    RDPMC:               $RDPMC
    Core IDs:            $CORE_IDS

DPF LOGGING:
    Log Arms:            $LOG_ARMS
    Log IPC:             $LOG_IPC
    Log Bandwidth:       $LOG_BW

MAB PARAMETERS:
    Epsilon:             $EPSILON
    Gamma:               $GAMMA
    C (learning rate):   $C
    Arm Configuration:   $ARM_CONFIGURATION
    Reward Type:         $REWARD

Configuration loaded from: $config_file
EOF
    else
        echo "ERROR: Configuration file not found at $config_file"
    fi
}

show_help() {
    cat << EOF
Benchmark Performance Analysis Suite

DESCRIPTION:
    Automated benchmark execution suite for running benchmarks with different configurations.
    Use scripts/analysis/compare_performance.py for performance analysis after benchmarks complete.

USAGE:
    $0 [MODE] [FLAGS]

OPTIONS:
    --baseline             Run comprehensive baseline (5 iterations across all benchmarks)
    --full                 Run full suite (1 iteration across all benchmarks) - this is the default
    --quick                Run development test (1 iteration across xalancbmk only)
    --benchmark BENCHMARK  Run single benchmark (specify benchmark name)
    --iterations N         Number of iterations for single benchmark (default: 5)
    --dpf                  Add dpf analysis to any mode
    --verbose              Enable verbose output
    -l, --list             List available benchmarks
    --config               Show current configuration parameters
    --set-baseline         Set the most recent run as reference baseline
    --baseline-help        Show baseline management help
    -h, --help             Show this help message

MODES:
    (default)            FULL MODE: 1 iteration across all benchmarks
                         * Estimated time: 12-24 hours
                         * Purpose: Standard benchmarking
    
    --baseline           COMPREHENSIVE MODE: 5 iterations across all benchmarks  
                         * Estimated time: 3 days
                         * Purpose: Stable baseline for comparison

    --quick              DEVELOPMENT MODE: 1 iteration across xalancbmk only
                         * Estimated time: 5 minutes
                         * Purpose: Quick testing

    --benchmark NAME     SINGLE BENCHMARK MODE: Run specific benchmark
                         * Estimated time: 1-3 hours per benchmark
                         * Purpose: Individual benchmark testing

FLAGS:
    --dpf                Add DPF configuration analysis to any mode 
                         * Runs baseline + DPF configuration + comparison
    
    --verbose            Detailed output (can be combined with any mode)

EXAMPLES:
    $0                        # Full suite: 1 iteration across all benchmarks (12-24 hours)
    $0 --baseline             # Comprehensive: 5 iterations across all benchmarks (3 days)
    $0 --quick                # Development: 1 iteration across xalancbmk only (5 minutes)
    $0 --benchmark 602.gcc    # Single benchmark: GCC (5 iterations, 1-3 hours)
    $0 --benchmark 623.xalancbmk --iterations 3  # Single benchmark with custom iterations
    $0 --dpf                  # Full suite + dpf analysis
    $0 --baseline --dpf       # Comprehensive + dpf analysis
    $0 --quick --dpf          # Quick test + dpf analysis
    $0 --benchmark 641.leela --dpf  # Single benchmark with DPF analysis
    $0 --list                 # List available benchmarks
    $0 --verbose              # Full suite with detailed output

WORKFLOW:
    1. Run baseline benchmarks 
    2. Run current configuration benchmarks  
    3. Report benchmark execution status
    
    For analysis: Run scripts/analysis/compare_performance.py after benchmarks complete

REQUIREMENTS:
    - SPEC CPU2017 installed and configured
    - Root privileges for benchmark execution
    - Python 3.6+ with pandas, matplotlib, numpy
    - Sufficient disk space (~500MB for complete analysis)

EOF
}

check_prerequisites() {
    print_section "Checking Prerequisites"
    
    # Check if we're in the right directory
    if [ ! -f "$PROJECT_ROOT/1-README.md" ] || [ ! -d "$PROJECT_ROOT/scripts" ]; then
        print_error "Please run this script from the project root directory"
        exit 1
    fi
    
    # Check for required directories
    for dir in "config" "scripts/execution" "scripts/analysis" "results"; do
        if [ ! -d "$PROJECT_ROOT/$dir" ]; then
            print_error "Required directory not found: $dir"
            exit 1
        fi
    done
    
    # Check for required scripts
    for script in "scripts/execution/run_suite.sh" "scripts/execution/run_dpf_suite.sh" "scripts/analysis/compare_performance.py"; do
        if [ ! -f "$PROJECT_ROOT/$script" ]; then
            print_error "Required script not found: $script"
            exit 1
        fi
    done
    
    # Check Python dependencies
    if ! python3 -c "import pandas, matplotlib, numpy" 2>/dev/null; then
        echo "Installing Python dependencies..."
        pip3 install -r python-packages.txt || {
            print_error "Failed to install Python dependencies"
            exit 1
        }
    fi
    
    # Check benchmark installation 
    if [ ! -d "$SPEC_CPU_DIR" ]; then
        print_error "Benchmark directory not found at: $SPEC_CPU_DIR"
        print_error "Please update SPEC_CPU_DIR in config/benchsuite.conf"
        exit 1
    fi
    
    # Run system stability check
    if [ -f "$PROJECT_ROOT/scripts/utils/system_stability.sh" ]; then
        if ! sudo "$PROJECT_ROOT/scripts/utils/system_stability.sh" pre-run >/dev/null 2>&1; then
            print_warning "System stability check found issues - continuing with caution"
        fi
    fi
}

estimate_runtime() {
    if [[ "$VERBOSE" != true ]]; then
        return  # Skip runtime estimation in non-verbose mode
    fi
    
    print_section "Runtime Estimation"
    
    local baseline_time=0
    local dpf_time=0
    
    # Calculate baseline time based on run mode
    case "$RUN_MODE" in
        "baseline")
            baseline_time=4320  # 72 hours (5 iterations of all benchmarks)
            ;;
        "quick")
            baseline_time=90    # 1.5 hours (1 iteration of xalancbmk only)
            ;;
        "full")
            baseline_time=720   # 12 hours (1 iteration of all benchmarks)
            ;;
    esac
    
    # Add DPF time if DPF is enabled
    if [ "$DPF_ENABLED" = true ]; then
        case "$RUN_MODE" in
            "baseline")
                dpf_time=720    # 12 hours (1 iteration DPF run)
                ;;
            "quick")
                dpf_time=90     # 1.5 hours (1 iteration of xalancbmk only)
                ;;
            "full")
                dpf_time=720    # 12 hours (1 iteration of all benchmarks)
                ;;
        esac
    fi
    
    local total_time=$((baseline_time + dpf_time + 10))  # +10 minutes for analysis
    local hours=$((total_time / 60))
    local minutes=$((total_time % 60))
    
    print_info "Run mode: $RUN_MODE"
    if [ "$DPF_ENABLED" = true ]; then
        print_info "DPF analysis: enabled"
    fi
    print_info "Estimated runtime: ${hours}h ${minutes}m"
    
    if [ "$RUN_MODE" = "default" ]; then
        print_warning "DEFAULT MODE: This will take 3+ days for comprehensive baseline data"
        echo -e "${YELLOW}INFO: This generates high-quality baseline data with 5 iterations per benchmark${NC}"
        echo -e "${YELLOW}INFO: For quick testing, use --full or --quick modes${NC}"
    fi
    
    if [ $total_time -gt 60 ]; then
        print_warning "This is a long-running process. Consider using screen/tmux for remote sessions."
        echo -e "${YELLOW}Recommendation: screen -S spec_analysis ./run_all.sh${NC}"
    fi
}

run_baseline_benchmarks() {
    print_section "Running Baseline Benchmarks"
    
    # Always run baseline - DPF is additional, not replacement
    
    case "$RUN_MODE" in
        "baseline")
            print_info "COMPREHENSIVE MODE: Executing 5 iterations on all benchmarks..."
            print_warning "This will take 3+ days to complete!"
            ;;
        "quick")
            print_info "DEVELOPMENT MODE: Executing 1 iteration on xalancbmk benchmark only..."
            ;;
        "full")
            print_info "FULL MODE: Executing 1 iteration on all benchmarks..."
            ;;
    esac
    
    print_info "Progress will be logged to: $LOG_FILE"
    
    cd "$PROJECT_ROOT"
    
    local cmd="$SCRIPT_DIR/scripts/execution/run_suite.sh"
    
    # Export run mode and verbose flag for suite scripts  
    export RUN_MODE="$RUN_MODE"
    export VERBOSE="$VERBOSE"
    
    if [ "$VERBOSE" = true ]; then
        $cmd 2>&1 | tee -a "$LOG_FILE"
        local exit_code=${PIPESTATUS[0]}
    else
        print_info "Running baseline benchmarks... (output logged to $LOG_FILE)"
        $cmd >> "$LOG_FILE" 2>&1
        local exit_code=$?
    fi
    if [ $exit_code -eq 0 ]; then
        print_success "Baseline benchmarks completed successfully"
        return 0
    else
        print_warning "Baseline benchmarks had some failures (exit code: $exit_code) but continuing..."
        print_info "Check log file: $LOG_FILE"
        return 1  # Still return 1 to indicate issues, but main script will handle gracefully
    fi
}

run_dpf_benchmarks() {
    print_section "Running DPF Benchmarks (With DPF Enabled)"
    
    case "$RUN_MODE" in
        "baseline")
            print_info "COMPREHENSIVE DPF MODE: Executing 1 iteration on all benchmarks with DPF..."
            ;;
        "quick")
            print_info "DEVELOPMENT DPF MODE: Executing 1 iteration on xalancbmk with DPF..."
            ;;
        "full")
            print_info "FULL DPF MODE: Executing 1 iteration on all benchmarks with DPF..."
            ;;
    esac
    
    print_info "Progress will be logged to: $LOG_FILE"
    
    cd "$PROJECT_ROOT"
    
    local cmd="$SCRIPT_DIR/scripts/execution/run_dpf_suite.sh"
    
    # Export run mode and verbose flag for suite scripts  
    export RUN_MODE="$RUN_MODE"
    export VERBOSE="$VERBOSE"
    
    if [ "$VERBOSE" = true ]; then
        $cmd 2>&1 | tee -a "$LOG_FILE"
        local exit_code=${PIPESTATUS[0]}
    else
        print_info "Running DPF benchmarks... (output logged to $LOG_FILE)"
        $cmd >> "$LOG_FILE" 2>&1
        local exit_code=$?
    fi
    if [ $exit_code -eq 0 ]; then
        print_success "DPF benchmarks completed successfully"
        return 0
    else
        print_warning "DPF benchmarks had some failures (exit code: $exit_code) but continuing..."
        print_info "Check log file: $LOG_FILE"
        return 1  # Still return 1 to indicate issues, but main script will handle gracefully
    fi
}

run_standard_benchmarks() {
    print_section "Running Current Configuration"
    
    case "$RUN_MODE" in
        "baseline")
            print_info "COMPREHENSIVE MODE: Executing 5 iterations on all benchmarks with current configuration..."
            print_warning "This will take 3+ days to complete!"
            ;;
        "quick")
            print_info "DEVELOPMENT MODE: Executing 1 iteration on xalancbmk with current configuration..."
            ;;
        "full")
            print_info "FULL MODE: Executing 1 iteration on all benchmarks with current configuration..."
            ;;
    esac
    
    print_info "Progress will be logged to: $LOG_FILE"
    
    cd "$PROJECT_ROOT"
    
    local cmd="$SCRIPT_DIR/scripts/execution/run_suite.sh"
    
    # Export run mode and verbose flag for suite scripts  
    export RUN_MODE="$RUN_MODE"
    export VERBOSE="$VERBOSE"
    
    if [ "$VERBOSE" = true ]; then
        print_info "Running current configuration... (output logged to $LOG_FILE)"
        sudo -E $cmd 2>&1 | tee -a "$LOG_FILE"
        local exit_code=${PIPESTATUS[0]}
    else
        sudo -E $cmd >> "$LOG_FILE" 2>&1
        local exit_code=$?
    fi
    
    if [ $exit_code -eq 0 ]; then
        print_success "Current configuration completed successfully"
        return 0
    else
        print_warning "Current configuration had some failures (exit code: $exit_code) but continuing..."
        print_info "Check log file: $LOG_FILE"
        return 1  # Still return 1 to indicate issues, but main script will handle gracefully
    fi
}

run_single_benchmark() {
    local benchmark="$1"
    local iterations="$2"
    local dpf_enabled="$3"
    
    print_section "Running Single Benchmark: $benchmark"
    print_info "Iterations: $iterations"
    if [ "$dpf_enabled" = true ]; then
        print_info "Configuration: DPF enabled"
    else
        print_info "Configuration: Standard"
    fi
    
    cd "$PROJECT_ROOT"
    
    local cmd="$SCRIPT_DIR/scripts/execution/run_single_benchmark.sh --benchmark $benchmark --iterations $iterations"
    
    if [ "$dpf_enabled" = true ]; then
        cmd="$cmd --dpf"
    fi
    
    if [ "$VERBOSE" = true ]; then
        cmd="$cmd --verbose"
    fi
    
    # Export environment variables
    export RUN_MODE="$RUN_MODE"
    export VERBOSE="$VERBOSE"
    
    if [ "$VERBOSE" = true ]; then
        print_info "Running: $cmd"
        sudo -E $cmd 2>&1 | tee -a "$LOG_FILE"
        local exit_code=${PIPESTATUS[0]}
    else
        print_info "Running single benchmark... (output logged to $LOG_FILE)"
        sudo -E $cmd >> "$LOG_FILE" 2>&1
        local exit_code=$?
    fi
    
    if [ $exit_code -eq 0 ]; then
        print_success "Single benchmark completed successfully"
        return 0
    else
        print_warning "Single benchmark had some failures (exit code: $exit_code) but continuing..."
        print_info "Check log file: $LOG_FILE"
        return 1
    fi
}

# REMOVED: extract_performance_data() function
# Analysis should be run separately using compare_performance.py
# Benchmark execution should focus only on running benchmarks successfully

generate_analysis() {
    if [[ "$VERBOSE" == true ]]; then
        print_section "Generating Performance Analysis and Visualizations"
    fi
    
    # Always perform comparison if we have baseline data - regardless of DPF mode
    # Current configuration (DPF or baseline) should be compared against reference baseline
    if [ ! -f "$DATA_DIR/baseline/aggregated.csv" ]; then
        print_error "Comparative analysis requires baseline data"
        return 1
    fi
    
    cd "$PROJECT_ROOT"
    
    # ALL comparisons are against baseline reference
    local baseline_file="$DATA_DIR/baseline/aggregated.csv"
    local current_file
    local current_label
    
    if [ "$RUN_MODE" = "baseline" ]; then
        # Baseline mode: No comparison needed (this IS the reference)
        print_info "Baseline reference established - no comparison needed"
        return 0
    elif [ "$DPF_ENABLED" = true ]; then
        # DPF mode: Compare baseline vs DPF
        current_file="$DATA_DIR/dpf/aggregated.csv"
        current_label="dpf"
        print_info "Comparison mode: Baseline vs DPF"
    else
        # Default mode: Compare baseline vs current configuration
        current_file="$DATA_DIR/current/aggregated.csv"
        current_label="current"
        print_info "Comparison mode: Baseline vs Current Configuration"
    fi
    
    # Verify baseline reference exists
    if [ ! -f "$baseline_file" ]; then
        print_error "Baseline reference not found at $baseline_file"
        print_error "Please run with --baseline first to establish reference point"
        return 1
    fi
    
    # Verify current data exists
    if [ ! -f "$current_file" ]; then
        print_error "Current configuration data not found at $current_file"
        return 1
    fi
    
    # Provide explicit arguments for reliable comparison  
    if [ "$VERBOSE" = true ]; then
        python3 "$SCRIPT_DIR/scripts/analysis/compare_performance.py" \
            --baseline-file "$baseline_file" \
            --current-file "$current_file" \
            --baseline-type "baseline" \
            --current-type "$current_label" 2>&1 | tee -a "$LOG_FILE"
    else
        python3 "$SCRIPT_DIR/scripts/analysis/compare_performance.py" \
            --baseline-file "$baseline_file" \
            --current-file "$current_file" \
            --baseline-type "baseline" \
            --current-type "$current_label" >> "$LOG_FILE" 2>&1
    fi
    
    local exit_code=${PIPESTATUS[0]}
    if [ $exit_code -eq 0 ]; then
        print_success "Performance analysis completed"
    else
        print_error "Performance analysis failed"
        return 1
    fi
    
    # Verify output files
    if [ -f "results/performance_comparison.png" ]; then
        print_success "Performance visualization created"
    fi
    
    if [ -f "$DATA_DIR/comparison/performance_comparison.csv" ]; then
        print_success "Performance comparison CSV created"
    fi
}

show_results_summary() {
    print_section "Results Summary"
    
    print_info "Benchmark execution complete! Generated files:"
    echo ""
    
    # Benchmark data files (stored in reports directory)
    echo -e "${BLUE}Benchmark Data:${NC}"
    if [ -d "${RESULTS_DIR}/reports" ]; then
        local run_count=$(ls -1 "${RESULTS_DIR}/reports" | wc -l)
        echo "  [OK] ${RESULTS_DIR}/reports/ - Benchmark run data ($run_count runs)"
    fi
    
    # Logs
    echo -e "\n${BLUE}Logs:${NC}"
    echo "  [OK] $LOG_FILE - Benchmark execution log"
    
    echo ""
    print_info "All benchmark results are available in the ${RESULTS_DIR}/reports/ directory"
    print_info "Run 'python3 scripts/analysis/compare_performance.py' for performance analysis"
}

cleanup_on_error() {
    print_error "Script interrupted or failed"
    print_info "Partial results may be available in data/ and results/ directories"
    print_info "Check log file for details: $LOG_FILE"
    exit 1
}

#################################################################################
# Main Execution
#################################################################################

# Parse command line arguments
RUN_MODE="full"    # Default to full mode (1 iteration × all benchmarks)
DPF_ENABLED=false
VERBOSE=false
SINGLE_BENCHMARK=""
BENCHMARK_ITERATIONS=5

while [[ $# -gt 0 ]]; do
    case $1 in
        --baseline)
            RUN_MODE="baseline"
            shift
            ;;
        --full)
            RUN_MODE="full"
            shift
            ;;
        --quick)
            RUN_MODE="quick"
            shift
            ;;
        --benchmark)
            if [ -z "$2" ]; then
                print_error "--benchmark requires a benchmark name"
                exit 1
            fi
            # Validate benchmark name
            valid_benchmarks=("600.perlbench" "602.gcc" "605.mcf" "620.omnetpp" "623.xalancbmk" "625.x264" "631.deepsjeng" "641.leela" "648.exchange2" "657.xz")
            if [[ ! " ${valid_benchmarks[@]} " =~ " $2 " ]]; then
                print_error "Invalid benchmark: $2"
                print_error "Use --list to see available benchmarks"
                exit 1
            fi
            RUN_MODE="single"
            SINGLE_BENCHMARK="$2"
            shift 2
            ;;
        --iterations)
            if [ -z "$2" ] || ! [[ "$2" =~ ^[0-9]+$ ]]; then
                print_error "--iterations requires a positive number"
                exit 1
            fi
            BENCHMARK_ITERATIONS="$2"
            shift 2
            ;;
        --dpf)
            DPF_ENABLED=true
            shift
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        -l|--list)
            show_benchmark_list
            exit 0
            ;;
        --config)
            show_config
            exit 0
            ;;
        --set-baseline)
            # Set the most recent run as baseline
            if [ -d "results/reports" ]; then
                latest_run=$(ls -1t results/reports/ | head -1)
                if [ -n "$latest_run" ]; then
                    echo "Setting '$latest_run' as reference baseline..."
                    "$PROJECT_ROOT/scripts/utils/set_baseline_reference.sh" --set "$latest_run"
                    exit $?
                else
                    print_error "No benchmark runs found in results/reports/"
                    exit 1
                fi
            else
                print_error "No results directory found"
                exit 1
            fi
            ;;
        --baseline-help)
            "$PROJECT_ROOT/scripts/utils/set_baseline_reference.sh" --help
            exit $?
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Load configuration early so LOG_FILE can be set
if [ -f "$PROJECT_ROOT/config/benchsuite.conf" ]; then
    # Determine appropriate home directory based on execution context
    # Use the home directory of the current execution environment
    if [ "$(id -u)" = "0" ]; then
        # Running as root - use /root regardless of how we got root access
        ORIGINAL_HOME="/root"
    else
        # Running as regular user - use their actual home
        ORIGINAL_HOME="$HOME"
    fi
    
    # Load config file line by line with proper path expansion
    while IFS='=' read -r key value; do
        # Skip comments and empty lines
        [[ $key =~ ^#.*$ ]] && continue
        [[ -z "$key" ]] && continue
        
        # Remove leading/trailing whitespace
        key=$(echo "$key" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
        value=$(echo "$value" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
        
        # Expand ~ to original user's home directory
        value="${value/#\~/$ORIGINAL_HOME}"
        
        # Export the variable
        export "$key"="$value"
        
    done < "$PROJECT_ROOT/config/benchsuite.conf"
    
    # Derive internal directories from the simplified configuration
    BENCHSUITE_ROOT="$PROJECT_ROOT"
    LOGS_DIR="${RESULTS_DIR}/logs"
    ANALYSIS_DIR="${RESULTS_DIR}/analysis"
    VISUALIZATIONS_DIR="${RESULTS_DIR}/reports"  # Consolidated with reports
    DPF_CONFIG="$(dirname "$DPF_BINARY")/mab_config.json"
    
    # Set log file path now that LOGS_DIR is available
    LOG_FILE="$LOGS_DIR/complete_analysis_$TIMESTAMP.log"
    
    # Export variables for Python scripts
    export BENCHSUITE_ROOT SPEC_CPU_DIR RESULTS_DIR LOGS_DIR ANALYSIS_DIR VISUALIZATIONS_DIR DPF_BINARY DPF_CONFIG
else
    print_error "Configuration file not found: $PROJECT_ROOT/config/benchsuite.conf"
    exit 1
fi

# Set up error handling
trap cleanup_on_error ERR INT TERM

# Create log directory
mkdir -p "$(dirname "$LOG_FILE")"

# Start execution
print_header "Complete Performance Analysis"
log_and_print "Started at: $(date)"
log_and_print "Log file: $LOG_FILE"

# Display mode information
case "$RUN_MODE" in
    "baseline")
        print_info "Mode: Comprehensive baseline (5 iterations across all benchmarks)"
        ;;
    "full")
        print_info "Mode: Full suite (1 iteration across all benchmarks)"
        ;;
    "quick")
        print_info "Mode: Development test (1 iteration across xalancbmk only)"
        ;;
    "single")
        print_info "Mode: Single benchmark ($SINGLE_BENCHMARK, $BENCHMARK_ITERATIONS iterations)"
        ;;
esac

if [ "$DPF_ENABLED" = true ]; then
    print_info "Configuration: DPF enabled"
else
    print_info "Configuration: Standard (no DPF)"
fi

if [ "$VERBOSE" = true ]; then
    print_info "Output: Verbose mode enabled"
fi

case "$RUN_MODE" in
    "baseline")
        if [ "$DPF_ENABLED" = true ]; then
            print_info "Estimated time: 6+ days (comprehensive baseline + configuration testing)"
        else
            print_info "Estimated time: 3+ days (comprehensive baseline establishment)"
        fi
        ;;
    "full")
        if [ "$has_existing_baseline" = true ]; then
            if [ "$DPF_ENABLED" = true ]; then
                print_info "Estimated time: 12-24 hours (full configuration test vs existing baseline)"
            else
                print_info "Estimated time: 12-24 hours (full configuration test vs existing baseline)"
            fi
        else
            if [ "$DPF_ENABLED" = true ]; then
                print_info "Estimated time: 24-48 hours (establish baseline + configuration test)"
            else
                print_info "Estimated time: 12-24 hours (establish baseline for future comparisons)"
            fi
        fi
        ;;
    "quick")
        if [ "$has_existing_baseline" = true ]; then
            if [ "$DPF_ENABLED" = true ]; then
                print_info "Estimated time: 5 minutes (quick configuration test vs existing baseline)"
            else
                print_info "Estimated time: 5 minutes (quick configuration test vs existing baseline)"
            fi
        else
            if [ "$DPF_ENABLED" = true ]; then
                print_info "Estimated time: 2-4 hours (establish baseline + configuration test)"
            else
                print_info "Estimated time: 5 minutes (establish baseline for future comparisons)"
            fi
        fi
        ;;
esac

# Execute workflow
check_prerequisites
estimate_runtime

baseline_success=false
dpf_success=false
current_config_success=false

# Check if baseline reference is configured (simpler approach)
has_existing_baseline=false
if [ -n "$REFERENCE_BASELINE" ] && [ -d "${RESULTS_DIR}/reports/${REFERENCE_BASELINE}" ]; then
    has_existing_baseline=true
fi

# Run baseline benchmarks ONLY if explicitly requested with --baseline flag
baseline_success=false
if [ "$RUN_MODE" = "baseline" ]; then
    if execute_baseline_workflow "$RUN_MODE" "$VERBOSE" "$LOG_FILE" "$SCRIPT_DIR"; then
        baseline_success=true
    fi
else
    print_info "Skipping baseline run - baseline mode not requested"
    baseline_success=true  # Mark as success since baseline wasn't requested
fi

# For non-baseline modes, run the current configuration
dpf_success=false
current_config_success=false
if [ "$RUN_MODE" = "single" ]; then
    # Single benchmark mode
    if [ "$DPF_ENABLED" = true ]; then
        if run_single_benchmark "$SINGLE_BENCHMARK" "$BENCHMARK_ITERATIONS" true; then
            dpf_success=true
        fi
    else
        if run_single_benchmark "$SINGLE_BENCHMARK" "$BENCHMARK_ITERATIONS" false; then
            current_config_success=true
        fi
    fi
elif [ "$RUN_MODE" != "baseline" ]; then
    # Determine what type of current configuration to run
    if [ "$DPF_ENABLED" = true ]; then
        # Run with DPF configuration
        if execute_dpf_workflow "$RUN_MODE" "$VERBOSE" "$LOG_FILE" "$SCRIPT_DIR"; then
            dpf_success=true
        fi
    else
        # Run with standard configuration
        if execute_standard_workflow "$RUN_MODE" "$VERBOSE" "$LOG_FILE" "$SCRIPT_DIR" "$DPF_ENABLED"; then
            current_config_success=true
        fi
    fi
fi

# Legacy DPF section (only for baseline mode compatibility)
if [ "$RUN_MODE" = "baseline" ] && [ "$DPF_ENABLED" = true ]; then
    if execute_dpf_workflow "$RUN_MODE" "$VERBOSE" "$LOG_FILE" "$SCRIPT_DIR"; then
        dpf_success=true
    fi
fi

# Skip analysis for quick mode - only produce raw results
# Benchmark execution complete - analysis is separate
print_section "Benchmark Execution Complete"
print_info "Raw benchmark results generated in results/reports/"
print_info ""
print_info "For performance analysis, use one of these commands:"
print_info "  python3 scripts/analysis/compare_performance.py --no-comparison  # Extract data only"
print_info "  python3 scripts/analysis/compare_performance.py                  # Full analysis with comparison"

# Final summary
print_header "Benchmark Execution Complete!"

# Provide detailed summary of what was accomplished
if [ "$baseline_success" = true ] && [ "$dpf_success" = true ]; then
    if [ "$RUN_MODE" = "baseline" ]; then
        print_success "COMPLETE SUCCESS: Both baseline and DPF benchmarks completed successfully!"
    else
        print_success "COMPLETE SUCCESS: DPF benchmarks completed successfully!"
    fi
elif [ "$baseline_success" = true ]; then
    if [ "$RUN_MODE" = "baseline" ]; then
        print_success "SUCCESS: Baseline benchmarks completed successfully"
        print_info "You can now test different configurations with --quick or --full"
    elif [ "$current_config_success" = true ]; then
        print_success "SUCCESS: Benchmarks completed successfully"
    else
        print_warning "PARTIAL SUCCESS: Baseline skipped, but current configuration had issues"
        print_info "Check logs for current configuration problems"
    fi
elif [ "$dpf_success" = true ]; then
    print_success "SUCCESS: DPF benchmarks completed successfully"
elif [ "$current_config_success" = true ]; then
    print_success "SUCCESS: Benchmarks completed successfully"
else
    print_warning "MIXED RESULTS: Some issues occurred during benchmark execution"
    print_info "Check logs for details"
fi

show_results_summary

log_and_print "Completed at: $(date)"

# Determine final completion status and exit appropriately
determine_completion_status "$RUN_MODE" "$baseline_success" "$dpf_success" "$current_config_success" "$PROJECT_ROOT" "$LOG_FILE"
exit $?