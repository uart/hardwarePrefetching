#define _GNU_SOURCE

#include <linux/timekeeping.h>

#include "../include/atom_msr.h"
#include "kernel_primitive.h"

union msr_u pf_msr[MAX_NUM_CORES]; //msr values, 0... --> 0x1320...
int pf_msr_dirty[MAX_NUM_CORES / 4] = {0}; //0 if no update is needed, 1 if it is

#define PMU_COUNTERS (7)
uint64_t pmu_result[PMU_COUNTERS]; //delta since last read


int ddr_bw_target = 7000000; //quick hack, should be properly set	


int inline msr_is_dirty(int core_id)
{
	return pf_msr_dirty[core_id / 4];
}

int msr_load(int core_id)
{
	return 0;
}

int msr_update(int core_id)
{
	pf_msr_dirty[core_id / 4] = 0; 

	return 0;
}

int pmu_update(int core_id)
{
	return 0;
}

// Set value in MSR table
int msr_set_l2xq(int core_id, int value)
{
	pf_msr[core_id].msr1320.L2_STREAM_AMP_XQ_THRESHOLD = value;

	return 0;
}

int msr_get_l2xq(int core_id)
{
	return pf_msr[core_id].msr1320.L2_STREAM_AMP_XQ_THRESHOLD;
}

// Set value in MSR table
int msr_set_l3xq(int core_id, int value)
{
	pf_msr[core_id].msr1320.LLC_STREAM_XQ_THRESHOLD = value;

	return 0;
}

int msr_get_l3xq(int core_id)
{
	return pf_msr[core_id].msr1320.LLC_STREAM_XQ_THRESHOLD;
}

//only tunealg 0 is supported at this time
int kernel_basicalg(int tunealg)
{
	uint64_t ddr_rd_bw; //only used for the first thread
	uint64_t time_now, time_old = 0;
	uint64_t time_delta;

	//
	// Grab all PMU data
	//

	ddr_rd_bw = 10000000; //hardcode to 10MB for now   should be: pmu_ddr(&ddr, DDR_PMU_RD);
	//pr_info("DDR RD BW: %ld MB/s\n", ddr_rd_bw/(1024*1024));

	if (time_old == 0) {
		time_old = ktime_get_ns();
		return 0; //no selection the first time since all counters will be odd
	}

	time_now = ktime_get_ns();
	time_delta = (time_now - time_old) / 1000000;
	time_old = time_now;

	int ddr_rd_percent = (ddr_rd_bw/(1024*1024)) / ddr_bw_target;
	ddr_rd_percent /= time_delta;
	//pr_info("Time delta %f, Running at %.1f percent rd bw (%ld MB/s)\n", time_delta, ddr_rd_percent * 100, ddr_rd_bw/(1024*1024));

	int l2_hitr[MAX_NUM_CORES];
	int l3_hitr[MAX_NUM_CORES];
	int good_pf[MAX_NUM_CORES];
	int core_contr_to_ddr[MAX_NUM_CORES];
	int total_ddr_hit = 0;

	for (int i = 0; i < ACTIVE_CORES; i++)
		total_ddr_hit += pmu_result[3];

	for (int i = 0; i < ACTIVE_CORES; i++) {
		l2_hitr[i] = pmu_result[1] / pmu_result[1]
			+ pmu_result[2]	+ pmu_result[3];

		l3_hitr[i] = pmu_result[2] / pmu_result[2]
				+ pmu_result[3];

		core_contr_to_ddr[i] = pmu_result[3] / total_ddr_hit;

		good_pf[i] = pmu_result[4] / pmu_result[1] +
				pmu_result[2] + pmu_result[3];

		pr_debug("core %02d PMU delta LD: %10lld  HIT(L2: %d  L3: %d) DDRpressure: %d  GOODPF: %d\n", i, 
			pmu_result[0], l2_hitr[i], l3_hitr[i], core_contr_to_ddr[i], good_pf[i]);

	}


	//
	//Now we can make a decission...
	//
	// Below are two naive examples of tuning using the L2XQ respective L2 max distance parameter
	// All cores are set the same at this time
	//

	if (tunealg == 0) {

		for (int i = 0; i < ACTIVE_CORES; i++) {
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
				pf_msr_dirty[i / 4] = 1; //only set dirty in first core of module
				if (i == 0)
					pr_debug("l2xq %d\n", l2xq);
			}

		}


	} //if (tunealg)...

	return 0;
}

