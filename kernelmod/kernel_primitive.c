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

	pr_info("DDR RD BW: %llu MB/s\n", ddr_rd_bw);
	pr_info("DDR WR BW: %llu MB/s\n", ddr_wr_bw);
	pr_info("DDR BW target: %u MB/s\n", ddr_bw_target);

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
	pr_info("DDR BW percent: %u\n", ddr_bw_percent);

	//
	//Process PMU data
	//
	for (int i = 0; i < active_cores(); i++) {
		for (int j = 0; j < PMU_COUNTERS ; j++) {
			pmu_delta[i][j] = corestate[i].pmu_raw[j] - corestate[i].pmu_old[j];
		}

		pr_debug("core %u PMUs: LD %llu  L2hit %llu  L3hit %llu\n  DDRhit %llu  XQprom %llu  clk %llu  ret %llu", i,
			pmu_delta[i][PERF_MEM_UOPS_RETIRED_ALL_LOADS],
			pmu_delta[i][PERF_MEM_LOAD_UOPS_RETIRED_L2_HIT],
			pmu_delta[i][PERF_MEM_LOAD_UOPS_RETIRED_L3_HIT],
			pmu_delta[i][PERF_MEM_LOAD_UOPS_RETIRED_DRAM_HIT],
			pmu_delta[i][PERF_XQ_PROMOTION_ALL],
			pmu_delta[i][PERF_CPU_CLK_UNHALTED_THREAD],
			pmu_delta[i][PERF_INST_RETIRED_ANY_P]);
	}

	uint64_t total_ddr_hit = 0;

	for (int i = 0; i < active_cores(); i++) {
		total_ddr_hit += pmu_delta[i][PERF_MEM_LOAD_UOPS_RETIRED_DRAM_HIT];
	}

	//check for divide by zero
	if(total_ddr_hit == 0) {
		pr_err("basicalg() div by zero: total_ddr_hit %llu\n", total_ddr_hit);
		return -1;
	}

	pr_info("delta for total_ddr_hit %llu or %llu MB/s\n", total_ddr_hit, (total_ddr_hit*64) >> 20);

	for (int i = 0; i < active_cores(); i++) {
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

		pr_info("core %2d PMU delta LD: %13lld  HIT(L2: %3d  L3: %3d) DDRpressure: %3d  GOODPF: %3d\n", i,
			pmu_delta[i][0], l2_hitr[i], l3_hitr[i], core_contr_to_ddr[i], good_pf[i]);

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
				if (i == 0)
					pr_info("Core0 l2xq %d\n", l2xq);
			}

		}


	} //if (tunealg)...

	return 0;
}

