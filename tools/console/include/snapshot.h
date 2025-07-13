#ifndef __SNAPSHOT_H
#define __SNAPSHOT_H

extern int core_first;
extern int core_last;

#define NUM_PMU (7)
#define NUM_MSR (6)
#define MAX_CORES (64)


enum tuning_state_t {
    TUNING_DISABLED = 0,
    TUNING_ENABLED = 1
};

// PMU and MSR values for each core
struct core_metrics {
	int core_id;
	uint64_t pmu_values[NUM_PMU];
	uint64_t msr_values[NUM_MSR];
};

// hold all core and system metrics
// including PMU and MSR values
struct dpf_console_snapshot {
	struct core_metrics cores[MAX_CORES];
	int core_count;

	uint64_t ddr_read_bw;
	uint64_t ddr_write_bw;

	int tuning_enabled;
};

// populates the snapshot struct with all core and system metrics
int update_console_snapshot(struct dpf_console_snapshot *snapshot);

#endif /* __SNAPSHOT_H */