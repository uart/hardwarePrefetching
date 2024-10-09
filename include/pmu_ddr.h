#ifndef __PMU_DDR_H
#define __PMU_DDR_H

#include <stdio.h>
#include <stdint.h>

#define DDR_NONE (-1)
#define DDR_CLIENT (1)
#define DDR_SERVER (2)


//IMC - the Intel DDR controller, client version on Alder/Raptor-lake i9

//DDR Controller offset
#define CLIENT_DDR0_OFFSET (0x00000)
#define CLIENT_DDR1_OFFSET (0x10000)

#define CLIENT_DDR_RANGE (0x10000)

//register offsets
#define DDR_RD_BW (0xd858)
#define DDR_WR_BW (0xd8a0)

/* Example:
DDR0:
0xfedc0000  base
0x00000000  DDR0
0x0000d858  RD reg
------------------
0xfedcd858


DDR1:
0xfedc0000  base
0x00010000  DDR1
0x0000d858  RD reg
------------------
0xfeddd858
*/


struct ddr_s{
	uint64_t rd_last_update[16];
	uint64_t wr_last_update[16];
	char *mmap[16]; //ddr ch 0, 1, ...
	int mem_file; //file desc
};

int pmu_ddr_init(struct ddr_s *ddr);
uint64_t pmu_ddr(struct ddr_s *ddr, int type);


#endif
