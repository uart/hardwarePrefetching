#ifndef __ATOM_MSR_H
#define __ATOM_MSR_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define L2MAXDIST_MAX (31)
#define L3MAXDIST_MAX (63)
#define L2XQ_MAX (31)
#define L3XQ_MAX (31)
#define LOW_DMDDENS_L2L3XQ_MAX (63)

struct msr1320_s{
	uint64_t L2_STREAM_AMP_XQ_THRESHOLD : 5;
	uint64_t pad0 : 15;
	uint64_t L2_STREAM_MAX_DISTANCE : 5;
	uint64_t pad1 : 5;
	uint64_t L2_AMP_DISABLE_RECURSION : 1;
	uint64_t pad2 : 6;
	uint64_t LLC_STREAM_MAX_DISTANCE : 6;
	uint64_t LLC_STREAM_DISABLE : 1;
	uint64_t pad3 : 14;
	uint64_t LLC_STREAM_XQ_THRESHOLD : 5;
};

struct msr1321_s{
	uint64_t L2_STREAM_AMP_CREATE_IL1 : 1;
	uint64_t pad0 : 20;
	uint64_t L2_STREAM_DEMAND_DENSITY : 8;
	uint64_t L2_STREAM_DEMAND_DENSITY_OVR : 4;
	uint64_t pad1 : 7;
	uint64_t L2_DISABLE_NEXT_LINE_PREFETCH : 1;
	uint64_t L2_LLC_STREAM_AMP_XQ_THRESHOLD : 6;
};

struct msr1322_s{
	uint64_t pad0 : 14;
	uint64_t LLC_STREAM_DEMAND_DENSITY : 9;
	uint64_t LLC_STREAM_DEMAND_DENSITY_OVR : 4;
	uint64_t L2_AMP_CONFIDENCE_DPT0 : 6;
	uint64_t L2_AMP_CONFIDENCE_DPT1 : 6;
	uint64_t L2_AMP_CONFIDENCE_DPT2 : 6;
	uint64_t L2_AMP_CONFIDENCE_DPT3 : 6;
	uint64_t pad1 : 8;
	uint64_t L2_LLC_STREAM_DEMAND_DENSITY_XQ : 3;
};

struct msr1323_s{
	uint64_t pad0 : 34;
	uint64_t L2_STREAM_AMP_CREATE_SWPFRFO : 1;
	uint64_t L2_STREAM_AMP_CREATE_SWPFRD : 1;
	uint64_t pad1 : 1;
	uint64_t L2_STREAM_AMP_CREATE_HWPFD : 1;
	uint64_t L2_STREAM_AMP_CREATE_DRFO : 1;
	uint64_t STABILIZE_PREF_ON_SWPFRFO : 1;
	uint64_t STABILIZE_PREF_ON_SWPFRD : 1;
	uint64_t STABILIZE_PREF_ON_IL1 : 1;
	uint64_t pad2 : 1;
	uint64_t STABILIZE_PREF_ON_HWPFD : 1;
	uint64_t STABILIZE_PREF_ON_DRFO : 1;
	uint64_t L2_STREAM_AMP_CREATE_PFNPP : 1;
	uint64_t L2_STREAM_AMP_CREATE_PFIPP : 1;
	uint64_t STABILIZE_PREF_ON_PFNPP : 1;
	uint64_t STABILIZE_PREF_ON_PFIPP : 1;
};

struct msr1324_s{
	uint64_t pad0 : 54;
	uint64_t L1_HOMELESS_THRESHOLD : 8;
};

struct msr1A4_s{
	uint64_t L2_STREAM_DISABLED : 1;
	uint64_t pad0 : 1;
	uint64_t L1_DATA_STREAM_DISABLED : 1;
	uint64_t L1_INSTRUCTION_STREAM_DISABLED : 1;
	uint64_t L1_NEXT_PAGE_DISABLED : 1;
	uint64_t L2_AMP_DISABLED : 1;
};


union msr_u{
	struct msr1320_s msr1320;
	struct msr1321_s msr1321;
	struct msr1322_s msr1322;
	struct msr1323_s msr1323;
	struct msr1324_s msr1324;
	struct msr1A4_s msr1A4;
	uint64_t v;
};

#endif

