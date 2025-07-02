#define _GNU_SOURCE
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/mman.h>

#include "common.h"
#include "pmu_core.h"
#include "pmu_ddr.h"
#include "msr.h"
#include "rdt_mbm.h"
#include "log.h"
#include "sysdetect.h"

#define TAG "PRIMITIVE"


int basicalg(int tunealg)
{
	uint64_t ddr_rd_bw,ddr_wr_bw;
	static uint64_t time_now, time_old = 0;
	float time_delta;


	//
	// Grab all PMU data
	//

	if (!rdt_enabled) {
		ddr_rd_bw = pmu_ddr(&ddr, DDR_PMU_RD);
		ddr_wr_bw = pmu_ddr(&ddr, DDR_PMU_WR);
	} else {
		ddr_rd_bw = rdt_mbm_bw_get();
		ddr_wr_bw = rdt_mbm_bw_get();
	}

	loga(TAG, "DDR RD BW: %ld MB/s\n", ddr_rd_bw / (1024 * 1024));
	loga(TAG, "DDR WR BW: %ld MB/s\n", ddr_wr_bw / (1024 * 1024));

	if (time_old == 0) {
		time_old = time_ms();
		return 0; //no selection the first time since all counters will be odd
	}

	time_now = time_ms();
	time_delta = (time_now - time_old) / 1000.0;
	time_old = time_now;

	float ddr_rd_percent = ((float)ddr_rd_bw / (1024 * 1024)) / (float)ddr_bw_target;
	ddr_rd_percent /= time_delta;

	loga(TAG, "Time delta %f, Running at %.1f percent rd bw (%ld MB/s)\n", time_delta, ddr_rd_percent * 100, ddr_rd_bw / (1024 * 1024));

	float ddr_wr_percent = ((float)ddr_wr_bw / (1024 * 1024)) / (float)ddr_bw_target;
	ddr_wr_percent /= time_delta;

	loga(TAG, "Time delta %f, Running at %.1f percent wr bw (%ld MB/s)\n", time_delta, ddr_wr_percent * 100, ddr_wr_bw / (1024 * 1024));

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


		logd(TAG, "core %02d PMU delta LD: %10ld  HIT(L2: %.2f  L3: %.2f) DDRpressure: %.2f  GOODPF: %.2f\n", i, gtinfo[i].pmu_result[0],
			l2_hitr[i], l3_hitr[i], core_contr_to_ddr[i], good_pf[i]);

//		logd(TAG, "   LD: %ld  HIT(L2: %ld  L3: %ld  DDR: %ld)  GOODPF: %ld\n", gtinfo[i].pmu_result[0], gtinfo[i].pmu_result[1],
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
					logv(TAG, "l2xq %d\n", l2xq);
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
				logv(TAG, "l3xq %d\n", l3xq);
			}
		}


	} //if (tunealg)...
	else if (tunealg == 1) {
		logd(TAG, "L2HR %.2f %.2f %.2f %.2f  %.2f %.2f %.2f %.2f  %.2f %.2f %.2f %.2f  %.2f %.2f %.2f %.2f\n", l2_hitr[0], l2_hitr[1], l2_hitr[2], l2_hitr[3], l2_hitr[4],
			l2_hitr[5], l2_hitr[6], l2_hitr[7], l2_hitr[8], l2_hitr[9], l2_hitr[10], l2_hitr[11], l2_hitr[12], l2_hitr[13], l2_hitr[14], l2_hitr[15]);

		for (int i = 0; i < ACTIVE_THREADS; i++) {
			int l2maxdist = msr_get_l2maxdist(&gtinfo[i].hwpf_msr_value[0]);
			int old_l2maxdist = l2maxdist;

			if (ddr_rd_percent < 0.10); //idle system
			else if (ddr_rd_percent < 0.20)
				l2maxdist += lround(+8 * aggr);
			else if (ddr_rd_percent < 0.30)
				l2maxdist += lround(+4 * aggr);
			else if (ddr_rd_percent < 0.40)
				l2maxdist += lround(+2 * aggr);
			else if (ddr_rd_percent < 0.50)
				l2maxdist += lround(+1 * aggr);
			else if (ddr_rd_percent < 0.60)
				l2maxdist += lround(+1 * aggr);
			else if (ddr_rd_percent < 0.70)
				l2maxdist += lround(+1 * aggr);
			else if (ddr_rd_percent < 0.80)
				l2maxdist += lround(+1 * aggr);
			else if (ddr_rd_percent < 0.90)
				l2maxdist += lround(-1 * aggr);
			else if (ddr_rd_percent < 0.93)
				l2maxdist += lround(-2 * aggr);
			else if (ddr_rd_percent < 0.96)
				l2maxdist += lround(-4 * aggr);
			else
				l2maxdist += lround(-8 * aggr);

			if (l2maxdist <= 0)
				l2maxdist = 1;
			if (l2maxdist > L2MAXDIST_MAX)
				l2maxdist = L2MAXDIST_MAX;

			if (old_l2maxdist != l2maxdist) {
				msr_set_l2maxdist(
					&gtinfo[i].hwpf_msr_value[0],
						l2maxdist);
				gtinfo[i].hwpf_msr_dirty = 1;
				if (i == 0)
					logv(TAG, "l2maxdist %d\n",
						l2maxdist);
			}

			int l3maxdist = msr_get_l3maxdist(&gtinfo[i].hwpf_msr_value[0]);
			int old_l3maxdist = l3maxdist;

			if (ddr_rd_percent < 0.10); //idle system
			else if (ddr_rd_percent < 0.20)
				l3maxdist += lround(+8 * aggr);
			else if (ddr_rd_percent < 0.30)
				l3maxdist += lround(+4 * aggr);
			else if (ddr_rd_percent < 0.40)
				l3maxdist += lround(+2 * aggr);
			else if (ddr_rd_percent < 0.50)
				l3maxdist += lround(+1 * aggr);
			else if (ddr_rd_percent < 0.60)
				l3maxdist += lround(+1 * aggr);
			else if (ddr_rd_percent < 0.70)
				l3maxdist += lround(+1 * aggr);
			else if (ddr_rd_percent < 0.80)
				l3maxdist += lround(+1 * aggr);
			else if (ddr_rd_percent < 0.90)
				l3maxdist += lround(-1 * aggr);
			else if (ddr_rd_percent < 0.93)
				l3maxdist += lround(-2 * aggr);
			else if (ddr_rd_percent < 0.96)
				l3maxdist += lround(-4 * aggr);
			else
				l3maxdist += lround(-8 * aggr);

			if (l3maxdist <= 0)
				l3maxdist = 1;
			if (l3maxdist > L3MAXDIST_MAX)
				l3maxdist = L3MAXDIST_MAX;

			if (old_l3maxdist != l3maxdist) {
				msr_set_l3maxdist(&gtinfo[i].hwpf_msr_value[0], l3maxdist);
				gtinfo[i].hwpf_msr_dirty = 1;
				if (i == 0)
					logv(TAG, "l3maxdist %d\n", l3maxdist);
			}

		}
	}

	return 0;
}

