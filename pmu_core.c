#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/perf_event.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/signal.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include "log.h"
#include "msr.h"
#include "pmu_core.h"

#define TAG "PMU_CORE"

#define PMU_CORE_EVENT_COUNT (7)

static long open_perf_event(struct perf_event_attr *attr, pid_t pid, int cpu,
			    int group_fd, unsigned long flags)
{
	return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

// Configure perf events
int perf_configure_events(struct perf_event_attr *event_attrs, int *num_events)
{
	*num_events = PMU_CORE_EVENT_COUNT;

	// Array of event configurations
	static const uint64_t event_configs[] = {
	    PERF_MEM_UOPS_RETIRED_ALL_LOADS,
	    PERF_MEM_LOAD_UOPS_RETIRED_L2_HIT,
	    PERF_MEM_LOAD_UOPS_RETIRED_L3_HIT,
	    PERF_MEM_LOAD_UOPS_RETIRED_DRAM_HIT,
	    PERF_XQ_PROMOTION_ALL,
	    PERF_CPU_CLK_UNHALTED_THREAD,
	    PERF_INST_RETIRED_ANY_P};

	// Configure each event
	for (int i = 0; i < *num_events; i++) {
		memset(&event_attrs[i], 0, sizeof(struct perf_event_attr));
		event_attrs[i].size = sizeof(struct perf_event_attr);
		event_attrs[i].type = PERF_TYPE_RAW;
		event_attrs[i].disabled = 1;
		event_attrs[i].exclude_kernel = 0;
		event_attrs[i].exclude_hv = 0;
		event_attrs[i].exclude_idle = 0;
		event_attrs[i].config = event_configs[i];
	}

	return 0;
}

// Initialize perf events on a specific CPU core
int perf_init(struct perf_event_attr *event_attrs, int event_fds[MAX_EVENTS],
	      int num_events, int core_id)
{
	int group_fd = -1;

	// Check if core_id is valid
	if (core_id < 0) {
		loge(TAG, "Invalid core_id: %d\n", core_id);
		return -1;
	}

	for (int i = 0; i < num_events; i++) {
		event_fds[i] = open_perf_event(&event_attrs[i], -1, core_id,
					       group_fd, 0);
		if (event_fds[i] == -1) {
			loge(TAG, "Failed to open event %d on core %d: %s "
				  "(errno=%d)\n",
			     i, core_id, strerror(errno), errno);
			// Cleanup previously opened events
			for (int j = 0; j < i; j++) {
				if (event_fds[j] != -1) {
					close(event_fds[j]);
					event_fds[j] = -1;
				}
			}

			return -1;
		}

		// Enable the event
		if (ioctl(event_fds[i], PERF_EVENT_IOC_ENABLE, 0) == -1) {
			loge(TAG, "Failed to enable event %d: %s\n",
			     i, strerror(errno));
			// Cleanup
			for (int j = 0; j <= i; j++) {
				if (event_fds[j] != -1) {
					close(event_fds[j]);
					event_fds[j] = -1;
				}
			}

			return -1;
		}
	}

	return 0;
}

// Read the performance counters
int perf_read(int event_fds[MAX_EVENTS], uint64_t *event_counts,
	      int num_events) {
	for (int i = 0; i < num_events; i++) {
		if (read(event_fds[i], &event_counts[i],
			 sizeof(uint64_t)) == -1) {
			loge(TAG, "Failed to read event %d: %s\n",
			     i, strerror(errno));
			return -1;
		}
	}

	return 0;
}

// Cleanup and close perf events
int perf_deinit(int event_fds[MAX_EVENTS], int num_events) {
	for (int i = 0; i < num_events; i++) {
		if (event_fds[i] != -1) {
			close(event_fds[i]);
		}
	}

	return 0;
}

int pmu_core_clear(int msr_file) {
	// lets exaggerate, max 20 PMU couters, "should" be more than enough
	uint64_t events[20] = {0};

	msr_corepmu_setup(msr_file, PMU_COUNTERS, events);

	// Reset counter values to zero - avoids potential overflow when //
	// running benchmarks.
	// A more robust solution would likely be needed for general use
	uint64_t zero_value = 0;
	for (int i = 0; i < PMU_COUNTERS; i++) {
		if (pwrite(msr_file, &zero_value, sizeof(zero_value),
			   PMU_PMC0 + i) != sizeof(zero_value)) {
			loge(TAG, "Could not reset PMU counter value for "
				  "counter %d\n",
			     i);
			return -1;
		}
	}

	return 0;
}

int pmu_core_config(int msr_file) {
	uint64_t events[PMU_CORE_EVENT_COUNT] = {
	    EVENT_MEM_UOPS_RETIRED_ALL_LOADS,
	    EVENT_MEM_LOAD_UOPS_RETIRED_L2_HIT,
	    EVENT_MEM_LOAD_UOPS_RETIRED_L3_HIT,
	    EVENT_MEM_LOAD_UOPS_RETIRED_DRAM_HIT,
	    EVENT_XQ_PROMOTION_ALL};

	pmu_core_clear(msr_file); // reset
	msr_corepmu_setup(msr_file, PMU_CORE_EVENT_COUNT, events);

	return 0;
}

int pmu_core_read(int msr_file, uint64_t *result_p, uint64_t *inst_retired,
		  uint64_t *cpu_cycles) {
	msr_corepmu_read(msr_file, PMU_CORE_EVENT_COUNT, result_p,
			 inst_retired, cpu_cycles);

	return 0;
}
