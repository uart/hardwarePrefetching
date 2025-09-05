#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/cpumask.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

// Global variables for PMU logging
char *pmu_log_buffer = NULL;
size_t pmu_log_buffer_size = 0;
size_t pmu_log_data_size = 0;
bool pmu_logging_active = false;

#define TIMER_INTERVAL_SEC 1
#include <linux/io.h>
#include <linux/ioport.h>

#include "kernel_common.h"
#include "kernel_api.h"
#include "kernel_pmu_ddr.h"

// External variables from kernel_dpf.c
extern bool keep_running;
extern struct hrtimer monitor_timer;
extern ktime_t kt_period;
extern char *proc_buffer;
extern size_t proc_buffer_size;
extern __u64 ddr_bar_address;
extern cpumask_t enabled_cpus;
extern __u32 ddr_cpu_type;
extern __u32 num_ddr_controllers;
extern struct ddr_s ddr;

// Handle initialization request and response to the user space
// Arguments: None
// Returns: 0 on success, -ENOMEM on failure
int api_init(void)
{
	struct dpf_resp_init_s *resp;

	resp = kmalloc(sizeof(struct dpf_resp_init_s), GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	resp->header.type = DPF_MSG_INIT;
	resp->header.payload_size = sizeof(struct dpf_resp_init_s);
	resp->version = DPF_API_VERSION;

	kfree(proc_buffer);

	proc_buffer = (char *)resp;
	proc_buffer_size = sizeof(struct dpf_resp_init_s);

	pr_info("%s: Initialized with version %d\n",
	       __func__, resp->version);

	return 0;
}

// Handle core range configuration request and response to the user space
// It accepts a request to specify the range of cores to monitor
// returns 0 on success, -ENOMEM on failure, -EINVAL on invalid input
int api_core_range(struct dpf_core_range_s *req_data)
{
	struct dpf_core_range_s *req = req_data;
	struct dpf_resp_core_range_s *resp;
	int core_id;

	// Range checks to validate input parameters
	pr_info("%s: Received core range request: start=%d, end=%d\n",
	       __func__, req->core_start, req->core_end);
	
	// Check that core_start is non-negative
	if (req->core_start < 0) {
		pr_err("%s: Invalid core_start value %d (must be >= 0)\n", 
		       __func__, req->core_start);
		return -EINVAL;
	}
	
	// Check that core_end is within system limits
	if (req->core_end >= MAX_NUM_CORES) {
		pr_err("%s: Invalid core_end value %d (must be < %d)\n", 
		       __func__, req->core_end, MAX_NUM_CORES);
		return -EINVAL;
	}
	
	// Check that core_start <= core_end
	if (req->core_start > req->core_end) {
		pr_err("%s: Invalid core range: start=%d is greater than end=%d\n", 
		       __func__, req->core_start, req->core_end);
		return -EINVAL;
	}
	
	// Check that the range isn't excessive (optional, adjust as needed)
	if ((req->core_end - req->core_start + 1) > MAX_NUM_CORES) {
		pr_err("%s: Core range too large: %d cores requested, max is %d\n", 
		       __func__, req->core_end - req->core_start + 1, MAX_NUM_CORES);
		return -EINVAL;
	}

	resp = kmalloc(sizeof(struct dpf_resp_core_range_s), GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	resp->header.type = DPF_MSG_CORE_RANGE;
	resp->header.payload_size = sizeof(struct dpf_resp_core_range_s);
	resp->core_start = req->core_start;
	resp->core_end = req->core_end;
	resp->thread_count = req->core_end - req->core_start + 1;

	sys_first_core = req->core_start;
	sys_active_cores = (req->core_end + 1) - req->core_start;

	cpumask_clear(&enabled_cpus); 
	pr_info("api_core_range: cleared enabled_cpus mask");

	// Only process cores within the valid range to avoid unnecessary iterations
	for (core_id = 0; core_id < MAX_NUM_CORES; core_id++) {
		// Mark cores as enabled/disabled based on range
		corestate[core_id].core_disabled =
		    (core_id < req->core_start || core_id > req->core_end);
		
		// Configure and enable valid cores
		if (!corestate[core_id].core_disabled) {
			// Verify the core exists on the system before configuring
			if (cpu_online(core_id)) {
				configure_pmu(core_id);
				cpumask_set_cpu(core_id, &enabled_cpus);
				pr_info("api_core_range: added core %d to enabled_cpus mask", core_id);
			} else {
				// Core doesn't exist, mark it as disabled
				corestate[core_id].core_disabled = 1;
				pr_warn("api_core_range: core %d not available on this system", core_id);
			}
		}
	}
	kfree(proc_buffer);

	proc_buffer = (char *)resp;
	proc_buffer_size = sizeof(struct dpf_resp_core_range_s);

	pr_info("%s: Processed core range request: start=%d, end=%d, thread_count=%d\n",
	       __func__, resp->core_start, resp->core_end, resp->thread_count);

	return 0;
}

// Handle core weight configuration request and response to the user space
// It accepts a request to specify the weight of each core
// returns 0 on success, -ENOMEM on failure
int api_core_weight(void *req_data)
{
	struct dpf_core_weight_s *req = req_data;
	struct dpf_resp_core_weight_s *resp;
	size_t resp_size;

	if (!req_data)
		return -EINVAL;

	pr_info("%s: Received core weight request with count=%d\n",
	       __func__, req->count);

	resp_size = sizeof(struct dpf_resp_core_weight_s) + req->count *
							      sizeof(__u32);
	resp = kmalloc(resp_size, GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	resp->header.type = DPF_MSG_CORE_WEIGHT;
	resp->header.payload_size = resp_size;
	resp->count = req->count;
	memcpy(resp->confirmed_weights, req->weights,
	       req->count * sizeof(__u32));

	kfree(proc_buffer);

	proc_buffer = (char *)resp;
	proc_buffer_size = resp_size;

	pr_info("%s: Processed core weight request with count=%d\n",
	       __func__, resp->count);

	return 0;
}

// Handle tuning request and response to the user space
// It accepts a request to enable or disable the monitoring
// returns 0 on success, -ENOMEM on failure
int api_tuning(struct dpf_req_tuning_s *req_data)
{
	struct dpf_req_tuning_s *req = req_data;
	struct dpf_resp_tuning_s *resp;
	int core_id;

	resp = kmalloc(sizeof(struct dpf_resp_tuning_s), GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	resp->header.type = DPF_MSG_TUNING;
	resp->header.payload_size = sizeof(struct dpf_resp_tuning_s);
	resp->status = req->enable;

	// Validate aggressiveness factor range
	if (req->aggr < MIN_AGGR || req->aggr > MAX_AGGR) {
		pr_warn("Aggressiveness factor out of range (%d-%d): %d\n",
		       MIN_AGGR, MAX_AGGR, req->aggr);
		// If out of range, clamp to valid range
		req->aggr = req->aggr < MIN_AGGR ? MIN_AGGR : MAX_AGGR;
	}

	// Store tuning algorithm and aggressiveness factor
	tune_alg = req->tunealg;
	aggr = req->aggr;

	// Set response values
	resp->confirmed_tunealg = tune_alg;
	resp->confirmed_aggr = aggr;

	pr_info("Tuning control: alg=%d, aggr=%d\n", tune_alg, aggr);

	if (req->enable == 1) {
		// Load current Prefetch MSR settings for enabled
		// cores before starting tuning
		for_each_online_cpu(core_id) {
			if (corestate[core_id].core_disabled == 0 && core_in_module(core_id) == 0) {
				msr_load(core_id);
				pr_info("Loaded MSR for core %d\n", core_id);
			}
		}
		keep_running = true;
		hrtimer_start(&monitor_timer, kt_period, HRTIMER_MODE_REL);
		pr_info("Monitoring enabled\n");
	} else {
		keep_running = false;
		hrtimer_cancel(&monitor_timer);
		pr_info("Monitoring disabled\n");
	}

	kfree(proc_buffer);

	proc_buffer = (char *)resp;
	proc_buffer_size = sizeof(struct dpf_resp_tuning_s);

	return 0;
}

// Handle DDR bandwidth set request and response to the user space
// It accepts a request to set the DDR bandwidth target
// returns 0 on success, -ENOMEM on failure
int api_ddrbw_set(struct dpf_ddrbw_set_s *req_data)
{
	struct dpf_ddrbw_set_s *req = req_data;
	struct dpf_resp_ddrbw_set_s *resp;

	pr_info("Received request with value=%u\n", req->set_value);

	resp = kmalloc(sizeof(struct dpf_resp_ddrbw_set_s), GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	resp->header.type = DPF_MSG_DDRBW_SET;
	resp->header.payload_size = sizeof(struct dpf_resp_ddrbw_set_s);
	resp->confirmed_value = req->set_value;

	ddr_bw_target = req->set_value;

	kfree(proc_buffer);

	proc_buffer = (char *)resp;
	proc_buffer_size = sizeof(struct dpf_resp_ddrbw_set_s);

	pr_info("DDR bandwidth target set to %u MB/s\n", ddr_bw_target);

	return 0;
}

// Handles MSR read request, retrieves MSR values for a core
// returns 0 on success, -ENOMEM on failure
int api_msr_read(struct dpf_msr_read_s *req_data)
{
	struct dpf_msr_read_s *req = req_data;
	struct dpf_resp_msr_read_s *resp;

	pr_info("Received request for core %u\n", req->core_id);

	if (req->core_id >= MAX_NUM_CORES ||
	    corestate[req->core_id].core_disabled) {
		pr_err("Invalid or disabled core %u\n", req->core_id);
		return -EINVAL;
	}

	resp = kmalloc(sizeof(struct dpf_resp_msr_read_s), GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	resp->header.type = DPF_MSG_MSR_READ;
	resp->header.payload_size = sizeof(struct dpf_resp_msr_read_s);

	if (corestate[req->core_id].pf_msr[MSR_1320_INDEX].v == 0)
		msr_load(req->core_id);

	for (int i = 0; i < NR_OF_MSR; i++)
		resp->msr_values[i] = corestate[req->core_id].pf_msr[i].v;

	kfree(proc_buffer);
	proc_buffer = (char *)resp;
	proc_buffer_size = sizeof(struct dpf_resp_msr_read_s);

	pr_info("MSR values retrieved for core %u\n", req->core_id);
	return 0;
}

// Handles PMU read request, retrieves PMU counter values for a core
// returns 0 on success, -ENOMEM on failure
int api_pmu_read(struct dpf_pmu_read_s *req_data)
{
	struct dpf_pmu_read_s *req = req_data;
	struct dpf_resp_pmu_read_s *resp;

	pr_info("Received request for core %u\n", req->core_id);

	if (req->core_id >= MAX_NUM_CORES || corestate[req->core_id].core_disabled) {
		pr_err("Invalid or disabled core %u\n", req->core_id);
		return -EINVAL;
	}

	resp = kmalloc(sizeof(struct dpf_resp_pmu_read_s), GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	resp->header.type = DPF_MSG_PMU_READ;
	resp->header.payload_size = sizeof(struct dpf_resp_pmu_read_s);

	pmu_update(req->core_id);

	for (int i = 0; i < PMU_COUNTERS; i++) {
		resp->pmu_values[i] = corestate[req->core_id].pmu_result[i];
		pr_debug("PMU %d for core %d: %llu\n", i, req->core_id,
			 resp->pmu_values[i]);
	}

	kfree(proc_buffer);
	proc_buffer = (char *)resp;
	proc_buffer_size = sizeof(struct dpf_resp_pmu_read_s);

	pr_info("PMU values retrieved for core %u\n", req->core_id);
	return 0;
}

// Handle DDR configuration request
// returns 0 on success, -ENOMEM on failure
int api_ddr_config(struct dpf_ddr_config_s *req_data)
{
	struct dpf_ddr_config_s *req = req_data;
	struct dpf_resp_ddr_config_s *resp;
	int ret;

	pr_info("Received BAR=0x%llx, CPU type=%u\n", req->bar_address, req->cpu_type);

	if (req->num_controllers == 0 || req->num_controllers > MAX_NUM_DDR_CONTROLLERS) {
		pr_err("Invalid controller count %u (max %d)\n", req->num_controllers, MAX_NUM_DDR_CONTROLLERS);
		return -EINVAL;
	}

	// Validate num_controllers
	if (req->num_controllers == 0 || req->num_controllers > MAX_NUM_DDR_CONTROLLERS) {
		pr_err("Invalid number of DDR controllers (%u), must be 1 to %d\n", req->num_controllers, MAX_NUM_DDR_CONTROLLERS);
		return -EINVAL;
	}

	resp = kmalloc(sizeof(struct dpf_resp_ddr_config_s), GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	// Clean up prior mappings
	for (int i = 0; i < MAX_NUM_DDR_CONTROLLERS; i++) {
		if (ddr.mmap[i]) {
			iounmap((void __iomem *)ddr.mmap[i]);
			release_mem_region(ddr.bar_address +
					       (ddr_cpu_type == DDR_CLIENT ? (i == 0 ? CLIENT_DDR0_OFFSET : CLIENT_DDR1_OFFSET) : GRR_SRF_MC_ADDRESS(i) + GRR_SRF_FREE_RUN_CNTR_READ),
					       ddr_cpu_type == DDR_CLIENT ? CLIENT_DDR_RANGE : GRR_SRF_DDR_RANGE);
			ddr.mmap[i] = NULL;
		}
	}

	ddr_bar_address = req->bar_address;
	ddr_cpu_type = req->cpu_type;
	num_ddr_controllers = req->num_controllers;

	if (ddr_cpu_type == DDR_CLIENT) {
		pr_info("CLIENT detected\n");
		ret = kernel_pmu_ddr_init_client(&ddr, ddr_bar_address);
	} else if (ddr_cpu_type == DDR_GRR_SRF) {
		pr_info("GRR/SRF detected\n");
		ret = kernel_pmu_ddr_init_grr_srf(&ddr, ddr_bar_address);
	} else {
		pr_err("Unknown DDR type detected (%u)\n", ddr_cpu_type);
		kfree(resp);
		return -EINVAL;
	}

	if (ret < 0) {
		pr_err("DDR init failed (type %d, ret %d)\n", ddr_cpu_type, ret);
		kfree(resp);
		return -EINVAL;
	}

	resp->header.type = DPF_MSG_DDR_CONFIG;
	resp->header.payload_size = sizeof(struct dpf_resp_ddr_config_s);
	resp->confirmed_bar = ddr_bar_address;
	resp->confirmed_type = ddr_cpu_type;

	kfree(proc_buffer);
	proc_buffer = (char *)resp;
	proc_buffer_size = sizeof(struct dpf_resp_ddr_config_s);

	pr_info("%s: DDR config set - BAR=0x%llx, Type=%u\n",
	       __func__, ddr_bar_address, ddr_cpu_type);

	return 0;
}

// Handles DDR bandwidth read request, retrieves read_bw and write_bw
// returns 0 on success, -ENOMEM on failure
int api_ddr_bw_read(struct dpf_ddr_bw_read_s *req_data)
{
	struct dpf_resp_ddr_bw_read_s *resp;
	uint64_t read_bw, write_bw;

	pr_info("%s: Reading DDR bandwidth\n", __func__);

	resp = kmalloc(sizeof(struct dpf_resp_ddr_bw_read_s), GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	read_ddr_counters(&read_bw, &write_bw);

	resp->header.type = DPF_MSG_DDR_BW_READ;
	resp->header.payload_size = sizeof(struct dpf_resp_ddr_bw_read_s);
	resp->read_bw = read_bw;
	resp->write_bw = write_bw;

	kfree(proc_buffer);
	proc_buffer = (char *)resp;
	proc_buffer_size = sizeof(struct dpf_resp_ddr_bw_read_s);

	pr_info("%s: Retrieved DDR bandwidth: Read=%llu bytes, Write=%llu bytes\n",
	       __func__, resp->read_bw, resp->write_bw);
	return 0;
}




// Handle PMU logging control request
// It initializes or configures PMU metrics logging with options for buffer size and mode
// returns 0 on success, negative error code on failure
int api_pmu_log_control(struct dpf_pmu_log_control_s *req_data)
{
	struct dpf_pmu_log_control_s *req = req_data;
	struct dpf_resp_pmu_log_control_s *resp;
	size_t requested_buffer_size;
	char *new_buffer = NULL;
	__u32 mode;
	__s32 status = 0;

	pr_info("%s: Received PMU log control request: buffer_size=%u, mode=%u\n",
	       __func__, req->buffer_size, req->mode);

	resp = kmalloc(sizeof(struct dpf_resp_pmu_log_control_s), GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	// Process request parameters
	requested_buffer_size = req->buffer_size;
	mode = req->mode;

	// Validate mode (0=reset, 1=append)
	if (mode > 1) {
		pr_err("%s: Invalid mode %u, must be 0 (reset) or 1 (append)\n", 
		       __func__, mode);
		kfree(resp);
		return -EINVAL;
	}

	// For the first time or reset mode, allocate or reallocate the buffer
	if (!pmu_log_buffer || mode == 0) {
		// Free existing buffer if any
		if (pmu_log_buffer) {
			kfree(pmu_log_buffer);
			pmu_log_buffer = NULL;
		}

		// Allocate new buffer
		if (requested_buffer_size > 0) {
			new_buffer = kmalloc(requested_buffer_size, GFP_KERNEL);
			if (!new_buffer) {
				pr_err("%s: Failed to allocate %zu bytes for PMU log buffer\n",
				       __func__, requested_buffer_size);
				kfree(resp);
				return -ENOMEM;
			}
			memset(new_buffer, 0, requested_buffer_size);
			pmu_log_buffer = new_buffer;
			pmu_log_buffer_size = requested_buffer_size;
			pmu_log_data_size = 0; // Reset data size for new buffer
		} else {
			// Zero buffer size is invalid
			pr_err("%s: Buffer size must be greater than zero\n", __func__);
			kfree(resp);
			return -EINVAL;
		}
	} else if (mode == 0 && pmu_log_buffer) {
		// Reset mode with existing buffer, just reset data size
		pmu_log_data_size = 0;
	}

	// Initialize PMU counters on all cores
	if (mode == 0 || !pmu_logging_active) {
		// Reset core states
		for (int i = 0; i < MAX_NUM_CORES; i++) {
			if (corestate[i].core_disabled == 0) {
				// Initialize PMU counters
				memset(corestate[i].pmu_result, 0, sizeof(corestate[i].pmu_result));
				// Enable PMU monitoring
				configure_pmu(i);
			}
		}
	}

	// Start monitoring timer if not already running
	if (!keep_running) {
		keep_running = true;
		kt_period = ktime_set(0, TIMER_INTERVAL_SEC * NSEC_PER_SEC);
		hrtimer_start(&monitor_timer, kt_period, HRTIMER_MODE_REL);
	}

	// Enable logging
	pmu_logging_active = true;

	// Prepare response
	resp->header.type = DPF_MSG_PMU_LOG_CONTROL;
	resp->header.payload_size = sizeof(struct dpf_resp_pmu_log_control_s);
	resp->confirmed_buffer_size = pmu_log_buffer_size;
	resp->confirmed_mode = mode;
	resp->status = status;

	// Set response in proc buffer
	kfree(proc_buffer);
	proc_buffer = (char *)resp;
	proc_buffer_size = sizeof(struct dpf_resp_pmu_log_control_s);

	pr_info("%s: PMU logging %s with buffer size %zu bytes\n",
	       __func__, (mode == 0) ? "reset" : "configured", pmu_log_buffer_size);

	return 0;
}

// Handle PMU logging stop request
// Stops or pauses the ongoing PMU metrics logging session
// returns 0 on success, negative error code on failure
int api_pmu_log_stop(struct dpf_pmu_log_stop_s *req_data)
{
	struct dpf_resp_pmu_log_stop_s *resp;
	__s32 status = 0;

	pr_info("%s: Received PMU log stop request\n", __func__);

	resp = kmalloc(sizeof(struct dpf_resp_pmu_log_stop_s), GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	// Check if logging is already stopped
	if (!pmu_logging_active) {
		pr_info("%s: PMU logging already stopped\n", __func__);
		resp->status = -EINVAL;
		goto cleanup;
	}

	// Disable logging
	pmu_logging_active = false;

	// Stop monitoring timer
	keep_running = false;
	if (hrtimer_active(&monitor_timer)) {
		hrtimer_cancel(&monitor_timer);
	}

	// Disable PMU counters on all cores
	for (int i = 0; i < MAX_NUM_CORES; i++) {
		if (corestate[i].core_disabled == 0) {
			// Disable PMU monitoring
			configure_pmu(i);
		}
	}

	// Prepare response
	resp->header.type = DPF_MSG_PMU_LOG_STOP;
	resp->header.payload_size = sizeof(struct dpf_resp_pmu_log_stop_s);
	resp->status = status;

	// Set response in proc buffer
	kfree(proc_buffer);
	proc_buffer = (char *)resp;
	proc_buffer_size = sizeof(struct dpf_resp_pmu_log_stop_s);

	pr_info("%s: PMU logging stopped\n", __func__);

	return 0;

cleanup:
	kfree(resp);
	resp = kmalloc(sizeof(struct dpf_resp_pmu_log_stop_s), GFP_KERNEL);
	if (!resp)
		return -ENOMEM;
	resp->header.type = DPF_MSG_PMU_LOG_STOP;
	resp->header.payload_size = sizeof(struct dpf_resp_pmu_log_stop_s);
	resp->status = -EINVAL;
	kfree(proc_buffer);
	proc_buffer = (char *)resp;
	proc_buffer_size = sizeof(struct dpf_resp_pmu_log_stop_s);
	return -EINVAL;
}

// Handle PMU log read request
// Reads the contents of the PMU metrics buffer
// returns 0 on success, negative error code on failure
int api_pmu_log_read(struct dpf_pmu_log_read_s *req_data)
{
	struct dpf_pmu_log_read_s *req = req_data;
	struct dpf_resp_pmu_log_read_s *resp = NULL;
	__u32 max_bytes;
	__u64 bytes_to_read = 0;
	size_t resp_size;

	pr_info("%s: Received PMU log read request: max_bytes=%u\n",
	       __func__, req ? req->max_bytes : 0);

	// Check if buffer exists
	if (!pmu_log_buffer || pmu_log_buffer_size == 0) {
		pr_err("%s: PMU log buffer not allocated\n", __func__);
		return -ENODATA;
	}

	// Check if we have data
	if (pmu_log_data_size == 0) {
		pr_info("%s: PMU log buffer is empty\n", __func__);
		// Return success but with zero data size
		resp_size = sizeof(struct dpf_resp_pmu_log_read_s);
		resp = kmalloc(resp_size, GFP_KERNEL);
		if (!resp) {
			pr_err("%s: Failed to allocate empty response buffer\n", __func__);
			return -ENOMEM;
		}
		resp->header.type = DPF_MSG_PMU_LOG_READ;
		resp->header.payload_size = resp_size;
		resp->data_size = 0;
		
		kfree(proc_buffer);
		proc_buffer = (char *)resp;
		proc_buffer_size = resp_size;
		return 0;
	}

	// Determine how much data to read
	max_bytes = req ? req->max_bytes : 0;
	if (max_bytes == 0) {
		// If max_bytes is 0, read all available data
		bytes_to_read = pmu_log_data_size;
	} else {
		// Otherwise, read up to max_bytes
		bytes_to_read = min_t(__u64, max_bytes, pmu_log_data_size);
	}

	// Calculate response size including flexible array
	resp_size = sizeof(struct dpf_resp_pmu_log_read_s) + bytes_to_read;

	// Allocate response with enough space for data
	resp = kmalloc(resp_size, GFP_KERNEL);
	if (!resp) {
		pr_err("%s: Failed to allocate response buffer of size %zu\n", 
		       __func__, resp_size);
		return -ENOMEM;
	}

	// Prepare response header
	memset(resp, 0, resp_size);
	resp->header.type = DPF_MSG_PMU_LOG_READ;
	resp->header.payload_size = resp_size;
	resp->data_size = bytes_to_read;

	// Copy data if any
	if (bytes_to_read > 0) {
		memcpy(resp->data, pmu_log_buffer, bytes_to_read);
		pr_info("%s: Copied %llu bytes of PMU data to response (first entry: core=%u, ts=%llu)\n", 
		       __func__, bytes_to_read, 
		       ((dpf_pmu_log_entry_t *)resp->data)->core_id,
		       ((dpf_pmu_log_entry_t *)resp->data)->timestamp);
	} else {
		pr_info("%s: No data to copy (bytes_to_read=%llu)\n", 
		       __func__, bytes_to_read);
	}

	// Set response in proc buffer
	kfree(proc_buffer);
	proc_buffer = (char *)resp;
	proc_buffer_size = resp_size;

	pr_info("%s: Read %llu/%zu bytes from PMU log buffer\n", 
	       __func__, bytes_to_read, pmu_log_data_size);

	return 0;
}

// Function to append data to the PMU log buffer
// Called during monitoring to collect PMU metrics
int api_pmu_log_append_data(void *data, size_t data_size)
{
	// Check if logging is enabled and buffer exists
	if (!pmu_logging_active) {
		pr_debug("%s: PMU logging not active\n", __func__);
		return -EINVAL;
	}

	if (!pmu_log_buffer || pmu_log_buffer_size == 0) {
		pr_err("%s: PMU log buffer not initialized\n", __func__);
		return -EINVAL;
	}

	if (!data || data_size == 0) {
		pr_err("%s: Invalid data or data size\n", __func__);
		return -EINVAL;
	}

	// Check if there's enough space in the buffer
	if (pmu_log_data_size + data_size > pmu_log_buffer_size) {
		pr_warn("%s: PMU log buffer full (%zu + %zu > %zu), dropping data\n",
		       __func__, pmu_log_data_size, data_size, pmu_log_buffer_size);
		return -ENOSPC;
	}

	// Debug: Print first few bytes of the data being appended
	dpf_pmu_log_entry_t *entry = (dpf_pmu_log_entry_t *)data;
	pr_debug("%s: Appending %zu bytes (core %u, ts %llu)\n",
	       __func__, data_size, entry->core_id, entry->timestamp);

	// Append data to buffer
	memcpy(pmu_log_buffer + pmu_log_data_size, data, data_size);
	pmu_log_data_size += data_size;

	pr_debug("%s: New buffer size: %zu bytes\n", __func__, pmu_log_data_size);
	return 0;
}

