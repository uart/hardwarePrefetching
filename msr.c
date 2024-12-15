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
#include "pmu_core.h"
#include "log.h"
#include "mab.h"
#include "common.h"

#define TAG "MSR"

// Open MSR file
int msr_open(int core)
{
	int msr_file;
	char filename[128];

	sprintf(filename, "/dev/cpu/%d/msr", core);
	msr_file = open(filename, O_RDWR);

	if (msr_file < 0){
		 loge(TAG, "Could not open MSR file %s, running as root/sudo?\n", filename);
		exit(-1);
	}
	return msr_file;
}
//
// Open and read MSR values
//
int msr_init(int core, union msr_u msr[])
{
	int msr_file;

	if (msr_file_id[core])
		msr_file = msr_file_id[core];
	else
		msr_file = msr_open(core);
	for(int i = 0; i < HWPF_MSR_FIELDS-1; i++){
		if(pread(msr_file, &msr[i], 8, HWPF_MSR_BASE + i) != 8){
			 loge(TAG, "Could not read MSR on core %d, is that an atom core?\n", core);
			exit(-1);
		}

		//logd(TAG, "0x%x: 0x%lx\n", HWPF_MSR_BASE + i, msr[i].v);
	}
	msr_file_id[core] = msr_file;


    if(pread(msr_file, &msr[HWPF_MSR_FIELDS-1], 8, HWPF_MSR_0X1A4) != 8){
			 loge(TAG, "Could not read MSR on core %d, is that an atom core?\n", core);
			exit(-1);
	}

	return msr_file;
}

int msr_fixed_int(int core)
{
    int msr_file;
    char filename[128];

    sprintf(filename, "/dev/cpu/%d/msr", core);
    msr_file = open(filename, O_RDWR);

    if (msr_file < 0) {
        loge(TAG, "Could not open MSR file %s, running as root/sudo?\n", filename);
        exit(-1);
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

static inline uint64_t rdtsc(void) {
    unsigned int lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

int msr_corepmu_read(int msr_file, int nr_events, uint64_t *result, uint64_t *inst_retired, uint64_t *cpu_cycles)
{
	if(nr_events > PMU_COUNTERS){
		loge(TAG, "Too many PMU events, max is %d\n", PMU_COUNTERS);
		exit(-1);
	}

	if (tunealg != MAB) {
		for(int i = 0; i < nr_events; i++){
			if(pread(msr_file, &result[i], 8, PMU_PMC0 + i) != 8){
				loge(TAG, "Could not read MSR 0x%x\n", PMU_PMC0 + i);
				exit(-1);
			}
		}
	}
//	else {  //Should we only read instructions and cycles for MAB tuner?

		// Read fixed counters for IPC calculation
		if (pread(msr_file, inst_retired, sizeof(uint64_t), MSR_FIXED_CTR0) != sizeof(uint64_t)) {
			loge(TAG, "Could not read fixed counter for instructions retired\n");
			return -1;
		}

		*cpu_cycles = rdtsc();

		// if (pread(msr_file, cpu_cycles, sizeof(uint64_t), MSR_FIXED_CTR1) != sizeof(uint64_t)) {
		//     loge(TAG, "Could not read fixed counter for CPU cycles\n");
		//     return -1;
		// }
//	}

	return 0;
}

int msr_enable_fixed(int msr_file) {
    uint64_t fixed_ctr_ctrl_value = 0x33;
    uint64_t zero_value = 0;

    // Write to IA32_FIXED_CTR_CTRL to enable counting
    if (pwrite(msr_file, &fixed_ctr_ctrl_value, sizeof(fixed_ctr_ctrl_value), IA32_FIXED_CTR_CTRL) != sizeof(fixed_ctr_ctrl_value)) {
        loge(TAG, "Failed to write IA32_FIXED_CTR_CTRL\n");
        return -1;
    }

    // Reset MSR_FIXED_CTR0 counter to zero
    if (pwrite(msr_file, &zero_value, sizeof(zero_value), MSR_FIXED_CTR0) != sizeof(zero_value)) {
        loge(TAG, "Could not reset fixed counter for instructions retired\n");
        return -1;
    }

    // Reset MSR_FIXED_CTR1 counter to zero
    if (pwrite(msr_file, &zero_value, sizeof(zero_value), MSR_FIXED_CTR1) != sizeof(zero_value)) {
        loge(TAG, "Could not reset fixed counter for CPU cycles\n");
        return -1;
	}

	return 0;
}

//
// Write new HWPF MSR values
//
int msr_hwpf_write(int msr_file, union msr_u msr[])
{
	for(int i = 0; i < HWPF_MSR_FIELDS-1; i++){
		if(pwrite(msr_file, &msr[i], 8, HWPF_MSR_BASE + i) != 8){
			loge(TAG, "Could not write MSR %d\n", HWPF_MSR_BASE + i);
			return -1;
		}
	}

    if(pwrite(msr_file, &msr[HWPF_MSR_FIELDS-1], 8, HWPF_MSR_0X1A4) != 8){
        loge(TAG, "Could not write MSR %d\n", HWPF_MSR_0X1A4);
        return -1;
	}

	return 0;
}

// Set value in MSR table
int msr_set_mlc_disable(union msr_u msr[], int value)
{
	msr[5].msr1A4.L2_STREAM_DISABLED = value;

	return 0;
}

int msr_get_mlc_disable(union msr_u msr[])
{
	return msr[5].msr1A4.L2_STREAM_DISABLED;
}

// Set value in MSR table
int msr_set_l1_data_disable(union msr_u msr[], int value)
{
	msr[5].msr1A4.L1_DATA_STREAM_DISABLED = value;

	return 0;
}

int msr_get_l1_data_disable(union msr_u msr[])
{
	return msr[5].msr1A4.L1_DATA_STREAM_DISABLED;
}

// Set value in MSR table
int msr_set_l1_instruction_disable(union msr_u msr[], int value)
{
	msr[5].msr1A4.L1_INSTRUCTION_STREAM_DISABLED = value;

	return 0;
}

int msr_get_l1_instruction_disable(union msr_u msr[])
{
	return msr[5].msr1A4.L1_INSTRUCTION_STREAM_DISABLED;
}

// Set value in MSR table
int msr_set_l1_next_page_disable(union msr_u msr[], int value)
{
	msr[5].msr1A4.L1_NEXT_PAGE_DISABLED = value;

	return 0;
}

int msr_get_l1_next_page_disable(union msr_u msr[])
{
	return msr[5].msr1A4.L1_NEXT_PAGE_DISABLED;
}

// Set value in MSR table
int msr_set_amp_disable(union msr_u msr[], int value)
{
	msr[5].msr1A4.L2_AMP_DISABLED = value;

	return 0;
}

int msr_get_amp_disable(union msr_u msr[])
{
	return msr[5].msr1A4.L2_AMP_DISABLED;
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

// Set value in MSR table
int msr_set_l2adr(union msr_u msr[], int value) {
    msr[0].msr1320.L2_AMP_DISABLE_RECURSION = value;
    return 0;
}

int msr_get_l2adr(union msr_u msr[]) {
    return msr[0].msr1320.L2_AMP_DISABLE_RECURSION;
}

// Set value in MSR table
int msr_set_llcoff(union msr_u msr[], int value) {
    msr[0].msr1320.LLC_STREAM_DISABLE = value;
    return 0;
}

int msr_get_llcoff(union msr_u msr[]) {
    return msr[0].msr1320.LLC_STREAM_DISABLE;
}

// Set value in MSR table
int msr_set_l2sacil1(union msr_u msr[], int value) {
    msr[1].msr1321.L2_STREAM_AMP_CREATE_IL1 = value;
    return 0;
}

int msr_get_l2sacil1(union msr_u msr[]) {
    return msr[1].msr1321.L2_STREAM_AMP_CREATE_IL1;
}

// Set value in MSR table
int msr_set_l2dd(union msr_u msr[], int value) {
    msr[1].msr1321.L2_STREAM_DEMAND_DENSITY = value;
    return 0;
}

int msr_get_l2dd(union msr_u msr[]) {
    return msr[1].msr1321.L2_STREAM_DEMAND_DENSITY;
}

// Set value in MSR table
int msr_set_l2ddovr(union msr_u msr[], int value) {
    msr[1].msr1321.L2_STREAM_DEMAND_DENSITY_OVR = value;
    return 0;
}

int msr_get_l2ddovr(union msr_u msr[]) {
    return msr[1].msr1321.L2_STREAM_DEMAND_DENSITY_OVR;
}

// Set value in MSR table
int msr_set_nlpoff(union msr_u msr[], int value) {
    msr[1].msr1321.L2_DISABLE_NEXT_LINE_PREFETCH = value;
    return 0;
}

int msr_get_nlpoff(union msr_u msr[]) {
    return msr[1].msr1321.L2_DISABLE_NEXT_LINE_PREFETCH;
}

// Set value in MSR table
int msr_set_l2llcxq(union msr_u msr[], int value) {
    msr[1].msr1321.L2_LLC_STREAM_AMP_XQ_THRESHOLD = value;
    return 0;
}

int msr_get_l2llcxq(union msr_u msr[]) {
    return msr[1].msr1321.L2_LLC_STREAM_AMP_XQ_THRESHOLD;
}

// Set value in MSR table
int msr_set_l3dd(union msr_u msr[], int value) {
    msr[2].msr1322.LLC_STREAM_DEMAND_DENSITY = value;
    return 0;
}

int msr_get_l3dd(union msr_u msr[]) {
    return msr[2].msr1322.LLC_STREAM_DEMAND_DENSITY;
}

// Set value in MSR table
int msr_set_l3ddovr(union msr_u msr[], int value) {
    msr[2].msr1322.LLC_STREAM_DEMAND_DENSITY_OVR = value;
    return 0;
}

int msr_get_l3ddovr(union msr_u msr[]) {
    return msr[2].msr1322.LLC_STREAM_DEMAND_DENSITY_OVR;
}

// Set value in MSR table
int msr_set_ampconf0(union msr_u msr[], int value) {
    msr[2].msr1322.L2_AMP_CONFIDENCE_DPT0 = value;
    return 0;
}

int msr_get_ampconf0(union msr_u msr[]) {
    return msr[2].msr1322.L2_AMP_CONFIDENCE_DPT0;
}

// Set value in MSR table
int msr_set_ampconf1(union msr_u msr[], int value) {
    msr[2].msr1322.L2_AMP_CONFIDENCE_DPT1 = value;
    return 0;
}

int msr_get_ampconf1(union msr_u msr[]) {
    return msr[2].msr1322.L2_AMP_CONFIDENCE_DPT1;
}

// Set value in MSR table
int msr_set_ampconf2(union msr_u msr[], int value) {
    msr[2].msr1322.L2_AMP_CONFIDENCE_DPT2 = value;
    return 0;
}

int msr_get_ampconf2(union msr_u msr[]) {
    return msr[2].msr1322.L2_AMP_CONFIDENCE_DPT2;
}

// Set value in MSR table
int msr_set_ampconf3(union msr_u msr[], int value) {
    msr[2].msr1322.L2_AMP_CONFIDENCE_DPT3 = value;
    return 0;
}

int msr_get_ampconf3(union msr_u msr[]) {
    return msr[2].msr1322.L2_AMP_CONFIDENCE_DPT3;
}

// Set value in MSR table
int msr_set_l2llcddxq(union msr_u msr[], int value) {
    msr[2].msr1322.L2_LLC_STREAM_DEMAND_DENSITY_XQ = value;
    return 0;
}

int msr_get_l2llcddxq(union msr_u msr[]) {
    return msr[2].msr1322.L2_LLC_STREAM_DEMAND_DENSITY_XQ;
}

// Set value in MSR table
int msr_set_ampcswpfrfo(union msr_u msr[], int value) {
    msr[3].msr1323.L2_STREAM_AMP_CREATE_SWPFRFO = value;
    return 0;
}

int msr_get_ampcswpfrfo(union msr_u msr[]) {
    return msr[3].msr1323.L2_STREAM_AMP_CREATE_SWPFRFO;
}

// Set value in MSR table
int msr_set_ampcswpfrd(union msr_u msr[], int value) {
    msr[3].msr1323.L2_STREAM_AMP_CREATE_SWPFRD = value;
    return 0;
}

int msr_get_ampcswpfrd(union msr_u msr[]) {
    return msr[3].msr1323.L2_STREAM_AMP_CREATE_SWPFRD;
}

// Set value in MSR table
int msr_set_ampchwpfd(union msr_u msr[], int value) {
    msr[3].msr1323.L2_STREAM_AMP_CREATE_HWPFD = value;
    return 0;
}

int msr_get_ampchwpfd(union msr_u msr[]) {
    return msr[3].msr1323.L2_STREAM_AMP_CREATE_HWPFD;
}

// Set value in MSR table
int msr_set_ampcdrfo(union msr_u msr[], int value) {
    msr[3].msr1323.L2_STREAM_AMP_CREATE_DRFO = value;
    return 0;
}

int msr_get_ampcdrfo(union msr_u msr[]) {
    return msr[3].msr1323.L2_STREAM_AMP_CREATE_DRFO;
}

// Set value in MSR table
int msr_set_stabswpfrfo(union msr_u msr[], int value) {
    msr[3].msr1323.STABILIZE_PREF_ON_SWPFRFO = value;
    return 0;
}

int msr_get_stabswpfrfo(union msr_u msr[]) {
    return msr[3].msr1323.STABILIZE_PREF_ON_SWPFRFO;
}

// Set value in MSR table
int msr_set_stabswpfrd(union msr_u msr[], int value) {
    msr[3].msr1323.STABILIZE_PREF_ON_SWPFRD = value;
    return 0;
}

int msr_get_stabswpfrd(union msr_u msr[]) {
    return msr[3].msr1323.STABILIZE_PREF_ON_SWPFRD;
}

// Set value in MSR table
int msr_set_stabil1(union msr_u msr[], int value) {
    msr[3].msr1323.STABILIZE_PREF_ON_IL1 = value;
    return 0;
}

int msr_get_stabil1(union msr_u msr[]) {
    return msr[3].msr1323.STABILIZE_PREF_ON_IL1;
}

// Set value in MSR table
int msr_set_stabhwpfd(union msr_u msr[], int value) {
    msr[3].msr1323.STABILIZE_PREF_ON_HWPFD = value;
    return 0;
}

int msr_get_stabhwpfd(union msr_u msr[]) {
    return msr[3].msr1323.STABILIZE_PREF_ON_HWPFD;
}

// Set value in MSR table
int msr_set_stabdrfo(union msr_u msr[], int value) {
    msr[3].msr1323.STABILIZE_PREF_ON_DRFO = value;
    return 0;
}

int msr_get_stabdrfo(union msr_u msr[]) {
    return msr[3].msr1323.STABILIZE_PREF_ON_DRFO;
}

// Set value in MSR table
int msr_set_ampcpfnpp(union msr_u msr[], int value) {
    msr[3].msr1323.L2_STREAM_AMP_CREATE_PFNPP = value;
    return 0;
}

int msr_get_ampcpfnpp(union msr_u msr[]) {
    return msr[3].msr1323.L2_STREAM_AMP_CREATE_PFNPP;
}

// Set value in MSR table
int msr_set_ampcpfipp(union msr_u msr[], int value) {
    msr[3].msr1323.L2_STREAM_AMP_CREATE_PFIPP = value;
    return 0;
}

int msr_get_ampcpfipp(union msr_u msr[]) {
    return msr[3].msr1323.L2_STREAM_AMP_CREATE_PFIPP;
}

// Set value in MSR table
int msr_set_stabpfnpp(union msr_u msr[], int value) {
    msr[3].msr1323.STABILIZE_PREF_ON_PFNPP = value;
    return 0;
}

int msr_get_stabpfnpp(union msr_u msr[]) {
    return msr[3].msr1323.STABILIZE_PREF_ON_PFNPP;
}

// Set value in MSR table
int msr_set_stabpfipp(union msr_u msr[], int value) {
    msr[3].msr1323.STABILIZE_PREF_ON_PFIPP = value;
    return 0;
}

int msr_get_stabpfipp(union msr_u msr[]) {
    return msr[3].msr1323.STABILIZE_PREF_ON_PFIPP;
}

// Set value in MSR table
int msr_set_l1ht(union msr_u msr[], int value) {
    msr[4].msr1324.L1_HOMELESS_THRESHOLD = value;
    return 0;
}

int msr_get_l1ht(union msr_u msr[]) {
    return msr[4].msr1324.L1_HOMELESS_THRESHOLD;
}

void populate_msr1320(union msr_u msr[]) {
    msr[0].msr1320.L2_STREAM_AMP_XQ_THRESHOLD = L2_STREAM_AMP_XQ_THRESHOLD_1320;
    msr[0].msr1320.L2_STREAM_MAX_DISTANCE = L2_STREAM_MAX_DISTANCE_1320;
    msr[0].msr1320.L2_AMP_DISABLE_RECURSION = L2_AMP_DISABLE_RECURSION_1320;
    msr[0].msr1320.LLC_STREAM_MAX_DISTANCE = LLC_STREAM_MAX_DISTANCE_1320;
    msr[0].msr1320.LLC_STREAM_DISABLE = LLC_STREAM_DISABLE_1320;
    msr[0].msr1320.LLC_STREAM_XQ_THRESHOLD = LLC_STREAM_XQ_THRESHOLD_1320;
}

void populate_msr1321(union msr_u msr[]) {
    msr[1].msr1321.L2_STREAM_AMP_CREATE_IL1 = L2_STREAM_AMP_CREATE_IL1_1321;
    msr[1].msr1321.L2_STREAM_DEMAND_DENSITY = L2_STREAM_DEMAND_DENSITY_1321;
    msr[1].msr1321.L2_STREAM_DEMAND_DENSITY_OVR = L2_STREAM_DEMAND_DENSITY_OVR_1321;
    msr[1].msr1321.L2_DISABLE_NEXT_LINE_PREFETCH = L2_DISABLE_NEXT_LINE_PREFETCH_1321;
    msr[1].msr1321.L2_LLC_STREAM_AMP_XQ_THRESHOLD = L2_LLC_STREAM_AMP_XQ_THRESHOLD_1321;
}

void populate_msr1322(union msr_u msr[]) {
    msr[2].msr1322.LLC_STREAM_DEMAND_DENSITY = LLC_STREAM_DEMAND_DENSITY_1322;
    msr[2].msr1322.LLC_STREAM_DEMAND_DENSITY_OVR = LLC_STREAM_DEMAND_DENSITY_OVR_1322;
    msr[2].msr1322.L2_AMP_CONFIDENCE_DPT0 = L2_AMP_CONFIDENCE_DPT0_1322;
    msr[2].msr1322.L2_AMP_CONFIDENCE_DPT1 = L2_AMP_CONFIDENCE_DPT1_1322;
    msr[2].msr1322.L2_AMP_CONFIDENCE_DPT2 = L2_AMP_CONFIDENCE_DPT2_1322;
    msr[2].msr1322.L2_AMP_CONFIDENCE_DPT3 = L2_AMP_CONFIDENCE_DPT3_1322;
    msr[2].msr1322.L2_LLC_STREAM_DEMAND_DENSITY_XQ = L2_LLC_STREAM_DEMAND_DENSITY_XQ_1322;
}

void populate_msr1323(union msr_u msr[]) {
    msr[3].msr1323.L2_STREAM_AMP_CREATE_SWPFRFO = L2_STREAM_AMP_CREATE_SWPFRFO_1323;
    msr[3].msr1323.L2_STREAM_AMP_CREATE_SWPFRD = L2_STREAM_AMP_CREATE_SWPFRD_1323;
    msr[3].msr1323.L2_STREAM_AMP_CREATE_HWPFD = L2_STREAM_AMP_CREATE_HWPFD_1323;
    msr[3].msr1323.L2_STREAM_AMP_CREATE_DRFO = L2_STREAM_AMP_CREATE_DRFO_1323;
    msr[3].msr1323.STABILIZE_PREF_ON_SWPFRFO = STABILIZE_PREF_ON_SWPFRFO_1323;
    msr[3].msr1323.STABILIZE_PREF_ON_SWPFRD = STABILIZE_PREF_ON_SWPFRD_1323;
    msr[3].msr1323.STABILIZE_PREF_ON_IL1 = STABILIZE_PREF_ON_IL1_1323;
    msr[3].msr1323.STABILIZE_PREF_ON_HWPFD = STABILIZE_PREF_ON_HWPFD_1323;
    msr[3].msr1323.STABILIZE_PREF_ON_DRFO = STABILIZE_PREF_ON_DRFO_1323;
    msr[3].msr1323.L2_STREAM_AMP_CREATE_PFNPP = L2_STREAM_AMP_CREATE_PFNPP_1323;
    msr[3].msr1323.L2_STREAM_AMP_CREATE_PFIPP = L2_STREAM_AMP_CREATE_PFIPP_1323;
    msr[3].msr1323.STABILIZE_PREF_ON_PFNPP = STABILIZE_PREF_ON_PFNPP_1323;
    msr[3].msr1323.STABILIZE_PREF_ON_PFIPP = STABILIZE_PREF_ON_PFIPP_1323;
}

// Get RMID from PQOS ASSOC MSR
int msr_get_rmid(int core, uint64_t *val)
{
	uint64_t data;

	if (pread(msr_file_id[core], &data, sizeof data, PQOS_MSR_ASSOC) != sizeof data) {
		if (errno == EIO) {
			fprintf(stderr, "rdmsr: CPU %d cannot read "
				"MSR 0x%X\n",
				core, PQOS_MSR_ASSOC);
			exit(4);
		} else {
			perror("rdmsr: pread");
			exit(127);
		}
	}

	*val = data;
	return 0;
}

// Set RMID on PQOS ASSOC MSR
int msr_set_rmid(unsigned core, uint64_t rmid)
{
	if (pwrite(msr_file_id[core], &rmid, sizeof(rmid), PQOS_MSR_ASSOC) != sizeof(rmid)) {
		if (errno == EIO) {
			fprintf(stderr,
				"msr_set_rmid(): CPU %d cannot set MSR "
				"0x%X to 0x%lX\n",
				core, PQOS_MSR_ASSOC, rmid);
			return -1;
		} else {
			perror("msr_set_rmid(): pwrite");
			exit(127);
		}
	}

	return 0;
}

int msr_get_evtsel(unsigned core, uint64_t *event)
{
	uint64_t data;

	if (pread(msr_file_id[core], &data, sizeof data, PQOS_MSR_MON_EVTSEL) != sizeof data) {
		if (errno == EIO) {
			fprintf(stderr, "msr_get_evtsel: CPU %d cannot read "
				"MSR 0x%X\n",
				core, PQOS_MSR_MON_EVTSEL);
			exit(4);
		} else {
			perror("msr_get_evtsel: pread");
			exit(127);
		}
	}

	*event = data;
	return 0;
}

int msr_set_evtsel(unsigned core, uint64_t event)
{
	if (pwrite(msr_file_id[core], &event, sizeof(event), PQOS_MSR_MON_EVTSEL) != sizeof(event)) {
		if (errno == EIO) {
			fprintf(stderr,
				"msr_set_evtsel(): CPU %d cannot set MSR "
				"0x%X to 0x%lX\n",
				core, PQOS_MSR_MON_EVTSEL, event);
			return -1;
		} else {
			perror("msr_set_evtsel(): pwrite");
			exit(127);
		}
	}

	return 0;
}

int msr_get_mon_count(int core, uint64_t *val)
{
	uint64_t data;

	if (pread(msr_file_id[core], &data, sizeof data, PQOS_MSR_MON_QMC) != sizeof data) {
		if (errno == EIO) {
			fprintf(stderr, "rdmsr: CPU %d cannot read "
				"MSR 0x%X\n",
				core, PQOS_MSR_MON_QMC);
			exit(4);
		} else {
			perror("rdmsr: pread");
			exit(127);
		}
	}

	*val = data;
	return 0;
}
