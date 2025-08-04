#ifndef __KERNEL_API_H__
#define __KERNEL_API_H__

// Forward declarations and constants needed for the API structures
#define PMU_COUNTERS (7)
#define NR_OF_MSR (6)

#define DPF_API_VERSION (1)   // API version for user-kernel communication

#include "kernel_common.h"

// Common message header
struct dpf_msg_header_s {
    __u32 type;         // Message type (from dpf_msg_type)
    __u32 payload_size; // Size of payload following the header
};

// Request structure for API version
struct dpf_req_init_s {
    struct dpf_msg_header_s header;
};

// Response structure for API version
struct dpf_resp_init_s {
    struct dpf_msg_header_s header;
    __u32 version;      // Returned API version
};

// Request structure for core range
struct dpf_core_range_s {
    struct dpf_msg_header_s header;
    __u32 core_start;   // First core in range
    __u32 core_end;     // Last core in range
};

// Response structure for core range
struct dpf_resp_core_range_s {
    struct dpf_msg_header_s header;
    __u32 core_start;   // Confirmed first core
    __u32 core_end;     // Confirmed last core
    __u32 thread_count; // Number of threads (cores) in range
};

// Request structure for core weights
struct dpf_core_weight_s {
    struct dpf_msg_header_s header;
    __u32 count;        // Number of weights
    __u32 weights[];    // Flexible array of core weights
};

// Response structure for core weights
struct dpf_resp_core_weight_s {
    struct dpf_msg_header_s header;
    __u32 count;            // Number of confirmed weights
    __u32 confirmed_weights[]; // Flexible array of confirmed weights
};

// Request structure for tuning
struct dpf_req_tuning_s {
    struct dpf_msg_header_s header;
    __u32 enable;       // Enable tuning (non-zero) or disable (0)
    __u32 tunealg;      // Tuning algorithm selection
    __u32 aggr;         // Aggressiveness factor (scaled)
};

// Response structure for tuning
struct dpf_resp_tuning_s {
    struct dpf_msg_header_s header;
    __u32 status;       // Tuning status (e.g., enabled/disabled)
    __u32 confirmed_tunealg;  // Confirmed tuning algorithm
    __u32 confirmed_aggr;     // Confirmed aggressiveness factor
};

// Request structure for DDR bandwidth setting
struct dpf_ddrbw_set_s {
    struct dpf_msg_header_s header;
    __u32 set_value;    // DDR bandwidth value to set (MB/s)
};

// Response structure for DDR bandwidth setting
struct dpf_resp_ddrbw_set_s {
    struct dpf_msg_header_s header;
    __u32 confirmed_value; // Confirmed DDR bandwidth value
};

// Request structure for MSR read
struct dpf_msr_read_s {
    struct dpf_msg_header_s header;
    __u32 core_id;      // Core ID to read MSRs from
};

// Response structure for MSR read
struct dpf_resp_msr_read_s {
    struct dpf_msg_header_s header;
    __u64 msr_values[NR_OF_MSR]; // Array of MSR values
};

// Request structure for PMU read
struct dpf_pmu_read_s {
    struct dpf_msg_header_s header;
    __u32 core_id;      // Core ID to read PMU from
};

// Response structure for PMU read
struct dpf_resp_pmu_read_s {
    struct dpf_msg_header_s header;
    __u64 pmu_values[PMU_COUNTERS]; // Array of PMU counter values
};

// Request structure for DDR configuration
struct dpf_ddr_config_s {
	struct dpf_msg_header_s header;
	__u64 bar_address; // BAR address for DDR config space
	__u32 cpu_type;    // CPU type (e.g., DDR_CLIENT, DDR_GRR_SRF)
	__u32 num_controllers; // Number of DDR controllers
};

// Response structure for DDR configuration
struct dpf_resp_ddr_config_s {
	struct dpf_msg_header_s header;
	__u64 confirmed_bar;  // Confirmed BAR address
	__u32 confirmed_type; // Confirmed CPU type
};

// Request structure for reading DDR bandwidth
struct dpf_ddr_bw_read_s {
	struct dpf_msg_header_s header;
};

// Response structure for reading DDR bandwidth
struct dpf_resp_ddr_bw_read_s {
	struct dpf_msg_header_s header;
	uint64_t read_bw;
	uint64_t write_bw;
};

// Global tuning algorithm settings, these should be set through
// the dpf_tuning_control API.
extern int tune_alg;
extern int aggr;

// Kernel API function prototypes
int api_init(void);
int api_core_range(struct dpf_core_range_s *req_data);
int api_core_weight(void *req_data);
int api_tuning(struct dpf_req_tuning_s *req_data);
int api_ddrbw_set(struct dpf_ddrbw_set_s *req_data);
int api_msr_read(struct dpf_msr_read_s *req_data);
int api_pmu_read(struct dpf_pmu_read_s *req_data);
int api_ddr_config(struct dpf_ddr_config_s *req_data);
int api_ddr_bw_read(struct dpf_ddr_bw_read_s *req_data);

#endif // __KERNEL_API_H__
