#ifndef __COMMON_H
#define __COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>

#include "msr.h"
#include "pmu_core.h"
#include "log.h"

#define MAX_THREADS (1024)

#define CORE_IN_MODULE ((tstate->core_id - core_first) % 4)
#define ACTIVE_THREADS (core_last - core_first + 1)

struct thread_state {
	pthread_t thread_id; // from pthread_create()
	int core_id;
	int hwpf_msr_dirty; //0 not updated, 1 updated
	union msr_u hwpf_msr_value[HWPF_MSR_FIELDS]; //0... -> 0x1320...
	uint64_t pmu_result[PMU_COUNTERS]; //delta since last read
    uint64_t instructions_retired; // delta since last read
    uint64_t cpu_cycles; // delta since last read
};

uint64_t time_ms(void);


extern struct thread_state gtinfo[MAX_THREADS]; //global thread state
extern int core_last;
extern int core_first;
extern int tunealg;
extern float time_intervall;

extern uint32_t rdt_enabled;
extern struct ddr_s ddr;
extern int ddr_bw_target;
extern float aggr; //alg retuning aggressiveness

#endif

