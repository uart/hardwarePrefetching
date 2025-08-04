#include <stdio.h>

#include "tuning.h"
#include "user_api.h"

// start kernel tuning
// accepts 1 to start tuning, 0 to stop tuning
// returns 0 on success, -1 on failure
#define DEFAULT_TUNING_ALG 0
#define DEFAULT_AGGRESSION_FACTOR 1.0

int start_tuning(int tuning_enabled)
{
	int ret;

	ret = kernel_tuning_control(tuning_enabled, DEFAULT_TUNING_ALG, DEFAULT_AGGRESSION_FACTOR);
	if (ret < 0) {
		fprintf(stderr, "Failed to start tuning: %d\n", ret);
		return -1;
	}

	return 0;
}

// stop kernel tuning
// accepts 1 to start tuning, 0 to stop tuning
// returns 0 on success, -1 on failure
int stop_tuning(int tuning_enabled)
{
	int ret;

	ret = kernel_tuning_control(tuning_enabled, DEFAULT_TUNING_ALG, DEFAULT_AGGRESSION_FACTOR);
	if (ret < 0) {
		fprintf(stderr, "Failed to stop tuning: %d\n", ret);
		return -1;
	}

	return 0;
}
