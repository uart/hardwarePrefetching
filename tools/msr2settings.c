#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "../include/atom_msr.h"

int main(int argc, char *argv[])
{
	uint32_t msr_id = 0;
	union msr_u msr_value;

	printf("dPF MSR2setting\n");

	if(argc < 4){
		printf("Error, call with <a/b> <MSR> <MSR value> <...>\n");
		printf("MSRs supported are 0x1a4 and 0x1320 to 0x1327\n");
		printf(" a: convert msr value to settings, ./msr2settings a 0x1320 0x700007e041000018\n");
		printf(" b: convert settings to msr value, ./msr2settings b 0x1320 0x18 0x10 0x01 0x3f 0x00 0x1c\n");

		return -1;
	}

	msr_id = strtol(argv[2], NULL, 16);

	if(argv[1][0] == 'a'){
		msr_value.v = strtoll(argv[3], NULL, 16);
		printf("MSR 0x%x Value 0x%lx:\n", msr_id, msr_value.v);

		switch(msr_id){
			case 0x1a4: //updated to DKT
				printf("L2_STREAM_DISABLED: 0x%02x\n", msr_value.msr1A4.L2_STREAM_DISABLED);
				printf("L2_ADJACENT_CACHE_LINE_PREFETCHER_DISABLE: 0x%02x\n", msr_value.msr1A4.L2_ADJACENT_CACHE_LINE_PREFETCHER_DISABLE);
				printf("L1_DATA_STREAM_DISABLED: 0x%02x\n", msr_value.msr1A4.L1_DATA_STREAM_DISABLED);
				printf("L1_INSTRUCTION_STREAM_DISABLED: 0x%02x\n", msr_value.msr1A4.L1_INSTRUCTION_STREAM_DISABLED);
				printf("L1_NEXT_PAGE_DISABLED: 0x%02x\n", msr_value.msr1A4.L1_NEXT_PAGE_DISABLED);
				printf("L2_AMP_DISABLED: 0x%02x\n", msr_value.msr1A4.L2_AMP_DISABLED);
				printf("LLC_PAGE_PREFETCH_DISABLE: 0x%02x\n", msr_value.msr1A4.LLC_PAGE_PREFETCH_DISABLE);
				printf("AOP_PREFETCH_DISABLE: 0x%02x (DKT)\n", msr_value.msr1A4.AOP_PREFETCH_DISABLE);
				printf("STREAM_PREFETCH_CODE_FETCH_DISABLE: 0x%02x\n", msr_value.msr1A4.STREAM_PREFETCH_CODE_FETCH_DISABLE);
				printf("DYNAMIC_L2PREFETCHER_CONFIG_DISABLE: 0x%02x (DKT)\n", msr_value.msr1A4.DYNAMIC_L2PREFETCHER_CONFIG_DISABLE);
			break;

			case 0x1320: //updated to DKT
				printf(" L2_STREAM_AMP_XQ_THRESHOLD: 0x%02x\n", msr_value.msr1320.L2_STREAM_AMP_XQ_THRESHOLD);
				printf(" INIT_PRE_PENDING: 0x%02x\n", msr_value.msr1320.INIT_PRE_PENDING);
				printf(" SKPAHD_PREF: 0x%02x\n", msr_value.msr1320.SKPAHD_PREF);
				printf(" MAX_PREF_PENDING: 0x%02x\n", msr_value.msr1320.MAX_PREF_PENDING);
				printf(" INIT_TRIG_WINDOW: 0x%02x\n", msr_value.msr1320.INIT_TRIG_WINDOW);
				printf(" L2_STREAM_MAX_DISTANCE: 0x%02x\n", msr_value.msr1320.L2_STREAM_MAX_DISTANCE);
				printf(" TRIG_PREF_HIT: 0x%02x\n", msr_value.msr1320.TRIG_PREF_HIT);
				printf(" L2_AMP_DISABLE_RECURSION: 0x%02x\n", msr_value.msr1320.L2_AMP_DISABLE_RECURSION);
				printf(" DIS_AMP_TRIV_REC: 0x%02x\n", msr_value.msr1320.DIS_AMP_TRIV_REC);
				printf(" L2HL_LLCHL_MIN_DIST: 0x%02x\n", msr_value.msr1320.L2HL_LLCHL_MIN_DIST);
				printf(" LLC_STREAM_MAX_DISTANCE: 0x%02x\n", msr_value.msr1320.LLC_STREAM_MAX_DISTANCE);
				printf(" LLC_STREAM_DISABLE: 0x%02x\n", msr_value.msr1320.LLC_STREAM_DISABLE);
				printf(" LLC_INIT_PREF_PEND: 0x%02x\n", msr_value.msr1320.LLC_INIT_PREF_PEND);
				printf(" LLC_MAX_PREF_PEND: 0x%02x\n", msr_value.msr1320.LLC_MAX_PREF_PEND);
				printf(" LLCPREF_LQ_THRESHOLD: 0x%02x\n", msr_value.msr1320.LLCPREF_LQ_THRESHOLD);
				printf(" LLC_STREAM_XQ_THRESHOLD: 0x%02x\n", msr_value.msr1320.LLC_STREAM_XQ_THRESHOLD);
			break;

			case 0x1321: //updated to DKT
				printf("L2_STREAM_AMP_CREATE_IL1: 0x%02x\n", msr_value.msr1321.L2_STREAM_AMP_CREATE_IL1);
				printf("ALTERNATIVE_ISIDE_PREFETCH_ENABLE: 0x%02x\n", msr_value.msr1321.ALTERNATIVE_ISIDE_PREFETCH_ENABLE);
				printf("ALTERNATIVE_KICKSTART_PREFETCHES: 0x%02x\n", msr_value.msr1321.ALTERNATIVE_KICKSTART_PREFETCHES);
				printf("ALTERNATIVE_REGULAR_PREFETCHES: 0x%02x\n", msr_value.msr1321.ALTERNATIVE_REGULAR_PREFETCHES);
				printf("DTP_ENABLE: 0x%02x\n", msr_value.msr1321.DTP_ENABLE);
				printf("L2_STREAM_DEMAND_DENSITY: 0x%02x\n", msr_value.msr1321.L2_STREAM_DEMAND_DENSITY);
				printf("L2_STREAM_DEMAND_DENSITY_OVR: 0x%02x\n", msr_value.msr1321.L2_STREAM_DEMAND_DENSITY_OVR);
				printf("CREATE_PMH: 0x%02x\n", msr_value.msr1321.CREATE_PMH);
				printf("L2_DISABLE_NEXT_LINE_PREFETCH: 0x%02x\n", msr_value.msr1321.L2_DISABLE_NEXT_LINE_PREFETCH);
				printf("L2_LLC_STREAM_AMP_XQ_THRESHOLD: 0x%02x\n", msr_value.msr1321.L2_LLC_STREAM_AMP_XQ_THRESHOLD);
				printf("RESTORE_L2PREFETCHER_DEFAULTS (write only): 0x%02x\n", msr_value.msr1321.RESTORE_L2PREFETCHER_DEFAULTS);
			break;

			case 0x1322: //updated to DKT
				printf("LLPREF_THROTTLE_ISSUE_FACTOR: 0x%02x\n", msr_value.msr1322.LLPREF_THROTTLE_ISSUE_FACTOR);
				printf("LLPREF_THROTTLE_HIT_FACTOR: 0x%02x\n", msr_value.msr1322.LLPREF_THROTTLE_HIT_FACTOR);
				printf("LLPREF_UNTHROTTLE_ISSUE_FACTOR: 0x%02x\n", msr_value.msr1322.LLPREF_UNTHROTTLE_ISSUE_FACTOR);
				printf("LLPREF_UNTHROTTLE_HIT_FACTOR: 0x%02x\n", msr_value.msr1322.LLPREF_UNTHROTTLE_HIT_FACTOR);
				printf("LLC_STREAM_DEMAND_DENSITY: 0x%02x\n", msr_value.msr1322.LLC_STREAM_DEMAND_DENSITY);
				printf("LLC_STREAM_DEMAND_DENSITY_OVR: 0x%02x\n", msr_value.msr1322.LLC_STREAM_DEMAND_DENSITY_OVR);
				printf("L2_AMP_CONFIDENCE_DPT0: 0x%02x\n", msr_value.msr1322.L2_AMP_CONFIDENCE_DPT0);
				printf("L2_AMP_CONFIDENCE_DPT1: 0x%02x\n", msr_value.msr1322.L2_AMP_CONFIDENCE_DPT1);
				printf("L2_AMP_CONFIDENCE_DPT2: 0x%02x\n", msr_value.msr1322.L2_AMP_CONFIDENCE_DPT2);
				printf("L2_AMP_CONFIDENCE_DPT3: 0x%02x\n", msr_value.msr1322.L2_AMP_CONFIDENCE_DPT3);
				printf("L2_LLC_STREAM_DEMAND_DENSITY_XQ: 0x%02x\n", msr_value.msr1322.L2_LLC_STREAM_DEMAND_DENSITY_XQ);
			break;

			case 0x1323: //same up to DKT
				printf(" L2_STREAM_AMP_CREATE_SWPFRFO: 0x%02x\n", msr_value.msr1323.L2_STREAM_AMP_CREATE_SWPFRFO);
				printf(" L2_STREAM_AMP_CREATE_SWPFRD: 0x%02x\n", msr_value.msr1323.L2_STREAM_AMP_CREATE_SWPFRD);
				printf(" L2_STREAM_AMP_CREATE_HWPFD: 0x%02x\n", msr_value.msr1323.L2_STREAM_AMP_CREATE_HWPFD);
				printf(" L2_STREAM_AMP_CREATE_DRFO: 0x%02x\n", msr_value.msr1323.L2_STREAM_AMP_CREATE_DRFO);
				printf(" STABILIZE_PREF_ON_SWPFRFO: 0x%02x\n", msr_value.msr1323.STABILIZE_PREF_ON_SWPFRFO);
				printf(" STABILIZE_PREF_ON_SWPFRD: 0x%02x\n", msr_value.msr1323.STABILIZE_PREF_ON_SWPFRD);
				printf(" STABILIZE_PREF_ON_IL1: 0x%02x\n", msr_value.msr1323.STABILIZE_PREF_ON_IL1);
				printf(" STABILIZE_PREF_ON_HWPFD: 0x%02x\n", msr_value.msr1323.STABILIZE_PREF_ON_HWPFD);
				printf(" STABILIZE_PREF_ON_DRFO: 0x%02x\n", msr_value.msr1323.STABILIZE_PREF_ON_DRFO);
				printf(" L2_STREAM_AMP_CREATE_PFNPP: 0x%02x\n", msr_value.msr1323.L2_STREAM_AMP_CREATE_PFNPP);
				printf(" L2_STREAM_AMP_CREATE_PFIPP: 0x%02x\n", msr_value.msr1323.L2_STREAM_AMP_CREATE_PFIPP);
				printf(" STABILIZE_PREF_ON_PFNPP: 0x%02x\n", msr_value.msr1323.STABILIZE_PREF_ON_PFNPP);
				printf(" STABILIZE_PREF_ON_PFIPP: 0x%02x\n", msr_value.msr1323.STABILIZE_PREF_ON_PFIPP);
			break;

			case 0x1324: //same up to DKT
				printf("L1_HOMELESS_THRESHOLD: 0x%02x\n", msr_value.msr1324.L1_HOMELESS_THRESHOLD);
			break;

			case 0x1325: //new with CMT
				printf("STR_WINDOW_SIZE: 0x%02x\n", msr_value.msr1325.STR_WINDOW_SIZE);
				printf("KICK_START: 0x%02x\n", msr_value.msr1325.KICK_START);
				printf("LQ_THRESHOLD: 0x%02x\n", msr_value.msr1325.LQ_THRESHOLD);
				printf("AMP_MAX_TRIV_REC_PREF: 0x%02x\n", msr_value.msr1325.AMP_MAX_TRIV_REC_PREF);
				printf("AMP_DELTA_CNT_INC_VAL: 0x%02x\n", msr_value.msr1325.AMP_DELTA_CNT_INC_VAL);
				printf("PREFACC_COUNTER_THRESHOLD: 0x%02x\n", msr_value.msr1325.PREFACC_COUNTER_THRESHOLD);
				printf("PREFACC_DISABLE_LQ2_THRESHOLD: 0x%02x\n", msr_value.msr1325.PREFACC_DISABLE_LQ2_THRESHOLD);
				printf("PREFACC_DISABLE_XQ_THRESHOLD: 0x%02x\n", msr_value.msr1325.PREFACC_DISABLE_XQ_THRESHOLD);
				printf("PREFACC_COUNTER_INCR: 0x%02x\n", msr_value.msr1325.PREFACC_COUNTER_INCR);
				printf("PREFACC_COUNT: 0x%02x\n", msr_value.msr1325.PREFACC_COUNT);
				printf("PREFACC_DISABLE: 0x%02x\n", msr_value.msr1325.PREFACC_DISABLE);
			break;

			case 0x1326: //new with CMT
				printf("AMP_RECUR_PREFETCHMINCOUNT: 0x%02x\n", msr_value.msr1326.AMP_RECUR_PREFETCHMINCOUNT);
			break;

			case 0x1327: //new with CMT
				printf("RMT_INC: 0x%02x\n", msr_value.msr1327.RMT_INC);
				printf("RMT_DEC: 0x%02x\n", msr_value.msr1327.RMT_DEC);
				printf("RMT_POSITIVE_SATURATIONRMT_POSITIVE_SATURATION: 0x%02x\n", msr_value.msr1327.RMT_POSITIVE_SATURATION);
				printf("RMT_POSITIVE_UPPER_QUARTILE: 0x%02x\n", msr_value.msr1327.RMT_POSITIVE_UPPER_QUARTILE);
				printf("RMT_POSITIVE_LOWER_QUARTILE: 0x%02x\n", msr_value.msr1327.RMT_POSITIVE_LOWER_QUARTILE);
				printf("RMT_NEGATIVE_LOWER_QUARTILE: 0x%02x\n", msr_value.msr1327.RMT_NEGATIVE_LOWER_QUARTILE);
				printf("RMT_NEGATIVE_UPPER_QUARTILE: 0x%02x\n", msr_value.msr1327.RMT_NEGATIVE_UPPER_QUARTILE);
				printf("RMT_NEGATIVE_SATURATION: 0x%02x\n", msr_value.msr1327.RMT_NEGATIVE_SATURATION);
				printf("RMT_ENRMT_EN: 0x%02x\n", msr_value.msr1327.RMT_EN);
			break;

			default:
				printf("Incorrect MSR ID\n");
			break;
		}
	}
	else if(argv[1][0] == 'b'){
		msr_value.v = 0;

		switch(msr_id){
			case 0x1a4:  //updated to DKT
				msr_value.msr1A4.L2_STREAM_DISABLED = strtol(argv[3], NULL, 16);
				msr_value.msr1A4.L2_ADJACENT_CACHE_LINE_PREFETCHER_DISABLE = strtol(argv[4], NULL, 16);
				msr_value.msr1A4.L1_DATA_STREAM_DISABLED = strtol(argv[5], NULL, 16);
				msr_value.msr1A4.L1_INSTRUCTION_STREAM_DISABLED = strtol(argv[6], NULL, 16);
				msr_value.msr1A4.L1_NEXT_PAGE_DISABLED = strtol(argv[7], NULL, 16);
				msr_value.msr1A4.L2_AMP_DISABLED = strtol(argv[8], NULL, 16);
				msr_value.msr1A4.LLC_PAGE_PREFETCH_DISABLE = strtol(argv[9], NULL, 16);
				msr_value.msr1A4.AOP_PREFETCH_DISABLE = strtol(argv[10], NULL, 16);
				msr_value.msr1A4.STREAM_PREFETCH_CODE_FETCH_DISABLE = strtol(argv[11], NULL, 16);
				msr_value.msr1A4.DYNAMIC_L2PREFETCHER_CONFIG_DISABLE = strtol(argv[12], NULL, 16);
			break;

			case 0x1320: //updated to DKT
				msr_value.msr1320.L2_STREAM_AMP_XQ_THRESHOLD = strtol(argv[3], NULL, 16);
				msr_value.msr1320.INIT_PRE_PENDING = strtol(argv[4], NULL, 16);
				msr_value.msr1320.SKPAHD_PREF = strtol(argv[5], NULL, 16);
				msr_value.msr1320.MAX_PREF_PENDING = strtol(argv[6], NULL, 16);
				msr_value.msr1320.INIT_TRIG_WINDOW = strtol(argv[7], NULL, 16);
				msr_value.msr1320.L2_STREAM_MAX_DISTANCE = strtol(argv[8], NULL, 16);
				msr_value.msr1320.TRIG_PREF_HIT = strtol(argv[9], NULL, 16);
				msr_value.msr1320.L2_AMP_DISABLE_RECURSION = strtol(argv[10], NULL, 16);
				msr_value.msr1320.DIS_AMP_TRIV_REC = strtol(argv[11], NULL, 16);
				msr_value.msr1320.L2HL_LLCHL_MIN_DIST = strtol(argv[12], NULL, 16);
				msr_value.msr1320.LLC_STREAM_MAX_DISTANCE = strtol(argv[13], NULL, 16);
				msr_value.msr1320.LLC_STREAM_DISABLE = strtol(argv[14], NULL, 16);
				msr_value.msr1320.LLC_INIT_PREF_PEND = strtol(argv[15], NULL, 16);
				msr_value.msr1320.LLC_MAX_PREF_PEND = strtol(argv[16], NULL, 16);
				msr_value.msr1320.LLCPREF_LQ_THRESHOLD = strtol(argv[17], NULL, 16);
				msr_value.msr1320.LLC_STREAM_XQ_THRESHOLD = strtol(argv[18], NULL, 16);
			break;

			case 0x1321: //updated to DKT
				msr_value.msr1321.L2_STREAM_AMP_CREATE_IL1 = strtol(argv[3], NULL, 16);
				msr_value.msr1321.ALTERNATIVE_ISIDE_PREFETCH_ENABLE = strtol(argv[4], NULL, 16);
				msr_value.msr1321.ALTERNATIVE_KICKSTART_PREFETCHES = strtol(argv[5], NULL, 16);
				msr_value.msr1321.ALTERNATIVE_REGULAR_PREFETCHES = strtol(argv[6], NULL, 16);
				msr_value.msr1321.DTP_ENABLE = strtol(argv[7], NULL, 16);
				msr_value.msr1321.L2_STREAM_DEMAND_DENSITY = strtol(argv[8], NULL, 16);
				msr_value.msr1321.L2_STREAM_DEMAND_DENSITY_OVR = strtol(argv[9], NULL, 16);
				msr_value.msr1321.CREATE_PMH = strtol(argv[10], NULL, 16);
				msr_value.msr1321.L2_DISABLE_NEXT_LINE_PREFETCH = strtol(argv[11], NULL, 16);
				msr_value.msr1321.L2_LLC_STREAM_AMP_XQ_THRESHOLD = strtol(argv[12], NULL, 16);
				msr_value.msr1321.RESTORE_L2PREFETCHER_DEFAULTS = strtol(argv[13], NULL, 16);
			break;

			case 0x1322:  //updated to DKT
				msr_value.msr1322.LLPREF_THROTTLE_ISSUE_FACTOR = strtol(argv[3], NULL, 16);
				msr_value.msr1322.LLPREF_THROTTLE_HIT_FACTOR = strtol(argv[4], NULL, 16);
				msr_value.msr1322.LLPREF_UNTHROTTLE_ISSUE_FACTOR = strtol(argv[5], NULL, 16);
				msr_value.msr1322.LLPREF_UNTHROTTLE_HIT_FACTOR = strtol(argv[6], NULL, 16);
				msr_value.msr1322.LLC_STREAM_DEMAND_DENSITY = strtol(argv[7], NULL, 16);
				msr_value.msr1322.LLC_STREAM_DEMAND_DENSITY_OVR = strtol(argv[8], NULL, 16);
				msr_value.msr1322.L2_AMP_CONFIDENCE_DPT0 = strtol(argv[9], NULL, 16);
				msr_value.msr1322.L2_AMP_CONFIDENCE_DPT1 = strtol(argv[10], NULL, 16);
				msr_value.msr1322.L2_AMP_CONFIDENCE_DPT2 = strtol(argv[11], NULL, 16);
				msr_value.msr1322.L2_AMP_CONFIDENCE_DPT3 = strtol(argv[12], NULL, 16);
				msr_value.msr1322.L2_LLC_STREAM_DEMAND_DENSITY_XQ = strtol(argv[13], NULL, 16);
			break;

			case 0x1323:  //same up to DKT
				msr_value.msr1323.L2_STREAM_AMP_CREATE_SWPFRFO = strtol(argv[3], NULL, 16);
				msr_value.msr1323.L2_STREAM_AMP_CREATE_SWPFRD = strtol(argv[4], NULL, 16);
				msr_value.msr1323.L2_STREAM_AMP_CREATE_HWPFD = strtol(argv[5], NULL, 16);
				msr_value.msr1323.L2_STREAM_AMP_CREATE_DRFO = strtol(argv[6], NULL, 16);
				msr_value.msr1323.STABILIZE_PREF_ON_SWPFRFO = strtol(argv[7], NULL, 16);
				msr_value.msr1323.STABILIZE_PREF_ON_SWPFRD = strtol(argv[8], NULL, 16);
				msr_value.msr1323.STABILIZE_PREF_ON_IL1 = strtol(argv[9], NULL, 16);
				msr_value.msr1323.STABILIZE_PREF_ON_HWPFD = strtol(argv[10], NULL, 16);
				msr_value.msr1323.STABILIZE_PREF_ON_DRFO = strtol(argv[11], NULL, 16);
				msr_value.msr1323.L2_STREAM_AMP_CREATE_PFNPP = strtol(argv[12], NULL, 16);
				msr_value.msr1323.L2_STREAM_AMP_CREATE_PFIPP = strtol(argv[13], NULL, 16);
				msr_value.msr1323.STABILIZE_PREF_ON_PFNPP = strtol(argv[14], NULL, 16);
				msr_value.msr1323.STABILIZE_PREF_ON_PFIPP = strtol(argv[15], NULL, 16);
			break;

			case 0x1324: //same up to DKT
				msr_value.msr1324.L1_HOMELESS_THRESHOLD = strtol(argv[3], NULL, 16);
			break;

			case 0x1325: //new with CMT
				msr_value.msr1325.STR_WINDOW_SIZE = strtol(argv[3], NULL, 16);
				msr_value.msr1325.KICK_START = strtol(argv[4], NULL, 16);
				msr_value.msr1325.LQ_THRESHOLD = strtol(argv[5], NULL, 16);
				msr_value.msr1325.AMP_MAX_TRIV_REC_PREF = strtol(argv[6], NULL, 16);
				msr_value.msr1325.AMP_DELTA_CNT_INC_VAL = strtol(argv[7], NULL, 16);
				msr_value.msr1325.PREFACC_COUNTER_THRESHOLD = strtol(argv[8], NULL, 16);
				msr_value.msr1325.PREFACC_DISABLE_LQ2_THRESHOLD = strtol(argv[9], NULL, 16);
				msr_value.msr1325.PREFACC_DISABLE_XQ_THRESHOLD = strtol(argv[10], NULL, 16);
				msr_value.msr1325.PREFACC_COUNTER_INCR = strtol(argv[11], NULL, 16);
				msr_value.msr1325.PREFACC_COUNT = strtol(argv[12], NULL, 16);
				msr_value.msr1325.PREFACC_DISABLE = strtol(argv[13], NULL, 16);
			break;

			case 0x1326: //new with CMT
				msr_value.msr1326.AMP_RECUR_PREFETCHMINCOUNT = strtol(argv[3], NULL, 16);
			break;

			case 0x1327: //new with CMT
				msr_value.msr1327.RMT_INC = strtol(argv[3], NULL, 16);
				msr_value.msr1327.RMT_DEC = strtol(argv[4], NULL, 16);
				msr_value.msr1327.RMT_POSITIVE_SATURATION = strtol(argv[5], NULL, 16);
				msr_value.msr1327.RMT_POSITIVE_UPPER_QUARTILE = strtol(argv[6], NULL, 16);
				msr_value.msr1327.RMT_POSITIVE_LOWER_QUARTILE = strtol(argv[7], NULL, 16);
				msr_value.msr1327.RMT_NEGATIVE_LOWER_QUARTILE = strtol(argv[8], NULL, 16);
				msr_value.msr1327.RMT_NEGATIVE_UPPER_QUARTILE = strtol(argv[9], NULL, 16);
				msr_value.msr1327.RMT_NEGATIVE_SATURATION = strtol(argv[10], NULL, 16);
				msr_value.msr1327.RMT_EN = strtol(argv[11], NULL, 16);
			break;

			default:
				printf("Incorrect MSR ID\n");
			break;
		}

		printf("MSR 0x%x value 0x%lx:\n", msr_id, msr_value.v);
	}
	else printf("Select either a) convert from MSR or b) convert to MSR");

	printf("done\n");

	return 0;
}
