#ifndef SYSINFO_H
#define SYSINFO_H

#include <stdint.h>


#define CPUID_LEAF_EXT_FEATURES (0x07) // CPUID leaf for extended features
#define CPUID_HYBRID_FLAG_BIT   (15) // bit 15 of CPUID leaf 0x7, subleaf 0x0

// CPUID instruction to get CPU features
#define __cpuid_count(level, count, a, b, c, d)				\
  __asm__ __volatile__ ("cpuid\n\t"					\
			: "=a" (a), "=b" (b), "=c" (c), "=d" (d)	\
			: "0" (level), "2" (count))

                        


struct dpf_console_sysinfo {
    int is_hybrid;
    int first_core;
    int last_core;

    uint64_t confirmed_bar;
    int confirmed_ddr_type;
    int num_ddr_channels;

    int theoretical_bw; 
};

int collect_sysinfo(struct dpf_console_sysinfo *out);

#endif
