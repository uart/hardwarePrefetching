#include <linux/ktime.h>
#include <linux/printk.h>

#include "kernel_mab.h"
#include "kernel_primitive.h"

// Runtime aggressiveness parameter set by userspace via api_tuning().
extern int aggr;

// define persistent kernel MAB state.
// Runtime logic wiring is intentionally deferred to later tasks.
struct mab_core mab_cores[MAX_NUM_CORES];
struct mab_module mab_modules[MAX_MODULES];
struct mab_arm_config mab_arm_configs[MAX_ARMS];

// Returns the first non-disabled core in a module, regardless of idle state.
// Used to assign responsibility for module-level idle tracking.
// Returns -1 if all cores in the module are disabled.
int mab_first_enabled_core(int module_idx)
{
	int i;
	int module_start;

	module_start = sys_first_core + module_idx * CORES_PER_COMPUTE_MODULE;

	for (i = module_start; i < module_start + CORES_PER_COMPUTE_MODULE; i
		++) {
		if (!corestate[i].core_disabled)
			return i;
	}

	return -1;
}

// Returns the index of the first active, non-disabled core in a module.
// Returns -1 if all cores in the module are idle or disabled.
int mab_first_active_core(int module_idx)
{
	int i;
	int module_start;

	module_start = sys_first_core + module_idx * CORES_PER_COMPUTE_MODULE;

	for (i = module_start; i < module_start + CORES_PER_COMPUTE_MODULE; i
		++) {
		if (corestate[i].core_disabled || mab_cores[i].idle_counter >
			0) {
			// pr_info("MAB first_active: skipping core %d
			// (disabled=%d idle=%d)\n",
			// 	 i, corestate[i].core_disabled, mab_cores[i].
			// idle_counter);
			continue;
		}

		return i;
	}

	return -1;
}

// Compute the average score for one arm across all active non-idle cores in a
// module.
// Returns 0 if no active cores contribute — safe for arm comparisons.
static __u32 mab_module_score(int module_idx, int arm_id)
{
	int i;
	int module_start;
	__u64 total_score = 0;
	int cores_count = 0;

	// Determine the core range for this module.
	module_start = sys_first_core + module_idx * CORES_PER_COMPUTE_MODULE;

	// check the aggregate score for this arm across all active non-idle
	// cores in the module.
	for (i = module_start; i
		< module_start + CORES_PER_COMPUTE_MODULE; i++) {

		if (corestate[i].core_disabled)
			continue;

		if (mab_cores[i].idle_counter > 0)
			continue;

		total_score += mab_cores[i].arms[arm_id].score;
		cores_count++;
	}

	pr_info("MAB module_score mod %d arm %d: total=%llu count=%d avg=%u\n",
		 module_idx, arm_id, total_score, cores_count,
		 cores_count ? (unsigned)(total_score / cores_count) : 0);

	// If no active cores contribute to the score, return 0 to avoid
	// division by zero.
	if (cores_count == 0) {
		// pr_debug("MAB module_score mod %d arm %d: no active cores,
		// returning 0\n", module_idx, arm_id);
		return 0;
	}

	return (__u32)(total_score / cores_count);
}

// Apply one arm's MSR configuration to the selected core.
// Returns 0 on success, -EINVAL if arm_id is out of bounds.
static int mab_apply_msr_config(int core_id, int arm_id)
{
	int i;

	if (arm_id < 0 || arm_id >= MAX_ARMS) {
		pr_err("MAB: invalid arm_id %d in mab_apply_msr_config\n",
			arm_id);

		return -EINVAL;
	}

	for (i = 0; i < NR_OF_MSR; i++)
		corestate[core_id].pf_msr[i] = mab_arm_configs[arm_id]
			 .pf_msr[i];

	// pr_debug("MAB MSR apply core %d arm %d\n", core_id, arm_id);

	msr_set_dirty(core_id);

	return 0;
}

// Compute the active arm score for the given core.
// Uses shift-scaled PMU deltas: (event_a >> SHIFT_A) / (event_b >> SHIFT_B).
// Returns 0 if b_scaled would be zero to avoid division by zero.
static __u32 mab_compute_score(int core_id)
{
	__u64 event_a;
	__u64 event_b;
	__u64 a_scaled;
	__u64 b_scaled;

	event_a = corestate[core_id].pmu_raw[SCORE_EVENT_A] -
		corestate[core_id].pmu_old[SCORE_EVENT_A];
	event_b = corestate[core_id].pmu_raw[SCORE_EVENT_B] -
		corestate[core_id].pmu_old[SCORE_EVENT_B];

	a_scaled = event_a >> SHIFT_A;
	b_scaled = event_b >> SHIFT_B;

	pr_info("MAB score core %d: instr=%llu cyc=%llu a_sc=%llu b_sc=%llu"
		"score=%u\n",
		core_id, event_a, event_b, a_scaled, b_scaled,
		(__u32)(a_scaled / b_scaled));


	if (b_scaled == 0) {
		// pr_debug("MAB compute_score core %d: b_scaled=0, returning
		// 0\n", core_id);
		return 0;
	}

	return (__u32)(a_scaled / b_scaled);
}


// Returns 1 if core is active, 0 if idle, -1 on error or first call.
// Computes time delta from mab_cores[core_id].time_old and updates it.
int mab_core_is_active(int core_id)
{
	__u64 core_cycles;
	__u64 cycles_per_ms;
	__u64 time_now;
	__u64 time_delta_ms;
	time_now = ktime_get_ns();

	// First call: seed the timestamp and skip this tick.
	if (mab_cores[core_id].time_old == 0) {
		mab_cores[core_id].time_old = time_now;
		return -1;
	}

	time_delta_ms = (time_now - mab_cores[core_id].time_old) / 1000000;
	mab_cores[core_id].time_old = time_now;

	if (time_delta_ms == 0)
		return -1;

	core_cycles = corestate[core_id].pmu_raw[PERF_CPU_CLK_UNHALTED_THREAD] -
		corestate[core_id].pmu_old[PERF_CPU_CLK_UNHALTED_THREAD];
	cycles_per_ms = core_cycles / time_delta_ms;

	// Diagnostic: log actual values to understand idle detection behaviour.
	// pr_debug("MAB diag core %d: raw=%llu old=%llu delta=%llu dt_ms=%llu
	// cpm=%llu thr=%llu\n",
	// 	core_id,
	// 	corestate[core_id].pmu_raw[PERF_CPU_CLK_UNHALTED_THREAD],
	// 	corestate[core_id].pmu_old[PERF_CPU_CLK_UNHALTED_THREAD],
	// 	core_cycles, time_delta_ms, cycles_per_ms,
	// 	(unsigned long long)IDLE_CYCLES_THRESHOLD);

	if (cycles_per_ms < IDLE_CYCLES_THRESHOLD)
		return 0;

	return 1;
}

// to test the mab after initialization
void mab_setup_default_arms(void)
{
	// arm 0: boot default prefetcher config
	mab_arm_configs[0].pf_msr[MSR_1320_INDEX].v = 0x10883fea070906c4ULL;
	mab_arm_configs[0].pf_msr[MSR_1321_INDEX].v = 0x0000251134140001ULL;
	mab_arm_configs[0].pf_msr[MSR_1322_INDEX].v = 0x2807ffff4cd0046cULL;
	mab_arm_configs[0].pf_msr[MSR_1323_INDEX].v = 0x0001f9c0c0000000ULL;
	mab_arm_configs[0].pf_msr[MSR_1324_INDEX].v = 0x0600000000000000ULL;
	mab_arm_configs[0].pf_msr[MSR_1327_INDEX].v = 0x0000000005920014ULL;
	mab_arm_configs[0].pf_msr[MSR_1A4_INDEX].v  = 0x0000000000000004ULL;

	// arm 1: aggressive prefetcher config
	// 1323/1324/1A4 are not tuned per arm , carry boot-default values to
	// avoid zeroing them
	mab_arm_configs[1].pf_msr[MSR_1320_INDEX].v = 0x008837ea070906c0ULL;
	mab_arm_configs[1].pf_msr[MSR_1321_INDEX].v = 0x0000251134040001ULL;
	mab_arm_configs[1].pf_msr[MSR_1322_INDEX].v = 0x280020820cd0046cULL;
	mab_arm_configs[1].pf_msr[MSR_1323_INDEX].v = 0x0001f9c0c0000000ULL;
	mab_arm_configs[1].pf_msr[MSR_1324_INDEX].v = 0x0600000000000000ULL;
	mab_arm_configs[1].pf_msr[MSR_1327_INDEX].v = 0x0000000001920014ULL;
	mab_arm_configs[1].pf_msr[MSR_1A4_INDEX].v  = 0x0000000000000004ULL;

	pr_info("MAB: default arm configs loaded (arm0=boot_default,"
		"arm1=aggressive)\n");
}

// Perform one non-blocking initialization step for one core.
// Caller must ensure initialized < MAB_INIT_DONE and core is active.
// Returns 0 on success, -1 on error.
int mab_init_core_step(int core_id)
{
	int i;
	int best_arm;
	int active_arm;
	__u32 score;

	// First call: apply arm 0 and begin scanning.
	if (mab_cores[core_id].initialized == MAB_INIT_NOT_STARTED) {
		if (mab_apply_msr_config(core_id, 0) < 0)
			return -EINVAL;

		mab_cores[core_id].active_arm = 0;
		mab_cores[core_id].initialized = MAB_INIT_IN_PROGRESS;

		return 0;
	}

	// Score the arm that ran last interval.
	score = mab_compute_score(core_id);
	active_arm = mab_cores[core_id].active_arm;
	mab_cores[core_id].arms[active_arm].last_score = mab_cores[core_id]
		.arms[active_arm].score;
	mab_cores[core_id].arms[active_arm].score = score;

	// Advance to next arm if there are more to test.
	if (active_arm + 1 < AVAILABLE_ARMS) {
		mab_cores[core_id].active_arm++;

		if (mab_apply_msr_config(core_id, mab_cores[core_id].
			active_arm) < 0)
			return -EINVAL;

		return 0;
	}

	// All arms scored: find the best and activate it.
	best_arm = 0;
	for (i = 1; i < AVAILABLE_ARMS; i++) {
		if (mab_cores[core_id].arms[i].score > mab_cores[core_id].arms
			[best_arm].score)
			best_arm = i;
	}

	mab_cores[core_id].active_arm = best_arm;

	if (mab_apply_msr_config(core_id, best_arm) < 0)
		return -EINVAL;
	mab_cores[core_id].initialized = MAB_INIT_DONE;

	return 0;
}



// Main MAB algorithm for initialized cores.
// All cores update their own per-core arm scores each tick.
// Only the module leader aggregates scores and makes arm selection for the //
// whole module.
// Returns 0 on success.
int mab_tuning(int core_id)
{
	__u32 score;
	int i;
	int active_arm;
	int best_arm;
	int module_idx;
	int module_start;
	struct mab_module *mod;
	__u32 scores[AVAILABLE_ARMS];

	module_idx = module_id(core_id);
	mod = &mab_modules[module_idx];
	active_arm = mod->active_arm;

	// Per-core: update the active arm score with the latest PMU data.
	score = mab_compute_score(core_id);
	mab_cores[core_id].arms[active_arm].last_score = mab_cores[core_id]
		.arms[active_arm].score;
	mab_cores[core_id].arms[active_arm].score = score;

	// Per-core: increase passive arm scores for exploration.
	for (i = 0; i < AVAILABLE_ARMS; i++) {

		if (i != active_arm)
			mab_cores[core_id].arms[i].score += (__u32)aggr;
	}

	pr_info("MAB tuning core %d: active_arm=%d active_score=%u"
		"passive_arm=%d passive_score=%u\n",
		core_id, active_arm, mab_cores[core_id].arms[active_arm].score,
		1 - active_arm, mab_cores[core_id].arms[1 - active_arm].score);

	// non-leaders return here since arm selection is at module scope
	if (core_id != mab_first_active_core(module_id(core_id)))
		return 0;

	module_start = sys_first_core + module_idx * CORES_PER_COMPUTE_MODULE;

	// Compute aggregate score per arm once to avoid redundant calls in
	// comparison.
	// Start with current active_arm so ties keep the running arm (per
	// spec).
	best_arm = active_arm;

	for (i = 0; i < AVAILABLE_ARMS; i++) {

		// aggregate score for this arm across the module, stored in
		// the leader's mab_core for access during init and tuning.
		scores[i] = mab_module_score(module_idx, i);

		// track the best arm for potential switching after scoring
		if (scores[i] > scores[best_arm])
		best_arm = i;
	}

	if (best_arm != active_arm) {
		pr_info("MAB module %d switching arm %d -> %d\n", module_idx,
			active_arm, best_arm);

		// Reset outgoing arm score to prevent rapid oscillation back.
		mod->active_arm = best_arm;

		mab_apply_msr_config(core_id, best_arm);
	}

	return 0;
}

// Perform one non-blocking initialization step for a compute module.
// Scans one arm per tick, scores it via module aggregate, selects best when
// all arms are scored.
// Caller must ensure core_id is the first active core in the module (see
// mab_first_active_core).
int mab_init_module_step(int core_id)
{
	int i;
	int best_arm;
	int module_idx;
	int module_start;
	struct mab_module *mod;
	__u32 score;

	module_idx = module_id(core_id);
	mod = &mab_modules[module_idx];
	module_start = sys_first_core + module_idx * CORES_PER_COMPUTE_MODULE;

	// First apply arm 0 to all cores in the module and begin scanning.
	if (mod->initialized == MAB_INIT_NOT_STARTED) {

		mab_apply_msr_config(core_id, 0);

		mod->active_arm = 0;
		mod->initialized = MAB_INIT_IN_PROGRESS;

		return 0;
	}

	// Score the arm that ran last tick: compute fresh PMU scores for each
	// active core
	// in the module, store them in their per-core arms[], then aggregate
	// via mab_module_score.
	// This must happen before mab_module_score() is called — during init
	// the per-core
	// arms[].score fields are zero until mab_compute_score() writes into
	// them.

	for (i = module_start; i < module_start + CORES_PER_COMPUTE_MODULE; i
		++) {

		if (corestate[i].core_disabled)
			continue;
		if (mab_cores[i].idle_counter > 0)
			continue;

		mab_cores[i].arms[mod->active_arm].score = mab_compute_score(i);
	}

	score = mab_module_score(module_idx, mod->active_arm);
	mab_cores[core_id].arms[mod->active_arm].score = score;

	pr_info("MAB init_step mod %d leader %d state %d active_arm %d score %" 
		"u\n ", module_idx, core_id, mod->initialized, mod->active_arm, 
		score);

	// Advance to next arm if more remain to be tested.
	if (mod->active_arm + 1 < AVAILABLE_ARMS) {
		mod->active_arm++;

		mab_apply_msr_config(core_id, mod->active_arm);

		return 0;
	}

	// All arms scored: find the best using scores stored in the module
	// leader's mab_core.
	best_arm = 0;
	for (i = 1; i < AVAILABLE_ARMS; i++) {

		if (mab_cores[core_id].arms[i].score > mab_cores[core_id].arms
			[best_arm].score)
			best_arm = i;
	}

	pr_info("MAB module %d init done: best arm %d applied via core %d\n",
		module_idx, best_arm, core_id);

	mod->active_arm = best_arm;
	mab_apply_msr_config(core_id, best_arm);

	mod->initialized = MAB_INIT_DONE;

	return 0;
}
