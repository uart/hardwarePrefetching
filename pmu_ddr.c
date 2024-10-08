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
#include "pmu_ddr.h"
#include "pcie.h"
#include "log.h"

#define TAG "PMU_DDR"

#define DDR_NONE (-1)
#define DDR_CLIENT (1)
#define DDR_SERVER (2)

static int ddr_interface_type;

//Initialize DDR for a client CPU
//ddr_bar is the base address of the DDR config space
static int pmu_ddr_init_client(struct ddr_s *ddr, uint64_t ddr_bar)
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


//Searches and initializes the DDR PMU
//Returns interface type, including DDR_NONE if nothing found
int pmu_ddr_init(struct ddr_s *ddr)
{
	struct pci_dev *dev;
	uint64_t ddr_bar;
					
	dev = pcie_get_devices();

	while(dev != NULL) {
		pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS);	// Fill in header info we need
		logd(TAG, "%04x:%02x:%02x.%d vendor=%04x device=%04x class=%04x base0=%lx\n",
			dev->domain, dev->bus, dev->dev, dev->func, dev->vendor_id, dev->device_id,
			dev->device_class, (long) dev->base_addr[0]);


		if(dev->vendor_id == 0x8086) {
			switch(dev->device_id) {
				case 0x7d05: //client DDR controller			
					ddr_bar = pci_read_long(dev, 0x48) & 0xfffffff0; //get base address
					logv(TAG, "PCIe %x: DDR BAR: %x\n",dev->device_id, ddr_bar);	
					ddr_interface_type = DDR_CLIENT;				
				break;
							
				default:
				break;	
			}
		} //if(0x8086)
		
		dev = dev->next;
	}
	
	if(ddr_interface_type == DDR_CLIENT) {
		pmu_ddr_init_client(ddr, ddr_bar);
	}

	return ddr_interface_type;
}


//
//type is either DDR_RD_BW or DDR_WR_BW - yes, it is overloaded and both the addr and type identifier
static uint64_t pmu_ddr_client(struct ddr_s *ddr, int type)
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


//Reads current DDR counter values in bytes from the boot / initialization
//type: DDR_RD_BW or DDR_WR_BW
//returns current counter value for either RD or WR, -1 if error
uint64_t pmu_ddr(struct ddr_s *ddr, int type)
{
	if(ddr_interface_type == DDR_CLIENT) {
		return pmu_ddr_client(ddr, type);
	}

	return -1;
}
