#include <linux/errno.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/types.h>

#include "../include/pmu_ddr.h"
#include "kernel_pmu_ddr.h"

__u32 ddr_cpu_type = DDR_NONE;
__u32 num_ddr_controllers = 0;
struct ddr_s ddr = {0};

// Reads DDR performance counters for Sierra Forest and Grandridge CPUs
// Accepts: pointer to ddr_s struct (ddr) and type of counter
// Returns: total bandwidth in bytes , updates ddr_s struct
static uint64_t kernel_pmu_ddr_grr_srf(struct ddr_s *ddr, int type)
{
	uint64_t total = 0;
	uint64_t oldvalue_rd[MAX_NUM_DDR_CONTROLLERS];
	uint64_t oldvalue_wr[MAX_NUM_DDR_CONTROLLERS];
	void __iomem *read_addr, *write_addr;
	int i;

	for (i = 0; i < num_ddr_controllers; i++) {
		if (!ddr->mmap[i]) {
			pr_err("Controller %d unmapped, skipping\n", i);
			continue;
		}

		oldvalue_rd[i] = ddr->rd_last_update[i];
		oldvalue_wr[i] = ddr->wr_last_update[i];

		read_addr = (void __iomem *)(ddr->mmap[i]);
		ddr->rd_last_update[i] = readq(read_addr);
		pr_debug("Controller %d read counter at 0x%p: %llu\n",
			 i, read_addr, ddr->rd_last_update[i]);

		write_addr =
		    (void __iomem *)(ddr->mmap[i] +
				     (GRR_SRF_FREE_RUN_CNTR_WRITE -
				      GRR_SRF_FREE_RUN_CNTR_READ));

		ddr->wr_last_update[i] = readq(write_addr);
		pr_debug("Controller %d write counter at 0x%p: %llu\n",
			 i, write_addr, ddr->wr_last_update[i]);

		if (type == DDR_PMU_RD)
			total += ddr->rd_last_update[i] - oldvalue_rd[i];
		else
			total += ddr->wr_last_update[i] - oldvalue_wr[i];
	}

	pr_info("Total %s bandwidth: %llu bytes\n",
		type == DDR_PMU_RD ? "read" : "write", total * 64);

	return total * 64;
}

// Reads DDR performance counters for Client CPUs
// Accepts: pointer to ddr_s struct (ddr) and type of counter
// Returns: total bandwidth in bytes, updates ddr_s struct
static uint64_t kernel_pmu_ddr_client(struct ddr_s *ddr, int type)
{
	uint64_t total = 0;
	uint64_t oldvalue_rd[2];
	uint64_t oldvalue_wr[2];
	void __iomem *addr;
	int i;

	pr_info("Reading %s counters for CLIENT (2 controllers)\n",
		type == DDR_PMU_RD ? "read" : "write");

	for (i = 0; i < num_ddr_controllers; i++) {
		if (!ddr->mmap[i]) {
			pr_err("Controller %d unmapped, skipping\n", i);
			continue;
		}

		oldvalue_rd[i] = ddr->rd_last_update[i];
		oldvalue_wr[i] = ddr->wr_last_update[i];

		addr = (void __iomem *)(ddr->mmap[i] + CLIENT_DDR_RD_BW);
		ddr->rd_last_update[i] = readq(addr);
		pr_debug("Controller %d read counter at 0x%p: %llu\n",
			 i, addr, ddr->rd_last_update[i]);

		addr = (void __iomem *)(ddr->mmap[i] + CLIENT_DDR_WR_BW);
		ddr->wr_last_update[i] = readq(addr);
		pr_debug("Controller %d write counter at 0x%p: %llu\n",
			 i, addr, ddr->wr_last_update[i]);

		if (type == DDR_PMU_RD)
			total += ddr->rd_last_update[i] - oldvalue_rd[i];
		else
			total += ddr->wr_last_update[i] - oldvalue_wr[i];
	}

	return total * 64;
}

// Reads DDR performance counters based on CPU type
// Accepts: pointer to ddr_s struct (ddr) and type of counter
// Returns: total bandwidth in bytes, or -EINVAL on error
uint64_t kernel_pmu_ddr(struct ddr_s *ddr, int type)
{
	pr_info("kernel_pmu_ddr: Processing %s counter request (type %d)\n",
		type == DDR_PMU_RD ? "read" : "write", ddr_cpu_type);

	if (ddr_cpu_type == DDR_CLIENT) {
		pr_info("kernel_pmu_ddr: Handling CLIENT DDR\n");
		return kernel_pmu_ddr_client(ddr, type);
	} else if (ddr_cpu_type == DDR_GRR_SRF) {
		pr_info("kernel_pmu_ddr: Handling GRR/SRF DDR\n");
		return kernel_pmu_ddr_grr_srf(ddr, type);
	}

	pr_err("kernel_pmu_ddr: Invalid DDR type %d\n", ddr_cpu_type);

	return -EINVAL;
}

// Initializes DDR performance monitoring for Sierra Forest and Grandridge CPUs
// Accepts: pointer to ddr_s struct (ddr) and base address for DDR BAR (ddr_bar)
// Returns: 0 on success, -ENODEV if no controllers, -ENOMEM on mapping failure
int kernel_pmu_ddr_init_grr_srf(struct ddr_s *ddr, uint64_t ddr_bar)
{
	int i;
	void __iomem *mapping[MAX_NUM_DDR_CONTROLLERS] = {NULL};

	if (num_ddr_controllers > MAX_NUM_DDR_CONTROLLERS) {
		pr_err("Controller count %d exceeds max %d\n",
		       num_ddr_controllers, MAX_NUM_DDR_CONTROLLERS);
		num_ddr_controllers = MAX_NUM_DDR_CONTROLLERS;
	}

	pr_info("Initializing GRR/SRF DDR with %d controllers, BAR=0x%llx\n",
		num_ddr_controllers, ddr_bar);

	memset(ddr, 0, sizeof(struct ddr_s));
	ddr->ddr_interface_type = DDR_GRR_SRF;
	ddr->bar_address = ddr_bar;

	if (num_ddr_controllers <= 0) {
		pr_err("No controllers configured (%d)\n",
		       num_ddr_controllers);
		return -ENODEV;
	}

	for (i = 0; i < num_ddr_controllers; i++) {
		uint64_t reg_base =
		    ddr_bar + GRR_SRF_MC_ADDRESS(i) + GRR_SRF_FREE_RUN_CNTR_READ;

		pr_info("Mapping controller %d at 0x%llx\n", i, reg_base);

		if (!request_mem_region(reg_base, GRR_SRF_DDR_RANGE,
					"dpf_ddr_server")) {
			pr_err("Cannot reserve region 0x%llx (size=0x%x)\n",
			       reg_base, GRR_SRF_DDR_RANGE);
			struct resource *conflicting =
			    __request_region(&iomem_resource, reg_base,
					     GRR_SRF_DDR_RANGE, NULL, 0);

			pr_info("Region 0x%llx used by %s\n",
				reg_base, conflicting ? conflicting->name : "unknown");
		}

		mapping[i] = ioremap(reg_base, GRR_SRF_DDR_RANGE);
		if (!mapping[i]) {
			pr_err("Failed to map controller %d at 0x%llx\n",
			       i, reg_base);
			release_mem_region(reg_base, GRR_SRF_DDR_RANGE);
			goto cleanup;
		}

		ddr->mmap[i] = (char *)mapping[i];
		pr_info("Controller %d mapped: 0x%llx-0x%llx\n",
			i, reg_base, reg_base + GRR_SRF_DDR_RANGE - 1);
	}

	kernel_pmu_ddr(ddr, DDR_PMU_RD);
	kernel_pmu_ddr(ddr, DDR_PMU_WR);

	pr_info("Initialization complete\n");

	return 0;

cleanup:
	while (--i >= 0) {
		if (mapping[i]) {
			iounmap(mapping[i]);
			release_mem_region(ddr_bar + GRR_SRF_MC_ADDRESS(i) +
					       GRR_SRF_FREE_RUN_CNTR_READ,
					   GRR_SRF_DDR_RANGE);

			ddr->mmap[i] = NULL;
		}
	}

	pr_err("Initialization failed\n");

	return -ENOMEM;
}

// Initializes DDR performance monitoring for Client CPUs
// Accepts: pointer to ddr_s struct (ddr) and base address for DDR BAR (ddr_bar)
// Returns: 0 on success, -ENOMEM on mapping failure
int kernel_pmu_ddr_init_client(struct ddr_s *ddr, uint64_t ddr_bar)
{
	int i;
	void __iomem *mapping[2] = {NULL};
	const uint64_t offsets[2] = {CLIENT_DDR0_OFFSET, CLIENT_DDR1_OFFSET};

	pr_info("Initializing CLIENT DDR with 2 controllers, BAR=0x%llx\n",
		ddr_bar);

	memset(ddr, 0, sizeof(struct ddr_s));
	ddr->ddr_interface_type = DDR_CLIENT;
	ddr->bar_address = ddr_bar;

	for (i = 0; i < num_ddr_controllers; i++) {
		uint64_t reg_base = ddr_bar + offsets[i];

		pr_info("Mapping controller %d at 0x%llx\n", i, reg_base);

		if (!request_mem_region(reg_base, CLIENT_DDR_RANGE,
					"dpf_ddr_client")) {
			pr_err("Cannot reserve region 0x%llx (size=0x%x)\n",
			       reg_base, CLIENT_DDR_RANGE);
			struct resource *conflicting =
			    __request_region(&iomem_resource, reg_base,
					     CLIENT_DDR_RANGE, NULL, 0);

			pr_info("Region 0x%llx used by %s\n",
				reg_base, conflicting ? conflicting->name : "unknown");
		}

		mapping[i] = ioremap(reg_base, CLIENT_DDR_RANGE);
		if (!mapping[i]) {
			pr_err("Failed to map controller %d at 0x%llx\n",
			       i, reg_base);
			release_mem_region(reg_base, CLIENT_DDR_RANGE);
			goto cleanup;
		}

		ddr->mmap[i] = (char *)mapping[i];
		pr_info("Controller %d mapped: 0x%llx-0x%llx\n",
			i, reg_base, reg_base + CLIENT_DDR_RANGE - 1);
	}

	kernel_pmu_ddr(ddr, DDR_PMU_RD);
	kernel_pmu_ddr(ddr, DDR_PMU_WR);

	pr_info("Initialization complete\n");

	return 0;

cleanup:
	while (--i >= 0) {
		if (mapping[i]) {
			iounmap(mapping[i]);
			release_mem_region(ddr_bar + offsets[i],
					   CLIENT_DDR_RANGE);
			ddr->mmap[i] = NULL;
		}
	}

	pr_err("Initialization failed\n");

	return -ENOMEM;
}

// Reads DDR read and write bandwidth counters
// Accepts: pointers to store read bandwidth and write bandwidth
// Returns: 0 on success, -ENOMEM if DDR not initialized or no controllers
int read_ddr_counters(uint64_t *read_bw, uint64_t *write_bw)
{
	if (ddr_cpu_type != DDR_GRR_SRF && ddr_cpu_type != DDR_CLIENT) {
		pr_err("read_ddr_counters: DDR not initialized (type %d)\n",
		       ddr_cpu_type);
		*read_bw = 0;
		*write_bw = 0;

		return -ENOMEM;
	}

	if (num_ddr_controllers <= 0) {
		pr_err("No controllers configured for type %d\n", ddr_cpu_type);
		*read_bw = 0;
		*write_bw = 0;

		return -ENOMEM;
	}

	*read_bw = kernel_pmu_ddr(&ddr, DDR_PMU_RD);
	*write_bw = kernel_pmu_ddr(&ddr, DDR_PMU_WR);

	if (*read_bw == (uint64_t)-EINVAL || *write_bw == (uint64_t)-EINVAL) {
		pr_err("Failed to read counters for type %d\n", ddr_cpu_type);
		*read_bw = 0;
		*write_bw = 0;

		return -ENOMEM;
	}

	return 0;
}
