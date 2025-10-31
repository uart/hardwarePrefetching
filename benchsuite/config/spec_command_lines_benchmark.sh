#!/bin/bash

#################################################################################
# This file defines the command lines for all SPEC Integer Speed 
# benchmarks in a structured associative array format.
#
# Usage: source this file to load the benchmark_commands array
#################################################################################

declare -A benchmark_commands

### 1. Benchmark Binaries Directory ###
# Load from config file if available, otherwise use default
CONFIG_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ -f "$CONFIG_DIR/benchsuite.conf" ]]; then
    source "$CONFIG_DIR/benchsuite.conf"
    # Expand tilde in path
    commands_dir=$(eval echo "$SPEC_CPU_DIR")
else
    commands_dir="$HOME/benchmark/spec_bin_dir_new"
fi

### 2. Integer Speed Benchmark Binaries ###
perlbench_bin="${commands_dir}/600.perlbench_s/exe/perlbench_s_base.mytest-m64"
gcc_bin="${commands_dir}/602.gcc_s/exe/sgcc_base.mytest-m64"
mcf_bin="${commands_dir}/605.mcf_s/exe/mcf_s_base.mytest-m64"
omnetpp_bin="${commands_dir}/620.omnetpp_s/exe/omnetpp_s_base.mytest-m64"
xalancbmk_bin="${commands_dir}/623.xalancbmk_s/exe/xalancbmk_s_base.mytest-m64"
x264_bin="${commands_dir}/625.x264_s/exe/x264_s_base.mytest-m64"
deepsjeng_bin="${commands_dir}/631.deepsjeng_s/exe/deepsjeng_s_base.mytest-m64"
leela_bin="${commands_dir}/641.leela_s/exe/leela_s_base.mytest-m64"
exchange2_bin="${commands_dir}/648.exchange2_s/exe/exchange2_s_base.mytest-m64"
xz_bin="${commands_dir}/657.xz_s/exe/xz_s_base.mytest-m64"

### 3. Input File Directories ###
perlbench_input="${commands_dir}/600.perlbench_s/data/refspeed/input"
gcc_input="${commands_dir}/602.gcc_s/data/refspeed/input"
mcf_input="${commands_dir}/605.mcf_s/data/refspeed/input"
omnetpp_input="${commands_dir}/620.omnetpp_s/data/refspeed/input"
xalancbmk_input="${commands_dir}/623.xalancbmk_s/data/refspeed/input"
x264_input="${commands_dir}/625.x264_s/data/refspeed/input"
deepsjeng_input="${commands_dir}/631.deepsjeng_s/data/refspeed/input"
leela_input="${commands_dir}/641.leela_s/data/refspeed/input"
xz_input="${commands_dir}/657.xz_s/data/refspeed/input"

### 4. SpecInt Speed Benchmark Commands ###

# 600.perlbench_s - Perl interpreter
benchmark_commands["600.perlbench"]="${perlbench_bin} -I${perlbench_input}/lib ${perlbench_input}/checkspam.pl 2500 5 25 11 150 1 1 1 1 > checkspam.out 2>> checkspam.err"

# 602.gcc_s - GNU C Compiler
benchmark_commands["602.gcc"]="${gcc_bin} ${gcc_input}/gcc-pp.c -O5 -fipa-pta -o gcc-pp.s > gcc-pp.out 2>> gcc-pp.err"

# 605.mcf_s - Network flow solver
benchmark_commands["605.mcf"]="${mcf_bin} ${mcf_input}/inp.in > inp.out 2>> inp.err"

# 620.omnetpp_s - Network simulation
benchmark_commands["620.omnetpp"]="${omnetpp_bin} -c General -r 0 ${omnetpp_input}/omnetpp.ini > omnetpp.out 2>> omnetpp.err"

# 623.xalancbmk_s - XML transformation
benchmark_commands["623.xalancbmk"]="${xalancbmk_bin} -v ${xalancbmk_input}/t5.xml ${xalancbmk_input}/xalanc.xsl > ref.out 2>> ref.err"

# 625.x264_s - Video compression
benchmark_commands["625.x264"]="${x264_bin} --pass 1 --stats x264_stats.log --bitrate 1000 --frames 1000 -o BuckBunny_New.264 ${x264_input}/BuckBunny.yuv 1280x720 > pass1.out 2>> pass1.err"

# 631.deepsjeng_s - Chess engine
benchmark_commands["631.deepsjeng"]="${deepsjeng_bin} ${deepsjeng_input}/ref.txt > ref.out 2>> ref.err"

# 641.leela_s - Go game engine
benchmark_commands["641.leela"]="${leela_bin} ${leela_input}/ref.sgf > ref.out 2>> ref.err"

# 648.exchange2_s - Artificial intelligence
benchmark_commands["648.exchange2"]="${exchange2_bin} 6 > exchange2.out 2>> exchange2.err"

# 657.xz_s - Data compression
benchmark_commands["657.xz"]="${xz_bin} ${xz_input}/cpu2006docs.tar.xz 6643 055ce243071129412e9dd0b3b69a21654033a9b723d874b2015c774fac1553d9713be561ca86f74e4f16f22e664fc17a79f30caa5ad2c04fbc447549c2810fae 1036078272 1111795472 4 > cpu2006docs.out 2>> cpu2006docs.err"

### 5. Helper Functions ###

### 6. Export for external use ###
export commands_dir
export -A benchmark_commands
