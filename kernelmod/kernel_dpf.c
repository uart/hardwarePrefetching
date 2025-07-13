// SPDX-License-Identifier: Dual BSD/GPL
#include <asm/msr.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include "../include/pmu_ddr.h"
#include "kernel_common.h"
#include "kernel_pmu_ddr.h"
#include "kernel_primitive.h"

#define TIMER_INTERVAL_SEC 1
#define PROC_FILE_NAME "dynamicPrefetch"
#define PROC_BUFFER_SIZE (1024)

static bool keep_running;
static struct hrtimer monitor_timer;
static ktime_t kt_period;
static char *proc_buffer;
static size_t proc_buffer_size;
static __u64 ddr_bar_address;
static DEFINE_MUTEX(dpf_mutex);
static cpumask_t enabled_cpus;

// Configures PMU for a core, sets up performance counters
// core_id: The CPU core to configure
static void configure_pmu_on_core(void *info)
{

	// Configure Performance Event Select registers (PERFEVTSELx MSRs)
	native_write_msr(0x186, EVENT_MEM_UOPS_RETIRED_ALL_LOADS & 0xFFFFFFFF,
			 EVENT_MEM_UOPS_RETIRED_ALL_LOADS >> 32);
	native_write_msr(0x187, EVENT_MEM_LOAD_UOPS_RETIRED_L2_HIT & 0xFFFFFFFF,
			 EVENT_MEM_LOAD_UOPS_RETIRED_L2_HIT >> 32);
	native_write_msr(0x188, EVENT_MEM_LOAD_UOPS_RETIRED_L3_HIT & 0xFFFFFFFF,
			 EVENT_MEM_LOAD_UOPS_RETIRED_L3_HIT >> 32);
	native_write_msr(0x189,
			 EVENT_MEM_LOAD_UOPS_RETIRED_DRAM_HIT & 0xFFFFFFFF,
			 EVENT_MEM_LOAD_UOPS_RETIRED_DRAM_HIT >> 32);
	native_write_msr(0x18A, EVENT_XQ_PROMOTION_ALL & 0xFFFFFFFF,
			 EVENT_XQ_PROMOTION_ALL >> 32);
	native_write_msr(0x18B, EVENT_CPU_CLK_UNHALTED_THREAD & 0xFFFFFFFF,
			 EVENT_CPU_CLK_UNHALTED_THREAD >> 32);
	native_write_msr(0x18C, EVENT_INST_RETIRED_ANY_P & 0xFFFFFFFF,
			 EVENT_INST_RETIRED_ANY_P >> 32);

	// Reset the performance counter control register first
	native_write_msr(0x38D, 0, 0); // Clear IA32_PERF_GLOBAL_CTRL

	// Enable PMC0-6
	native_write_msr(0x38F, 0x7F, 0);
}

// Configures PMU for a core, sets up performance counters
// core_id: The CPU core to configure
static int configure_pmu(int core_id)
{
	cpumask_t *target_cpu;
	int ret = 0;

	// Allocate cpumask on heap instead of stack to reduce frame size
	target_cpu = kmalloc(sizeof(cpumask_t), GFP_KERNEL);
	if (!target_cpu)
		return -ENOMEM;

	cpumask_clear(target_cpu);
	cpumask_set_cpu(core_id, target_cpu);

	// Execute the configuration function on the specified core
	// smp_call_function_many returns void, not an int
	smp_call_function_many(target_cpu, configure_pmu_on_core, NULL, 1);

	kfree(target_cpu);
	return ret;
}

// Handle initialization request and response to the user space
// Arguments: None
// Returns: 0 on success, -ENOMEM on failure
static int handle_init(void)
{
	struct dpf_resp_init *resp;

	resp = kmalloc(sizeof(struct dpf_resp_init), GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	resp->header.type = DPF_MSG_INIT;
	resp->header.payload_size = sizeof(struct dpf_resp_init);
	resp->version = DPF_API_VERSION;

	kfree(proc_buffer);

	proc_buffer = (char *)resp;
	proc_buffer_size = sizeof(struct dpf_resp_init);

	pr_info("%s: Initialized with version %d\n",
	       __func__, resp->version);

	return 0;
}

// Handle core range configuration request and response to the user space
// It accepts a request to specify the range of cores to monitor
// returns 0 on success, -ENOMEM on failure
static int handle_core_range(struct dpf_core_range *req_data)
{
	struct dpf_core_range *req = req_data;
	struct dpf_resp_core_range *resp;
	int core_id;

	pr_info("%s: Received core range request: start=%d, end=%d\n",
	       __func__, req->core_start, req->core_end);

	resp = kmalloc(sizeof(struct dpf_resp_core_range), GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	resp->header.type = DPF_MSG_CORE_RANGE;
	resp->header.payload_size = sizeof(struct dpf_resp_core_range);
	resp->core_start = req->core_start;
	resp->core_end = req->core_end;
	resp->thread_count = req->core_end - req->core_start + 1;

	for (core_id = 0; core_id < MAX_NUM_CORES; core_id++) {
		corestate[core_id].core_disabled =
		    (core_id < req->core_start || core_id > req->core_end);
		if (!corestate[core_id].core_disabled)
			configure_pmu(core_id);
	}

	kfree(proc_buffer);

	proc_buffer = (char *)resp;
	proc_buffer_size = sizeof(struct dpf_resp_core_range);

	pr_info("%s: Processed core range request: start=%d, end=%d, thread_count=%d\n",
	       __func__, resp->core_start, resp->core_end, resp->thread_count);

	return 0;
}

// Handle core weight configuration request and response to the user space
// It accepts a request to specify the weight of each core
// returns 0 on success, -ENOMEM on failure
static int handle_core_weight(void *req_data)
{
	struct dpf_core_weight *req = req_data;
	struct dpf_resp_core_weight *resp;
	size_t resp_size;

	if (!req_data)
		return -EINVAL;

	pr_info("%s: Received core weight request with count=%d\n",
	       __func__, req->count);

	resp_size = sizeof(struct dpf_resp_core_weight) + req->count *
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
static int handle_tuning(struct dpf_req_tuning *req_data)
{
	struct dpf_req_tuning *req = req_data;
	struct dpf_resp_tuning *resp;
	int core_id;

	resp = kmalloc(sizeof(struct dpf_resp_tuning), GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	resp->header.type = DPF_MSG_TUNING;
	resp->header.payload_size = sizeof(struct dpf_resp_tuning);
	resp->status = req->enable;

	if (req->enable == 1) {
		// Load current Prefetch MSR settings for enabled
		// cores before starting tuning
		for_each_online_cpu(core_id) {
			if (corestate[core_id].core_disabled == 0 && CORE_IN_MODULE == 0) {
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
	proc_buffer_size = sizeof(struct dpf_resp_tuning);

	return 0;
}

// Handle DDR bandwidth set request and response to the user space
// It accepts a request to set the DDR bandwidth target
// returns 0 on success, -ENOMEM on failure
static int handle_ddrbw_set(struct dpf_ddrbw_set *req_data)
{
	struct dpf_ddrbw_set *req = req_data;
	struct dpf_resp_ddrbw_set *resp;

	pr_info("Received request with value=%u\n", req->set_value);

	resp = kmalloc(sizeof(struct dpf_resp_ddrbw_set), GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	resp->header.type = DPF_MSG_DDRBW_SET;
	resp->header.payload_size = sizeof(struct dpf_resp_ddrbw_set);
	resp->confirmed_value = req->set_value;

	ddr_bw_target = req->set_value;

	kfree(proc_buffer);

	proc_buffer = (char *)resp;
	proc_buffer_size = sizeof(struct dpf_resp_ddrbw_set);

	pr_info("DDR bandwidth target set to %u MB/s\n", ddr_bw_target);

	return 0;
}

// Handles MSR read request, retrieves MSR values for a core
// returns 0 on success, -ENOMEM on failure
static int handle_msr_read(struct dpf_msr_read *req_data)
{
	struct dpf_msr_read *req = req_data;
	struct dpf_resp_msr_read *resp;

	pr_info("Received request for core %u\n", req->core_id);

	if (req->core_id >= MAX_NUM_CORES ||
	    corestate[req->core_id].core_disabled) {
		pr_err("Invalid or disabled core %u\n", req->core_id);
		return -EINVAL;
	}

	resp = kmalloc(sizeof(struct dpf_resp_msr_read), GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	resp->header.type = DPF_MSG_MSR_READ;
	resp->header.payload_size = sizeof(struct dpf_resp_msr_read);

	if (corestate[req->core_id].pf_msr[MSR_1320_INDEX].v == 0)
		msr_load(req->core_id);

	for (int i = 0; i < NR_OF_MSR; i++)
		resp->msr_values[i] = corestate[req->core_id].pf_msr[i].v;

	kfree(proc_buffer);
	proc_buffer = (char *)resp;
	proc_buffer_size = sizeof(struct dpf_resp_msr_read);

	pr_info("MSR values retrieved for core %u\n", req->core_id);
	return 0;
}

// Handles PMU read request, retrieves PMU counter values for a core
// returns 0 on success, -ENOMEM on failure
static int handle_pmu_read(struct dpf_pmu_read *req_data)
{
	struct dpf_pmu_read *req = req_data;
	struct dpf_resp_pmu_read *resp;

	pr_info("Received request for core %u\n", req->core_id);

	if (req->core_id >= MAX_NUM_CORES || corestate[req->core_id].core_disabled) {
		pr_err("Invalid or disabled core %u\n", req->core_id);
		return -EINVAL;
	}

	resp = kmalloc(sizeof(struct dpf_resp_pmu_read), GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	resp->header.type = DPF_MSG_PMU_READ;
	resp->header.payload_size = sizeof(struct dpf_resp_pmu_read);

	pmu_update(req->core_id);

	for (int i = 0; i < PMU_COUNTERS; i++) {
		resp->pmu_values[i] = corestate[req->core_id].pmu_result[i];
		pr_debug("PMU %d for core %d: %llu\n", i, req->core_id,
			 resp->pmu_values[i]);
	}

	kfree(proc_buffer);
	proc_buffer = (char *)resp;
	proc_buffer_size = sizeof(struct dpf_resp_pmu_read);

	pr_info("PMU values retrieved for core %u\n", req->core_id);
	return 0;
}

// Handle DDR configuration request
// returns 0 on success, -ENOMEM on failure
static int handle_ddr_config(struct dpf_ddr_config *req_data)
{
	struct dpf_ddr_config *req = req_data;
	struct dpf_resp_ddr_config *resp;
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

	resp = kmalloc(sizeof(struct dpf_resp_ddr_config), GFP_KERNEL);
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
	resp->header.payload_size = sizeof(struct dpf_resp_ddr_config);
	resp->confirmed_bar = ddr_bar_address;
	resp->confirmed_type = ddr_cpu_type;

	kfree(proc_buffer);
	proc_buffer = (char *)resp;
	proc_buffer_size = sizeof(struct dpf_resp_ddr_config);

	pr_info("%s: DDR config set - BAR=0x%llx, Type=%u\n",
	       __func__, ddr_bar_address, ddr_cpu_type);

	return 0;
}

// Handles DDR bandwidth read request, retrieves read_bw and write_bw
// returns 0 on success, -ENOMEM on failure
static int handle_ddr_bw_read(struct dpf_ddr_bw_read *req_data)
{
	struct dpf_resp_ddr_bw_read *resp;
	uint64_t read_bw, write_bw;

	pr_info("%s: Reading DDR bandwidth\n", __func__);

	resp = kmalloc(sizeof(struct dpf_resp_ddr_bw_read), GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	read_ddr_counters(&read_bw, &write_bw);

	resp->header.type = DPF_MSG_DDR_BW_READ;
	resp->header.payload_size = sizeof(struct dpf_resp_ddr_bw_read);
	resp->read_bw = read_bw;
	resp->write_bw = write_bw;

	kfree(proc_buffer);
	proc_buffer = (char *)resp;
	proc_buffer_size = sizeof(struct dpf_resp_ddr_bw_read);

	pr_info("%s: Retrieved DDR bandwidth: Read=%llu bytes, Write=%llu bytes\n",
	       __func__, resp->read_bw, resp->write_bw);
	return 0;
}

// Handles the read request from the user space
static ssize_t proc_read(struct file *file, char __user *buffer,
			 size_t count, loff_t *pos)
{
	if (*pos > 0 || !proc_buffer || count < proc_buffer_size) {
		pr_info("%s: Nothing to read or buffer size mismatch\n", __func__);
		return 0;
	}

	if (copy_to_user(buffer, proc_buffer, proc_buffer_size)) {
		pr_err("%s: Failed to copy data to user\n", __func__);
		return -EFAULT;
	}

	*pos = proc_buffer_size; // Indicate end of file
	return proc_buffer_size;
}

// Handles the write request from the user space
// returns 0 on success, -EINVAL on failure
static ssize_t dpf_proc_write(struct file *file, const char __user *buffer,
			      size_t count, loff_t *ppos)
{
	struct dpf_msg_header header;
	void *msg_data = NULL;
	int ret = -EINVAL;

	if (count < sizeof(struct dpf_msg_header) || count > MAX_MSG_SIZE)
		return -EINVAL;

	mutex_lock(&dpf_mutex);

	kfree(proc_buffer);
	proc_buffer = NULL;
	proc_buffer_size = 0;

	if (copy_from_user(&header, buffer, sizeof(header))) {
		mutex_unlock(&dpf_mutex);
		return -EFAULT;
	}

	msg_data = kmalloc(count, GFP_KERNEL);
	if (!msg_data) {
		mutex_unlock(&dpf_mutex);
		return -ENOMEM;
	}

	if (copy_from_user(msg_data, buffer, count)) {
		kfree(msg_data);
		mutex_unlock(&dpf_mutex);
		return -EFAULT;
	}

	switch (header.type) {
	case DPF_MSG_INIT:
		ret = handle_init();
		break;
	case DPF_MSG_CORE_RANGE:
		ret = handle_core_range(msg_data);
		break;
	case DPF_MSG_CORE_WEIGHT:
		ret = handle_core_weight(msg_data);
		break;
	case DPF_MSG_TUNING:
		ret = handle_tuning(msg_data);
		break;
	case DPF_MSG_DDRBW_SET:
		ret = handle_ddrbw_set(msg_data);
		break;
	case DPF_MSG_PMU_READ:
		ret = handle_pmu_read(msg_data);
		break;
	case DPF_MSG_MSR_READ:
		ret = handle_msr_read(msg_data);
		break;
	case DPF_MSG_DDR_CONFIG:
		ret = handle_ddr_config(msg_data);
		break;
	case DPF_MSG_DDR_BW_READ:
		ret = handle_ddr_bw_read(msg_data);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	kfree(msg_data);
	mutex_unlock(&dpf_mutex);

	return ret < 0 ? ret : count;
}

// proc file operations
static const struct proc_ops proc_fops = {
	.proc_read = proc_read,
	.proc_write = dpf_proc_write,
};

// Per-core work function executed on each CPU core
// info: Pointer to data passed via smp_call_function_many; NULL if no data is
// provided
static void per_core_work(void *info)
{
	// Get the ID of the current CPU core
	int core_id = smp_processor_id();

	if (corestate[core_id].core_disabled == 0) {
		pmu_update(core_id);
		if (core_id == FIRST_CORE)
			kernel_basicalg(0);
		if (CORE_IN_MODULE == 0 && is_msr_dirty(core_id) == 1)
			msr_update(core_id);
	}
}

// Optimized monitor callback using precomputed cpumask
static enum hrtimer_restart monitor_callback(struct hrtimer *timer)
{
	if (!keep_running)
		return HRTIMER_NORESTART;

	if (!cpumask_empty(&enabled_cpus)) {
		smp_call_function_many(&enabled_cpus, per_core_work,
				       NULL, false);
	}

	hrtimer_forward_now(timer, kt_period);
	return HRTIMER_RESTART;
}

// Module initialization
static int __init dpf_module_init(void)
{
	struct proc_dir_entry *entry;
	int core_id;

	pr_info("dPF Module Loaded\n");

	for (core_id = 0; core_id < MAX_CORES; core_id++)
		corestate[core_id].core_disabled = 1;

	proc_buffer = kmalloc(PROC_BUFFER_SIZE, GFP_KERNEL);
	if (!proc_buffer)
		return -ENOMEM;

	entry = proc_create(PROC_FILE_NAME, 0444, NULL, &proc_fops);
	if (!entry) {
		pr_err("Failed to create /proc entry\n");
		kfree(proc_buffer);
		return -ENOMEM;
	}

	kt_period = ktime_set(TIMER_INTERVAL_SEC, 0);
	hrtimer_init(&monitor_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	monitor_timer.function = monitor_callback;

	return 0;
}

// Module cleanup
static void __exit dpf_module_exit(void)
{
	pr_info("Stopping dPF monitor thread\n");
	keep_running = false;

	hrtimer_cancel(&monitor_timer);

	remove_proc_entry(PROC_FILE_NAME, NULL);
	kfree(proc_buffer);

	// Cleanup DDR mappings
	for (int i = 0; i < MAX_NUM_DDR_CONTROLLERS; i++) {
		if (ddr.mmap[i]) {
			iounmap((void __iomem *)ddr.mmap[i]);
			release_mem_region(ddr_bar_address +
					       (ddr_cpu_type == DDR_CLIENT ? (i == 0 ? CLIENT_DDR0_OFFSET : CLIENT_DDR1_OFFSET) : GRR_SRF_MC_ADDRESS(i) + GRR_SRF_FREE_RUN_CNTR_READ),
					   ddr_cpu_type == DDR_CLIENT ? CLIENT_DDR_RANGE : GRR_SRF_DDR_RANGE);
			ddr.mmap[i] = NULL;
		}
	}

	pr_info("dPF Module Unloaded\n");
}

module_init(dpf_module_init);
module_exit(dpf_module_exit);

MODULE_LICENSE("Dual BSD/GPL");
