#ifndef __PMU_DDR_H
#define __PMU_DDR_H


#define DDR_NONE (-1)
#define DDR_CLIENT (1)
#define DDR_GRR_SRF (2)

#define DDR_PMU_RD (1)
#define DDR_PMU_WR (2)
//IMC - the Intel DDR controller, client version on Alder/Raptor-lake i9

//DDR Controller offset
#define CLIENT_DDR0_OFFSET (0x00000)
#define CLIENT_DDR1_OFFSET (0x10000)
#define GRR_SRF_MC_ADDRESS(n) (0x24C000 + (0x4000 * (n)))

#define CLIENT_DDR_RANGE (0x10000)
#define GRR_SRF_DDR_RANGE (0x10000)

//register offsets
#define CLIENT_DDR_RD_BW (0xd858)
#define CLIENT_DDR_WR_BW (0xd8a0)

#define GRR_SRF_FREE_RUN_CNTR_READ (0x1A40)
#define GRR_SRF_FREE_RUN_CNTR_WRITE (0x1A48)

#define MAX_NUM_DDR_CONTROLLERS (16)


//structs for ddr
struct ddr_s {
	uint64_t rd_last_update[MAX_NUM_DDR_CONTROLLERS];
	uint64_t wr_last_update[MAX_NUM_DDR_CONTROLLERS];
	char *mmap[MAX_NUM_DDR_CONTROLLERS]; //ddr ch 0, 1, ...
	int mem_file; //file desc
	uint64_t bar_address;
	int ddr_interface_type;
	int num_ddr_controllers;
};

int pmu_ddr_init(struct ddr_s *ddr, int kernel_mode);
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
