#include <stdio.h>
#include <string.h>

#include "pcie.h"
#include "pmu_ddr.h"
#include "sysdetect.h"
#include "sysinfo.h"
#include "user_api.h"

// detect DDR configuration using kernel mode functions
// accepts a pointer to a ddr_s struct for output
// returns 0 on success, -1 on failure
int kernel_ddr_detect(struct ddr_s *ddr)
{
	int ret;

	if (!ddr) {
		fprintf(stderr, "DDR pointer is NULL\n");
		return -1;
	}

	ret = kernel_mode_init();
	if (ret < 0) {
		fprintf(stderr, "Kernel mode initialization failed\n");
		return -1;
	}

	ret = kernel_set_ddr_bandwidth(0); // trigger DDR detection
	if (ret < 0) {
		fprintf(stderr, "Kernel set DDR bandwidth failed\n");
		return -1;
	}

	return 0;
}

// check if the processor is a hybrid architecture
// Accepts no parameters
// returns 1 if hybrid, 0 otherwise
static int get_hybridflag(void)
{
	unsigned int values[4];
	unsigned int leaf = CPUID_LEAF_EXT_FEATURES;
	unsigned int subleaf = 0;

	__cpuid_count(leaf, subleaf, values[0], values[1], values[2], values[3]);

	return (values[3] >> CPUID_HYBRID_FLAG_BIT) & 0x01;
}

// collect system information and fill the provided struct
// accepts a pointer to a dpf_console_sysinfo struct for output
// returns 0 on success, -1 on failure
int collect_sysinfo(struct dpf_console_sysinfo *out)
{
	struct e_cores_layout_s cores; // efficient core IDs
	struct ddr_s ddr_config;
	int ret;

	if (!out) {
		fprintf(stderr, "Output pointer is NULL\n");
		return -1;
	}

	ret = pcie_init();
	if (ret < 0) {
		fprintf(stderr, "PCIe initialization failed\n");
		return -1;
	}
	ret = kernel_mode_init();
	if (ret < 0) {
		fprintf(stderr, "Kernel mode initialization failed\n");
		return -1;
	}

	out->is_hybrid = get_hybridflag();

	cores = get_efficient_core_ids();
	out->first_core = cores.first_efficiency_core;
	out->last_core = cores.last_efficiency_core;

	// DDR config detection
	memset(&ddr_config, 0, sizeof(ddr_config));
	pmu_ddr_init(&ddr_config, 1);

	ret = kernel_ddr_detect(&ddr_config);
	if (ret < 0) {
		printf("DDR detection failed.\n");
		return -1;
	}

	ret = kernel_set_ddr_config(&ddr_config);
	if (ret < 0) {
		printf("DDR config confirmation failed.\n");
		return -1;
	}

	out->confirmed_bar = ddr_config.bar_address;
	out->confirmed_ddr_type = ddr_config.ddr_interface_type;
	out->num_ddr_channels = ddr_config.num_ddr_controllers;

	// theoretical memory bandwidth
	out->theoretical_bw = dmi_get_bandwidth();

	return 0;
}
