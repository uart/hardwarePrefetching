#define _GNU_SOURCE

#include <linux/timekeeping.h>
#include <linux/printk.h>
#include <linux/types.h>

#include "kernel_common.h"
#include "kernel_primitive.h"
#include "kernel_pmu_ddr.h"

//Only tunealg 1 is supported at this time
int kernel_basicalg(int tunealg, int aggr)
{
	uint64_t ddr_rd_bw,ddr_wr_bw; //only used for the first thread
	static uint64_t time_old = 0; // Make static to persist between calls

	uint64_t time_now;
	uint64_t time_delta_ms;
        uint64_t ddr_rd_bw_mb_total;
        uint64_t ddr_wr_bw_mb_total;
        uint64_t ddr_rd_bw_mb_per_sec = 0;
        uint64_t ddr_wr_bw_mb_per_sec = 0;
        int ddr_rd_percent = 0;

	// Grab all PMU data
	//

	ddr_rd_bw = kernel_pmu_ddr(&ddr, DDR_PMU_RD);
	ddr_wr_bw = kernel_pmu_ddr(&ddr, DDR_PMU_WR);

	if (ddr_rd_bw == (uint64_t)-EINVAL || ddr_wr_bw == (uint64_t)-EINVAL) {
		pr_err("kernel_basicalg: DDR PMU read failed (RD=%llu, WR=%llu)\n", 
		       ddr_rd_bw, ddr_wr_bw);
		return -EINVAL;
	}

	if (time_old == 0) {
		time_old = ktime_get_ns();
		return 0; //no selection the first time since all counters will be odd
	}

	time_now = ktime_get_ns();
	time_delta_ms = (time_now - time_old) / 1000000; // convert ns to ms
	time_old = time_now;

        // calculate actual bandwidth
        ddr_rd_bw_mb_total = ddr_rd_bw / (1024 * 1024); 
        ddr_wr_bw_mb_total = ddr_wr_bw / (1024 * 1024); 

        // calculate MB/s  (MB_total * 1000) / time_delta_ms
        if (time_delta_ms > 0) {
                ddr_rd_bw_mb_per_sec = (ddr_rd_bw_mb_total * 1000) / time_delta_ms;
                ddr_wr_bw_mb_per_sec = (ddr_wr_bw_mb_total * 1000) / time_delta_ms;
        }

        pr_info("DDR RD BW: %llu MB (%llu MB/s over %llu ms)\n", 
                ddr_rd_bw_mb_total, ddr_rd_bw_mb_per_sec, time_delta_ms);
        pr_info("DDR WR BW: %llu MB (%llu MB/s over %llu ms)\n", 
                ddr_wr_bw_mb_total, ddr_wr_bw_mb_per_sec, time_delta_ms);

        // calculate percentage of target bandwidth (using actual MB/s rate)
        if (ddr_bw_target > 0) {
                ddr_rd_percent = (int)((ddr_rd_bw_mb_per_sec * 100) / ddr_bw_target);
        }
        
        pr_debug("Running at %d%% of target bandwidth (%llu MB/s of %d MB/s target)\n", 
                 ddr_rd_percent, ddr_rd_bw_mb_per_sec, ddr_bw_target);

	int l2_hitr[MAX_NUM_CORES];
	int l3_hitr[MAX_NUM_CORES];
	int good_pf[MAX_NUM_CORES];
	int core_contr_to_ddr[MAX_NUM_CORES];
	int total_ddr_hit = 0;

	for (int i = 0; i < active_cores(); i++)
		total_ddr_hit += corestate[i].pmu_result[3];

	for (int i = 0; i < active_cores(); i++) {
		l2_hitr[i] = corestate[i].pmu_result[1] / corestate[i].pmu_result[1]
			+ corestate[i].pmu_result[2]	+ corestate[i].pmu_result[3];

		l3_hitr[i] = corestate[i].pmu_result[2] / corestate[i].pmu_result[2]
				+ corestate[i].pmu_result[3];

		core_contr_to_ddr[i] = corestate[i].pmu_result[3] / total_ddr_hit;

		good_pf[i] = corestate[i].pmu_result[4] / corestate[i].pmu_result[1] +
				corestate[i].pmu_result[2] + corestate[i].pmu_result[3];

		pr_debug("core %02d PMU delta LD: %10lld  HIT(L2: %d  L3: %d) DDRpressure: %d  GOODPF: %d\n", i, 
			corestate[i].pmu_result[0], l2_hitr[i], l3_hitr[i], core_contr_to_ddr[i], good_pf[i]);

	}


	//
	//Now we can make a decission...
	//
	// Below are two naive examples of tuning using the L2XQ respective L2 max distance parameter
	// All cores are set the same at this time
	//

	if (tunealg == 1) {

		for (int i = 0; i < active_cores(); i++) {
			int l2xq = msr_get_l2xq(i);

			int old_l2xq = l2xq;

			if (ddr_rd_percent < 10) {
				//idle system
			} else if (ddr_rd_percent < 20)
				l2xq += -8;
			else if (ddr_rd_percent < 30)
				l2xq += -4;
			else if (ddr_rd_percent < 40)
				l2xq += -2;
			else if (ddr_rd_percent < 50)
				l2xq += -1;
			else if (ddr_rd_percent < 60)
				l2xq += -1;
			else if (ddr_rd_percent < 70)
				l2xq += -1;
			else if (ddr_rd_percent < 80)
				l2xq += -1;
			else if (ddr_rd_percent < 90)
				l2xq += 1;
			else if (ddr_rd_percent < 93)
				l2xq += 2;
			else if (ddr_rd_percent < 96)
				l2xq += 4;
			else
				l2xq += 8;
			if (l2xq <= 0)
				l2xq = 1;
			if (l2xq > L2XQ_MAX)
				l2xq = L2XQ_MAX;
			if (old_l2xq != l2xq) {
				msr_set_l2xq(i, l2xq);
				msr_set_dirty(i);
				if (i == 0)
					pr_debug("l2xq %d\n", l2xq);
			}

		}


	} //if (tunealg)...

	return 0;
}

