#ifndef __MSR_H
#define __MSR_H

#include <stdint.h>

#include "atom_msr.h"

#define HWPF_MSR_FIELDS (6)
#define HWPF_MSR_BASE (0x1320)
#define HWPF_MSR_0X1A4 (0x1A4)

#define MSR_FIXED_CTR0 0x309 // INST_RETIRED.ANY
#define MSR_FIXED_CTR1 0x30A // CPU_CLK_UNHALTED.CORE
#define IA32_FIXED_CTR_CTRL 0x38D

// Default prefetcher settings. The settings below are for the 12th generation Intel Alderlake chip 

// Default msr1320 settings
#define L2_STREAM_AMP_XQ_THRESHOLD_1320 4
#define L2_STREAM_MAX_DISTANCE_1320 16
#define L2_AMP_DISABLE_RECURSION_1320 1
#define LLC_STREAM_MAX_DISTANCE_1320 63
#define LLC_STREAM_DISABLE_1320 0
#define LLC_STREAM_XQ_THRESHOLD_1320 4

// Default msr1321 settings
#define L2_STREAM_AMP_CREATE_IL1_1321 1
#define L2_STREAM_DEMAND_DENSITY_1321 16
#define L2_STREAM_DEMAND_DENSITY_OVR_1321 9
#define L2_DISABLE_NEXT_LINE_PREFETCH_1321 1
#define L2_LLC_STREAM_AMP_XQ_THRESHOLD_1321 18

// Default msr1322 settings
#define LLC_STREAM_DEMAND_DENSITY_1322 320
#define LLC_STREAM_DEMAND_DENSITY_OVR_1322 9
#define L2_AMP_CONFIDENCE_DPT0_1322 1
#define L2_AMP_CONFIDENCE_DPT1_1322 3
#define L2_AMP_CONFIDENCE_DPT2_1322 5
#define L2_AMP_CONFIDENCE_DPT3_1322 7
#define L2_LLC_STREAM_DEMAND_DENSITY_XQ_1322 5

// Default msr1323 settings
#define L2_STREAM_AMP_CREATE_SWPFRFO_1323 1
#define L2_STREAM_AMP_CREATE_SWPFRD_1323 1
#define L2_STREAM_AMP_CREATE_HWPFD_1323 0
#define L2_STREAM_AMP_CREATE_DRFO_1323 1
#define STABILIZE_PREF_ON_SWPFRFO_1323 1
#define STABILIZE_PREF_ON_SWPFRD_1323 1
#define STABILIZE_PREF_ON_IL1_1323 0
#define STABILIZE_PREF_ON_HWPFD_1323 1
#define STABILIZE_PREF_ON_DRFO_1323 1
#define L2_STREAM_AMP_CREATE_PFNPP_1323 1
#define L2_STREAM_AMP_CREATE_PFIPP_1323 1
#define STABILIZE_PREF_ON_PFNPP_1323 1
#define STABILIZE_PREF_ON_PFIPP_1323 1


int msr_corepmu_setup(int msr_file, int nr_events, uint64_t *event);
int msr_corepmu_read(int msr_file, int nr_events, uint64_t *result, uint64_t *inst_retired, uint64_t *cpu_cycles);

int msr_int(int core, union msr_u msr[]);
int msr_hwpf_write(int msr_file, union msr_u msr[]);

int msr_fixed_int(int core);
int msr_enable_fixed(int msr_file);

// Declarations for msr1A4_s
int msr_set_l1_data_disable(union msr_u msr[], int value);
int msr_set_l1_instruction_disable(union msr_u msr[], int value);
int msr_set_l1_next_page_disable(union msr_u msr[], int value);

int msr_set_mlc_disable(union msr_u msr[], int value);
int msr_set_amp_disable(union msr_u msr[], int value);

// Declarations for msr1320_s
int msr_set_l2xq(union msr_u msr[], int value);
int msr_get_l2xq(union msr_u msr[]);
int msr_set_l3xq(union msr_u msr[], int value);
int msr_get_l3xq(union msr_u msr[]);

int msr_set_l2maxdist(union msr_u msr[], int value);
int msr_get_l2maxdist(union msr_u msr[]);
int msr_set_l3maxdist(union msr_u msr[], int value);
int msr_get_l3maxdist(union msr_u msr[]);

int msr_set_l2adr(union msr_u msr[], int value);
int msr_get_l2adr(union msr_u msr[]);
int msr_set_llcoff(union msr_u msr[], int value); //TODO: CHange name to "_disable" for consistency
int msr_get_llcoff(union msr_u msr[]);

// Declarations for msr1321_s
int msr_set_l2sacil1(union msr_u msr[], int value);
int msr_get_l2sacil1(union msr_u msr[]);

int msr_set_l2dd(union msr_u msr[], int value);
int msr_get_l2dd(union msr_u msr[]);
int msr_set_l2ddovr(union msr_u msr[], int value);
int msr_get_l2ddovr(union msr_u msr[]);

int msr_set_nlpoff(union msr_u msr[], int value); //TODO: CHange name to "_disable" for consistency
int msr_get_nlpoff(union msr_u msr[]);

int msr_set_l2llcxq(union msr_u msr[], int value);
int msr_get_l2llcxq(union msr_u msr[]);

// Declarations for msr1322_s
int msr_set_l3dd(union msr_u msr[], int value);
int msr_get_l3dd(union msr_u msr[]);
int msr_set_l3ddovr(union msr_u msr[], int value);
int msr_get_l3ddovr(union msr_u msr[]);

int msr_set_ampconf0(union msr_u msr[], int value);
int msr_get_ampconf0(union msr_u msr[]);
int msr_set_ampconf1(union msr_u msr[], int value);
int msr_get_ampconf1(union msr_u msr[]);
int msr_set_ampconf2(union msr_u msr[], int value);
int msr_get_ampconf2(union msr_u msr[]);
int msr_set_ampconf3(union msr_u msr[], int value);
int msr_get_ampconf3(union msr_u msr[]);

int msr_set_l2llcddxq(union msr_u msr[], int value);
int msr_get_l2llcddxq(union msr_u msr[]);

// Declarations for msr1323_s
int msr_set_ampcswpfrfo(union msr_u msr[], int value);
int msr_get_ampcswpfrfo(union msr_u msr[]);
int msr_set_ampcswpfrd(union msr_u msr[], int value);
int msr_get_ampcswpfrd(union msr_u msr[]);
int msr_set_ampchwpfd(union msr_u msr[], int value);
int msr_get_ampchwpfd(union msr_u msr[]);
int msr_set_ampcdrfo(union msr_u msr[], int value);
int msr_get_ampcdrwo(union msr_u msr[]);

int msr_set_stabswpfrfo(union msr_u msr[], int value);
int msr_get_stabswpfrfo(union msr_u msr[]);
int msr_set_stabswpfrd(union msr_u msr[], int value);
int msr_get_stabswpfrd(union msr_u msr[]);

int msr_set_stabil1(union msr_u msr[], int value);
int msr_get_stabil1(union msr_u msr[]);
int msr_set_stabhwpfd(union msr_u msr[], int value);
int msr_get_stabhwpfd(union msr_u msr[]);
int msr_set_stabdrfo(union msr_u msr[], int value);
int msr_get_stabdrfo(union msr_u msr[]);

int msr_set_ampcpfnpp(union msr_u msr[], int value);
int msr_get_ampcpfnpp(union msr_u msr[]);
int msr_set_ampcpfipp(union msr_u msr[], int value);
int msr_get_ampcpfipp(union msr_u msr[]);

int msr_set_stabpfnpp(union msr_u msr[], int value);
int msr_get_stabpfnpp(union msr_u msr[]);
int msr_set_stabpfipp(union msr_u msr[], int value);
int msr_get_stabpfipp(union msr_u msr[]);

// Declarations for msr1324_s
int msr_set_l1ht(union msr_u msr[], int value);
int msr_get_l1ht(union msr_u msr[]);

void populate_msr1320(union msr_u msr[]);
void populate_msr1321(union msr_u msr[]);
void populate_msr1322(union msr_u msr[]);
void populate_msr1323(union msr_u msr[]);

#endif


