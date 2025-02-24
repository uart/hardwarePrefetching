#define _GNU_SOURCE

#include <linux/timekeeping.h>

//#include "../include/atom_msr.h"
#include "kernel_common.h"

struct core_state_s corestate[MAX_NUM_CORES];

int ddr_bw_target;

//update the pmu field in the corestate struct for the given core.
// IMPORTANT: This has to be the core with core_id that calls this function or incorrect state will be updated
int pmu_update(int core_id)
{
        for(int i = 0; i < PMU_COUNTERS; i++) {
                corestate[core_id].pmu_result[i] = native_read_pmc(i); //executes the RDPMC instruction
        }

	//NOTE: We should also have cycles and instructions added!

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
	corestate[core_id].pf_msr_dirty = 0; //reset msr state

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
