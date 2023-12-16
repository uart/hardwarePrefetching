#define _GNU_SOURCE
#include <stdio.h>
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

#include "msr.h"
#include "pmu.h"
#include "log.h"

#define TAG "MSR"


//
// Open and read MSR values
//
int msr_int(int core, union msr_u msr[])
{
	int msr_file;
	char filename[128];

	sprintf(filename, "/dev/cpu/%d/msr", core);
	msr_file = open(filename, O_RDWR);

	if (msr_file < 0){
		 loge(TAG, "Could not open MSR file %s, running as root/sudo?\n", filename);
		exit(-1);
	}

	for(int i = 0; i < HWPF_MSR_FIELDS; i++){
		if(pread(msr_file, &msr[i], 8, HWPF_MSR_BASE + i) != 8){
			 loge(TAG, "Could not read MSR on core %d, is that an atom core?\n", core);
			exit(-1);
		}

		//logd(TAG, "0x%x: 0x%lx\n", HWPF_MSR_BASE + i, msr[i].v);
	}

	return msr_file;
}

//
// Write new HWPF MSR values
int msr_corepmu_setup(int msr_file, int nr_events, uint64_t *event)
{
	if(nr_events > PMU_COUNTERS){
		loge(TAG, "Too many PMU events, max is %d\n", PMU_COUNTERS);
		exit(-1);
	}

	for(int i = 0; i < nr_events; i++){
		if(pwrite(msr_file, &event[i], 8, PMU_PERFEVTSEL0 + i) != 8){
			loge(TAG, "Could not write MSR %d\n", PMU_PERFEVTSEL0 + i);
			return -1;
		}
	}

	return 0;
}

int msr_corepmu_read(int msr_file, int nr_events, uint64_t *result)
{
	if(nr_events > PMU_COUNTERS){
		loge(TAG, "Too many PMU events, max is %d\n", PMU_COUNTERS);
		exit(-1);
	}

	for(int i = 0; i < nr_events; i++){
		if(pread(msr_file, &result[i], 8, PMU_PMC0 + i) != 8){
			loge(TAG, "Could not read MSR 0x%x\n", PMU_PMC0 + i);
			exit(-1);
		}
	}

	return 0;
}

//
// Write new HWPF MSR values
//
int msr_hwpf_write(int msr_file, union msr_u msr[])
{
	for(int i = 0; i < HWPF_MSR_FIELDS; i++){
		if(pwrite(msr_file, &msr[i], 8, HWPF_MSR_BASE + i) != 8){
			loge(TAG, "Could not write MSR %d\n", HWPF_MSR_BASE + i);
			return -1;
		}
	}

	return 0;
}

// Set value in MSR table
int msr_set_l2xq(union msr_u msr[], int value)
{
	msr[0].msr1320.L2_STREAM_AMP_XQ_THRESHOLD = value;

	return 0;
}

int msr_get_l2xq(union msr_u msr[])
{
	return msr[0].msr1320.L2_STREAM_AMP_XQ_THRESHOLD;
}

// Set value in MSR table
int msr_set_l3xq(union msr_u msr[], int value)
{
	msr[0].msr1320.LLC_STREAM_XQ_THRESHOLD = value;

	return 0;
}

int msr_get_l3xq(union msr_u msr[])
{
	return msr[0].msr1320.LLC_STREAM_XQ_THRESHOLD;
}

// Set value in MSR table
int msr_set_l2maxdist(union msr_u msr[], int value)
{
	msr[0].msr1320.L2_STREAM_MAX_DISTANCE = value;

	return 0;
}

int msr_get_l2maxdist(union msr_u msr[])
{
	return msr[0].msr1320.L2_STREAM_MAX_DISTANCE;
}

// Set value in MSR table
int msr_set_l3maxdist(union msr_u msr[], int value)
{
	msr[0].msr1320.LLC_STREAM_MAX_DISTANCE = value;

	return 0;
}

int msr_get_l3maxdist(union msr_u msr[])
{
	return msr[0].msr1320.LLC_STREAM_MAX_DISTANCE;
}
