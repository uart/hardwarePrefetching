#ifndef __PMU_DDR_H
#define __PMU_DDR_H

#include <stdio.h>
#include <stdint.h>

#define DDR_NONE (-1)
#define DDR_CLIENT (1)
#define DDR_SERVER (2)
#define DDR_GRANDRIDGE (3)


//IMC - the Intel DDR controller, client version on Alder/Raptor-lake i9

//DDR Controller offset
#define CLIENT_DDR0_OFFSET (0x00000)
#define CLIENT_DDR1_OFFSET (0x10000)

#define CLIENT_DDR_RANGE (0x10000)

//register offsets
#define DDR_RD_BW (0xd858)
#define DDR_WR_BW (0xd8a0)

#define GRANDRIDGE_MC0_BASE (0x24C000)
#define GRANDRIDGE_MC1_BASE (0x250000)

#define GRANDRIDGE_FREE_RUN_CNTR_READ (0x1A40)
#define GRANDRIDGE_FREE_RUN_CNTR_WRITE (0x1A48)



struct ddr_s{
	uint64_t rd_last_update[16];
	uint64_t wr_last_update[16];
	char *mmap[16]; //ddr ch 0, 1, ...
	int mem_file; //file desc
};

int pmu_ddr_init(struct ddr_s *ddr);
uint64_t pmu_ddr(struct ddr_s *ddr, int type);


#endif

/* Example, Client:
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



/* Example, Grand Ridge:

SCF_BAR:
0x000001ec  MMIO_BASE (bits 0-28 maps to base address 23-51)
0x80fdc000  SCF_BAR offset (bits 0-10 maps to base address 12-22)
------------------
0x000001EC = 0001 1110 1100
0x80Fdc000 = 1000 0000 1111 1101 1100 0000 0000 0000
-------------------
1111 0110 0000 0000 0000 0000 0000 0000
0000 0000 0000
-------------------
0xF6000000  Combined SCF_BAR


Memory Controller 0 (MC0):
0xF6000000  SCF_BAR base
0x0024C000  MC0 base
0x00001A40  Read reg offset
------------------
0xF624DA40


Memory Controller 1 (MC1):
0xF6000000  SCF_BAR base
0x00250000  MC1 base
0x00001A40  Read reg offset
------------------
0xF6251A40
*/
