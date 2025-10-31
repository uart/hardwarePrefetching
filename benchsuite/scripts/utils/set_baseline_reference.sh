#!/usr/bin/env bash

# Reference Baseline Management Utility
# Manages reference baselines for performance comparisons

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SUITE_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
CONFIG_FILE="$SUITE_ROOT/config/benchsuite.conf"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

show_usage() {
    cat << 'EOF'
Reference Baseline Management Utility

USAGE:
    set_baseline_reference.sh [OPTION] [RUN_ID]

OPTIONS:
    --set <run_id>      Set a specific run as reference baseline
    --current           Show current reference baseline
    --reset             Clear reference baseline setting
    --help              Show this help message

EXAMPLES:
    ./set_baseline_reference.sh --set 20251014-111024_baseline
    ./set_baseline_reference.sh --current
    ./set_baseline_reference.sh --reset

DESCRIPTION:
    This utility manages the reference baseline used for performance comparisons.
    Any benchmark run can be set as a reference point for future comparisons.
EOF
}

set_reference() {
    local run_id="$1"
    
    if [ -z "$run_id" ]; then
        echo -e "${RED}Error: Run ID required${NC}"
        return 1
    fi
    
    if [ ! -f "$CONFIG_FILE" ]; then
        echo -e "${RED}Error: Configuration file not found: $CONFIG_FILE${NC}"
        return 1
    fi
    
    source "$CONFIG_FILE"
    
    local run_dir="$RESULTS_DIR/reports/$run_id"
    if [ ! -d "$run_dir" ]; then
        echo -e "${RED}Error: Run directory not found: $run_dir${NC}"
        return 1
    fi
    
    # Update configuration
    if grep -q "^REFERENCE_BASELINE=" "$CONFIG_FILE"; then
        sed -i "s/^REFERENCE_BASELINE=.*/REFERENCE_BASELINE=$run_id/" "$CONFIG_FILE"
    else
        echo "REFERENCE_BASELINE=$run_id" >> "$CONFIG_FILE"
    fi
    
    echo -e "${GREEN}Reference baseline set to: $run_id${NC}"
}

show_current() {
    if [ ! -f "$CONFIG_FILE" ]; then
        echo -e "${RED}Error: Configuration file not found: $CONFIG_FILE${NC}"
        return 1
    fi
    
    source "$CONFIG_FILE"
    
    if [ -n "$REFERENCE_BASELINE" ]; then
        echo -e "${GREEN}Current reference baseline: $REFERENCE_BASELINE${NC}"
    else
        echo -e "${YELLOW}No reference baseline currently set${NC}"
    fi
}

reset_reference() {
    if [ ! -f "$CONFIG_FILE" ]; then
        echo -e "${RED}Error: Configuration file not found: $CONFIG_FILE${NC}"
        return 1
    fi
    
    sed -i 's/^REFERENCE_BASELINE=.*/# REFERENCE_BASELINE=/' "$CONFIG_FILE"
    echo -e "${GREEN}Reference baseline cleared${NC}"
}

# Main argument parsing
case "$1" in
    --set)
        set_reference "$2"
        ;;
    --current)
        show_current
        ;;
    --reset)
        reset_reference
        ;;
    --help)
        show_usage
        ;;
    *)
        echo -e "${RED}Error: Unknown option '$1'${NC}"
        echo ""
        show_usage
        exit 1
        ;;
esac
