#define _GNU_SOURCE

#include <linux/timekeeping.h>

//#include "../include/atom_msr.h"
#include "kernel_common.h"

int sys_first_core = 0; //set by core_range API
int sys_active_cores = 0; //set by core_range API

struct core_state_s corestate[MAX_NUM_CORES];
int ddr_bw_target;

//returns the core index of the first core.
//use this function and NOT the raw variable
inline int first_core(void)
{
	return sys_first_core;
}

//returns the number of active cores.
//use this function and NOT the raw variable
inline int active_cores(void)
{
	return sys_active_cores;
}

//call with current core_id
//returns the position within the 4-core module, from 0 to 3
inline int core_in_module(int core_id)
{
	return (core_id - sys_first_core) % 4;
}

//call with current core_id
//returns the index of the 4-core module, starting at 0.
inline int module_id(int core_id)
{
	return (core_id - sys_first_core) / 4;
}


//update the pmu field in the corestate struct for the given core.
// IMPORTANT: This has to be the core with core_id that calls this function or incorrect state will be updated

int pmu_update(int core_id)
{
	if (core_id < 0 || core_id >= MAX_NUM_CORES || corestate[core_id].core_disabled)
		return -EINVAL;

	// Update PMU counters using the defined indices from kernel_common.h
	corestate[core_id].pmu_result[PERF_MEM_UOPS_RETIRED_ALL_LOADS] = native_read_pmc(0);	 // PMC0
	corestate[core_id].pmu_result[PERF_MEM_LOAD_UOPS_RETIRED_L2_HIT] = native_read_pmc(1);	 // PMC1
	corestate[core_id].pmu_result[PERF_MEM_LOAD_UOPS_RETIRED_L3_HIT] = native_read_pmc(2);	 // PMC2
	corestate[core_id].pmu_result[PERF_MEM_LOAD_UOPS_RETIRED_DRAM_HIT] = native_read_pmc(3); // PMC3
	corestate[core_id].pmu_result[PERF_XQ_PROMOTION_ALL] = native_read_pmc(4);		 // PMC4
	corestate[core_id].pmu_result[PERF_CPU_CLK_UNHALTED_THREAD] = native_read_pmc(5);	 // PMC5
	corestate[core_id].pmu_result[PERF_INST_RETIRED_ANY_P] = native_read_pmc(6);		 // PMC6

	return 0;
}

//returns the msr dirty indicator, 0 if not dirty, 1 if dirty
int inline is_msr_dirty(int core_id)
{
	return corestate[core_id].pf_msr_dirty;
}

//sets the msr dirty indicator for the given core
int inline msr_set_dirty(int core_id)
{
	corestate[core_id].pf_msr_dirty = 1;

	return 0;
}


//Loads MSR values into the corestate msr field
// IMPORTANT: This has to be the core with core_id that calls this function or incorrect state will be updated
int msr_load(int core_id)
{
	corestate[core_id].pf_msr[MSR_1320_INDEX].v = __rdmsr(0x1320);
	corestate[core_id].pf_msr[MSR_1321_INDEX].v = __rdmsr(0x1321);
	corestate[core_id].pf_msr[MSR_1322_INDEX].v = __rdmsr(0x1322);
	corestate[core_id].pf_msr[MSR_1323_INDEX].v = __rdmsr(0x1323);
	corestate[core_id].pf_msr[MSR_1324_INDEX].v = __rdmsr(0x1324);
	corestate[core_id].pf_msr[MSR_1A4_INDEX].v = __rdmsr(0x1a4);

	return 0;
}

//Update MSR values from corestate msr field if the msr dirty bit has been set for this core
// IMPORTANT: This has to be the core with core_id that calls this function or incorrect state will be updated
int msr_update(int core_id)
{
	corestate[core_id].pf_msr_dirty = 1; //reset msr state

	__wrmsr(0x1320, (u32)corestate[core_id].pf_msr[MSR_1320_INDEX].v, (u32) (corestate[core_id].pf_msr[MSR_1320_INDEX].v >> 32));
	__wrmsr(0x1321, (u32)corestate[core_id].pf_msr[MSR_1321_INDEX].v, (u32) (corestate[core_id].pf_msr[MSR_1321_INDEX].v >> 32));
	__wrmsr(0x1322, (u32)corestate[core_id].pf_msr[MSR_1322_INDEX].v, (u32) (corestate[core_id].pf_msr[MSR_1322_INDEX].v >> 32));
	__wrmsr(0x1323, (u32)corestate[core_id].pf_msr[MSR_1323_INDEX].v, (u32) (corestate[core_id].pf_msr[MSR_1323_INDEX].v >> 32));
	__wrmsr(0x1324, (u32)corestate[core_id].pf_msr[MSR_1324_INDEX].v, (u32) (corestate[core_id].pf_msr[MSR_1324_INDEX].v >> 32));
	__wrmsr(0x1a4, (u32)corestate[core_id].pf_msr[MSR_1A4_INDEX].v, (u32) (corestate[core_id].pf_msr[MSR_1A4_INDEX].v >> 32));

	return 0;
}

// Set value in MSR table
int msr_set_l2xq(int core_id, int value)
{
	corestate[core_id].pf_msr[MSR_1320_INDEX].msr1320.L2_STREAM_AMP_XQ_THRESHOLD = value;

	return 0;
}

int msr_get_l2xq(int core_id)
{
	return corestate[core_id].pf_msr[MSR_1320_INDEX].msr1320.L2_STREAM_AMP_XQ_THRESHOLD;
}

// Set value in MSR table
int msr_set_l3xq(int core_id, int value)
{
	corestate[core_id].pf_msr[MSR_1320_INDEX].msr1320.LLC_STREAM_XQ_THRESHOLD = value;

	return 0;
}

int msr_get_l3xq(int core_id)
{
	return corestate[core_id].pf_msr[MSR_1320_INDEX].msr1320.LLC_STREAM_XQ_THRESHOLD;
}
