#ifndef __PMU_DDR_H
#define __PMU_DDR_H

#include <stdio.h>
#include <stdint.h>

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

int pmu_ddr_init(struct ddr_s *ddr);
uint64_t pmu_ddr(struct ddr_s *ddr, int type);


#endif
