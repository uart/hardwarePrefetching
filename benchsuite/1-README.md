#  Benchmark Performance Analysis Suite

A comprehensive benchmarking and analysis framework for comparing benchmark 
performance across different system configurations and optimizations.

## System Requirements

### Hardware
- **CPU**: Intel x86_64 processor 
- **Storage**: 10GB free space
- **Root Access**: Required for benchmark execution

### Software
- **Benchmark**: Full installation required
- **Python**: 3.6+ with pandas, matplotlib, numpy
- **System Tools**: build-essential, gfortran, cpufrequtils

## Project Overview

This framework provides automated:
- **Benchmark Execution**: Benchmarks across different system configurations
- **Data Extraction**: Performance metrics parsing and analysis  
- **Visualization**: Statistical comparison and performance plots
- **System Stability**: Fault-tolerant execution with comprehensive error handling

## Key Features

- **Fault-tolerant**: Continues execution when individual benchmarks fail
- **Statistical**: Multiple iterations for confidence intervals  
- **Automated**: Single command execution with complete pipeline
- **Flexible**: Baseline and configurable system comparison analysis
- **Modular**: Run full suites, individual benchmarks, or quick tests


## Documentation

- **[INSTALL.md](INSTALL.md)** - Installation and setup guide
- **[RUN.md](RUN.md)** - Execution guide and detailed usage

## Project Structure

```
bench_suite/
├── run_all.sh                # Main execution script
├── config/                   # Configuration files
├── scripts/                  # Execution and analysis scripts
│   └── execution/
│       ├── run_single_benchmark.sh   # Individual benchmark runner
│       ├── run_suite.sh              # Benchmark suite runner
│       └── run_dpf_suite.sh          # DPF benchmark suite runner
└── results/                  # Analysis outputs and logs
    ├── data/                 # Generated performance data (CSV files)
    ├── report/               # Timestamped benchmark execution results
    ├── logs/                 # Execution logs
    ├── reports/              # Analysis reports
    └── reports/              # Analysis reports and performance charts
```

## Support
- **Troubleshooting**: Check `results/logs/`
- **Quick test**: `./run_all.sh --quick`
