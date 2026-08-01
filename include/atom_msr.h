#ifndef __ATOM_MSR_H
#define __ATOM_MSR_H

#ifdef __KERNEL__
#include <linux/types.h>
#endif

#ifndef __KERNEL__
#include <stdint.h>
#include <stdlib.h>
#endif

#define L2MAXDIST_MAX (31)
#define L3MAXDIST_MAX (63)
#define L2XQ_MAX (31)
#define L3XQ_MAX (31)
#define LOW_DMDDENS_L2L3XQ_MAX (63)

struct msr1A4_s{
	uint64_t L2_STREAM_DISABLED : 1;
	uint64_t L2_ADJACENT_CACHE_LINE_PREFETCHER_DISABLE : 1;
	uint64_t L1_DATA_STREAM_DISABLED : 1;
	uint64_t L1_INSTRUCTION_STREAM_DISABLED : 1;
	uint64_t L1_NEXT_PAGE_DISABLED : 1;
	uint64_t L2_AMP_DISABLED : 1;
        uint64_t LLC_PAGE_PREFETCH_DISABLE : 1;
        uint64_t AOP_PREFETCH_DISABLE : 1;
        uint64_t STREAM_PREFETCH_CODE_FETCH_DISABLE : 1;
        uint64_t pad0 : 3;
        uint64_t DYNAMIC_L2PREFETCHER_CONFIG_DISABLE : 1;

};

struct msr1320_s{
        uint64_t L2_STREAM_AMP_XQ_THRESHOLD : 5;
        uint64_t pad0 : 1;
        uint64_t INIT_PRE_PENDING : 3;
        uint64_t SKPAHD_PREF: 3;
        uint64_t MAX_PREF_PENDING: 5;
        uint64_t INIT_TRIG_WINDOW: 3;
        uint64_t L2_STREAM_MAX_DISTANCE : 5;
        uint64_t TRIG_PREF_HIT : 3;
        uint64_t pad1 : 2;
        uint64_t L2_AMP_DISABLE_RECURSION : 1;
        uint64_t DIS_AMP_TRIV_REC : 1;
        uint64_t L2HL_LLCHL_MIN_DIST : 5;
        uint64_t LLC_STREAM_MAX_DISTANCE : 6;
        uint64_t LLC_STREAM_DISABLE : 1;
        uint64_t LLC_INIT_PREF_PEND : 3;
        uint64_t LLC_MAX_PREF_PEND : 5;
        uint64_t pad2 : 1;
        uint64_t LLCPREF_LQ_THRESHOLD : 5;
        uint64_t LLC_STREAM_XQ_THRESHOLD : 5;
};

struct msr1321_s{
        uint64_t L2_STREAM_AMP_CREATE_IL1 : 1;
        uint64_t pad0 : 10;
        uint64_t ALTERNATIVE_ISIDE_PREFETCH_ENABLE: 1;
        uint64_t ALTERNATIVE_KICKSTART_PREFETCHES: 4;
        uint64_t ALTERNATIVE_REGULAR_PREFETCHES: 4;
        uint64_t DTP_ENABLE: 1;
        uint64_t L2_STREAM_DEMAND_DENSITY : 8;
        uint64_t L2_STREAM_DEMAND_DENSITY_OVR : 4;
        uint64_t pad1 : 3;
        uint64_t CREATE_PMH : 1;
        uint64_t pad2 : 3;
        uint64_t L2_DISABLE_NEXT_LINE_PREFETCH : 1;
        uint64_t L2_LLC_STREAM_AMP_XQ_THRESHOLD : 6;
        uint64_t pad3 : 16;
        uint64_t RESTORE_L2PREFETCHER_DEFAULTS : 1;
};

struct msr1322_s{
        uint64_t LLPREF_THROTTLE_ISSUE_FACTOR : 3;
        uint64_t LLPREF_THROTTLE_HIT_FACTOR : 3;
        uint64_t LLPREF_UNTHROTTLE_ISSUE_FACTOR : 3;
        uint64_t LLPREF_UNTHROTTLE_HIT_FACTOR : 3;
        uint64_t pad0 : 2;
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

struct msr1325_s{
        uint64_t  STR_WINDOW_SIZE: 5;
        uint64_t  KICK_START: 1;
        uint64_t  LQ_THRESHOLD: 5;
        uint64_t  pad0 : 3;
        uint64_t  AMP_MAX_TRIV_REC_PREF: 3;
        uint64_t  AMP_DELTA_CNT_INC_VAL: 3;
        uint64_t  pad1 : 12;
        uint64_t  PREFACC_COUNTER_THRESHOLD : 8;
        uint64_t  PREFACC_DISABLE_LQ2_THRESHOLD : 5;
        uint64_t  PREFACC_DISABLE_XQ_THRESHOLD : 5;
        uint64_t  PREFACC_COUNTER_INCR : 6;
        uint64_t  PREFACC_COUNT : 3;
        uint64_t  PREFACC_DISABLE : 1;
};

struct msr1326_s{
        uint64_t pad0 : 56;
        uint64_t AMP_RECUR_PREFETCHMINCOUNT : 6;
};

struct msr1327_s{
        uint64_t RMT_INC : 4;
        uint64_t RMT_DEC : 4;
        uint64_t RMT_POSITIVE_SATURATION : 3;
        uint64_t RMT_POSITIVE_UPPER_QUARTILE : 3;
        uint64_t RMT_POSITIVE_LOWER_QUARTILE : 3;
        uint64_t RMT_NEGATIVE_LOWER_QUARTILE : 3;
        uint64_t RMT_NEGATIVE_UPPER_QUARTILE : 3;
        uint64_t RMT_NEGATIVE_SATURATION : 3;
        uint64_t RMT_EN : 1;
};


union msr_u{
	struct msr1A4_s msr1A4;
	struct msr1320_s msr1320;
	struct msr1321_s msr1321;
	struct msr1322_s msr1322;
	struct msr1323_s msr1323;
	struct msr1324_s msr1324;
	struct msr1325_s msr1325;
	struct msr1326_s msr1326;
	struct msr1327_s msr1327;
	uint64_t v;
};

#endif
