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

static int ddr_interface_type;

//Initialize DDR for a grandridge CPU
//ddr_bar is the base address of the DDR config space
static int pmu_ddr_init_grandridge(struct ddr_s *ddr, uint64_t ddr_bar)
{
	int mem_file;

	mem_file = open("/dev/mem", O_RDONLY);

	if (mem_file < 0) {
		loge(TAG, "Could not open MEM file /dev/mem, running as root/sudo?\n");
		return -1;
	}

	ddr->mem_file = mem_file;

	ddr->mmap[0] = (char *)mmap(NULL, CLIENT_DDR_RANGE, PROT_READ,
		MAP_SHARED, mem_file, ddr_bar + GRANDRIDGE_MC0_BASE);
	if (ddr->mmap[0] == MAP_FAILED) {
		loge(TAG, "Could not mmap() Grand Ridge MC0 area\n");
		return -1;
	}

	ddr->mmap[1] = (char *)mmap(NULL, CLIENT_DDR_RANGE, PROT_READ,
		MAP_SHARED, mem_file, ddr_bar + GRANDRIDGE_MC1_BASE);
	if (ddr->mmap[1] == MAP_FAILED) {
		loge(TAG, "Could not mmap() Grand Ridge MC1 area\n");
		return -1;
	}


	pmu_ddr(ddr, DDR_RD_BW);
	pmu_ddr(ddr, DDR_WR_BW);

	return 0;
}


//Initialize DDR for a client CPU
//ddr_bar is the base address of the DDR config space
static int pmu_ddr_init_client(struct ddr_s *ddr, uint64_t ddr_bar)
{
	int mem_file;

	mem_file = open("/dev/mem", O_RDONLY);

	if (mem_file < 0) {
		loge(TAG, "Could not open MEM file /dev/mem, running as root/sudo?\n");
		return -1;
	}

	ddr->mem_file = mem_file;

	//printf("Opening 0x%x\n", ddr_bar + CLIENT_DDR0_OFFSET);
	ddr->mmap[0] = (char *)mmap(NULL, CLIENT_DDR_RANGE, PROT_READ, MAP_SHARED, mem_file, ddr_bar + CLIENT_DDR0_OFFSET);
	if (ddr->mmap[0] == MAP_FAILED) {
		loge(TAG, "Could not mmap() DDR0,0 area\n");
		return -1;
	}

	//printf("Opening 0x%x\n", ddr_bar + CLIENT_DDR1_OFFSET);
	ddr->mmap[1] = (char *)mmap(NULL, CLIENT_DDR_RANGE, PROT_READ, MAP_SHARED, mem_file, ddr_bar + CLIENT_DDR1_OFFSET);
	if (ddr->mmap[1] == MAP_FAILED) {
		loge(TAG, "Could not mmap() DDR0,1 area\n");
		return -1;
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
	uint64_t ddr_bar = 0;

	ddr_interface_type = DDR_NONE;


	dev = pcie_get_devices();

	while (dev != NULL) {
		pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS);	// Fill in header info we need
		logd(TAG, "%04x:%02x:%02x.%d vendor=%04x device=%04x class=%04x base0=%lx\n",
			dev->domain, dev->bus, dev->dev, dev->func, dev->vendor_id, dev->device_id,
			dev->device_class, (long) dev->base_addr[0]);


		if (dev->vendor_id == 0x8086) {
			switch (dev->device_id) {
			//client DDR controller
			case 0xa700: //RPL
			case 0x7d05: //MTL
				ddr_bar = pci_read_long(dev, 0x48) &
					0xfffffff0; //get base address
				logv(TAG, "PCIe %x: DDR BAR: %x\n",
					dev->device_id, ddr_bar);
				ddr_interface_type = DDR_CLIENT;
			break;
			case 0x3251:
				uint64_t mmio_base, scf_bar;

				mmio_base = pci_read_long(dev, 0xD0);
				scf_bar = pci_read_long(dev, 0xD4);
				ddr_bar = 0xF6000000;
				ddr_interface_type = DDR_GRANDRIDGE;

				logd(TAG, "MMIO BASE: %lx\n",
					mmio_base);
				logd(TAG, "SCF BAR: %lx\n", scf_bar);
			break;

			default:
			break;
			}
		} //if (0x8086)

		dev = dev->next;
	}

	if (ddr_interface_type == DDR_CLIENT) {
		int ret = pmu_ddr_init_client(ddr, ddr_bar);

		if (ret < 0)
			ddr_interface_type = DDR_NONE;
	} else if (ddr_interface_type == DDR_GRANDRIDGE) {
		int ret = pmu_ddr_init_grandridge(ddr, ddr_bar);

		if (ret < 0)
			ddr_interface_type = DDR_NONE;
	}

	return ddr_interface_type;
}


//DDR read functionality for Client CPUs
//Currently limited to two DDR controllers
//type is either DDR_RD_BW or DDR_WR_BW. The ddr_s struct is updated with both
//read and write counters but only the type is returned by the function.
static uint64_t pmu_ddr_client(struct ddr_s *ddr, int type)
{
	uint64_t total;
	uint64_t oldvalue_rd[2];
	uint64_t oldvalue_wr[2];

	char *addr;

	oldvalue_rd[0] = ddr->rd_last_update[0];
	oldvalue_rd[1] = ddr->rd_last_update[1];
	oldvalue_wr[0] = ddr->wr_last_update[0];
	oldvalue_wr[1] = ddr->wr_last_update[1];

	addr = ddr->mmap[0] + DDR_RD_BW;
	ddr->rd_last_update[0] = *((uint64_t *)addr);

	addr = ddr->mmap[0] + DDR_WR_BW;
	ddr->wr_last_update[0] = *((uint64_t *)addr);

	addr = ddr->mmap[1] + DDR_RD_BW;
	ddr->rd_last_update[1] = *((uint64_t *)addr);

	addr = ddr->mmap[1] + DDR_WR_BW;
	ddr->wr_last_update[1] = *((uint64_t *)addr);

	//what should we return? RD or WR?
	if (type == DDR_RD_BW) {
		total = ddr->rd_last_update[0] - oldvalue_rd[0];
		total += ddr->rd_last_update[1] - oldvalue_rd[1];
	} else {
		total = ddr->wr_last_update[0] - oldvalue_wr[0];
		total += ddr->wr_last_update[1] - oldvalue_wr[1];
	}

	return total * 64; //Count CAS, so multiply by 64 to get Bytes
}


// DDR read functionality for Grandridge CPUs
//The ddr_s struct is updated with both
//read and write counters but only the type is returned by the function.
static uint64_t pmu_ddr_grandridge(struct ddr_s *ddr, int type)
{
	uint64_t total;
	uint64_t oldvalue_rd[2];
	uint64_t oldvalue_wr[2];
	char *addr;

	// Store old values for calculations
	oldvalue_rd[0] = ddr->rd_last_update[0];
	oldvalue_rd[1] = ddr->rd_last_update[1];
	oldvalue_wr[0] = ddr->wr_last_update[0];
	oldvalue_wr[1] = ddr->wr_last_update[1];

	// Read from DDR0
	addr = ddr->mmap[0] + GRANDRIDGE_FREE_RUN_CNTR_READ;
	ddr->rd_last_update[0] = *((uint64_t *)addr);

	addr = ddr->mmap[0] + GRANDRIDGE_FREE_RUN_CNTR_WRITE;
	ddr->wr_last_update[0] = *((uint64_t *)addr);

	// Read from DDR1
	addr = ddr->mmap[1] + GRANDRIDGE_FREE_RUN_CNTR_READ;
	ddr->rd_last_update[1] = *((uint64_t *)addr);

	addr = ddr->mmap[1] + GRANDRIDGE_FREE_RUN_CNTR_WRITE;
	ddr->wr_last_update[1] = *((uint64_t *)addr);

	if (type == DDR_RD_BW) {
		total = ddr->rd_last_update[0] - oldvalue_rd[0];
		total += ddr->rd_last_update[1] - oldvalue_rd[1];
	} else {
		total = ddr->wr_last_update[0] - oldvalue_wr[0];
		total += ddr->wr_last_update[1] - oldvalue_wr[1];
	}

	return total * 64;
}

//Reads current DDR counter values in bytes from the boot / initialization
//type: DDR_RD_BW or DDR_WR_BW
//returns current counter value for either RD or WR, -1 if error
uint64_t pmu_ddr(struct ddr_s *ddr, int type)
{
	if (ddr_interface_type == DDR_CLIENT)
		return pmu_ddr_client(ddr, type);
	else if (ddr_interface_type == DDR_GRANDRIDGE)
		return pmu_ddr_grandridge(ddr, type);

	return -1;
}
