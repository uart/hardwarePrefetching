#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/mman.h>

#include "msr.h"
#include "pmu.h"
#include "log.h"

#define TAG "PMU"

#define PMU_CORE_EVENT_COUNT (5)

int pmu_core_clear(int msr_file)
{
	//lets exaggerate, max 20 PMU couters, "should" be more than enough
	uint64_t events[20] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

	msr_corepmu_setup(msr_file, PMU_COUNTERS, events);

	return 0;
}

int pmu_core_config(int msr_file)
{
	uint64_t events[PMU_CORE_EVENT_COUNT] = {
		EVENT_MEM_UOPS_RETIRED_ALL_LOADS,
		EVENT_MEM_LOAD_UOPS_RETIRED_L2_HIT,
		EVENT_MEM_LOAD_UOPS_RETIRED_L3_HIT,
		EVENT_MEM_LOAD_UOPS_RETIRED_DRAM_HIT,
		EVENT_XQ_PROMOTION_ALL
	};

	pmu_core_clear(msr_file); //reset
	msr_corepmu_setup(msr_file, PMU_CORE_EVENT_COUNT, events);

	return 0;
}

int pmu_core_read(int msr_file, uint64_t *result_p)
{
	msr_corepmu_read(msr_file, PMU_CORE_EVENT_COUNT, result_p);

	return 0;
}

int pmu_ddr_init(struct ddr_s *ddr)
{
	int mem_file;
	int size = 0x1000;

	mem_file = open("/dev/mem", O_RDONLY);

	if (mem_file < 0){
		 loge(TAG, "Could not open MEM file /dev/mem, running as root/sudo?\n");
		exit(-1);
	}

	ddr->mem_file = mem_file;

	ddr->mmap[0] = (char *)mmap(NULL, size, PROT_READ, MAP_SHARED, mem_file, DDR0_MODE0_BASE_ADDR);
	if(ddr->mmap[0] == MAP_FAILED){
		loge(TAG, "Could not mmap() DDR0,0 area\n");
		exit(-1);
	}

	ddr->mmap[1] = (char *)mmap(NULL, size, PROT_READ, MAP_SHARED, mem_file, DDR0_MODE1_BASE_ADDR);
	if(ddr->mmap[1] == MAP_FAILED){
		loge(TAG, "Could not mmap() DDR0,1 area\n");
		exit(-1);
	}

	ddr->mmap[2] = (char *)mmap(NULL, size, PROT_READ, MAP_SHARED, mem_file, DDR1_MODE0_BASE_ADDR);
	if(ddr->mmap[2] == MAP_FAILED){
		loge(TAG, "Could not mmap() DDR1,0 area\n");
		exit(-1);
	}

	ddr->mmap[3] = (char *)mmap(NULL, size, PROT_READ, MAP_SHARED, mem_file, DDR1_MODE1_BASE_ADDR);
	if(ddr->mmap[3] == MAP_FAILED){
		loge(TAG, "Could not mmap() DDR1,1 area\n");
		exit(-1);
	}

	pmu_ddr(ddr, DDR_RD_BW); //first read can be spiky, clean it
	pmu_ddr(ddr, DDR_WR_BW); //let's do another clean just to be safe

	return 0;
}

//
//type is either DDR_RD_BW or DDR_WR_BW - yes, it is overloaded and both the addr and type identifier
uint64_t pmu_ddr(struct ddr_s *ddr, int type)
{
	uint64_t total = 0;
	uint64_t value;
	uint64_t *lastupdate;

	if(type == DDR_RD_BW){
		lastupdate = &ddr->rd_last_update[0];
	}else{
		lastupdate = &ddr->wr_last_update[0];
	}

	value = *((uint64_t*)(ddr->mmap[0] + MODE0_OFFSET + type));
//	printf("DDR0,0 RD 0x%x: 0x%lx  delta %ld Bytes\n", MODE0_OFFSET + DDR_RD_BW, value, value - ddr->rd_last_update[0]);
	total += value - *lastupdate;
	*lastupdate = value;
//	total += value - ddr->rd_last_update[0];
//	ddr->rd_last_update[0] = value;


	value = *((uint64_t*)(ddr->mmap[1] + MODE1_OFFSET + type));
//	printf("DDR0,1 RD 0x%x: 0x%lx  delta %ld Bytes\n", MODE1_OFFSET + DDR_RD_BW, value, value - ddr->rd_last_update[1]);
	total += value - *(lastupdate+1);
	*(lastupdate+1) = value;
//	total += value - ddr->rd_last_update[1];
//	ddr->rd_last_update[1] = value;

	value = *((uint64_t*)(ddr->mmap[2] + MODE0_OFFSET + type));
//	printf("DDR1,0 RD 0x%x: 0x%lx  delta %ld Bytes\n", MODE1_OFFSET + DDR_RD_BW, value, value - ddr->rd_last_update[2]);
	total += value - *(lastupdate+2);
	*(lastupdate+2) = value;
//	total += value - ddr->rd_last_update[2];
//	ddr->rd_last_update[2] = value;

	value = *((uint64_t*)(ddr->mmap[3] + MODE1_OFFSET + type));
//	printf("DDR1,1 RD 0x%x: 0x%lx  delta %ld Bytes\n", MODE1_OFFSET + DDR_RD_BW, value, value - ddr->rd_last_update[3]);
	total += value - *(lastupdate+3);
	*(lastupdate+3) = value;
//	total += value - ddr->rd_last_update[3];
//	ddr->rd_last_update[3] = value;


	return total * 64; //Count CAS, so multiply by 64 to get Bytes
}
