#define _GNU_SOURCE

#include <linux/timekeeping.h>
#include <linux/printk.h>
#include <linux/types.h>

#include "kernel_common.h"
#include "kernel_primitive.h"
#include "kernel_pmu_ddr.h"

static int l2_hitr[MAX_NUM_CORES];
static int l3_hitr[MAX_NUM_CORES];
static int good_pf[MAX_NUM_CORES];
static int core_contr_to_ddr[MAX_NUM_CORES];
static uint64_t pmu_delta[MAX_NUM_CORES][PMU_COUNTERS]; //changes since last PMU readout
static int first_core_idx;
static int last_core_idx;

// store per-core decision for tunealg1
static int core_benefits[MAX_NUM_CORES];
static int module_mode[(MAX_NUM_CORES + CORES_PER_COMPUTE_MODULE - 1) /
	CORES_PER_COMPUTE_MODULE];

//Only tunealg 1 is supported at this time
int kernel_basicalg(int tunealg, int aggr)
{
	//Note that ddr_rd_bw,ddr_wr_bw and ddr_bw_target are specified in MB
	uint64_t ddr_rd_bw, ddr_wr_bw; //only used for the first thread

	static uint64_t time_old = 0; // Make static to persist between calls
	uint64_t time_now;
	uint64_t time_delta_ms;
	static int ddr_bw_target_ppms; //ddr_bw_target but *1000 (ms) and /100 (%), i.e. *10

	//
	// Grab all PMU data
	//

	ddr_rd_bw = kernel_pmu_ddr(&ddr, DDR_PMU_RD) >> 20;
	ddr_wr_bw = kernel_pmu_ddr(&ddr, DDR_PMU_WR) >> 20;

	if (ddr_rd_bw == (uint64_t)-EINVAL || ddr_wr_bw == (uint64_t)-EINVAL) {
		//Ensure we don't continue next time and get div zero
		//FIX: Proper handling is to halt all tuning

		pr_err("kernel_basicalg: DDR PMU read zero (RD=%llu, WR=%llu)\n",
		       ddr_rd_bw, ddr_wr_bw);
		return -EINVAL;
	}

	if (time_old == 0) {
		//no selection the first time since all counters will be odd
		time_old = ktime_get_ns();
		first_core_idx = first_core();
		last_core_idx = first_core_idx + active_cores();

		// Initialize module_mode to -1 to ensure the first time we 
		// always update MSRs based on the benefiting cores, regardless 
		// of the initial state of the MSRs.
		for (int i = 0; i < (MAX_NUM_CORES / CORES_PER_COMPUTE_MODULE); 
		i++)
			module_mode[i] = MODE_UNINITIALIZED;

		//first time, do some initialization

		//ppms : percent per ms, i.e. samt as ddr_bw_target but in % per ms time
		//pre-computed so we can reduce number of div and mul at run-time
		ddr_bw_target_ppms = ddr_bw_target * 10;

		if(ddr_bw_target_ppms == 0) {
			pr_err("basicalg() div by zero: ddr_bw_target %u (%u)\n",
				ddr_bw_target_ppms, ddr_bw_target);
			//Ensure we don't continue next time and get div zero
			//FIX: Proper handling is to halt all tuning
			time_old = 0;
                }
		return 0;
	}

	time_now = ktime_get_ns();
	time_delta_ms = (time_now - time_old) / 1000000; // convert ns to ms
	time_old = time_now;

	//check for divide by zero
	if((ddr_bw_target_ppms == 0) | (time_delta_ms == 0)) {
		pr_err("basicalg() div by zero: ddr_bw_target %u, time_delta_ms %llu\n",
			ddr_bw_target, time_delta_ms);
		return -1;
	}

	int ddr_bw_percent = ((ddr_rd_bw + ddr_wr_bw) * time_delta_ms) / (ddr_bw_target_ppms); //percent per ms

	//
	//Process PMU data
	//
	for (int i = first_core_idx; i < last_core_idx; i++) {
		for (int j = 0; j < PMU_COUNTERS ; j++) {
			pmu_delta[i][j] = corestate[i].pmu_raw[j] - corestate[i].pmu_old[j];
		}
	}

	uint64_t total_ddr_hit = 0;

	for (int i = first_core_idx; i < last_core_idx; i++) {
		total_ddr_hit += pmu_delta[i][PERF_MEM_LOAD_UOPS_RETIRED_DRAM_HIT];
	}

	//check for divide by zero
	if(total_ddr_hit == 0) {
		pr_err("basicalg() div by zero: total_ddr_hit %llu\n", total_ddr_hit);
		return -1;
	}

	for (int i = first_core_idx; i < last_core_idx; i++) {
		//check for divide by zero
		if((pmu_delta[i][PERF_MEM_LOAD_UOPS_RETIRED_L2_HIT] == 0) |
			(pmu_delta[i][PERF_MEM_LOAD_UOPS_RETIRED_L3_HIT] == 0) |
			(pmu_delta[i][PERF_MEM_LOAD_UOPS_RETIRED_DRAM_HIT] == 0)) {
			pr_err("basicalg() div by zero on PMU data core %u\n", i);
			l2_hitr[i] = 0;
			l3_hitr[i] = 0;
			core_contr_to_ddr[i] = 0;
			good_pf[i] = 0;
			continue;
		}

		//L2 hitrate = L2 hit / (L2hit + L2miss) = L2hit / (L2hit + L3hit + DDRhit)
		l2_hitr[i] = (pmu_delta[i][PERF_MEM_LOAD_UOPS_RETIRED_L2_HIT]*100) /
			(pmu_delta[i][PERF_MEM_LOAD_UOPS_RETIRED_L2_HIT]
			+ pmu_delta[i][PERF_MEM_LOAD_UOPS_RETIRED_L3_HIT]
			+ pmu_delta[i][PERF_MEM_LOAD_UOPS_RETIRED_DRAM_HIT]);

		//L3 hitrate = L3hit / (L3hit + L3miss) = L3hit / (L3hit + DDRhit)
		l3_hitr[i] = (pmu_delta[i][PERF_MEM_LOAD_UOPS_RETIRED_L3_HIT]*100) /
			(pmu_delta[i][PERF_MEM_LOAD_UOPS_RETIRED_L3_HIT]
			+ pmu_delta[i][PERF_MEM_LOAD_UOPS_RETIRED_DRAM_HIT]);

		//Cores specific contribution to DDR pressure
		core_contr_to_ddr[i] = (pmu_delta[i][PERF_MEM_LOAD_UOPS_RETIRED_DRAM_HIT]*100) /
			total_ddr_hit;

		//GoodPFration = XQpromition / (L2hit + L3hit + DDRhit)
		good_pf[i] = (pmu_delta[i][PERF_XQ_PROMOTION_ALL] * 100) /
			(pmu_delta[i][PERF_MEM_LOAD_UOPS_RETIRED_L2_HIT] +
			pmu_delta[i][PERF_MEM_LOAD_UOPS_RETIRED_L3_HIT] +
			pmu_delta[i][PERF_MEM_LOAD_UOPS_RETIRED_DRAM_HIT]);
	}

	//
	//Now we can make a decission...
	//
	// Below are two naive examples of tuning using the L2XQ respective L2 max distance parameter
	// All cores are set the same at this time
	//

	if (tunealg == 0) {

		for (int i = 0; i < active_cores(); i++) {
			int l2xq = msr_get_l2xq(i);

			int old_l2xq = l2xq;

			if (ddr_bw_percent < 10) {
				//idle system
			} else if (ddr_bw_percent < 20)
				l2xq += -8;
			else if (ddr_bw_percent < 30)
				l2xq += -4;
			else if (ddr_bw_percent < 40)
				l2xq += -2;
			else if (ddr_bw_percent < 50)
				l2xq += -1;
			else if (ddr_bw_percent < 60)
				l2xq += -1;
			else if (ddr_bw_percent < 70)
				l2xq += 1;
			else if (ddr_bw_percent < 80)
				l2xq += 1;
			else if (ddr_bw_percent < 90)
				l2xq += 1;
			else if (ddr_bw_percent < 93)
				l2xq += 2;
			else if (ddr_bw_percent < 96)
				l2xq += 4;
			else
				l2xq += 8;

			//jamdle overflow / underflow scnearios
			if (l2xq <= 0)
				l2xq = 1;
			if (l2xq > L2XQ_MAX)
				l2xq = L2XQ_MAX;

			//should we update the MSR?
			if (old_l2xq != l2xq) {
				msr_set_l2xq(i, l2xq);
				msr_set_dirty(i);
			}

		}


	} else if (tunealg == 1) {
		// For each core decide one mode using a single guard chain:
		// MODE_IDLE(0), MODE_AGGRESSIVE(+1), MODE_BOOT_DEFAULT(-1)

		for (int i = first_core_idx; i < last_core_idx; i++) {
			uint64_t l2_hit  = pmu_delta[i]
			[PERF_MEM_LOAD_UOPS_RETIRED_L2_HIT];
			uint64_t l2_miss = pmu_delta[i]
			[PERF_MEM_LOAD_UOPS_RETIRED_L2_MISS];
			uint64_t core_cycles = pmu_delta[i]
			[PERF_CPU_CLK_UNHALTED_THREAD];
			uint64_t cycles_per_ms = core_cycles / time_delta_ms;

			if (cycles_per_ms < IDLE_CYCLES_THRESHOLD) {
				core_benefits[i] = MODE_IDLE;
			} else if (l2_hit > l2_miss) {

				// benefitting
				core_benefits[i] = MODE_AGGRESSIVE;
			} else {

				// non-benefitting
				core_benefits[i] = MODE_BOOT_DEFAULT;
			}
		}

		// Decisions are made per compute module (4 cores/module).
		for (int module_start = first_core_idx;
			module_start < last_core_idx;
			module_start += CORES_PER_COMPUTE_MODULE) {
			int module_end = module_start +
			CORES_PER_COMPUTE_MODULE;
			int module_vote_sum = 0;
			int module_idx;
			int desired_mode;

			// handle case where total cores is not a multiple of 4
			if (module_end > last_core_idx)
				module_end = last_core_idx;

			// MODE_AGGRESSIVE=+1, MODE_BOOT_DEFAULT=-1, MODE_IDLE=0
			for (int i = module_start; i < module_end; i++) {
				module_vote_sum += core_benefits[i];
			}

			module_idx = (module_start - first_core_idx) /
				CORES_PER_COMPUTE_MODULE;

			if (module_vote_sum >= BENEFITING_CORES) {
				desired_mode = MODE_AGGRESSIVE;
			} else {
				desired_mode = MODE_BOOT_DEFAULT;
			}

			if (module_mode[module_idx] == desired_mode)
				continue;

			if (desired_mode == MODE_AGGRESSIVE) {
				corestate[module_start].pf_msr[MSR_1320_INDEX]
					.v = 0x008837ea070906c0ULL;
				corestate[module_start].pf_msr[MSR_1321_INDEX]
					.v = 0x0000251134040001ULL;
				corestate[module_start].pf_msr[MSR_1322_INDEX]
					.v = 0x280020820cd0046cULL;
				corestate[module_start].pf_msr[MSR_1327_INDEX]
					.v = 0x0000000001920014ULL;
			} else {
				corestate[module_start].pf_msr[MSR_1320_INDEX]
					.v = 0x10883fea070906c4ULL;
				corestate[module_start].pf_msr[MSR_1321_INDEX]
					.v = 0x0000251134140001ULL;
				corestate[module_start].pf_msr[MSR_1322_INDEX]
					.v = 0x2807ffff4cd0046cULL;
				corestate[module_start].pf_msr[MSR_1327_INDEX]
					.v = 0x0000000005920014ULL;
			}

			msr_set_dirty(module_start);
			module_mode[module_idx] = desired_mode;
		}

	}

	return 0;
}

