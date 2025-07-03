#include <stdio.h>
#include <string.h>

#include "kernel_common.h"
#include "metrics.h"
#include "user_api.h"

// read PMU values for a specific core
// accepts the core ID and a pointer to an array where the PMU values
// will be stored
// returns 0 on success, -1 on failure
int read_pmu(int core_id, uint64_t *pmu_values)
{
	int ret;

	if (core_id < 0 || core_id >= MAX_CORES) {
		fprintf(stderr, "Invalid core ID: %d\n", core_id);
		return -1;
	}

	if (!pmu_values) {
		fprintf(stderr, "PMU values pointer is NULL\n");
		return -1;
	}

	ret = kernel_pmu_read(core_id, pmu_values);
	if (ret < 0) {
		fprintf(stderr, "Error reading PMU values for core %d: %d\n",
			core_id, ret);
		return -1;
	}

	return 0;
}

// read MSR values for a specific core
// accepts the core ID and a pointer to an array where the MSR values
// will be stored
// returns 0 on success, -1 on failure
int read_msr(int core_id, uint64_t *msr_values)
{
	int ret;

	if (core_id < 0 || core_id >= MAX_CORES) {
		fprintf(stderr, "Invalid core ID: %d\n", core_id);
		return -1;
	}

	if (!msr_values) {
		fprintf(stderr, "MSR values pointer is NULL\n");
		return -1;
	}

	ret = kernel_msr_read(core_id, msr_values);
	if (ret < 0) {
		fprintf(stderr, "Error reading MSR values for core %d: %d\n",
			core_id, ret);
		return -1;
	}

	return 0;
}

// read DDR bandwidth values
// accepts the read and write bandwidth values as pointers
// returns 0 on success, -1 on failure
int read_ddr_bw(uint64_t *read_bw, uint64_t *write_bw)
{
	int ret;

	if (!read_bw || !write_bw) {
		fprintf(stderr, "Read or write bandwidth pointer is NULL\n");
		return -1;
	}

	ret = kernel_ddr_bw_read(read_bw, write_bw);

	if (ret < 0) {
		fprintf(stderr, "Failed to read DDR bandwidth values\n");
		return -1;
	}

	return 0;
}