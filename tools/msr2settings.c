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
		printf(" a: convert msr value to settings, ./msr2settings a 0x1320 0x700007e041000018\n");
		printf(" b: convert settings to msr value, ./msr2settings b 0x1320 0x18 0x10 0x01 0x3f 0x00 0x1c\n");

		return -1;
	}

	msr_id = strtol(argv[2], NULL, 16);

	if(argv[1][0] == 'a'){
		msr_value.v = strtoll(argv[3], NULL, 16);
		printf("MSR Value 0x%lx:\n", msr_value.v);

		switch(msr_id){
			case 0x1320:
				printf(" L2_STREAM_AMP_XQ_THRESHOLD: 0x%02x\n", msr_value.msr1320.L2_STREAM_AMP_XQ_THRESHOLD);
				printf(" L2_STREAM_MAX_DISTANCE: 0x%02x\n", msr_value.msr1320.L2_STREAM_MAX_DISTANCE);
				printf(" L2_AMP_DISABLE_RECURSION: 0x%02x\n", msr_value.msr1320.L2_AMP_DISABLE_RECURSION);
				printf(" LLC_STREAM_MAX_DISTANCE: 0x%02x\n", msr_value.msr1320.LLC_STREAM_MAX_DISTANCE);
				printf(" LLC_STREAM_DISABLE: 0x%02x\n", msr_value.msr1320.LLC_STREAM_DISABLE);
				printf(" LLC_STREAM_XQ_THRESHOLD: 0x%02x\n", msr_value.msr1320.LLC_STREAM_XQ_THRESHOLD);
			break;

			case 0x1321:
				printf(" L2_STREAM_AMP_CREATE_IL1: 0x%02x\n", msr_value.msr1321.L2_STREAM_AMP_CREATE_IL1);
				printf(" L2_STREAM_DEMAND_DENSITY: 0x%02x\n", msr_value.msr1321.L2_STREAM_DEMAND_DENSITY);
				printf(" L2_STREAM_DEMAND_DENSITY_OVR: 0x%02x\n", msr_value.msr1321.L2_STREAM_DEMAND_DENSITY_OVR);
				printf(" L2_DISABLE_NEXT_LINE_PREFETCH: 0x%02x\n", msr_value.msr1321.L2_DISABLE_NEXT_LINE_PREFETCH);
				printf(" L2_LLC_STREAM_AMP_XQ_THRESHOLD: 0x%02x\n", msr_value.msr1321.L2_LLC_STREAM_AMP_XQ_THRESHOLD);
			break;

			case 0x1322:
				printf(" LLC_STREAM_DEMAND_DENSITY: 0x%02x\n", msr_value.msr1322.LLC_STREAM_DEMAND_DENSITY);
				printf(" LLC_STREAM_DEMAND_DENSITY_OVR: 0x%02x\n", msr_value.msr1322.LLC_STREAM_DEMAND_DENSITY_OVR);
				printf(" L2_AMP_CONFIDENCE_DPT0: 0x%02x\n", msr_value.msr1322.L2_AMP_CONFIDENCE_DPT0);
				printf(" L2_AMP_CONFIDENCE_DPT1: 0x%02x\n", msr_value.msr1322.L2_AMP_CONFIDENCE_DPT1);
				printf(" L2_AMP_CONFIDENCE_DPT2: 0x%02x\n", msr_value.msr1322.L2_AMP_CONFIDENCE_DPT2);
				printf(" L2_AMP_CONFIDENCE_DPT3: 0x%02x\n", msr_value.msr1322.L2_AMP_CONFIDENCE_DPT3);
				printf(" L2_LLC_STREAM_DEMAND_DENSITY_XQ: 0x%02x\n", msr_value.msr1322.L2_LLC_STREAM_DEMAND_DENSITY_XQ);
			break;

			case 0x1323:
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

			case 0x1324:
				printf(" L1_HOMELESS_THRESHOLD: 0x%02x\n", msr_value.msr1324.L1_HOMELESS_THRESHOLD);
			break;

			default:
				printf("Incorrect MSR ID\n");
			break;
		}
	}
	else if(argv[1][0] == 'b'){
		msr_value.v = 0;

		switch(msr_id){
			case 0x1320:
				msr_value.msr1320.L2_STREAM_AMP_XQ_THRESHOLD = strtol(argv[3], NULL, 16);
				msr_value.msr1320.L2_STREAM_MAX_DISTANCE = strtol(argv[4], NULL, 16);
				msr_value.msr1320.L2_AMP_DISABLE_RECURSION = strtol(argv[5], NULL, 16);
				msr_value.msr1320.LLC_STREAM_MAX_DISTANCE = strtol(argv[6], NULL, 16);
				msr_value.msr1320.LLC_STREAM_DISABLE = strtol(argv[7], NULL, 16);
				msr_value.msr1320.LLC_STREAM_XQ_THRESHOLD = strtol(argv[8], NULL, 16);
			break;

			case 0x1321:
				msr_value.msr1321.L2_STREAM_AMP_CREATE_IL1 = strtol(argv[3], NULL, 16);
				msr_value.msr1321.L2_STREAM_DEMAND_DENSITY = strtol(argv[4], NULL, 16);
				msr_value.msr1321.L2_STREAM_DEMAND_DENSITY_OVR = strtol(argv[5], NULL, 16);
				msr_value.msr1321.L2_DISABLE_NEXT_LINE_PREFETCH = strtol(argv[6], NULL, 16);
				msr_value.msr1321.L2_LLC_STREAM_AMP_XQ_THRESHOLD = strtol(argv[7], NULL, 16);
			break;

			case 0x1322:
				msr_value.msr1322.LLC_STREAM_DEMAND_DENSITY = strtol(argv[3], NULL, 16);
				msr_value.msr1322.LLC_STREAM_DEMAND_DENSITY_OVR = strtol(argv[4], NULL, 16);
				msr_value.msr1322.L2_AMP_CONFIDENCE_DPT0 = strtol(argv[5], NULL, 16);
				msr_value.msr1322.L2_AMP_CONFIDENCE_DPT1 = strtol(argv[6], NULL, 16);
				msr_value.msr1322.L2_AMP_CONFIDENCE_DPT2 = strtol(argv[7], NULL, 16);
				msr_value.msr1322.L2_AMP_CONFIDENCE_DPT3 = strtol(argv[8], NULL, 16);
				msr_value.msr1322.L2_LLC_STREAM_DEMAND_DENSITY_XQ = strtol(argv[9], NULL, 16);
			break;

			case 0x1323:
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

			case 0x1324:
				msr_value.msr1324.L1_HOMELESS_THRESHOLD = strtol(argv[3], NULL, 16);
			break;

			default:
				printf("Incorrect MSR ID\n");
			break;
		}

		printf("MSR Value 0x%lx:\n", msr_value.v);
	}
	else printf("Select either a) convert from MSR or b) convert to MSR");

	printf("done\n");

	return 0;
}
