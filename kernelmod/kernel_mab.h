#ifndef __KERNEL_MAB_H__
#define __KERNEL_MAB_H__

#include <linux/types.h>

#include "kernel_common.h"
#include "kernel_primitive.h"

// Compile-time bounds for number of active arms in kernel MAB.
#define MIN_ARMS (2)
#define MAX_ARMS (10)

#define AVAILABLE_ARMS (2)

// Default PMU events used for active arm scoring.
#define SCORE_EVENT_A PERF_INST_RETIRED_ANY_P
#define SCORE_EVENT_B PERF_CPU_CLK_UNHALTED_THREAD

// Compile-time right-shift applied to PMU event deltas before scoring.
// SHIFT_A=0, SHIFT_B=10 gives (instructions>>0)/(cycles>>10) ≈ IPC*1000.
// Expected score range: 500 (IPC 0.5) to 2500 (IPC 2.5).
#define SHIFT_A (0)
#define SHIFT_B (10)

// Number of consecutive idle intervals before core MAB state is reset.
#define IDLE_THRESHOLD (5)

// Number of compute modules derived from core count and module size.
#define MAX_MODULES (MAX_NUM_CORES / CORES_PER_COMPUTE_MODULE)

// Initialization state values for mab_core.initialized.
#define MAB_INIT_NOT_STARTED (-1)
#define MAB_INIT_IN_PROGRESS (0)
#define MAB_INIT_DONE        (1)

// MAB arm state for reward calculation.
struct mab_arm {
	__u32 score;
	__u32 last_score;
};

// One arm maps to one MSR configuration.
struct mab_arm_config {
	union msr_u pf_msr[NR_OF_MSR];
};

// Per-core MAB state
struct mab_core {
	struct mab_arm arms[MAX_ARMS];
	int active_arm;
	int initialized; // MAB initialization state
	int idle_counter;
	__u64 time_old;  // timestamp of last tick, used to compute time delta
};

// Per-module MAB state: one entry per compute module.
// Arm selection and init scanning are driven at module scope by the module leader.
struct mab_module {
	int active_arm;   // arm currently applied to all cores in this module
	int initialized;  // uses MAB_INIT_NOT_STARTED / IN_PROGRESS / DONE
	int idle_counter; // consecutive idle ticks before module state is reset
};

// Declare the global MAB state arrays for cores, modules, and arm configurations.
extern struct mab_core mab_cores[MAX_NUM_CORES];
extern struct mab_module mab_modules[MAX_MODULES];
extern struct mab_arm_config mab_arm_configs[MAX_ARMS];

// Returns 1 if core is active, 0 if idle, -1 on error or first call.
// Updates mab_cores[core_id].time_old on each call.
int mab_core_is_active(int core_id);

// Executes one initialization step for a non-initialized core.
// Returns 0 on success, -1 on error.
int mab_init_core_step(int core_id);

// Main MAB algorithm for initialized cores: scoring and arm selection.
// Returns 0 on success, -EINVAL on bad core_id.
int mab_tuning(int core_id);

// Populates mab_arm_configs with the two default MSR configurations.
// arm 0 = boot default, arm 1 = aggressive.
// Must be called once before mab_init_core_step is used.
void mab_setup_default_arms(void);

// Returns the first non-disabled core in a module regardless of idle state.
// Used for module-level idle counter tracking.
// Returns -1 if all cores in the module are disabled.
int mab_first_enabled_core(int module_idx);

// Returns the index of the first active, non-disabled core in a module.
// Returns -1 if all cores in the module are idle or disabled.
int mab_first_active_core(int module_idx);

// Placeholder declaration for module-level init step (implemented in Task C).
int mab_init_module_step(int core_id);

#endif // __KERNEL_MAB_H__
