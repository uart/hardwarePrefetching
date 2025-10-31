# Makefile for Benchmark Performance Analysis
# 
# Common operations for setup, execution, and analysis

.PHONY: help install check clean analysis

# Default target
help:
	@echo "Benchmark Performance Analysis - Available Commands"
	@echo "=================================================="
	@echo ""
	@echo "Setup Commands:"
	@echo "  make check       - Verify installation"
	@echo ""
	@echo "Execution Commands:"
	@echo "  make run         - Run full suite (default mode)"
	@echo "  make baseline    - Run comprehensive baseline (5 iterations)"
	@echo "  make quick       - Run development test (xalancbmk only)"
	@echo "  make dpf         - Run full suite + DPF analysis"
	@echo "  make analysis    - Run analysis on existing data"
	@echo ""
	@echo "Maintenance Commands:"
	@echo "  make clean       - Clean generated files"
	@echo "  make clean-all   - Clean everything including data"
	@echo "  make archive     - Create tar archive of project"

# Installation and setup
install:
	@echo "See INSTALL.md for installation instructions"
	@echo "Install system packages: sudo apt install build-essential python3 python3-pip..."
	@echo "Install Python packages: pip3 install -r requirements.txt"
	@echo "Update config: nano config/benchsuite.conf"

check:
	@echo "Verify benchmark installation: sudo ls ~/benchmark/spec_bin_dir_new/6*"
	@echo "Verify DPF installation: sudo ls ~/dpf/dpf ~/dpf/mab_config.json"

# Execution commands
run:
	@echo "Running complete analysis pipeline..."
	./run_all.sh

baseline:
	@echo "Running baseline benchmarks..."
	./scripts/execution/run_suite.sh

dpf:
	@echo "Running DPF benchmarks..."
	./scripts/execution/run_dpf_suite.sh

analysis:
	@echo "Running analysis on existing data..."
	sudo python3 scripts/analysis/compare_performance.py --type all --output-dir data
	python3 scripts/analysis/compare_performance.py

# Maintenance
clean:
	@echo "Cleaning generated files..."
	rm -rf results/analysis/*.png
	rm -rf results/analysis/*.md
	rm -f *.csv
	find . -name "*.pyc" -delete
	find . -name "__pycache__" -delete

clean-all: clean
	@echo "Cleaning all data..."
	rm -rf data/*/
	rm -rf results/logs/*

archive:
	@echo "Creating project archive..."
	tar -czf performance_test_$(shell date +%Y%m%d_%H%M%S).tar.gz \
		--exclude='data/*' \
		--exclude='results/logs/*' \
		--exclude='*.pyc' \
		--exclude='__pycache__' \
		.
	@echo "Archive created: performance_test_$(shell date +%Y%m%d_%H%M%S).tar.gz"

# Progress monitoring
progress:
	@echo "Checking benchmark progress..."
	./scripts/analysis/check_progress.sh

# Development helpers
test-single:
	@echo "Testing single benchmark (600.perlbench)..."
	sudo ./run_all.sh --benchmark 600.perlbench

requirements-check:
	@echo "Checking Python requirements..."
	python3 -c "import pandas, matplotlib, numpy; print('Core packages OK')"
	python3 -c "import scipy, seaborn; print('Enhanced packages OK')" || echo "Enhanced packages not installed"

# Quick status check
status: check progress

# Development and debugging
debug:
	@echo "System information for debugging:"
	@echo "================================="
	uname -a
	python3 --version
	pip3 --version
	free -h
	df -h .
	lscpu | head -20
