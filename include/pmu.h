#ifndef __PMU_H
#define __PMU_H

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

//IMC - the Intel DDR controller, client version on Alder/Raptor-lake i9
#define DDR0_MODE0_BASE_ADDR (0xfedc5000)
#define DDR0_MODE1_BASE_ADDR (0xfedcd000)
#define DDR1_MODE0_BASE_ADDR (0xfedd5000)
#define DDR1_MODE1_BASE_ADDR (0xfeddd000)
#define MODE0_OFFSET (0x000)
#define MODE1_OFFSET (0x800)
#define DDR_RD_BW (0x58)
#define DDR_WR_BW (0xa0)

struct ddr_s{
	uint64_t rd_last_update[4];
	uint64_t wr_last_update[4];
	char *mmap[4]; //ctrl 0, mode 0; ctrl 0, mode 1; ctrl 1, mode 0; ctrl 1 mode 1;
	int mem_file; //file desc
};


int pmu_core_config(int msr_file);
int pmu_core_read(int msr_file, uint64_t *result_p, uint64_t *inst_retired, uint64_t *cpu_cycles);

int pmu_ddr_init(struct ddr_s *ddr);
uint64_t pmu_ddr(struct ddr_s *ddr, int type);


#endif
