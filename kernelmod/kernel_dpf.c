// SPDX-License-Identifier: Dual BSD/GPL
#include <asm/msr.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/ioport.h>

#include "kernel_common.h"
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
#include "kernel_api.h"

#define TIMER_INTERVAL_SEC 1
#define PROC_FILE_NAME "dynamicPrefetch"
#define PROC_BUFFER_SIZE (1024)

bool keep_running;
struct hrtimer monitor_timer;
ktime_t kt_period;
char *proc_buffer;
size_t proc_buffer_size;
__u64 ddr_bar_address;
static DEFINE_MUTEX(dpf_mutex);
cpumask_t enabled_cpus;


// Global tuning algorithm settings, these should be set through the dpf_tuning_control API.
int tune_alg;
int aggr;

// Configures PMU for a core, sets up performance counters
// core_id: The CPU core to configure
static void configure_pmu_on_core(void *info)
{

	// Configure Performance Event Select registers (PERFEVTSELx MSRs)
	native_write_msr(MSR_IA32_PERFEVTSEL0,
		EVENT_MEM_UOPS_RETIRED_ALL_LOADS & MSR_LOW_MASK,
		EVENT_MEM_UOPS_RETIRED_ALL_LOADS >> 32);
	native_write_msr(MSR_IA32_PERFEVTSEL1,
		EVENT_MEM_LOAD_UOPS_RETIRED_L2_HIT & MSR_LOW_MASK,
		EVENT_MEM_LOAD_UOPS_RETIRED_L2_HIT >> 32);
	native_write_msr(MSR_IA32_PERFEVTSEL2,
		EVENT_MEM_LOAD_UOPS_RETIRED_L3_HIT & MSR_LOW_MASK,
		EVENT_MEM_LOAD_UOPS_RETIRED_L3_HIT >> 32);
	native_write_msr(MSR_IA32_PERFEVTSEL3,
		EVENT_MEM_LOAD_UOPS_RETIRED_DRAM_HIT & MSR_LOW_MASK,
		EVENT_MEM_LOAD_UOPS_RETIRED_DRAM_HIT >> 32);
	native_write_msr(MSR_IA32_PERFEVTSEL4,
		EVENT_XQ_PROMOTION_ALL & MSR_LOW_MASK,
		EVENT_XQ_PROMOTION_ALL >> 32);
	native_write_msr(MSR_IA32_PERFEVTSEL5,
		EVENT_CPU_CLK_UNHALTED_THREAD & MSR_LOW_MASK,
		EVENT_CPU_CLK_UNHALTED_THREAD >> 32);
	native_write_msr(MSR_IA32_PERFEVTSEL6,
		EVENT_INST_RETIRED_ANY_P & MSR_LOW_MASK,
		EVENT_INST_RETIRED_ANY_P >> 32);

	// Reset the performance counter control register first
	native_write_msr(MSR_IA32_PERF_GLOBAL_STATUS, 0, 0); // Clear performance counters status

	// Enable PMC0-6
	native_write_msr(MSR_IA32_PERF_GLOBAL_CTRL, PMC_ENABLE_ALL, 0);
}

// Configures PMU for a core, sets up performance counters
// core_id: The CPU core to configure
int configure_pmu(int core_id)
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


// Handles the read request from the user space
static ssize_t proc_read(struct file *file, char __user *buffer,
	size_t count, loff_t *pos)
{
	size_t bytes_to_copy;

	pr_info("%s: Enter: count=%zu, pos=%lld, proc_buffer_size=%zu\n", 
	       __func__, count, *pos, proc_buffer_size);

	// Check if we have data to read
	if (!proc_buffer || proc_buffer_size == 0) {
		pr_info("%s: No data available to read\n", __func__);
		return 0;
	}

	// Check if we've already read everything
	if (*pos >= proc_buffer_size) {
		pr_info("%s: Reached end of data\n", __func__);
		return 0;
	}

	// Calculate how much we can copy
	bytes_to_copy = min(count, proc_buffer_size - *pos);

	pr_info("%s: Attempting to copy %zu bytes from offset %lld\n", 
	       __func__, bytes_to_copy, *pos);

	// Copy data to user space
	if (copy_to_user(buffer, proc_buffer + *pos, bytes_to_copy)) {
		pr_err("%s: Failed to copy %zu bytes to user buffer\n", 
		      __func__, bytes_to_copy);
		return -EFAULT;
	}

	// Update position
	*pos += bytes_to_copy;
	pr_info("%s: Successfully copied %zu bytes (new pos: %lld)\n", 
	       __func__, bytes_to_copy, *pos);

	return bytes_to_copy;
}

// Handles the write request from the user space
// returns 0 on success, -EINVAL on failure
static ssize_t dpf_proc_write(struct file *file, const char __user *buffer,
			      size_t count, loff_t *ppos)
{
	struct dpf_msg_header_s header;
	void *msg_data = NULL;
	int ret = -EINVAL;

	if (count < sizeof(struct dpf_msg_header_s) || count > MAX_MSG_SIZE)
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
		ret = api_init();
		break;
	case DPF_MSG_CORE_RANGE:
		ret = api_core_range(msg_data);
		break;
	case DPF_MSG_CORE_WEIGHT:
		ret = api_core_weight(msg_data);
		break;
	case DPF_MSG_TUNING:
		ret = api_tuning(msg_data);
		break;
	case DPF_MSG_DDRBW_SET:
		ret = api_ddrbw_set(msg_data);
		break;
	case DPF_MSG_PMU_READ:
		ret = api_pmu_read(msg_data);
		break;
	case DPF_MSG_MSR_READ:
		ret = api_msr_read(msg_data);
		break;
	case DPF_MSG_DDR_CONFIG:
		ret = api_ddr_config(msg_data);
		break;
	case DPF_MSG_DDR_BW_READ:
		ret = api_ddr_bw_read(msg_data);
		break;
		case DPF_MSG_PMU_LOG_CONTROL:
		ret = api_pmu_log_control(msg_data);
		break;
	case DPF_MSG_PMU_LOG_STOP:
		ret = api_pmu_log_stop(msg_data);
		break;
	case DPF_MSG_PMU_LOG_READ:
		ret = api_pmu_log_read(msg_data);
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

	pr_info("Core %d tuning\n", core_id);

	if (corestate[core_id].core_disabled == 0) {
		pmu_update(core_id);

		if (core_id == first_core()) {
			pr_info("First Core - run alg %d\n", tune_alg);

			if(tune_alg == 1)kernel_basicalg(tune_alg, aggr);
		}
		// Log PMU data if logging is active
		if (pmu_logging_active && pmu_log_buffer) {
			// Create a log entry with core_id and PMU values
			dpf_pmu_log_entry_t log_entry;

			// Fill the log entry
			log_entry.core_id = core_id;
			log_entry.timestamp = ktime_get_ns(); // nanosecond timestamp

			// Copy PMU values from corestate to log entry
			memcpy(log_entry.pmu_values, corestate[core_id].pmu_result, sizeof(log_entry.pmu_values));

			// Debug: Print first few PMU values
			pr_info("Core %d: PMU values[0]=%llu, [1]=%llu, [2]=%llu, [3]=%llu\n", 
			       core_id, 
			       log_entry.pmu_values[0],
			       log_entry.pmu_values[1],
			       log_entry.pmu_values[2],
			       log_entry.pmu_values[3]);

			// Append to log buffer
			int ret = api_pmu_log_append_data(&log_entry, sizeof(log_entry));
			if (ret < 0) {
				pr_err("Failed to append PMU data for core %d: %d\n", core_id, ret);
			}
		}

		if (core_in_module(core_id) == 0 && is_msr_dirty(core_id) == 1) {
			pr_info("Core %d update MSR\n", core_id);

			msr_update(core_id);
		}
	}
}

// Optimized monitor callback using precomputed cpumask
static enum hrtimer_restart monitor_callback(struct hrtimer *timer)
{
	if (!keep_running)
		return HRTIMER_NORESTART;

	if (!cpumask_empty(&enabled_cpus)) {
		preempt_disable(); //required by smp_call_function_*()
		//pr_info("Enabling smp_call_function_many\n");
		smp_call_function_many(&enabled_cpus, per_core_work,
					NULL, false);
		preempt_enable();
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

	for (core_id = 0; core_id < MAX_NUM_CORES; core_id++)
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

    // Stop timer and prevent further work
    keep_running = false;
    hrtimer_cancel(&monitor_timer);

    // Remove /proc entry and free resources
    remove_proc_entry(PROC_FILE_NAME, NULL);
    kfree(proc_buffer);

    // Cleanup PMU logging resources
    if (pmu_log_buffer) {
        pr_info("Cleaning up PMU logging resources\n");
        kfree(pmu_log_buffer);
        pmu_log_buffer = NULL;
        pmu_log_buffer_size = 0;
        pmu_log_data_size = 0;
        pmu_logging_active = false;
    }

    // Cleanup DDR mappings
    for (int i = 0; i < MAX_NUM_DDR_CONTROLLERS; i++) {
        if (ddr.mmap[i]) {
            pr_info("Unmapping DDR memory for controller %d\n", i);
            iounmap((void __iomem *)ddr.mmap[i]);
            release_mem_region(ddr_bar_address + 
                               (ddr_cpu_type == DDR_CLIENT ? 
                                (i == 0 ? CLIENT_DDR0_OFFSET : CLIENT_DDR1_OFFSET) : 
                                GRR_SRF_MC_ADDRESS(i) + GRR_SRF_FREE_RUN_CNTR_READ),
                               ddr_cpu_type == DDR_CLIENT ? CLIENT_DDR_RANGE : GRR_SRF_DDR_RANGE);
            ddr.mmap[i] = NULL;
        }
    }

    pr_info("dPF Module Unloaded\n");
}


module_init(dpf_module_init);
module_exit(dpf_module_exit);

MODULE_LICENSE("Dual BSD/GPL");
