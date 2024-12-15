#ifndef PMU_CORE_H
#define PMU_CORE_H

#include <linux/perf_event.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

// Define PMU Constants
#define PMU_COUNTERS (7)
#define PMU_PERF (0)
#define PMU_RAW (1)

// Perf Event constants
#define PERF_EVENT_CYCLES PERF_COUNT_HW_CPU_CYCLES
#define PERF_EVENT_INSTRUCTIONS PERF_COUNT_HW_INSTRUCTIONS

// Event Indices for perf events

#define PERF_INDEX_EVENT_MEM_UOPS_RETIRED_ALL_LOADS (0)
#define PERF_INDEX_EVENT_MEM_LOAD_UOPS_RETIRED_L2_HIT (1)
#define PERF_INDEX_EVENT_MEM_LOAD_UOPS_RETIRED_L3_HIT (2)
#define PERF_INDEX_EVENT_MEM_LOAD_UOPS_RETIRED_DRAM_HIT (3)
#define PERF_INDEX_EVENT_XQ_PROMOTION_ALL (4)
#define PERF_INDEX_EVENT_CYCLES (5)
#define PERF_INDEX_EVENT_INSTRUCTIONS (6)
 
#define MAX_EVENTS (8)
#define MAX_CORES (8)

// PMU PMC Registers (Performance Monitoring Counters)
#define PMU_PMC0 (0xc1)
#define PMU_PMC1 (0xc2)
#define PMU_PMC2 (0xc3)
#define PMU_PMC3 (0xc4)
#define PMU_PMC4 (0xc5)
#define PMU_PMC5 (0xc6)

// PMU PERF_EVENT Registers (Performance Monitoring Events)
#define PMU_PERFEVTSEL0 (0x186)
#define PMU_PERFEVTSEL1 (0x187)
#define PMU_PERFEVTSEL2 (0x188)
#define PMU_PERFEVTSEL3 (0x189)
#define PMU_PERFEVTSEL4 (0x18a)
#define PMU_PERFEVTSEL5 (0x18b)

// Event types for PMU configuration (event codes for various counters)
// Full 64-bit event codes including config bits
#define EVENT_CPU_CLK_UNHALTED_THREAD (0x00000000004300c0)
#define EVENT_INST_RETIRED_ANY_P (0x00000000004300c2)
#define EVENT_MEM_UOPS_RETIRED_ALL_LOADS (0x00000000004381d0)
#define EVENT_MEM_LOAD_UOPS_RETIRED_L2_HIT (0x00000000004302d1)
#define EVENT_MEM_LOAD_UOPS_RETIRED_L3_HIT (0x00000000004304d1)
#define EVENT_MEM_LOAD_UOPS_RETIRED_DRAM_HIT (0x00000000004380d1)
#define EVENT_XQ_PROMOTION_ALL (0x00000000004300f4)


#define PERF_CPU_CLK_UNHALTED_THREAD (0x00c0)
#define PERF_INST_RETIRED_ANY_P (0x00c2)
#define PERF_MEM_UOPS_RETIRED_ALL_LOADS (0x81d0)
#define PERF_MEM_LOAD_UOPS_RETIRED_L2_HIT (0x02d1)
#define PERF_MEM_LOAD_UOPS_RETIRED_L3_HIT (0x04d1)
#define PERF_MEM_LOAD_UOPS_RETIRED_DRAM_HIT (0x80d1)
#define PERF_XQ_PROMOTION_ALL (0x00f4)

// Function declarations for PMU configuration and interaction
// MSR-based PMU functions
int pmu_core_config(int msr_file);
int pmu_core_read(int msr_file, uint64_t *result_p, uint64_t *inst_retired, uint64_t *cpu_cycles);
int pmu_core_clear(int msr_file);

// Perf event configuration and interaction
int perf_configure_events(struct perf_event_attr *event_attrs, int *num_events);
int perf_init(struct perf_event_attr *event_attrs, int event_fds[MAX_EVENTS],
	      int num_events, int core_id);
int perf_read(int event_fds[MAX_EVENTS], uint64_t *event_counts,
	      int num_events);
int perf_deinit(int event_fds[MAX_EVENTS], int num_events);

#endif // PMU_CORE_H
