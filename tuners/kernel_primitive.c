#define _GNU_SOURCE

#include "../include/atom_msr.h"

//only tunealg 0 is supported at this time
int kernel_basicalg(int tunealg)
{

/*
	uint64_t ddr_rd_bw; //only used for the first thread
	static uint64_t time_now, time_old = 0;
	float time_delta;


	//
	// Grab all PMU data
	//

	if (!rdt_enabled)
		ddr_rd_bw = pmu_ddr(&ddr, DDR_PMU_RD);
	else
		ddr_rd_bw = rdt_mbm_bw_get();

	pr_info("DDR RD BW: %ld MB/s\n", ddr_rd_bw/(1024*1024));

	if (time_old == 0) {
		time_old = time_ms();
		return 0; //no selection the first time since all counters will be odd
	}

	time_now = time_ms();
	time_delta = (time_now - time_old) / 1000.0;
	time_old = time_now;

	float ddr_rd_percent = ((float)ddr_rd_bw/(1024*1024))
			/ (float)ddr_bw_target;
	ddr_rd_percent /= time_delta;
	pr_info("Time delta %f, Running at %.1f percent rd bw (%ld MB/s)\n", time_delta, ddr_rd_percent * 100, ddr_rd_bw/(1024*1024));

	//	float l2_l3_ddr_hits[ACTIVE_THREADS];

	float l2_hitr[ACTIVE_THREADS];
	float l3_hitr[ACTIVE_THREADS];
	float good_pf[ACTIVE_THREADS];

	float core_contr_to_ddr[ACTIVE_THREADS];

	int total_ddr_hit = 0;

	for (int i = 0; i < ACTIVE_THREADS; i++)
		total_ddr_hit += gtinfo[i].pmu_result[3];

	for (int i = 0; i < ACTIVE_THREADS; i++) {
		l2_hitr[i] = ((float)gtinfo[i].pmu_result[1])
			/ ((float)(gtinfo[i].pmu_result[1]
				+ gtinfo[i].pmu_result[2]
					+ gtinfo[i].pmu_result[3]));

		l3_hitr[i] = ((float)gtinfo[i].pmu_result[2])
			/ ((float)(gtinfo[i].pmu_result[2]
				+ gtinfo[i].pmu_result[3]));

		core_contr_to_ddr[i] = ((float)gtinfo[i].pmu_result
				[3]) / ((float)total_ddr_hit);

		good_pf[i] = ((float)gtinfo[i].pmu_result[4]) /
			((float)(gtinfo[i].pmu_result[1]) +
				(gtinfo[i].pmu_result[2]) +
				(gtinfo[i].pmu_result[3]));


		pr_debug("core %02d PMU delta LD: %10ld  HIT(L2: %.2f  L3: %.2f) DDRpressure: %.2f  GOODPF: %.2f\n", i, gtinfo[i].pmu_result[0],
			l2_hitr[i], l3_hitr[i], core_contr_to_ddr[i], good_pf[i]);

//		pr_debug("   LD: %ld  HIT(L2: %ld  L3: %ld  DDR: %ld)  GOODPF: %ld\n", gtinfo[i].pmu_result[0], gtinfo[i].pmu_result[1],
//			gtinfo[i].pmu_result[2], gtinfo[i].pmu_result[3], gtinfo[i].pmu_result[4]);
	}


	//
	//Now we can make a decission...
	//
	// Below are two naive examples of tuning using the L2XQ respective L2 max distance parameter
	// All cores are set the same at this time
	//

	if (tunealg == 0) {

		for (int i = 0; i < ACTIVE_THREADS; i++) {
			int l2xq = msr_get_l2xq(&gtinfo[i].hwpf_msr_value[0]);

			int old_l2xq = l2xq;

			if (ddr_rd_percent < 0.10) {
				//idle system
			} else if (ddr_rd_percent < 0.20)
				l2xq += lround(-8 * aggr);
			else if (ddr_rd_percent < 0.30)
				l2xq += lround(-4 * aggr);
			else if (ddr_rd_percent < 0.40)
				l2xq += lround(-2 * aggr);
			else if (ddr_rd_percent < 0.50)
				l2xq += lround(-1 * aggr);
			else if (ddr_rd_percent < 0.60)
				l2xq += lround(-1 * aggr);
			else if (ddr_rd_percent < 0.70)
				l2xq += lround(-1 * aggr);
			else if (ddr_rd_percent < 0.80)
				l2xq += lround(-1 * aggr);
			else if (ddr_rd_percent < 0.90)
				l2xq += lround(1 * aggr);
			else if (ddr_rd_percent < 0.93)
				l2xq += lround(2 * aggr);
			else if (ddr_rd_percent < 0.96)
				l2xq += lround(4 * aggr);
			else
				l2xq += lround(8 * aggr);
			if (l2xq <= 0)
				l2xq = 1;
			if (l2xq > L2XQ_MAX)
				l2xq = L2XQ_MAX;
			if (old_l2xq != l2xq) {
				msr_set_l2xq(&gtinfo[i].hwpf_msr_value[0], l2xq);
				gtinfo[i].hwpf_msr_dirty = 1;
				if (i == 0)
					pr_debug("l2xq %d\n", l2xq);
			}

			int l3xq = msr_get_l3xq(
				&gtinfo[i].hwpf_msr_value[0]);
			int old_l3xq = l3xq;

			if (ddr_rd_percent < 0.10);
				//idle system
			else if (ddr_rd_percent < 0.20)
				l3xq += lround(-8 * aggr);
			else if (ddr_rd_percent < 0.30)
				l3xq += lround(-4 * aggr);
			else if (ddr_rd_percent < 0.40)
				l3xq += lround(-2 * aggr);
			else if (ddr_rd_percent < 0.50)
				l3xq += lround(-1 * aggr);
			else if (ddr_rd_percent < 0.60)
				l3xq += lround(-1 * aggr);
			else if (ddr_rd_percent < 0.70)
				l3xq += lround(-1 * aggr);
			else if (ddr_rd_percent < 0.80)
				l3xq += lround(-1 * aggr);
			else if (ddr_rd_percent < 0.90)
				l3xq += lround(1 * aggr);
			else if (ddr_rd_percent < 0.93)
				l3xq += lround(2 * aggr);
			else if (ddr_rd_percent < 0.96)
				l3xq += lround(4 * aggr);
			else
				l3xq += lround(8 * aggr);

			if (l3xq <= 0)
				l3xq = 1;
			if (l3xq > L3XQ_MAX)
				l3xq = L3XQ_MAX;

			if (old_l3xq != l3xq) {
				msr_set_l3xq(&gtinfo[i].hwpf_msr_value[0], l3xq);
				gtinfo[i].hwpf_msr_dirty = 1;
			if (i == 0)
				pr_debug("l3xq %d\n", l3xq);
			}
		}


	} //if (tunealg)...
*/
	return 0;
}

