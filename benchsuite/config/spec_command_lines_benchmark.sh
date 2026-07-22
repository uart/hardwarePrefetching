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

### 7. Floating Point Speed Benchmark Binaries ###

bwaves_s_bin="${commands_dir}/603.bwaves_s/exe/speed_bwaves_base.mytest-m64"
cactuBSSN_s_bin="${commands_dir}/607.cactuBSSN_s/exe/cactuBSSN_s_base.mytest-m64"
lbm_s_bin="${commands_dir}/619.lbm_s/exe/lbm_s_base.mytest-m64"
wrf_s_bin="${commands_dir}/621.wrf_s/exe/wrf_s_base.mytest-m64"
cam4_s_bin="${commands_dir}/627.cam4_s/exe/cam4_s_base.mytest-m64"
pop2_s_bin="${commands_dir}/628.pop2_s/exe/speed_pop2_base.mytest-m64"
imagick_s_bin="${commands_dir}/638.imagick_s/exe/imagick_s_base.mytest-m64"
nab_s_bin="${commands_dir}/644.nab_s/exe/nab_s_base.mytest-m64"
fotonik3d_s_bin="${commands_dir}/649.fotonik3d_s/exe/fotonik3d_s_base.mytest-m64"
roms_s_bin="${commands_dir}/654.roms_s/exe/sroms_base.mytest-m64"
specrand_fs_bin="${commands_dir}/996.specrand_fs/exe/specrand_fs_base.mytest-m64"

### 8. Floating Point Rate Benchmark Binaries ###

bwaves_r_bin="${commands_dir}/503.bwaves_r/exe/bwaves_r_base.mytest-m64"
cactuBSSN_r_bin="${commands_dir}/507.cactuBSSN_r/exe/cactusBSSN_r_base.mytest-m64"
namd_r_bin="${commands_dir}/508.namd_r/exe/namd_r_base.mytest-m64"
parest_r_bin="${commands_dir}/510.parest_r/exe/parest_r_base.mytest-m64"
povray_r_bin="${commands_dir}/511.povray_r/exe/povray_r_base.mytest-m64"
lbm_r_bin="${commands_dir}/519.lbm_r/exe/lbm_r_base.mytest-m64"
wrf_r_bin="${commands_dir}/521.wrf_r/exe/wrf_r_base.mytest-m64"
blender_r_bin="${commands_dir}/526.blender_r/exe/blender_r_base.mytest-m64"
cam4_r_bin="${commands_dir}/527.cam4_r/exe/cam4_r_base.mytest-m64"
imagick_r_bin="${commands_dir}/538.imagick_r/exe/imagick_r_base.mytest-m64"
nab_r_bin="${commands_dir}/544.nab_r/exe/nab_r_base.mytest-m64"
fotonik3d_r_bin="${commands_dir}/549.fotonik3d_r/exe/fotonik3d_r_base.mytest-m64"
roms_r_bin="${commands_dir}/554.roms_r/exe/roms_r_base.mytest-m64"
specrand_fr_bin="${commands_dir}/997.specrand_fr/exe/specrand_fr_base.mytest-m64"

### 9. Floating Point Input Directories ###

# Speed input directories
bwaves_s_input="${commands_dir}/603.bwaves_s/run/run_base_refspeed_mytest-m64.0000"
cactuBSSN_s_input="${commands_dir}/607.cactuBSSN_s/run/run_base_refspeed_mytest-m64.0000"
lbm_s_input="${commands_dir}/619.lbm_s/run/run_base_refspeed_mytest-m64.0000"
wrf_s_input="${commands_dir}/621.wrf_s/run/run_base_refspeed_mytest-m64.0000"
cam4_s_input="${commands_dir}/627.cam4_s/run/run_base_refspeed_mytest-m64.0000"
pop2_s_input="${commands_dir}/628.pop2_s/run/run_base_refspeed_mytest-m64.0000"
imagick_s_input="${commands_dir}/638.imagick_s/run/run_base_refspeed_mytest-m64.0000"
nab_s_input="${commands_dir}/544.nab_r/data/refspeed/input"
fotonik3d_s_input="${commands_dir}/649.fotonik3d_s/run/run_base_refspeed_mytest-m64.0000"
roms_s_input="${commands_dir}/654.roms_s/run/run_base_refspeed_mytest-m64.0000"
specrand_fs_input="${commands_dir}/999.specrand_ir/data/refrate/input"

# Rate input directories
bwaves_r_input="${commands_dir}/503.bwaves_r/run/run_base_refrate_mytest-m64.0023"
cactuBSSN_r_input="${commands_dir}/507.cactuBSSN_r/run/run_base_refrate_mytest-m64.0023"
namd_r_input="${commands_dir}/508.namd_r/run/run_base_refrate_mytest-m64.0023"
parest_r_input="${commands_dir}/510.parest_r/run/run_base_refrate_mytest-m64.0023"
povray_r_input="${commands_dir}/511.povray_r/run/run_base_refrate_mytest-m64.0000"
lbm_r_input="${commands_dir}/519.lbm_r/run/run_base_refrate_mytest-m64.0023"
wrf_r_input="${commands_dir}/521.wrf_r/run/run_base_refrate_mytest-m64.0023"
blender_r_input="${commands_dir}/526.blender_r/run/run_base_refrate_mytest-m64.0023"
cam4_r_input="${commands_dir}/527.cam4_r/run/run_base_refrate_mytest-m64.0023"
imagick_r_input="${commands_dir}/538.imagick_r/run/run_base_refrate_mytest-m64.0023"
nab_r_input="${commands_dir}/544.nab_r/data/refrate/input"
fotonik3d_r_input="${commands_dir}/549.fotonik3d_r/run/run_base_refrate_mytest-m64.0023"
roms_r_input="${commands_dir}/554.roms_r/run/run_base_refrate_mytest-m64.0000"
specrand_fr_input="${commands_dir}/999.specrand_ir/data/refrate/input"

### 10. Floating Point Speed Benchmark Commands ###

# 603.bwaves_s
benchmark_commands["603.bwaves"]="${bwaves_s_bin} ${bwaves_s_input}/bwaves_1 < ${bwaves_s_input}/bwaves_1.in > bwaves_1.out 2>> bwaves_1.err"

# 607.cactuBSSN_s
benchmark_commands["607.cactuBSSN"]="${cactuBSSN_s_bin} ${cactuBSSN_s_input}/spec_ref.par > spec_ref.out 2>> spec_ref.err"

# 619.lbm_s
benchmark_commands["619.lbm"]="${lbm_s_bin} 3000 reference.dat 0 0 ${lbm_s_input}/200_200_260_ldc.of > lbm.out 2>> lbm.err"

# 621.wrf_s : uses run dir via input path
benchmark_commands["621.wrf"]="${wrf_s_bin} ${wrf_s_input}/namelist.input > rsl.out.0000 2>> wrf.err"

# 627.cam4_s : uses run dir via input path
benchmark_commands["627.cam4"]="${cam4_s_bin} ${cam4_s_input}/atm_in > cam4.txt 2>> cam4.err"

# 628.pop2_s : uses run dir via input path
benchmark_commands["628.pop2"]="${pop2_s_bin} ${pop2_s_input}/atm_modelio.nml > pop2.out 2>> pop2.err"

# 638.imagick_s
benchmark_commands["638.imagick"]="${imagick_s_bin} -limit disk 0 ${imagick_s_input}/refspeed_input.tga -resize 817% -rotate -2.76 -shave 540x375 -alpha remove -auto-level -contrast-stretch 1x1% -colorspace Lab -channel R -equalize +channel -colorspace sRGB -define histogram:unique-colors=false -adaptive-blur 0x5 -despeckle -auto-gamma -adaptive-sharpen 55 -enhance -brightness-contrast 10x10 -resize 30% refspeed_output.tga > refspeed_convert.out 2>> refspeed_convert.err"

# 644.nab_s
benchmark_commands["644.nab"]="${nab_s_bin} 3j1n 20140317 220 > ${nab_s_input}/3j1n.out 2>> ${nab_s_input}/3j1n.err"

# 649.fotonik3d_s : uses run dir via input path
benchmark_commands["649.fotonik3d"]="${fotonik3d_s_bin} ${fotonik3d_s_input}/OBJ.dat > fotonik3d.log 2>> fotonik3d.err"

# 654.roms_s
benchmark_commands["654.roms"]="${roms_s_bin} < ${roms_s_input}/ocean_benchmark3.in.x > ocean_benchmark3.log 2>> ocean_benchmark3.err"

# 996.specrand_fs
benchmark_commands["996.specrand"]="${specrand_fs_bin} 1255432124 234923 > ${specrand_fs_input}/rand.234923.out 2>> ${specrand_fs_input}/rand.234923.err"

### 11. Floating Point Rate Benchmark Commands ###

# 503.bwaves_r
benchmark_commands["503.bwaves"]="${bwaves_r_bin} ${bwaves_r_input}/bwaves_3 < ${bwaves_r_input}/bwaves_3.in > bwaves_3.out 2>> bwaves_3.err"

# 507.cactuBSSN_r
benchmark_commands["507.cactuBSSN"]="${cactuBSSN_r_bin} ${cactuBSSN_r_input}/spec_ref.par > spec_ref.out 2>> spec_ref.err"

# 508.namd_r
benchmark_commands["508.namd"]="${namd_r_bin} --input ${namd_r_input}/apoa1.input --output apoa1.ref.output --iterations 65 > namd.out 2>> namd.err"

# 510.parest_r
benchmark_commands["510.parest"]="${parest_r_bin} ${parest_r_input}/ref.prm > ref.out 2>> ref.err"

# 511.povray_r
benchmark_commands["511.povray"]="${povray_r_bin} ${povray_r_input}/SPEC-benchmark-ref.ini > SPEC-benchmark-ref.stdout 2>> SPEC-benchmark-ref.stderr"

# 519.lbm_r
benchmark_commands["519.lbm"]="${lbm_r_bin} 3000 reference.dat 0 0 ${lbm_r_input}/100_100_130_ldc.of > lbm.out 2>> lbm.err"

# 521.wrf_r : uses run dir via input path
benchmark_commands["521.wrf"]="${wrf_r_bin} ${wrf_r_input}/namelist.input > rsl.out.0000 2>> wrf.err"

# 526.blender_r
benchmark_commands["526.blender"]="${blender_r_bin} ${blender_r_input}/sh3_no_char.blend --render-output sh3_no_char_ --threads 1 -b -F RAWTGA -s 849 -e 849 -a > sh3_no_char.849.spec.out 2>> sh3_no_char.849.spec.err"

# 527.cam4_r : uses run dir via input path
benchmark_commands["527.cam4"]="${cam4_r_bin} ${cam4_r_input}/atm_in > cam4.txt 2>> cam4.err"

# 538.imagick_r
benchmark_commands["538.imagick"]="${imagick_r_bin} -limit disk 0 ${imagick_r_input}/refrate_input.tga -edge 41 -resample 181% -emboss 31 -colorspace YUV -mean-shift 19x19+15% -resize 30% refrate_output.tga > refrate_convert.out 2>> refrate_convert.err"

# 544.nab_r
benchmark_commands["544.nab"]="${nab_r_bin} 1am0 1122214447 122 > ${nab_r_input}/1am0.out 2>> ${nab_r_input}/1am0.err"

# 549.fotonik3d_r : uses run dir via input path
benchmark_commands["549.fotonik3d"]="${fotonik3d_r_bin} ${fotonik3d_r_input}/OBJ.dat > fotonik3d.log 2>> fotonik3d.err"

# 554.roms_r
benchmark_commands["554.roms"]="${roms_r_bin} < ${roms_r_input}/ocean_benchmark2.in.x > ocean_benchmark2.log 2>> ocean_benchmark2.err"

# 997.specrand_fr
benchmark_commands["997.specrand"]="${specrand_fr_bin} 1255432124 234923 > ${specrand_fr_input}/rand.234923.out 2>> ${specrand_fr_input}/rand.234923.err"

### 12. Export for external use ###
export commands_dir
export -A benchmark_commands