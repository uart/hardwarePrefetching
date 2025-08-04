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
		pr_info("Enabling smp_call_function_many\n");
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
