#ifndef __PMU_CORE_H
#define __PMU_CORE_H

#include <stdio.h>
#include <stdint.h>


#define PMU_COUNTERS (6)

#define PMU_PMC0 (0xc1)
#define PMU_PMC1 (0xc2)
#define PMU_PMC2 (0xc3)
#define PMU_PMC3 (0xc4)
#define PMU_PMC4 (0xc5)
#define PMU_PMC5 (0xc6)

#define PMU_PERFEVTSEL0 (0x186)
#define PMU_PERFEVTSEL1 (0x187)
#define PMU_PERFEVTSEL2 (0x188)
#define PMU_PERFEVTSEL3 (0x189)
#define PMU_PERFEVTSEL4 (0x18a)
#define PMU_PERFEVTSEL5 (0x18b)

#define EVENT_MEM_UOPS_RETIRED_ALL_LOADS  (0x00000000004381d0)
#define EVENT_MEM_LOAD_UOPS_RETIRED_L2_HIT (0x00000000004302d1)
#define EVENT_MEM_LOAD_UOPS_RETIRED_L3_HIT (0x00000000004304d1)
#define EVENT_MEM_LOAD_UOPS_RETIRED_DRAM_HIT (0x00000000004380d1)
#define EVENT_XQ_PROMOTION_ALL (0x00000000004300f4)

int pmu_core_config(int msr_file);
int pmu_core_read(int msr_file, uint64_t *result_p, uint64_t *inst_retired, uint64_t *cpu_cycles);

#endif
