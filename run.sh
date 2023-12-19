#Example for Alder/Rapor-lake Corei9 with 2x8 P-core/threads and 16 E-cores
# - we can only tune the E-cores
#Update every one second
#Set a target DDR Bandwidth, depending on your DDR config
./dpf --core 16-31 --intervall 1 --ddrbw 46000
