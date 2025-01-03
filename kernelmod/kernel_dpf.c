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

#define MAX_NUM_CORES (1024)
#define TIMER_INTERVAL_SEC 1
#define PROC_FILE_NAME "dpf_monitor"
#define PROC_BUFFER_SIZE (1024)

int kernel_basicalg(int tunealg);

static int active_core[MAX_NUM_CORES] = {0};
static bool keep_running = true;
static struct hrtimer monitor_timer;
static ktime_t kt_period;
static char *proc_buffer;
static size_t proc_buffer_size = 0;


static ssize_t proc_read(struct file *file, char __user *buffer,
	size_t count, loff_t *pos)
{
	if (*pos > 0 || count < proc_buffer_size)
		return 0;

	if (copy_to_user(buffer, proc_buffer, proc_buffer_size))
		return -EFAULT;

	*pos = proc_buffer_size; // Indicate end of file
	return proc_buffer_size;
}


static const struct proc_ops proc_fops = {
	.proc_read = proc_read,
};


static enum hrtimer_restart monitor_callback(struct hrtimer *timer)
{
	int cpu;

	if (!keep_running)
		return HRTIMER_NORESTART;

	for_each_online_cpu(cpu) {
		if (active_core[cpu] == 0) {
			kernel_basicalg(0);
		}
	}

	hrtimer_forward_now(timer, kt_period);
	return HRTIMER_RESTART;
}


static int __init dpf_module_init(void)
{
	struct proc_dir_entry *entry;
	
	pr_info("dPF Module Loaded\n");

	proc_buffer = kmalloc(PROC_BUFFER_SIZE, GFP_KERNEL);

	if (!proc_buffer) {
		pr_err("Failed to allocate memory for proc_buffer\n");
		return -ENOMEM;
	}
	// proc init should be here

	// Create /proc entry
	entry = proc_create(PROC_FILE_NAME, 0444, NULL, &proc_fops);
	if (!entry) {
		pr_err("Failed to create /proc entry\n");
		kfree(proc_buffer);
		return -ENOMEM;
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