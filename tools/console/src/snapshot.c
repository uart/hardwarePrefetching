#include <stdio.h>
#include <stdlib.h>

#include "log.h"
#include "metrics.h"
#include "snapshot.h"
#include "tuning.h"

// populate the snapshot struct with all core and system metrics
// accepts a pointer to a dpf_console_snapshot struct
// returns 0 on success, -1 on failure
int update_console_snapshot(struct dpf_console_snapshot *snapshot)
{
	enum tuning_state_t tuning_state;
	struct core_metrics *core_data;
	int core;
	int core_index = 0;

	if (!snapshot) {
		fprintf(stderr, "Snapshot pointer is NULL\n");
		return -1;
	}

	if (core_first < 0 || core_last >= MAX_CORES || core_first > core_last) {
		fprintf(stderr, "Invalid core range: %d to %d\n", core_first,
			core_last);
		return -1;
	}

	if ((core_last - core_first + 1) > MAX_CORES) {
		fprintf(stderr, "Core range exceeds MAX_CORES\n");
		return -1;
	}

	for (core = core_first; core <= core_last; core++) {
		if (core_index >= MAX_CORES) {
			fprintf(stderr, "core index exceeded MAX_CORES\n");
			break;
		}

		// the next core_metrics struct and set core_id
		core_data = &snapshot->cores[core_index];
		core_data->core_id = core;

		if (read_pmu(core, core_data->pmu_values) != 0) {
			fprintf(stderr, "Skipping core %d due to PMU read error\n", core);
			continue;
		}

		if (read_msr(core, core_data->msr_values) != 0) {
			fprintf(stderr, "Skipping core %d due to MSR read error\n", core);
			continue;
		}

		core_index++;
	}

	snapshot->core_count = core_index;

	if (core_index == 0) {
		fprintf(stderr, "No valid core data collected\n");
		return -1;
	}

	if (read_ddr_bw(&snapshot->ddr_read_bw, &snapshot->ddr_write_bw) != 0) {
		fprintf(stderr, "Error reading DDR bandwidth\n");
		// set to zero if read fails
		snapshot->ddr_read_bw = 0;
		snapshot->ddr_write_bw = 0;
		return -1;
	}

	tuning_state = snapshot->tuning_enabled;
	if (tuning_state == 1) {
		start_tuning(1);
		return 0;
	} else if (tuning_state == 0) {
		stop_tuning(0);
		return 0;
	}

	return 0;
}
