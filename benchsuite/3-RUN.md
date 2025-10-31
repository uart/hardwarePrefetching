# Execution Guide

## Quick Start

```bash
# Quick test (5 minutes), one iteration of xalancbmk
./run_all.sh --quick

# for running the full benchmark suit (default)
./run_all.sh

# Stable baseline, intended to compare against  (3+ days)
./run_all.sh --baseline
```

## Main Script: run_all.sh

**Default behavior: Full mode (1 iteration across all benchmarks) for standard benchmarking.**

### What happens automatically when you run `./run_all.sh` (no arguments):
- **Full mode execution** (1 iteration across all 10 benchmarks)
- **Baseline execution** takes 12-24 hours
- **Data extraction** to CSV files in `results/baseline-csv/`
- **Results stored** in timestamped directories

### What you need to do manually for complete analysis:
- **For DPF comparison**: Run with `--dpf` flag: `./run_all.sh --dpf`
- **Generate comparison report**: `python3 scripts/analysis/compare_performance.py`
- **View results**: Check `results/comparison-csv/` for performance comparison files

### Modes

| Mode | Est. Duration | Benchmarks | Iterations | Use Case |
|------|----------|------------|------------|----------|
| `./run_all.sh` | 12-24 hours | All | 1 | Default: Standard benchmarking |
| `--baseline` | 3 days | All | 5 | Comprehensive benchmarking |
| `--quick` | 5 minutes | 1 (xalancbmk) | 1 | Development/testing |

### Options

```bash
# Execution flags
./run_all.sh --dpf             # Add dpf analysis to any mode
./run_all.sh --verbose          # Detailed output

# Single benchmark
sudo ./run_all.sh --benchmark 623.xalancbmk  # Run single benchmark (xalancbmk)
sudo ./run_all.sh --benchmark 602.gcc --iterations 3 # Specify iterations

# Single benchmark examples
sudo ./run_all.sh --benchmark 623.xalancbmk
sudo ./run_all.sh --benchmark 600.perlbench --dpf --iterations 3

# Available benchmarks
./run_all.sh --list

```

### Data Analysis

```bash
# Extract and compare performance metrics
python3 scripts/analysis/compare_performance.py

# Check progress during execution
./scripts/analysis/check_progress.sh
```

### Reference Baseline System

The benchmark suite supports flexible baseline comparisons where any previous run can be set as a reference point for future comparisons, regardless of the original run type.

```bash
# Set a specific run as reference baseline
./scripts/utils/set_baseline_reference.sh --set 20251014-111024_baseline

# Check current reference baseline
./scripts/utils/set_baseline_reference.sh --current

# Clear reference baseline (return to default baseline comparison)
./scripts/utils/set_baseline_reference.sh --reset

# Run comparison against reference baseline
python3 scripts/analysis/compare_performance.py
```

**Workflow Example:**
1. Run a quick test: `./run_all.sh --benchmark 623.xalancbmk --iterations 1`
2. Set as reference: `./scripts/utils/set_baseline_reference.sh --set 20251014-111024_baseline` (use run directory name from results/reports/)
3. Run future benchmarks: Any subsequent runs (baseline, dpf, full suites)
4. Compare results: `python3 scripts/analysis/compare_performance.py` automatically compares latest run vs your reference
5. Reset when needed: `./scripts/utils/set_baseline_reference.sh --reset`

### NOTE:
You can run `--help` with any script to see all options.

## Output Files

### Data
- `results/baseline-csv/detailed.csv` - Raw baseline metrics
- `results/current-csv/detailed.csv` - Raw current configuration metrics
- `results/comparison-csv/performance_comparison.csv` - Analysis results

### Visualizations
- `results/reports/dpf_performance_comparison.png`

### Logs
- `results/logs/complete_analysis_YYYYMMDD_HHMMSS.log`
- `results/logs/benchmark_baseline_YYYYMMDD_HHMMSS.log`
- `results/logs/benchmark_dpf_YYYYMMDD_HHMMSS.log`

## Monitoring

```bash
# Check running processes
./scripts/analysis/check_progress.sh

# Follow log output
tail -f results/logs/complete_analysis_*.log

# Check for errors
grep -i error results/logs/*.log
```

## Common Usage Patterns

```bash
# Standard benchmarking (default)
./run_all.sh

# Development testing
./run_all.sh --quick --verbose

# Research analysis with comprehensive baseline
./run_all.sh --baseline

# Complete analysis with DPF comparison
./run_all.sh --dpf

# Comprehensive research with DPF
./run_all.sh --baseline --dpf --verbose
```

**CPU Configuration**: Currently configured to run on **cores 6 and 7**. 
To change this, edit the `core_ids` array in the script:

```bash
# Use run_all.sh with --benchmark option for individual testing
core_ids=(6 7)     # Change to your desired cores
# Example: core_ids=(0 1 2 3) for cores 0-3
```

```bash
# Default single benchmark: xalancbmk, 5 iterations
sudo ./run_all.sh --benchmark 623.xalancbmk

# Specific benchmark
sudo ./run_all.sh --benchmark 602.gcc

# Enable DPF for comparison
sudo ./run_all.sh --benchmark 641.leela --dpf

# Quick testing (1 iteration)
sudo ./run_all.sh --benchmark 623.xalancbmk --iterations 1
```