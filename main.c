#define _GNU_SOURCE
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <termios.h>

#include "common.h"
#include "primitive.h"
#include "mab.h"
#include "pmu_core.h"
#include "pmu_ddr.h"
#include "rdt_mbm.h"
#include "msr.h"
#include "log.h"
#include "sysdetect.h"
#include "pcie.h"
#include "user_api.h"

#include "json_parser.h"

#define TAG "MAIN"

//which core is this in the 4 core module? i.e. 0..3
#define CORE_IN_MODULE ((tstate->core_id - core_first) % 4)
#define ACTIVE_THREADS (core_last - core_first + 1)

#define DDR_BW_NOT_SET (-1)
#define DDR_BW_AUTOTEST (-2)

struct thread_state gtinfo[MAX_THREADS]; // global thread state
static struct perf_event_attr event_attrs[MAX_EVENTS];


//init variables
int ddr_bw_target = DDR_BW_NOT_SET; //MB/s (yes, bytes). Max _achievable_
// bandwidth
float time_intervall = 1.0; //one second by default
int core_first = -1;
int core_last = -1;
float aggr = 1.0; //retuning aggressiveness
int tunealg = 0;
uint32_t rdt_enabled = 0;
int num_events;
int pmu_method = PMU_RAW;
int kernel_mode = 0;
int enable_pmu_msg = 0;
int enable_msr_msg = 0;

//global runtime
volatile int quitflag = 0;
volatile int syncflag = 0;
volatile int ddrbwflag = 0;
volatile int msr_file_id[MAX_NUM_CORES];

int core_priority[MAX_THREADS]; // Array to store the priority values
int core_count;

struct ddr_s ddr;

void sigintHandler(int sig_num)
{
	//rework this to wake up the other threads and clean them up in a nice way
	loga(TAG, "sig %d, terminating dPF... hold on a second...\n", sig_num);

	if (tunealg == MAB && (mstate.dynamic_sd == ON ||
		mstate.dynamic_sd == STEP)) {
		free(mstate.ipc_buffer);
		free(mstate.sd_buffer);
		mstate.ipc_buffer = NULL;
		mstate.sd_buffer = NULL;
	}

	quitflag = 1;
	//sleep(time_intervall * 2);
	if (rdt_enabled)
		rdt_mbm_reset();
//	exit(1);
}

uint64_t time_ms(void)
{
	struct timespec time;

	clock_gettime(CLOCK_MONOTONIC, &time);

	return (uint64_t)(time.tv_nsec / 1000000)
	+((uint64_t)time.tv_sec * 1000ull);
}


int calculate_settings(void)
{
	if (tunealg == 0 || tunealg == 1)
		basicalg(tunealg);
	else if (tunealg == MAB)
		mab(&mstate);

	return 0;
}

static void *thread_start(void *arg)
{
	struct thread_state *tstate = arg;
	int msr_file;
	uint64_t pmu_new[MAX_EVENTS] = {0};
	uint64_t pmu_old[MAX_EVENTS] = {0};
	uint64_t instructions_new = 0, instructions_old = 0;
	uint64_t cpu_cycles_new = 0, cpu_cycles_old = 0;
	int event_fds[MAX_EVENTS];

	logd(TAG, "Thread running on core %d, this is #%d core in the module\n", tstate->core_id, CORE_IN_MODULE);

	cpu_set_t cpuset;

	CPU_ZERO(&cpuset);
	CPU_SET(tstate->core_id, &cpuset);

	int s = pthread_setaffinity_np
		(tstate->thread_id, sizeof(cpuset), &cpuset);
	if (s != 0)
		loge(TAG, "Could not set thread affinity for coreid %d, pthread_setaffinity_np()\n", tstate->core_id);

	if (ddr_bw_target == DDR_BW_AUTOTEST) {
		if (tstate->core_id == core_first) {
			if (ddrmembw_init() < 0)
				exit(-1);
			ddr_bw_target = 0;
		}

		atomic_fetch_add(&ddrbwflag, 1);

		while (ddrbwflag < ACTIVE_THREADS);

		// BW test assuming idle system. Wiil add ddr PMU counters for
		// proper measurement
		atomic_fetch_add(&ddr_bw_target, ddrmembw_measurement());

		atomic_fetch_add(&ddrbwflag, -1);

		while (ddrbwflag != 0);

		if (tstate->core_id == core_first) {
			logv(TAG, "bandwidth %d MB/s\n", ddr_bw_target);
			ddrmembw_deinit();
			if (ddr_bw_target == 0)
				exit(-1);
		}
	}


	msr_file = msr_init(tstate->core_id, tstate->hwpf_msr_value);

	msr_hwpf_write(msr_file, tstate->hwpf_msr_value);

	msr_enable_fixed(msr_file);

	// Initialize based on PMU method
	if (pmu_method == PMU_RAW) {
		pmu_core_config(msr_file);
	} else if (pmu_method == PMU_PERF) {
		perf_init(event_attrs, event_fds, num_events, tstate->core_id);
	}

	// Run until end of world...
	while (quitflag == 0) {
		usleep(time_intervall * 1000000);
		//logd(TAG, "1. Read Core PMU counters and update stats\n");

		if (tunealg != MAB) {
			for (int i = 0; i < PMU_COUNTERS; i++)
				pmu_old[i] = pmu_new[i];
		} else {
			instructions_old = instructions_new;
			cpu_cycles_old = cpu_cycles_new;
		}

		// Read PMU counters based on method
		if (pmu_method == PMU_RAW) {
			pmu_core_read(msr_file, pmu_new, &instructions_new, &cpu_cycles_new);
		} else if (pmu_method == PMU_PERF) {
			perf_read(event_fds, pmu_new, num_events);
			// Extract instructions and cycles like PMU_RAW
			instructions_new =
			    pmu_new[PERF_INDEX_EVENT_INSTRUCTIONS];
			cpu_cycles_new = pmu_new[PERF_INDEX_EVENT_CYCLES];
		}

		if (tunealg != MAB) {
			for (int i = 0; i < PMU_COUNTERS; i++)
				tstate->pmu_result[i] =
				    pmu_new[i] - pmu_old[i];
		} else {
			tstate->instructions_retired =
			    instructions_new - instructions_old;
			tstate->cpu_cycles =
			    cpu_cycles_new - cpu_cycles_old;
		}

		atomic_fetch_add(&syncflag, 1); // sync by increasing syncflag

		//select out the master core
		if (tstate->core_id == core_first) {
			//wait for all threads
			while (syncflag < ACTIVE_THREADS);

			calculate_settings();


			syncflag = 0; //done, release threads
		} else if (CORE_IN_MODULE == 0) {
			//only the primary core per module needs to sync,
			// rest can run free
			while (syncflag != 0);
				//wait for decission to be made by master
		}

		//logd(TAG, "3. Use decission to update MSRs\n");
		if (CORE_IN_MODULE == 0 && tstate->hwpf_msr_dirty == 1) {
			tstate->hwpf_msr_dirty = 0;

			if (tunealg == MAB)
				msr_hwpf_write(msr_file,
					arms.hwpf_msr_values[mstate.arm]);
			else
				msr_hwpf_write(msr_file,
					tstate->hwpf_msr_value);
		}
	}

        // Before pthread_exit or return
	if (pmu_method == PMU_PERF) {
		perf_deinit(event_fds, num_events);
	}
	close(msr_file);
	logi(TAG, "Thread on core %d done\n", tstate->core_id);

	return 0;
}

void print_usage(void)
{
	printf("\n*** System settings:\n");
	printf("Default is to auto-detect Atom E-cores and both Hybrid Clients "
	       "and E-core servers are supported.\n");
	printf("The --core argument can be used to direct dPF on only a "
	       "specific set of cores.\n");
	printf(" -c --core - set cores to use dPF. Starting from core id 0, eg."
	       " 8-15 for the 9th to 16th core.\n");
	printf("   --core 8-15\n");
	printf("\nDDR Bandwith is by default auto-detected based on DMI/BIOS"
	       "information and target is set to 70%% of\n");
	printf("theorethical max bandwidth which is typically the achivable "
	       "bandwidth.\n");
	printf(" -d --ddrbw-auto - set DDR bandwith from DMI/BIOS to a specific"
	       "percentage of max. Default is 0.70 (70%%).\n");
	printf("   --ddrbw-auto 0.65\n");
	printf(" -t --ddrbw-test - set DDR bandwidth by performing a quick "
	       "bandwidth test.\n");
	printf("   --ddrbw-test\n");
	printf("   Note that this gives a short but high load on the memory "
	       "subsystem.\n");
	printf(" -D --ddrbw-set - set DDR bandwidth target in MB/s. This should"
	       "be the max achievable.\n");
	printf("   --ddrbw-set 46000\n");
	printf("The -w or --weight argument can be used to set the priority "
	       "level of each core.\n");
	printf(" -w --weight - set core priorities by providing a "
	       "comma-separated list of integers.\n");
	printf("   Core priority determines the importance of each core's "
	       "workload. A higher value means\n");
	printf("   the core is given more CPU time relative to lower-priority "
	       "cores. Valid values range from\n");
	printf("   0 to 99, where 99 is the highest priority and 0 is the "
	       "lowest.\n");
	printf("   The number of values should match the number of active "
	       "cores. If fewer values are provided,\n");
	printf("   the remaining cores will default to a priority of 50.\n");
	printf("   --weight 55,43,99,80\n");

	printf("\n*** Algorithm tuning:\n");
	printf(" -i --intervall - update interval in seconds (1-60), default: "
	       "1\n");
	printf("   --intervall 2\n");
	printf(" -A --alg - set tune algorithm, default 0\n");
	printf("   --alg 2\n");
	printf(" -p --perf - use perf events for PMU monitoring (default: "
		"raw PMU)\n");
	printf("  --perf\n");
	printf(" -a --aggr - set retune aggressiveness (0.1 - 5.0), default 1."
		"0\n");
	printf("   --aggr 2.0\n");

	printf("\n*** Misc:\n");
	printf(" -l --log - set loglevel 1 - 5 (5=debug), default: 3\n");
	printf("   --log 3\n");
	printf(" -h --help - lists these arguments\n");
}

// parse_weights - Parses and validates core priorities from a comma-separated
// string. Sets priorities for each core, using default for any missing entries.
// @weights_args: Comma-separated core priorities. Returns 0 on success, or -1
// for invalid input (non-integer or out-of-range values).
int parse_weights(char *weights_args)
{
	char *token, *endptr;

	token = strtok(weights_args, ",");

	while (token != NULL) {
		if (core_count == ACTIVE_THREADS)
			break;

		int priority = strtol(token, &endptr, 10);

		if (*endptr != '\0') {
			loge(TAG, "Invalid input '%s', not a number\n",
				token);
			return -1;
		}

		if (priority < MIN_PRIORITY || priority > MAX_PRIORITY) {
			loge(TAG, "Priority %d is out of range (%d-%d)\n",
				priority, MIN_PRIORITY, MAX_PRIORITY);
			return -1;
		}

		// Assign the valid priority to the current core
		core_priority[core_count] = priority;
		core_count++;
		token = strtok(NULL, ",");
	}

	// If core_count is less than total cores,
	// then set the other cores to 50
	while (core_count < ACTIVE_THREADS) {
		core_priority[core_count] = DEFAULT_PRIORITY;
		core_count++;
	}

	logd(TAG, "Core Priorities:\n");
	for (int i = 0; i < core_count; i++)
		logd(TAG, "Core %d Priority: %d\n", i, core_priority[i]);

	return 0;
}

// Non-blocking check for 'q' or 'Q'
int kbhit(void)
{
	struct timeval tv = {0L, 0L};
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);

	return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
}

int main(int argc, char *argv[])
{
	int json_argc = 0;
	char **json_argv = NULL;

	char weight_string[MAX_WEIGHT_STR_LEN] = {0};
	float ddr_bw_auto_utilization = 0.7;

	for (int i = 0; i < MAX_THREADS; i++)
		core_priority[i] = MIN_PRIORITY;

	log_setlevel(3);
	loga(TAG, "This is the main file for the UU Hardware Prefetch and Control project\n");

	signal(SIGINT, sigintHandler);

	pcie_init();

	if (argc == 1) {
		if (json_init(&json_argv) < 0)
			return -1;

		// Parse the JSON configuration file
		json_argc = json_parse("config.json", argv, json_argv);
		if (json_argc < 0) {
			logi(TAG, "Unable to parse JSON configuration file\n");
			json_deinit(json_argv);
			return -1;
		}
	}

	// decode command-line
	while (1) {
		static struct option long_options[] = {
		    {"core", required_argument, 0, 'c'},
		    {"ddrbw-auto", required_argument, 0, 'd'},
		    {"ddrbw-test", no_argument, 0, 't'},
		    {"ddrbw-set", required_argument, 0, 'D'},
		    {"intervall", required_argument, 0, 'i'},
		    {"alg", required_argument, 0, 'A'},
		    {"aggr", required_argument, 0, 'a'},
		    {"log", required_argument, 0, 'l'},
		    {"weight", required_argument, 0, 'w'},
		    {"kernelmode", no_argument, 0, 'k'},
		    {"perf", no_argument, 0, 'p'},
		    {"msr", no_argument, 0, 'm'},
		    {"pmu", no_argument, 0, 'P'},
		    {"help", no_argument, 0, 'h'},
		    {NULL, no_argument, 0, 0},
		};

		int option_index = 0;
		int c;

		if (json_argc > 0) {
			c = getopt_long(json_argc, json_argv, "c:d:tD:i:A:a:l:w:ph:kPm", long_options, &option_index);
		} else {
			c = getopt_long(argc, argv, "c:d:tD:i:A:a:l:w:ph:kPm",
					long_options, &option_index);
		}

		// end of options
		if (c == -1)
			break;

		switch (c) {
		case 'c': // core
			core_first = strtol(optarg, 0, 10);
			if (strstr(optarg, "-") == NULL)
				core_last = core_first;
			else
				core_last = strtol(strstr(optarg, "-") + 1, 0, 10);

			logi(TAG, "Cores: %d -> %d = %d threads\n", core_first,
			     core_last, core_last - core_first + 1);

			if (core_last - core_first > MAX_THREADS) {
				loge(TAG, "Too many cores, max is %d\n",
				     MAX_THREADS);
				return -1;
			}
			break;

		case 'd': // ddrbw-auto
			// override the 70% utilization factor
			ddr_bw_auto_utilization = strtof(optarg, NULL);
			break;

		case 't': // ddrbw-test
			ddr_bw_target = DDR_BW_AUTOTEST;
			// let's auto-test this
			break;

		case 'D': // ddrbw-set
			ddr_bw_target = strtol(optarg, 0, 10);
			break;

		case 'i': // intervall
			time_intervall = strtof(optarg, NULL);
			if (time_intervall < 0.0001f)
				time_intervall = 0.0001f;
			if (time_intervall > 60.0f)
				time_intervall = 60.0f;
			break;

		case 'A': // alg
			tunealg = strtol(optarg, 0, 10);
			break;

		case 'a': // aggr
			aggr = strtof(optarg, 0);
			break;

		case 'k': // kernelmode
			kernel_mode = 1;
			break;

		case 'l': // log
			log_setlevel(strtol(optarg, 0, 10));
			break;

		case 'w': // weight
			strncpy(weight_string, optarg, MAX_WEIGHT_STR_LEN - 1);
			weight_string[MAX_WEIGHT_STR_LEN - 1] = '\0';
			break;

		case 'p':
			pmu_method = PMU_PERF;
			perf_configure_events(event_attrs, &num_events);
			break;

		case 'm': // MSR
			enable_msr_msg = 1;
			logi(TAG, "MSR logging enabled\n");
			break;

		case 'P': // PMU
			enable_pmu_msg = 1;
			logi(TAG, "PMU logging enabled\n");
			break;

		case '?': // getopt returns unknown argument
		case 'h': // help
			print_usage();
			return 0;
			break;
		default:
			loge(TAG, "Error, strange command-line argument %d\n",
			     c);
			return -1;
		}
	}

	if (json_argc > 0)
		json_deinit(json_argv);

	//--core has not been used, so let's autodetect
	if (core_first == -1 || core_last == -1) {
		// auto-detect Atom E-cores and set first/last core to max
		//  E-cores
		struct e_cores_layout_s e_cores;

		e_cores = get_efficient_core_ids();
		core_first = e_cores.first_efficiency_core;
		core_last = e_cores.last_efficiency_core;

		if (core_first == -1 || core_last == -1) {
			loge(TAG, "Error, no cores to run on! Do you have Atom "
				  "E-cores??\n");

			return -1;
		}
	}

	// If weight was provided, parse the values into array
	// core_priority[MAX_THREADS]
	if (strlen(weight_string) != 0) {
		if (parse_weights(weight_string) < 0)
			return -1;
		if (kernel_mode == 1) {
			if (kernel_set_core_weights(core_count,
					core_priority) < 0)
				return -1;
		}
	} else {
		for (int i = 0; i < ACTIVE_THREADS; i++)
			core_priority[i] = DEFAULT_PRIORITY;
	}

	//--ddrbw-set / ddrbw-test has not been used, so use ddrbw-auto
	if (ddr_bw_target == DDR_BW_NOT_SET) {
		ddr_bw_target = dmi_get_bandwidth() * ddr_bw_auto_utilization;
		logv(TAG, "DDR BW target set to %d MB/s\n", ddr_bw_target);

		if (ddr_bw_target == -1) {
			loge(TAG, "Error, no DDR bandwidth set or detected!\n");

			return -1;
		}
	}

	if (kernel_mode == 1) {

		if (kernel_mode_init() < 0)
			return -1;

		if (core_first != -1 || core_last != -1) {
			if (kernel_core_range(core_first, core_last) < 0) {
				loge(TAG, "Failed to configure core range\n");
				return -1;
			}
		}

		if (ddr_bw_target != DDR_BW_NOT_SET && ddr_bw_target != DDR_BW_AUTOTEST) {
			if (kernel_set_ddr_bandwidth(ddr_bw_target) < 0) {
				loge(TAG, "Failed to set DDR bandwidth\n");
				return -1;
			}
		}

		logi(TAG, "Entering kernel mode tuning, press 'Q' to quit, 'm' "
			  "to toggle MSR logging, 'p' to toggle PMU logging\n");

		if (kernel_tuning_control(1) < 0)
			return -1;

		struct termios oldt, newt;
		tcgetattr(STDIN_FILENO, &oldt);
		newt = oldt;
		newt.c_lflag &= ~(ICANON | ECHO);
		tcsetattr(STDIN_FILENO, TCSANOW, &newt);

		while (1) {
			usleep(100000);
			if (kbhit()) {
				int ch = getchar();
				if (ch == 'q' || ch == 'Q') {
					break;
				} else if (ch == 'm' || ch == 'M') {
					enable_msr_msg = !enable_msr_msg;
					logi(TAG, "MSR logging %s\n", enable_msr_msg ? "enabled" : "disabled");
				} else if (ch == 'p' || ch == 'P') {
					enable_pmu_msg = !enable_pmu_msg;
					logi(TAG, "PMU logging %s\n", enable_pmu_msg ? "enabled" : "disabled");
				}
			}

			for (int core_id = core_first;
				core_id <= core_last; core_id++) {
				if (enable_msr_msg == 1) {
					if (kernel_log_msr_values(core_id) < 0) {
						loge(TAG, "Error reading MSR values for core %d\n", core_id);
					}
				}
				if (enable_pmu_msg == 1) {
					if (kernel_log_pmu_values(core_id) < 0) {
						loge(TAG, "Error reading PMU values for core %d\n", core_id);
					}
				}
			}

			fflush(stdout);

		}

		tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

		if (kernel_tuning_control(0) < 0)
			return -1;
		logi(TAG, "Leaving kernel mode tuning - exiting dPF\n");
		pcie_deinit();

		return 0;
	}

	// Initialize DDR PMU
	if (pmu_ddr_init(&ddr) == DDR_NONE) {
		// lets try RDT instread

		// DDR init, with RDT if supported (servers)
		int ret_val = rdt_mbm_support_check();

		if (!ret_val) {
			logi(TAG, "RDT MBM supported\n");
			ret_val = rdt_mbm_init();
			if (ret_val) {
				loge(TAG, "Error in initializing RDT MBM\n");
				return ret_val;
			}
			rdt_enabled = 1;
		} else {
			loge(TAG, "Neither DDR nor RDT support was found\n");
			return -1;
		}
	}

	// Algorithm init
	if (tunealg == 2)
		mab_init(&mstate, ACTIVE_THREADS);

	// Initialization done - let's start running...

	for (int tnum = 0; tnum <= (core_last - core_first); tnum++) {
		gtinfo[tnum].core_id = core_first + tnum;
		pthread_create(&gtinfo[tnum].thread_id, NULL,
			       &thread_start, &gtinfo[tnum]);
	}

	// Run forever or until all threads are returning, then we wrap up

	void *ret;

	pthread_join(gtinfo[0].thread_id, &ret);

	close(ddr.mem_file);

	rdt_mbm_reset();
	pcie_deinit();
	loga(TAG, "dpf finished\n");

	return 0;
}

