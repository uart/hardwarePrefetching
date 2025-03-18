#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <asm/msr.h>

#include "kernel_common.h"
#include "kernel_primitive.h"

#define TIMER_INTERVAL_SEC 5
#define PROC_FILE_NAME "dpf_monitor"
#define PROC_BUFFER_SIZE (1024)

// IMPORTANT! MOVE disabled_core INTO THE corestate[] structure!
static int disabled_core[MAX_NUM_CORES] = {1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}; // set to 1 for all cores that should be disabled

static bool keep_running = true;
static struct hrtimer monitor_timer;
static ktime_t kt_period;
static char *proc_buffer;
static size_t proc_buffer_size = 0;

static DEFINE_MUTEX(dpf_mutex);

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
	resp->version = 1;

	if (proc_buffer)
		kfree(proc_buffer);

	proc_buffer = (char *)resp;
	proc_buffer_size = sizeof(struct dpf_resp_init);

	printk(KERN_INFO "handle_init: Initialized with version %d\n",
	       resp->version);

	return 0;
}


// Handle core range configuration request and response to the user space
// It accepts a request to specify the range of cores to monitor
// returns 0 on success, -ENOMEM on failure
static int handle_core_range(struct dpf_core_range *req_data)
{
	struct dpf_core_range *req = req_data;
	struct dpf_resp_core_range *resp;

	printk(KERN_INFO "handle_core_range: Received core range request:"
			 "start=%d, end=%d\n",
	       req->core_start, req->core_end);

	resp = kmalloc(sizeof(struct dpf_resp_core_range), GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	resp->header.type = DPF_MSG_CORE_RANGE;
	resp->header.payload_size = sizeof(struct dpf_resp_core_range);
	resp->core_start = req->core_start;
	resp->core_end = req->core_end;
	resp->thread_count = req->core_end - req->core_start + 1;

	if (proc_buffer)
		kfree(proc_buffer);

	proc_buffer = (char *)resp;
	proc_buffer_size = sizeof(struct dpf_resp_core_range);

	printk(KERN_INFO "handle_core_range: Processed core range request: "
			 "  start=%d, end=%d, thread_count=%d\n",
	       resp->core_start,
	       resp->core_end, resp->thread_count);

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

	printk(KERN_INFO "handle_core_weight: Received core weight request with"
			 "count=%d\n",
	       req->count);

	resp_size = sizeof(struct dpf_resp_core_weight) + req->count * sizeof(__u32);
	resp = kmalloc(resp_size, GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	resp->header.type = DPF_MSG_CORE_WEIGHT;

	resp->header.payload_size = resp_size;
	resp->count = req->count;
	memcpy(resp->confirmed_weights, req->weights, req->count * sizeof(__u32));

	if (proc_buffer)
		kfree(proc_buffer);

	proc_buffer = (char *)resp;
	proc_buffer_size = resp_size;

	printk(KERN_INFO "handle_core_weight: Processed core weight request"
                "with count=%d\n", resp->count);

	return 0;
}


// Handle tuning request and response to the user space
// It accepts a request to enable or disable the monitoring
// returns 0 on success, -ENOMEM on failure
static int handle_tuning(struct dpf_req_tuning *req_data)
{
	struct dpf_req_tuning *req = req_data;
	struct dpf_resp_tuning *resp;

	printk(KERN_INFO "handle_tuning: Received tuning request with"
               " enable=%d\n", req->enable);

	resp = kmalloc(sizeof(struct dpf_resp_tuning), GFP_KERNEL);
	if (!resp)
		return -ENOMEM;

	resp->header.type = DPF_MSG_TUNING;
	resp->header.payload_size = sizeof(struct dpf_resp_tuning);
	resp->status = req->enable;

	if (req->enable == 1) {
		keep_running = true;
		hrtimer_start(&monitor_timer, kt_period, HRTIMER_MODE_REL);
		printk(KERN_INFO "handle_tuning: Monitoring enabled\n");
	} else {
		keep_running = false;
		hrtimer_cancel(&monitor_timer);
		printk(KERN_INFO "handle_tuning: Monitoring disabled\n");
	}

	if (proc_buffer)
		kfree(proc_buffer);

	proc_buffer = (char *)resp;
	proc_buffer_size = sizeof(struct dpf_resp_tuning);

	printk(KERN_INFO "handle_tuning: Processed tuning request with " 
                "status=%d\n", resp->status);

	return 0;
}


// Handle DDR bandwidth set request and response to the user space
// It accepts a request to set the DDR bandwidth target
// returns 0 on success, -ENOMEM on failure
static int handle_ddrbw_set(struct dpf_ddrbw_set *req_data)
{
	struct dpf_ddrbw_set *req = req_data;
	struct dpf_resp_ddrbw_set *resp;

	pr_info("handle_ddrbw_set: Received request with value=%u\n", 
                req->set_value);

	resp = kmalloc(sizeof(struct dpf_resp_ddrbw_set), GFP_KERNEL);
	if (!resp) {
		pr_err("handle_ddrbw_set: Failed to allocate memory\n");
		return -ENOMEM;
	}

	resp->header.type = DPF_MSG_DDRBW_SET;
	resp->header.payload_size = sizeof(struct dpf_resp_ddrbw_set);
	resp->confirmed_value = req->set_value;

	ddr_bw_target = req->set_value;

	if (proc_buffer)
		kfree(proc_buffer);

	proc_buffer = (char *)resp;
	proc_buffer_size = sizeof(struct dpf_resp_ddrbw_set);

	pr_info("handle_ddrbw_set: DDR bandwidth target set to %u MB/s\n", 
                ddr_bw_target);

	return 0;
}


static ssize_t proc_read(struct file *file, char __user *buffer,
	size_t count, loff_t *pos)
{
	if (*pos > 0 || !proc_buffer || count < proc_buffer_size){
		printk(KERN_INFO "proc_read: Nothing to read or buffer" 
                        "size mismatch\n");
		return 0;
	}

	if (copy_to_user(buffer, proc_buffer, proc_buffer_size)){
		printk(KERN_ERR "proc_read: Failed to copy data to" 
                        "user\n");
		return -EFAULT;
	}

	*pos = proc_buffer_size; // Indicate end of file
	return proc_buffer_size;
}

static ssize_t dpf_proc_write(struct file *file, const char __user *buffer,
			      size_t count, loff_t *ppos)
{
	struct dpf_msg_header header;
	void *msg_data = NULL;
	int ret = -EINVAL;

	if (count < sizeof(struct dpf_msg_header) || count > MAX_MSG_SIZE)
		return -EINVAL;

	mutex_lock(&dpf_mutex);

	if (proc_buffer) {
		kfree(proc_buffer);
		proc_buffer = NULL;
		proc_buffer_size = 0;
	}

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
	default:
		ret = -EINVAL;
		break;
	}

	kfree(msg_data);
	mutex_unlock(&dpf_mutex);

	return ret < 0 ? ret : count;
}


static const struct proc_ops proc_fops = {
	.proc_read = proc_read,
	.proc_write = dpf_proc_write,
};

static enum hrtimer_restart monitor_callback(struct hrtimer *timer)
{
	int core_id;

	if (!keep_running)
		return HRTIMER_NORESTART;

	/*
	This should be replaced by something faster such as smp_call_function() instead. See this:
	https://yarchive.net/comp/linux/work_on_cpu.html

	*/

	for_each_online_cpu(core_id) {
		pr_info("for_each_online_cpu(core %d)\n", core_id); // REMOVE AFTER DEBUG

		if (disabled_core[core_id] == 0) {
			pr_info("PMU update, core %d\n", core_id); // REMOVE AFTER DEBUG

			// Read PMU counters based on method
			pmu_update(core_id);

			// select out the master core
			if (core_id == FIRST_CORE) {
				pr_info("FIRST_CORE, core %d\n", core_id); // REMOVE AFTER DEBUG

				kernel_basicalg(0);
			}

			if (CORE_IN_MODULE == 0 && is_msr_dirty(core_id) == 1) {
				// write new MSR settings, only one core in each module is needed
				msr_update(core_id);
				pr_info("MSR update on core %d\n", core_id); // REMOVE AFTER DEBUG
			}

		} // if disabled
	} // for each

	hrtimer_forward_now(timer, kt_period);
	return HRTIMER_RESTART;
}

static int __init dpf_module_init(void)
{
	struct proc_dir_entry *entry;
	int core_id;

	pr_info("dPF Module Loaded\n");

	proc_buffer = kmalloc(PROC_BUFFER_SIZE, GFP_KERNEL);

	if (!proc_buffer) {
		pr_err("Failed to allocate memory for proc_buffer\n");
		return -ENOMEM;
	}

	// Create /proc entry
	entry = proc_create(PROC_FILE_NAME, 0444, NULL, &proc_fops);
	if (!entry) {
		pr_err("Failed to create /proc entry\n");
		kfree(proc_buffer);
		return -ENOMEM;
	}

	//Load current Prefetch MSR settings
	for_each_online_cpu(core_id) {
		if ((disabled_core[core_id] == 0) && (CORE_IN_MODULE == 0))
			msr_load(core_id);
	}

	// Set the timer interval
	kt_period = ktime_set(TIMER_INTERVAL_SEC, 0);

	hrtimer_init(&monitor_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	monitor_timer.function = monitor_callback;

	hrtimer_start(&monitor_timer, kt_period, HRTIMER_MODE_REL);

	return 0;
}

static void __exit dpf_module_exit(void)
{
	pr_info("Stopping dPF monitor thread\n");
	keep_running = false;

	hrtimer_cancel(&monitor_timer);

	remove_proc_entry(PROC_FILE_NAME, NULL);
	kfree(proc_buffer);

	pr_info("dPF Module Unloaded\n");
}

module_init(dpf_module_init);
module_exit(dpf_module_exit);

MODULE_LICENSE("Dual BSD/GPL");
