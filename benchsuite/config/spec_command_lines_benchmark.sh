#!/bin/bash

#################################################################################
# This file defines the command lines for all SPEC Integer Speed and Rate
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
    commands_dir="$HOME/cpu2017/benchspec/CPU"
fi

### 2. Integer Speed Benchmark Binaries ###
perlbench_s_bin="${commands_dir}/600.perlbench_s/exe/perlbench_s_base.mytest-m64"
gcc_s_bin="${commands_dir}/602.gcc_s/exe/sgcc_base.mytest-m64"
mcf_s_bin="${commands_dir}/605.mcf_s/exe/mcf_s_base.mytest-m64"
omnetpp_s_bin="${commands_dir}/620.omnetpp_s/exe/omnetpp_s_base.mytest-m64"
xalancbmk_s_bin="${commands_dir}/623.xalancbmk_s/exe/xalancbmk_s_base.mytest-m64"
x264_s_bin="${commands_dir}/625.x264_s/exe/x264_s_base.mytest-m64"
deepsjeng_s_bin="${commands_dir}/631.deepsjeng_s/exe/deepsjeng_s_base.mytest-m64"
leela_s_bin="${commands_dir}/641.leela_s/exe/leela_s_base.mytest-m64"
exchange2_s_bin="${commands_dir}/648.exchange2_s/exe/exchange2_s_base.mytest-m64"
xz_s_bin="${commands_dir}/657.xz_s/exe/xz_s_base.mytest-m64"

### 3. Integer Rate Benchmark Binaries ###
perlbench_r_bin="${commands_dir}/500.perlbench_r/exe/perlbench_r_base.mytest-m64"
gcc_r_bin="${commands_dir}/502.gcc_r/exe/cpugcc_r_base.mytest-m64"
mcf_r_bin="${commands_dir}/505.mcf_r/exe/mcf_r_base.mytest-m64"
omnetpp_r_bin="${commands_dir}/520.omnetpp_r/exe/omnetpp_r_base.mytest-m64"
xalancbmk_r_bin="${commands_dir}/523.xalancbmk_r/exe/cpuxalan_r_base.mytest-m64"
x264_r_bin="${commands_dir}/525.x264_r/exe/x264_r_base.mytest-m64"
deepsjeng_r_bin="${commands_dir}/531.deepsjeng_r/exe/deepsjeng_r_base.mytest-m64"
leela_r_bin="${commands_dir}/541.leela_r/exe/leela_r_base.mytest-m64"
exchange2_r_bin="${commands_dir}/548.exchange2_r/exe/exchange2_r_base.mytest-m64"
xz_r_bin="${commands_dir}/557.xz_r/exe/xz_r_base.mytest-m64"

### 4. Input File Directories ###
# Speed benchmarks input directories
perlbench_s_input="${commands_dir}/500.perlbench_r/data/refrate/input"
gcc_s_input="${commands_dir}/502.gcc_r/data/refspeed/input"
mcf_s_input="${commands_dir}/505.mcf_r/data/refspeed/input"
omnetpp_s_input="${commands_dir}/520.omnetpp_r/data/refrate/input"
xalancbmk_s_input="${commands_dir}/523.xalancbmk_r/data/refrate/input"
x264_s_input="${commands_dir}/525.x264_r/data/refrate/input"
deepsjeng_s_input="${commands_dir}/531.deepsjeng_r/data/refrate/input"
leela_s_input="${commands_dir}/541.leela_r/data/refrate/input"
exchange2_s_input="${commands_dir}/548.exchange2_r/data/all/input"
xz_s_input="${commands_dir}/557.xz_r/data/all/input"

# Rate benchmarks input directories
perlbench_r_input="${commands_dir}/500.perlbench_r/data/refrate/input"
gcc_r_input="${commands_dir}/502.gcc_r/data/refrate/input"
mcf_r_input="${commands_dir}/505.mcf_r/data/refrate/input"
omnetpp_r_input="${commands_dir}/520.omnetpp_r/data/refrate/input"
xalancbmk_r_input="${commands_dir}/523.xalancbmk_r/data/refrate/input"
x264_r_input="${commands_dir}/525.x264_r/data/refrate/input"
deepsjeng_r_input="${commands_dir}/531.deepsjeng_r/data/refrate/input"
leela_r_input="${commands_dir}/541.leela_r/data/refrate/input"
exchange2_r_input="${commands_dir}/548.exchange2_r/data/all/input"
xz_r_input="${commands_dir}/557.xz_r/data/all/input"

### 5. Speed Benchmark Commands ###

# 600.perlbench_s - Perl interpreter
benchmark_commands["600.perlbench"]="${perlbench_s_bin} -I${perlbench_s_input}/lib ${perlbench_s_input}/checkspam.pl 2500 5 25 11 150 1 1 1 1 > checkspam.out 2>> checkspam.err"

# 602.gcc_s - GNU C Compiler
benchmark_commands["602.gcc"]="${gcc_s_bin} ${gcc_s_input}/gcc-pp.c -O5 -fipa-pta -o gcc-pp.s > gcc-pp.out 2>> gcc-pp.err"

# 605.mcf_s - Network flow solver
benchmark_commands["605.mcf"]="${mcf_s_bin} ${mcf_s_input}/inp.in > inp.out 2>> inp.err"

# 620.omnetpp_s - Network simulation
benchmark_commands["620.omnetpp"]="${omnetpp_s_bin} -c General -r 0 ${omnetpp_s_input}/omnetpp.ini > omnetpp.out 2>> omnetpp.err"

# 623.xalancbmk_s - XML transformation
benchmark_commands["623.xalancbmk"]="${xalancbmk_s_bin} -v ${xalancbmk_s_input}/t5.xml ${xalancbmk_s_input}/xalanc.xsl > ref.out 2>> ref.err"

# 625.x264_s - Video compression
benchmark_commands["625.x264"]="${x264_s_bin} --pass 1 --stats x264_stats.log --bitrate 1000 --frames 1000 -o BuckBunny_New.264 ${x264_s_input}/BuckBunny.264 1280x720 > pass1.out 2>> pass1.err"

# 631.deepsjeng_s - Chess engine
benchmark_commands["631.deepsjeng"]="${deepsjeng_s_bin} ${deepsjeng_s_input}/ref.txt > ref.out 2>> ref.err"

# 641.leela_s - Go game engine
benchmark_commands["641.leela"]="${leela_s_bin} ${leela_s_input}/ref.sgf > ref.out 2>> ref.err"

# 648.exchange2_s - Artificial intelligence
benchmark_commands["648.exchange2"]="${exchange2_s_bin} ${exchange2_s_input}/control 6 > exchange2.out 2>> exchange2.err"

# 657.xz_s - Data compression
benchmark_commands["657.xz"]="${xz_s_bin} ${xz_s_input}/cpu2006docs.tar.xz 6643 055ce243071129412e9dd0b3b69a21654033a9b723d874b2015c774fac1553d9713be561ca86f74e4f16f22e664fc17a79f30caa5ad2c04fbc447549c2810fae 1036078272 1111795472 4 > cpu2006docs.out 2>> cpu2006docs.err"

### 6. Rate Benchmark Commands ###

# 500.perlbench_r - Perl interpreter
benchmark_commands["500.perlbench"]="${perlbench_r_bin} -I${perlbench_r_input}/lib ${perlbench_r_input}/checkspam.pl 2500 5 25 11 150 1 1 1 1 > checkspam.out 2>> checkspam.err"

# 502.gcc_r - GNU C Compiler
benchmark_commands["502.gcc"]="${gcc_r_bin} ${gcc_r_input}/gcc-pp.c -O3 -finline-limit=0 -fif-conversion -fif-conversion2 -o gcc-pp.s > gcc-pp.out 2>> gcc-pp.err"

# 505.mcf_r - Network flow solver
benchmark_commands["505.mcf"]="${mcf_r_bin} ${mcf_r_input}/inp.in > inp.out 2>> inp.err"

# 520.omnetpp_r - Network simulation
benchmark_commands["520.omnetpp"]="${omnetpp_r_bin} -c General -r 0 ${omnetpp_r_input}/omnetpp.ini > omnetpp.out 2>> omnetpp.err"

# 523.xalancbmk_r - XML transformation
benchmark_commands["523.xalancbmk"]="${xalancbmk_r_bin} -v ${xalancbmk_r_input}/t5.xml ${xalancbmk_r_input}/xalanc.xsl > ref.out 2>> ref.err"

# 525.x264_r - Video compression
benchmark_commands["525.x264"]="${x264_r_bin} --pass 1 --stats x264_stats.log --bitrate 1000 --frames 1000 -o BuckBunny_New.264 ${x264_r_input}/BuckBunny.264 1280x720 > pass1.out 2>> pass1.err"

# 531.deepsjeng_r - Chess engine
benchmark_commands["531.deepsjeng"]="${deepsjeng_r_bin} ${deepsjeng_r_input}/ref.txt > ref.out 2>> ref.err"

# 541.leela_r - Go game engine
benchmark_commands["541.leela"]="${leela_r_bin} ${leela_r_input}/ref.sgf > ref.out 2>> ref.err"

# 548.exchange2_r - Artificial intelligence
benchmark_commands["548.exchange2"]="${exchange2_r_bin} ${exchange2_r_input}/control 6 > exchange2.out 2>> exchange2.err"

# 557.xz_r - Data compression
benchmark_commands["557.xz"]="${xz_r_bin} ${xz_r_input}/cpu2006docs.tar.xz 6643 055ce243071129412e9dd0b3b69a21654033a9b723d874b2015c774fac1553d9713be561ca86f74e4f16f22e664fc17a79f30caa5ad2c04fbc447549c2810fae 1036078272 1111795472 4 > cpu2006docs.out 2>> cpu2006docs.err"

### 7. Export for external use ###
export commands_dir
export -A benchmark_commands