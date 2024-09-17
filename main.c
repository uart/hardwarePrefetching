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

#include "common.h"
#include "mab.h"
#include "pmu.h"
#include "msr.h"
#include "log.h"
#include "rdt_mbm.h"
#include "sysdetect.h"

#define TAG "MAIN"

//which core is this in the 4 core module? i.e. 0..3
#define CORE_IN_MODULE ((tstate->core_id - core_first) % 4)

#define DDR_BW_NOT_SET (-1)
#define DDR_BW_AUTOTEST (-2)

struct thread_state gtinfo[MAX_THREADS]; //global thread state

//init variables
int ddr_bw_target = DDR_BW_NOT_SET; //MB/s (yes, bytes). Max _achievable_ bandwidth
float time_intervall = 1.0; //one second by default
int core_first = -1;
int core_last = -1;
float aggr = 1.0; //retuning aggressiveness
int tunealg = 0;
uint32_t rdt_enabled = 0;

#define ACTIVE_THREADS (core_last - core_first + 1)

//global runtime
volatile int quitflag = 0;
volatile int syncflag = 0;
volatile int msr_file_id[MAX_NUM_CORES];

struct ddr_s ddr;

void sigintHandler(int sig_num)
{
	//rework this to wake up the other threads and clean them up in a nice way
	loga(TAG, "sig %d, terminating dPF... hold on\n", sig_num);

	if (tunealg == MAB && (mstate.dynamic_sd == ON || mstate.dynamic_sd == STEP)) {
		free(mstate.ipc_buffer);
		free(mstate.sd_buffer);
		mstate.ipc_buffer = NULL;
		mstate.sd_buffer = NULL;
	}

	quitflag = 1;
	//sleep(time_intervall * 2); 
	if (rdt_enabled)
		rdt_mbm_reset();
	exit(1);
}

uint64_t time_ms()
{
    struct timespec time;

    clock_gettime(CLOCK_MONOTONIC, &time);

    return (uint64_t)(time.tv_nsec / 1000000) + ((uint64_t)time.tv_sec * 1000ull);
}


// Ref: Intel® 64 and IA-32 Architectures Software Developer Manuals - Volume 3
// Table 3-8 Information Returned by CPUID Instruction
int
lcpuid(const unsigned leaf, const unsigned subleaf, struct cpuid_out *out)
{
	if (out == NULL) {
		loge(TAG, "lcpuid(): NULL pointer error\n");
		return -1;
	}

        asm volatile("mov %4, %%eax\n\t"
                     "mov %5, %%ecx\n\t"
                     "cpuid\n\t"
                     "mov %%eax, %0\n\t"
                     "mov %%ebx, %1\n\t"
                     "mov %%ecx, %2\n\t"
                     "mov %%edx, %3\n\t"
                     : "=g"(out->eax), "=g"(out->ebx), "=g"(out->ecx),
                       "=g"(out->edx)
                     : "g"(leaf), "g"(subleaf)
                     : "%eax", "%ebx", "%ecx", "%edx");
	return 0;
}

int calculate_settings()
{
	if (tunealg != MAB) {
		uint64_t ddr_rd_bw; //only used for the first thread
		static uint64_t time_now, time_old = 0;
		float time_delta;


		//
		// Grab all PMU data
		//

		if (!rdt_enabled) {
			ddr_rd_bw = pmu_ddr(&ddr, DDR_RD_BW);
		} else
			ddr_rd_bw = rdt_mbm_bw_get();

		loga(TAG, "DDR RD BW: %ld MB/s\n", ddr_rd_bw/(1024*1024));

		if(time_old == 0){
			time_old = time_ms();
			return 0; //no selection the first time since all counters will be odd
		}

		time_now = time_ms();
		time_delta = (time_now - time_old) / 1000.0;
		time_old = time_now;

		float ddr_rd_percent = ((float)ddr_rd_bw/(1024*1024)) / (float)ddr_bw_target;
		ddr_rd_percent /= time_delta;
		loga(TAG, "Time delta %f, Running at %.1f percent rd bw (%ld MB/s)\n", time_delta, ddr_rd_percent * 100, ddr_rd_bw/(1024*1024));

	//	float l2_l3_ddr_hits[ACTIVE_THREADS];

		float l2_hitr[ACTIVE_THREADS];
		float l3_hitr[ACTIVE_THREADS];
		float good_pf[ACTIVE_THREADS];

		float core_contr_to_ddr[ACTIVE_THREADS];

		int total_ddr_hit = 0;
		for(int i = 0; i <ACTIVE_THREADS; i++)total_ddr_hit += gtinfo[i].pmu_result[3];

		for(int i = 0; i <ACTIVE_THREADS; i++){
			l2_hitr[i] = ((float)gtinfo[i].pmu_result[1]) / ((float)(gtinfo[i].pmu_result[1] + gtinfo[i].pmu_result[2] + gtinfo[i].pmu_result[3]));

			l3_hitr[i] = ((float)gtinfo[i].pmu_result[2]) / ((float)(gtinfo[i].pmu_result[2] + gtinfo[i].pmu_result[3]));

			core_contr_to_ddr[i] = ((float)gtinfo[i].pmu_result[3]) / ((float)total_ddr_hit);
			good_pf[i] = ((float)gtinfo[i].pmu_result[4]) / ((float)(gtinfo[i].pmu_result[1]) + (gtinfo[i].pmu_result[2]) + (gtinfo[i].pmu_result[3]));


			logd(TAG, "core %02d PMU delta LD: %10ld  HIT(L2: %.2f  L3: %.2f) DDRpressure: %.2f  GOODPF: %.2f\n", i, gtinfo[i].pmu_result[0],
				l2_hitr[i], l3_hitr[i], core_contr_to_ddr[i], good_pf[i]);

	//		logd(TAG, "   LD: %ld  HIT(L2: %ld  L3: %ld  DDR: %ld)  GOODPF: %ld\n", gtinfo[i].pmu_result[0], gtinfo[i].pmu_result[1],
	//			gtinfo[i].pmu_result[2], gtinfo[i].pmu_result[3], gtinfo[i].pmu_result[4]);
		}


		// 
		//Now we can make a decission...
		//
		// Below are two naive examples of tuning using the L2XQ respective L2 max distance parameter
		// All cores are set the same at this time
		//

		if(tunealg == 0){

			for(int i = 0; i <ACTIVE_THREADS; i++){
				int l2xq = msr_get_l2xq(&gtinfo[i].hwpf_msr_value[0]);
				int old_l2xq = l2xq;

				if(ddr_rd_percent < 0.10); //idle system
				else if(ddr_rd_percent < 0.20)l2xq += lround(-8 * aggr);
				else if(ddr_rd_percent < 0.30)l2xq += lround(-4 * aggr);
				else if(ddr_rd_percent < 0.40)l2xq += lround(-2 * aggr);
				else if(ddr_rd_percent < 0.50)l2xq += lround(-1 * aggr);
				else if(ddr_rd_percent < 0.60)l2xq += lround(-1 * aggr);
				else if(ddr_rd_percent < 0.70)l2xq += lround(-1 * aggr);
				else if(ddr_rd_percent < 0.80)l2xq += lround(-1 * aggr);
				else if(ddr_rd_percent < 0.90)l2xq += lround(1 * aggr);
				else if(ddr_rd_percent < 0.93)l2xq += lround(2 * aggr);
				else if(ddr_rd_percent < 0.96)l2xq += lround(4 * aggr);
				else l2xq += lround(8 * aggr);

				if(l2xq <= 0)l2xq = 1;
				if(l2xq > L2XQ_MAX)l2xq = L2XQ_MAX;

				if(old_l2xq != l2xq){
					msr_set_l2xq(&gtinfo[i].hwpf_msr_value[0], l2xq);
					gtinfo[i].hwpf_msr_dirty = 1;
					if(i == 0)logv(TAG, "l2xq %d\n", l2xq);
				}


				int l3xq = msr_get_l3xq(&gtinfo[i].hwpf_msr_value[0]);
				int old_l3xq = l3xq;

				if(ddr_rd_percent < 0.10); //idle system
				else if(ddr_rd_percent < 0.20)l3xq += lround(-8 * aggr);
				else if(ddr_rd_percent < 0.30)l3xq += lround(-4 * aggr);
				else if(ddr_rd_percent < 0.40)l3xq += lround(-2 * aggr);
				else if(ddr_rd_percent < 0.50)l3xq += lround(-1 * aggr);
				else if(ddr_rd_percent < 0.60)l3xq += lround(-1 * aggr);
				else if(ddr_rd_percent < 0.70)l3xq += lround(-1 * aggr);
				else if(ddr_rd_percent < 0.80)l3xq += lround(-1 * aggr);
				else if(ddr_rd_percent < 0.90)l3xq += lround(1 * aggr);
				else if(ddr_rd_percent < 0.93)l3xq += lround(2 * aggr);
				else if(ddr_rd_percent < 0.96)l3xq += lround(4 * aggr);
				else l3xq += lround(8 * aggr);

				if(l3xq <= 0)l3xq = 1;
				if(l3xq > L3XQ_MAX)l3xq = L3XQ_MAX;

				if(old_l3xq != l3xq){
					msr_set_l3xq(&gtinfo[i].hwpf_msr_value[0], l3xq);
					gtinfo[i].hwpf_msr_dirty = 1;
					if(i == 0)logv(TAG, "l3xq %d\n", l3xq);
				}
			}


		} //if(tunealg)...
		else if(tunealg == 1){
			logd(TAG, "L2HR %.2f %.2f %.2f %.2f  %.2f %.2f %.2f %.2f  %.2f %.2f %.2f %.2f  %.2f %.2f %.2f %.2f\n", l2_hitr[0], l2_hitr[1], l2_hitr[2], l2_hitr[3], l2_hitr[4],
				l2_hitr[5], l2_hitr[6], l2_hitr[7], l2_hitr[8], l2_hitr[9], l2_hitr[10], l2_hitr[11], l2_hitr[12], l2_hitr[13], l2_hitr[14], l2_hitr[15]);

			for(int i = 0; i <ACTIVE_THREADS; i++){
				int l2maxdist = msr_get_l2maxdist(&gtinfo[i].hwpf_msr_value[0]);
				int old_l2maxdist = l2maxdist;

				if(ddr_rd_percent < 0.10); //idle system
				else if(ddr_rd_percent < 0.20)l2maxdist += lround(+8 * aggr);
				else if(ddr_rd_percent < 0.30)l2maxdist += lround(+4 * aggr);
				else if(ddr_rd_percent < 0.40)l2maxdist += lround(+2 * aggr);
				else if(ddr_rd_percent < 0.50)l2maxdist += lround(+1 * aggr);
				else if(ddr_rd_percent < 0.60)l2maxdist += lround(+1 * aggr);
				else if(ddr_rd_percent < 0.70)l2maxdist += lround(+1 * aggr);
				else if(ddr_rd_percent < 0.80)l2maxdist += lround(+1 * aggr);
				else if(ddr_rd_percent < 0.90)l2maxdist += lround(-1 * aggr);
				else if(ddr_rd_percent < 0.93)l2maxdist += lround(-2 * aggr);
				else if(ddr_rd_percent < 0.96)l2maxdist += lround(-4 * aggr);
				else l2maxdist += lround(-8 * aggr);

				if(l2maxdist <= 0)l2maxdist = 1;
				if(l2maxdist > L2MAXDIST_MAX)l2maxdist = L2MAXDIST_MAX;

				if(old_l2maxdist != l2maxdist){
					msr_set_l2maxdist(&gtinfo[i].hwpf_msr_value[0], l2maxdist);
					gtinfo[i].hwpf_msr_dirty = 1;
					if(i == 0)logv(TAG, "l2maxdist %d\n", l2maxdist);
				}

				int l3maxdist = msr_get_l3maxdist(&gtinfo[i].hwpf_msr_value[0]);
				int old_l3maxdist = l3maxdist;

				if(ddr_rd_percent < 0.10); //idle system
				else if(ddr_rd_percent < 0.20)l3maxdist += lround(+8 * aggr);
				else if(ddr_rd_percent < 0.30)l3maxdist += lround(+4 * aggr);
				else if(ddr_rd_percent < 0.40)l3maxdist += lround(+2 * aggr);
				else if(ddr_rd_percent < 0.50)l3maxdist += lround(+1 * aggr);
				else if(ddr_rd_percent < 0.60)l3maxdist += lround(+1 * aggr);
				else if(ddr_rd_percent < 0.70)l3maxdist += lround(+1 * aggr);
				else if(ddr_rd_percent < 0.80)l3maxdist += lround(+1 * aggr);
				else if(ddr_rd_percent < 0.90)l3maxdist += lround(-1 * aggr);
				else if(ddr_rd_percent < 0.93)l3maxdist += lround(-2 * aggr);
				else if(ddr_rd_percent < 0.96)l3maxdist += lround(-4 * aggr);
				else l3maxdist += lround(-8 * aggr);

				if(l3maxdist <= 0)l3maxdist = 1;
				if(l3maxdist > L3MAXDIST_MAX)l3maxdist = L3MAXDIST_MAX;

				if(old_l3maxdist != l3maxdist){
					msr_set_l3maxdist(&gtinfo[i].hwpf_msr_value[0], l3maxdist);
					gtinfo[i].hwpf_msr_dirty = 1;
					if(i == 0)logv(TAG, "l3maxdist %d\n", l3maxdist);
				}

			}
		}
	}
	else if (tunealg == MAB){
		mab(&mstate);
	}

	return 0;
}

static void *thread_start(void *arg)
{
	struct thread_state *tstate = arg;
	int msr_file;
	uint64_t pmu_old[PMU_COUNTERS], pmu_new[PMU_COUNTERS];
	uint64_t instructions_old = 0, instructions_new = 0, cpu_cycles_old = 0, cpu_cycles_new = 0;

	logd(TAG, "Thread running on core %d, this is #%d core in the module\n", tstate->core_id, CORE_IN_MODULE);

//	struct cpuid_out res;

//	lcpuid(0x1a, 0, &res);
//	logd(TAG, "CPUID Coretype 0x%X\n", res.eax);

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(tstate->core_id, &cpuset);

	int s = pthread_setaffinity_np(tstate->thread_id, sizeof(cpuset), &cpuset);
	if (s != 0){
		loge(TAG, "Could not set thread affinity for coreid %d, pthread_setaffinity_np()\n", tstate->core_id);
	}

	msr_file = msr_init(tstate->core_id, tstate->hwpf_msr_value);

	msr_hwpf_write(msr_file, tstate->hwpf_msr_value);

	msr_enable_fixed(msr_file);
	pmu_core_config(msr_file);

	// Run until end of world...
	while(quitflag == 0){
		usleep(time_intervall * 1000000);
		//logd(TAG, "1. Read Core PMU counters and update stats\n");

		if (tunealg != MAB) {
			for(int i = 0; i < PMU_COUNTERS; i++)pmu_old[i] = pmu_new[i];
		}
		else {
			instructions_old = instructions_new;
			cpu_cycles_old = cpu_cycles_new;
		}

		pmu_core_read(msr_file, pmu_new, &instructions_new, &cpu_cycles_new);


			if (tunealg != MAB) {
				for(int i = 0; i < PMU_COUNTERS; i++)tstate->pmu_result[i] = pmu_new[i] - pmu_old[i];
			}
			else {
				tstate->instructions_retired = instructions_new - instructions_old;
				tstate->cpu_cycles = cpu_cycles_new - cpu_cycles_old;
			}

		atomic_fetch_add(&syncflag, 1); //sync by increasing syncflag

		//select out the master core
		if(tstate->core_id == core_first){
			//wait for all threads
			while(syncflag < ACTIVE_THREADS);

			calculate_settings();


			syncflag = 0; //done, release threads
		}else if(CORE_IN_MODULE == 0){ //only the primary core per module needs to sync, rest can run free
			while(syncflag != 0); //wait for decission to be made by master
		}

		//logd(TAG, "3. Use decission to update MSRs\n");
		if(CORE_IN_MODULE == 0 && tstate->hwpf_msr_dirty == 1){
			tstate->hwpf_msr_dirty = 0;

			if (tunealg == MAB) {
				msr_hwpf_write(msr_file, arms.hwpf_msr_values[mstate.arm]);
			}
			else {
				msr_hwpf_write(msr_file, tstate->hwpf_msr_value);
			}
		}
	}

	close(msr_file);
	logi(TAG, "Thread on core %d done\n", tstate->core_id);

	return 0;
}

//trigger max BW writes with all threads we have, this can be used if no target DDR BW has been set
//we could also derive a good value from DDR config
int measure_max_ddr_bw()
{
//TBD
	return 0;
}

void print_usage()
{
	printf("\n*** System settings:\n");
	printf("Default is to auto-detect Atom E-cores and both Hybrid Clients and E-core servers are supported.\n");
	printf("The --core argument can be used to direct dPF on only a specific set of cores.\n");
	printf(" -c --core - set cores to use dPF. Starting from core id 0, e.g. 8-15 for the 9th to 16th core.\n");
	printf("   --core 8-15\n");
	printf("\nDDR Bandwith is by default auto-detected based on DMI/BIOS information and target is set to 70%% of\n");
	printf("theorethical max bandwidth which is typically the achivable bandwidth.\n");
	printf(" -d --ddrbw-auto - set DDR bandwith from DMI/BIOS to a specific percentage of max. Default is 0.70 (70%%).\n");
	printf("   --ddrbw-auto 0.65\n");
	printf(" -t --ddrbw-test - set DDR bandwidth by performing a quick bandwidth test.\n");
	printf("   --ddrbw-test\n");
	printf("   Note that this gives a short but high load on the memory subsystem.\n");
	printf(" -D --ddrbw-set - set DDR bandwidth target in MB/s. This should be the max achievable.\n");
	printf("   --ddrbw-set 46000\n");

	printf("\n*** Algorithm tuning:\n");
	printf(" -i --intervall - update interval in seconds (1-60), default: 1\n");
	printf("   --intervall 2\n");
	printf(" -A --alg - set tune algorithm, default 0\n");
	printf("   --alg 2\n");
	printf(" -a --aggr - set retune aggressiveness (0.1 - 5.0), default 1.0\n");
	printf("   --aggr 2.0\n");

	printf("\n*** Misc:\n");
	printf(" -l --log - set loglevel 1 - 5 (5=debug), default: 3\n");
	printf("   --log 3\n");
	printf(" -h --help - lists these arguments\n");
}

int main(int argc, char *argv[])
{
	float ddr_bw_auto_utilization = 0.7;

	log_setlevel(3);
	loga(TAG, "This is the main file for the UU Hardware Prefetch and Control project\n");

	signal(SIGINT, sigintHandler);

/*	struct cpuid_out res;

	lcpuid(0x01, 0, &res);
	logi(TAG, "CPUID Platform: 0x%X\n", res.eax);
	lcpuid(0x07, 0, &res);
	if (res.edx & (1 << 15))
		logi(TAG, "Hybrid CPU Detected\n");
	else
		logi(TAG, "Not Hybrid CPU\n");
*/

	//decode command-line
	while (1) {
		static struct option long_options[] = {
			{"core",	required_argument,	0, 'c'},
			{"ddrbw-auto",	required_argument,	0, 'd'},
			{"ddrbw-test",	no_argument,		0, 't'},
			{"ddrbw-set",	required_argument,	0, 'D'},
			{"intervall",	required_argument,	0, 'i'},
			{"alg",		required_argument,	0, 'A'},
			{"aggr",	required_argument,	0, 'a'},
			{"log",		required_argument,	0, 'l'},
			{"help",	no_argument, 		0, 'h'},
			{NULL, 		no_argument, 		0, 0},
		};

		int option_index = 0;
		int c = getopt_long(argc, argv, "c:d:tD:i:A:a:l:h", long_options, &option_index);
		// end of options
		if (c == -1) break;

		switch(c) {
			case 'c': //core
                        	core_first = strtol(optarg, 0, 10);
				if(strstr(optarg,"-") == NULL)core_last = core_first;
				else core_last = strtol(strstr(optarg,"-") + 1, 0, 10);

				logi(TAG, "Cores: %d -> %d = %d threads\n", core_first, core_last, core_last - core_first + 1);

				if(core_last - core_first > MAX_THREADS){
					loge(TAG, "Too many cores, max is %d\n", MAX_THREADS);
					return -1;
				}
                        break;

			case 'd': //ddrbw-auto
				//override the 70% utilization factor
				ddr_bw_auto_utilization = strtof(optarg, NULL);
                        break;

			case 't': //ddrbw-test
				ddr_bw_target = DDR_BW_AUTOTEST; //let's auto-test this
                        break;

			case 'D': //ddrbw-set
				ddr_bw_target = strtol(optarg, 0, 10);
                        break;

			case 'i': //intervall
				time_intervall = strtof(optarg, NULL);
				if(time_intervall < 0.0001f)time_intervall = 0.0001f;
				if(time_intervall > 60.0f) time_intervall = 60.0f;
                        break;

			case 'A': //alg
				tunealg = strtol(optarg, 0, 10);
                        break;

			case 'a': //aggr
				aggr = strtof(optarg, 0);
                        break;

			case 'l': //log
				log_setlevel(strtol(optarg, 0, 10));
                        break;

			case '?': //getopt returns unknown argument
			case 'h': //help
				print_usage();
				return 0;
                        break;
		default:
			loge(TAG, "Error, strange command-line argument %d\n", c);
			return -1;
		}
	}

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
		logi(TAG, "RDT MBM not supported\n");
		pmu_ddr_init(&ddr);
	}

	if (tunealg == 2) {mab_init(&mstate, ACTIVE_THREADS);}

//	printf("main mmap_ddr0 %p\n", mmap_ddr0);
//	pmu_ddr_rd(&mmap_ddr0, &mmap_ddr1);

	//--core has not been used, so let's autodetect
	if(core_first == -1 || core_last == -1){
		//auto-detect Atom E-cores and set first/last core to max E-cores
		struct e_cores_layout_s e_cores;
		e_cores = get_efficient_core_ids();
		core_first = e_cores.first_efficiency_core;
		core_last = e_cores.last_efficiency_core;

		if(core_first == -1 || core_last == -1){
			loge(TAG, "Error, no cores to run on! Do you have Atom E-cores??\n");

			return -1;
		}
	}

	if(ddr_bw_target == DDR_BW_NOT_SET){
		ddr_bw_target = dmi_get_bandwidth() * ddr_bw_auto_utilization;
		logv(TAG, "DDR BW target set to %d MB/s\n", ddr_bw_target);

		if(ddr_bw_target == -1){
			loge(TAG, "Error, no DDR bandwidth set or detected!\n");

			return -1;
		}
	}

	//Initialization done - let's start running...

	for(int tnum = 0; tnum <= (core_last - core_first); tnum++){
		gtinfo[tnum].core_id = core_first + tnum;
		pthread_create(&gtinfo[tnum].thread_id, NULL, &thread_start, &gtinfo[tnum]);
	}

	//Run forever or until all threads are returning, then we wrap up

	void * ret;
	pthread_join(gtinfo[0].thread_id, &ret);

	close(ddr.mem_file);

	rdt_mbm_reset();
	loga(TAG, "dpf finished\n");

	return 0;
}

