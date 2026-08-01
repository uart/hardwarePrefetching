# Installation Guide

## Prerequisites

Before installation, ensure you meet the system requirements listed in [README.md](README.md).

## Configuration

Update the configuration file to match your installation paths:

### 1. Configure Paths
```bash
# Edit configuration
nano config/benchsuite.conf

# Update all paths to match your system installation
```

### 2. Verify Benchmark Installation
```bash
# Check benchmark installation
ls ~/benchmark/spec_bin_dir_new/6*
# Should show: 600.perlbench_s, 602.gcc_s, etc.
```

### 3. Load DPF Module
```bash
# Load kernel module
sudo insmod ~/dpf/kernelmod/dpf.ko

# Verify DPF binary
ls ~/dpf/dpf
```

## Installation

### 1. System Packages

```bash
# Install required system packages:

# Essential build tools, also for SpecInt execution 
sudo apt update
sudo apt install -y build-essential python3 python3-pip python3-dev gfortran

# System utilities
sudo apt install -y util-linux time cpufrequtils numactl sysstat htop

# Python packages (system-level)
sudo apt install -y python3-pandas python3-numpy python3-matplotlib python3.12-venv
```

### 2. Python Packages

```bash
# Activate virtual environment
python3 -m venv .bench_env # Create if not exists
source .bench_env/bin/activate

# Install Python dependencies
pip3 install -r python-packages.txt
```

## 3. Run Benchmarks

You can now start running benchmarks.
See [RUN.md](RUN.md) for detailed usage instructions.
