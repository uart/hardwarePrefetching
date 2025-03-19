#ifndef __KERNEL_COMMON_H__
#define __KERNEL_COMMON_H__

#include "../include/atom_msr.h"
#include <linux/types.h>

#define MAX_NUM_CORES (512)

// Hard-coded values for initial testing (e.g., GRR with 24 cores)
#define FIRST_CORE (6)      // First active core (adjust as needed)
#define ACTIVE_CORES (8)    // Number of active cores in this test
#define CORE_IN_MODULE ((core_id - FIRST_CORE) % 4)  // Core index within module
#define MODULE_ID ((core_id - FIRST_CORE) / 4)       // Module ID for core
#define DPF_API_VERSION (1)   // API version for user-kernel communication

// PMU counters: 7 total (cycles, instructions, and 5 additional events)
#define PMU_COUNTERS (7)

// MSR definitions: 6 MSRs monitored
#define NR_OF_MSR (6)
#define MSR_1320_INDEX (0)  // Index for MSR 0x1320 (e.g., L2_STREAM_AMP_XQ_THRESHOLD)
#define MSR_1321_INDEX (1)  // Index for MSR 0x1321
#define MSR_1322_INDEX (2)  // Index for MSR 0x1322
#define MSR_1323_INDEX (3)  // Index for MSR 0x1323
#define MSR_1324_INDEX (4)  // Index for MSR 0x1324
#define MSR_1A4_INDEX (5)   // Index for MSR 0x1A4 (miscellaneous features)

// Event types for PMU configuration (64-bit event codes including config bits)
#define EVENT_CPU_CLK_UNHALTED_THREAD       (0x00000000004300c0ULL) // Event 0x00, UMask 0xc0, Enable
#define EVENT_INST_RETIRED_ANY_P            (0x00000000004300c2ULL) // Event 0x00, UMask 0xc2, Enable
#define EVENT_MEM_UOPS_RETIRED_ALL_LOADS    (0x00000000004381d0ULL) // Event 0x81, UMask 0xd0, Enable
#define EVENT_MEM_LOAD_UOPS_RETIRED_L2_HIT  (0x00000000004302d1ULL) // Event 0x02, UMask 0xd1, Enable
#define EVENT_MEM_LOAD_UOPS_RETIRED_L3_HIT  (0x00000000004304d1ULL) // Event 0x04, UMask 0xd1, Enable
#define EVENT_MEM_LOAD_UOPS_RETIRED_DRAM_HIT (0x00000000004380d1ULL) // Event 0x80, UMask 0xd1, Enable
#define EVENT_XQ_PROMOTION_ALL              (0x00000000004300f4ULL) // Event 0x00, UMask 0xf4, Enable (custom)

// Enum for PMU metrics (mapped to pmu_result[] indices and event codes)
enum pmu_metrics {
    PERF_MEM_UOPS_RETIRED_ALL_LOADS = 0,    // Index 0: All load uops retired
    PERF_MEM_LOAD_UOPS_RETIRED_L2_HIT,      // Index 1: Load uops retired hitting L2 cache
    PERF_MEM_LOAD_UOPS_RETIRED_L3_HIT,      // Index 2: Load uops retired hitting L3 cache
    PERF_MEM_LOAD_UOPS_RETIRED_DRAM_HIT,    // Index 3: Load uops retired hitting DRAM
    PERF_XQ_PROMOTION_ALL,                  // Index 4: XQ promotion events (hardware-specific)
    PERF_CPU_CLK_UNHALTED_THREAD,           // Index 5: CPU cycles when thread is not halted
    PERF_INST_RETIRED_ANY_P                 // Index 6: Instructions retired
};

// Constants
#define MAX_MSG_SIZE (1024) // Maximum size of a message
#define MAX_CORES (64)      // Maximum number of CPU cores (for procfs or user-space limits)
#define MAX_WEIGHT (100)    // Maximum weight value for core priority

// Enum for message types between user space and kernel module
enum dpf_msg_type {
    DPF_MSG_INIT = 0,       // API version negotiation
    DPF_MSG_CORE_RANGE = 1, // Core range configuration
    DPF_MSG_DDRBW_SET = 2,  // DDR bandwidth setting
    DPF_MSG_CORE_WEIGHT = 3,// Core weight assignment
    DPF_MSG_TUNING = 4,     // Tuning control
    DPF_MSG_MSR_READ = 5,   // Read MSR values
    DPF_MSG_PMU_READ = 6    // Read PMU values
};

// Message structures
struct dpf_msg_header {
    __u32 type;         // Message type (from dpf_msg_type)
    __u32 payload_size; // Size of payload following the header
};

struct dpf_req_init {
    struct dpf_msg_header header;
};

struct dpf_resp_init {
    struct dpf_msg_header header;
    __u32 version;      // Returned API version
};

struct dpf_core_range {
    struct dpf_msg_header header;
    __u32 core_start;   // First core in range
    __u32 core_end;     // Last core in range
};

struct dpf_resp_core_range {
    struct dpf_msg_header header;
    __u32 core_start;   // Confirmed first core
    __u32 core_end;     // Confirmed last core
    __u32 thread_count; // Number of threads (cores) in range
};

struct dpf_core_weight {
    struct dpf_msg_header header;
    __u32 count;        // Number of weights
    __u32 weights[];    // Flexible array of core weights
};

struct dpf_resp_core_weight {
    struct dpf_msg_header header;
    __u32 count;            // Number of confirmed weights
    __u32 confirmed_weights[]; // Flexible array of confirmed weights
};

struct dpf_req_tuning {
    struct dpf_msg_header header;
    __u32 enable;       // Enable tuning (non-zero) or disable (0)
};

struct dpf_resp_tuning {
    struct dpf_msg_header header;
    __u32 status;       // Tuning status (e.g., enabled/disabled)
};

struct dpf_ddrbw_set {
    struct dpf_msg_header header;
    __u32 set_value;    // DDR bandwidth value to set (MB/s)
};

struct dpf_resp_ddrbw_set {
    struct dpf_msg_header header;
    __u32 confirmed_value; // Confirmed DDR bandwidth value
};

struct dpf_msr_read {
    struct dpf_msg_header header;
    __u32 core_id;      // Core ID to read MSRs from
};

struct dpf_resp_msr_read {
    struct dpf_msg_header header;
    __u64 msr_values[NR_OF_MSR]; // Array of MSR values
};

struct dpf_pmu_read {
    struct dpf_msg_header header;
    __u32 core_id;      // Core ID to read PMU from
};

struct dpf_resp_pmu_read {
    struct dpf_msg_header header;
    __u64 pmu_values[PMU_COUNTERS]; // Array of PMU counter values
};

// Core state structure
struct core_state_s {
    uint64_t pmu_result[PMU_COUNTERS]; // Delta since last PMU read (mapped to pmu_metrics)
    union msr_u pf_msr[NR_OF_MSR];     // MSR values (0x1320...0x1A4)
    int pf_msr_dirty;                  // 0 = no update needed, 1 = update needed
    int core_disabled;                 // 1 = core disabled, 0 = enabled
};

// External declarations
extern struct core_state_s corestate[MAX_NUM_CORES];
extern int ddr_bw_target;

// Function prototypes
int is_msr_dirty(int core_id);
int msr_set_dirty(int core_id);
int msr_load(int core_id);
int msr_update(int core_id);
int pmu_update(int core_id);

int msr_set_l2xq(int core_id, int value);
int msr_get_l2xq(int core_id);
int msr_set_l3xq(int core_id, int value);
int msr_get_l3xq(int core_id);

#endif /* __KERNEL_COMMON_H__ */
