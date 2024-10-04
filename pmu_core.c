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
#include "pmu_core.h"
#include "log.h"

#define TAG "PMU_CORE"

#define PMU_CORE_EVENT_COUNT (5)

int pmu_core_clear(int msr_file)
{
	//lets exaggerate, max 20 PMU couters, "should" be more than enough
	uint64_t events[20] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

	msr_corepmu_setup(msr_file, PMU_COUNTERS, events);

	// Reset counter values to zero - avoids potential overflow when running benchmarks.
	// A more robust solution would likely be needed for general use
    uint64_t zero_value = 0;
    for (int i = 0; i < PMU_COUNTERS; i++) {
        if (pwrite(msr_file, &zero_value, sizeof(zero_value), PMU_PMC0 + i) != sizeof(zero_value)) {
            loge(TAG, "Could not reset PMU counter value for counter %d\n", i);
            return -1;
        }
    }

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

int pmu_core_read(int msr_file, uint64_t *result_p, uint64_t *inst_retired, uint64_t *cpu_cycles)
{
	msr_corepmu_read(msr_file, PMU_CORE_EVENT_COUNT, result_p, inst_retired, cpu_cycles);

	return 0;
}
