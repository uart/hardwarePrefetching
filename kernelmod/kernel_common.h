#ifndef __KERNEL_COMMON__
#define __KERNEL_COMMON__

#include "../include/atom_msr.h"

#define MAX_NUM_CORES (512)

//This is a first test, so values are hard-coded. This will only work on GRR with 24 cores
#define FIRST_CORE (6)    //this should actually be the first of active cores, not 0
#define ACTIVE_CORES (8)
#define CORE_IN_MODULE ((core_id - FIRST_CORE) % 4)
#define MODULE_ID ((core_id - FIRST_CORE) / 4)

//PMU counters include cycles, instructions and 5 additional events
#define PMU_COUNTERS (7)


#define NR_OF_MSR (6)
#define MSR_1320_INDEX (0)
#define MSR_1321_INDEX (1)
#define MSR_1322_INDEX (2)
#define MSR_1323_INDEX (3)
#define MSR_1324_INDEX (4)
#define MSR_1A4_INDEX (5)

struct core_state_s {
	uint64_t pmu_result[PMU_COUNTERS]; //delta since last read
	union msr_u pf_msr[NR_OF_MSR]; //msr values, 0... --> 0x1320...
	int pf_msr_dirty; //0 if no update is needed, 1 if update is neeed
	int core_disabled;  //set to 1 for all cores that should be disabled
};

extern struct core_state_s corestate[MAX_NUM_CORES];
extern int ddr_bw_target;


int is_msr_dirty(int core_id);
int msr_set_dirty(int core_id);
int msr_load(int core_id);
int msr_update(int core_id);
int pmu_update(int core_id);

int msr_set_l2xq(int core_id, int value);
int msr_get_l2xq(int core_id);
int msr_set_l3xq(int core_id, int value);
int msr_get_l3xq(int core_id);

#endif

