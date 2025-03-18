#ifndef __KERNEL_COMMON__
#define __KERNEL_COMMON__

#include <linux/types.h>

#include "../include/atom_msr.h"

#define MAX_NUM_CORES (512)

//This is a first test, so values are hard-coded. This will only work on GRR with 24 cores
#define FIRST_CORE (6)    //this should actually be the first of active cores, not 0
#define ACTIVE_CORES (8)
#define CORE_IN_MODULE ((core_id - FIRST_CORE) % 4)
#define MODULE_ID ((core_id - FIRST_CORE) / 4)
#define DPF_API_VERSION (1)

//PMU counters include cycles, instructions and 5 additional events
#define PMU_COUNTERS (7)


#define NR_OF_MSR (6)
#define MSR_1320_INDEX (0)
#define MSR_1321_INDEX (1)
#define MSR_1322_INDEX (2)
#define MSR_1323_INDEX (3)
#define MSR_1324_INDEX (4)
#define MSR_1A4_INDEX (5)


#define MAX_MSG_SIZE (1024) // Maximum size of a message
#define MAX_CORES (64)	    // Maximum number of CPU cores
#define MAX_WEIGHT (100)    // Maximum weight value

// Enum for message types
enum dpf_msg_type {
	DPF_MSG_INIT = 0,	 // API version negotiation
	DPF_MSG_CORE_RANGE = 1,	 // Core range configuration
	DPF_MSG_DDRBW_SET = 2,	 // DDR bandwidth setting
	DPF_MSG_CORE_WEIGHT = 3, // Core weight assignment
	DPF_MSG_TUNING = 4, 	// Tuning control
};

// Common message header
struct dpf_msg_header {
	__u32 type;	    // Message type identifier
	__u32 payload_size; // Size of the entire message including the header
};

// Initialization request
struct dpf_req_init {
	struct dpf_msg_header header;
};

// Initialization response
struct dpf_resp_init {
	struct dpf_msg_header header;
	__u32 version; // API version supported by the kernel
};

// Core range configuration request
struct dpf_core_range {
	struct dpf_msg_header header;
	__u32 core_start; // Starting CPU core
	__u32 core_end;	  // Ending CPU core
};

// Core range configuration response
struct dpf_resp_core_range {
	struct dpf_msg_header header;
	__u32 core_start;   // Confirmed starting core
	__u32 core_end;	    // Confirmed ending core
	__u32 thread_count; // Actual number of available threads
};

// DDR bandwidth setting request
struct dpf_ddrbw_set {
	struct dpf_msg_header header;
	__u32 set_value; // Desired DDR bandwidth in MB/s
};

// DDR bandwidth setting response
struct dpf_resp_ddrbw_set {
	struct dpf_msg_header header;
	__u32 confirmed_value; // Confirmed DDR bandwidth in MB/s
};

// Core weight assignment request
struct dpf_core_weight {
	struct dpf_msg_header header;
	__u32 count;	 // Number of weights being set
	__u32 weights[]; // Array of weight values
};

// Core weight assignment response
struct dpf_resp_core_weight {
	struct dpf_msg_header header;
	__u32 count;		   // Number of confirmed weights
	__u32 confirmed_weights[]; // Array of confirmed weight values
};

// Tuning control request
struct dpf_req_tuning {
	struct dpf_msg_header header;
	__u32 enable; // Start tuning (1) or stop tuning (0)
};

// Tuning control response
struct dpf_resp_tuning {
	struct dpf_msg_header header;
	__u32 status; // Start tuning (1) or stop tuning (0)
};

struct core_state_s {
	uint64_t pmu_result[PMU_COUNTERS]; //delta since last read
	union msr_u pf_msr[NR_OF_MSR]; //msr values, 0... --> 0x1320...
	int pf_msr_dirty; //0 if no update is needed, 1 if update is neeed
	int core_disabled;  //set to 1 for all cores that should be disabled
};

extern struct core_state_s corestate[MAX_NUM_CORES];
extern int ddr_bw_target;


int is_msr_dirty(int core_id);
int msr_set_dirty(int core_id);
int msr_load(int core_id);
int msr_update(int core_id);
int pmu_update(int core_id);

int msr_set_l2xq(int core_id, int value);
int msr_get_l2xq(int core_id);
int msr_set_l3xq(int core_id, int value);
int msr_get_l3xq(int core_id);

#endif

