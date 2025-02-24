#ifndef __KERNEL_PRIMITIVE__
#define __KERNEL_PRIMITIVE__

#define MAX_NUM_CORES (1024)

//This is a first test, so values are hard-coded. This will only work on GRR with 24 cores
#define FIRST_CORE (6)    //this should actually be the first of active cores, not 0
#define ACTIVE_CORES (8)
#define CORE_IN_MODULE ((core_id - FIRST_CORE) % 4)
#define MODULE_ID ((core_id - FIRST_CORE) / 4)

int msr_is_dirty(int core_id);
int msr_load(int core_id);
int msr_update(int core_id);
int pmu_update(int core_id);

int msr_set_l2xq(int core_id, int value);
int msr_get_l2xq(int core_id);
int msr_set_l3xq(int core_id, int value);
int msr_get_l3xq(int core_id);

int kernel_basicalg(int tunealg);

#endif

