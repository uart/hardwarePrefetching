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

#include "pmu.h"
#include "msr.h"
#include "log.h"

#define TAG "MAIN"

//which core is this in the 4 core module? i.e. 0..3
#define CORE_IN_MODULE ((tstate->core_id - core_first) % 4)

#define MAX_THREADS (1024)

struct thread_state {
	pthread_t thread_id; // from pthread_create()
	int core_id;
	int hwpf_msr_dirty; //0 not updated, 1 updated
	union msr_u hwpf_msr_value[HWPF_MSR_FIELDS]; //0... -> 0x1320...
	uint64_t pmu_result[PMU_COUNTERS]; //delta since last read
};

struct thread_state gtinfo[MAX_THREADS]; //global thread state

//init variables
int ddr_bw_target = 40000; //MBps (yes, bytes)  //this should be auto-tuned if not set at cmd-line
int time_intervall = 1; //one second by default
int core_first = 0;
int core_last = 0;
float aggr = 1.0; //retuning aggressiveness
int tunealg = 0;

#define ACTIVE_THREADS (core_last - core_first + 1)

//global runtime
volatile int quitflag = 0;
volatile int syncflag = 0;


struct ddr_s ddr;

void sigintHandler(int sig_num)
{
	//rework this to wake up the other threads and clean them up in a nice way
	printf("sig %d, terminating dPF... hold on\n", sig_num);
	quitflag = 1;
	//sleep(time_intervall * 2); 
	exit(1);
}

uint64_t time_ms()
{
    struct timespec time;

    clock_gettime(CLOCK_MONOTONIC, &time);

    return (uint64_t)(time.tv_nsec / 1000000) + ((uint64_t)time.tv_sec * 1000ull);
}

//https://www.intel.com/content/www/us/en/developer/articles/guide/12th-gen-intel-core-processor-gamedev-guide.html
int get_cpuid(uint64_t leaf, uint64_t *rax, uint64_t *rbx, uint64_t *rcx, uint64_t *rdx)
{
	asm volatile (
		"\txchg %%rbx, %%rdi\n"
		"\tcpuid\n"
 		"\txchg %%rbx, %%rdi"
		: "=a" (*rax), "=D" (*rbx), "=c" (*rcx), "=d" (*rdx)
		: "a" (leaf));

	return 0;
}

int calculate_settings()
{
	uint64_t ddr_rd_bw; //only used for the first thread
	static uint64_t time_now, time_old = 0;
	float time_delta;


	//
	// Grab all PMU data
	//

	ddr_rd_bw = pmu_ddr(&ddr, DDR_RD_BW);
	//logd(TAG, "DDR RD BW: %ld MB/s\n", ddr_rd_bw/(1024*1024));

	if(time_old == 0){
		time_old = time_ms();
		return 0; //no selection the first time since all counters will be odd
	}

	time_now = time_ms();
	time_delta = (time_now - time_old) / 1000.0;
	time_old = time_now;

	float ddr_rd_percent = ((float)ddr_rd_bw/(1024*1024)) / (float)ddr_bw_target;
	ddr_rd_percent /= time_delta;
	logd(TAG, "Time delta %f, Running at %.1f percent rd bw (%ld MB/s)\n", time_delta, ddr_rd_percent * 100, ddr_rd_bw/(1024*1024));

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

	return 0;
}

static void *thread_start(void *arg)
{
	struct thread_state *tstate = arg;
	int msr_file;
	uint64_t pmu_old[5], pmu_new[5];

	logd(TAG, "Thread running on core %d, this is #%d core in the module\n", tstate->core_id, CORE_IN_MODULE);

	uint64_t rax, rbx, rcx, rdx;
	get_cpuid(0x1a, &rax, &rbx, &rcx, &rdx);
	logd(TAG, "CPUID Coretype 0x%lx\n", rax);

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(tstate->core_id, &cpuset);

	int s = pthread_setaffinity_np(tstate->thread_id, sizeof(cpuset), &cpuset);
	if (s != 0){
		loge(TAG, "Could not set thread affinity for coreid %d, pthread_setaffinity_np()\n", tstate->core_id);
	}

	msr_file = msr_int(tstate->core_id, tstate->hwpf_msr_value);
	msr_hwpf_write(msr_file, tstate->hwpf_msr_value);

	pmu_core_config(msr_file);

	// Run until end of world...
	while(quitflag == 0){
		usleep(time_intervall * 1000000);
		//logd(TAG, "1. Read Core PMU counters and update stats\n");

		for(int i = 0; i < 5; i++)pmu_old[i] = pmu_new[i];

		pmu_core_read(msr_file, pmu_new);

		for(int i = 0; i < 5; i++)tstate->pmu_result[i] = pmu_new[i] - pmu_old[i];

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

			msr_hwpf_write(msr_file, tstate->hwpf_msr_value);
		}
	}

	close(msr_file);
	printf("Thread on core %d done\n", tstate->core_id);

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
        loga(TAG, "  -i --intervall - update interval in seconds (1-60), default: 1s\n");
        loga(TAG, "  -c --core - list of cores impacted, e.g. 8-16\n");
        loga(TAG, "  -d --ddrbw - set DDR bandwidth target in MGbps, (TBD: default is 80% of max measured bandwidth)\n");
        loga(TAG, "  -a --aggr - set retune aggressiveness (0.1 - 5.0), default 1.0\n");
        loga(TAG, "  -l --log - set loglevel 1 - 5 (5=debug)\n");
        loga(TAG, "  -h --help - this info\n");
}

int main(int argc, char *argv[])
{
	int c;

	log_setlevel(5);
	loga(TAG, "This is the main file for the UU Hardware Prefetch and Control project\n");

	signal(SIGINT, sigintHandler);

	uint64_t rax, rbx, rcx, rdx;

	get_cpuid(0x01, &rax, &rbx, &rcx, &rdx);
	printf("CPUID Platform: 0x%lx\n", rax);
	get_cpuid(0x07, &rax, &rbx, &rcx, &rdx);
	printf("CPUID Hybrid? 0x%lx\n", rdx);

	//decode command-line
	while (1) {
		static struct option long_options[] = {
			{"aggr", required_argument, NULL, 'a'},
			{"core", required_argument, NULL, 'c'},
			{"ddrbw", required_argument, NULL, 'd'},
			{"log", required_argument, NULL, 'l'},
			{"intervall", required_argument, NULL, 'i'},
			{"tunealg", required_argument, NULL, 't'},
			{"help", no_argument, NULL, 'h'},
			{NULL, no_argument, NULL, 0},
		};

		int option_index = 0;
		//c = getopt_long(argc, argv, "hkm:s:f:a:p:", long_options, &option_index);
		c = getopt_long(argc, argv, "a:c:d:l:i:t:h", long_options, &option_index);
		// end of options
		if (c == -1) break;

		switch(c) {
			case 'a':
				aggr = strtof(optarg, 0);
                        break;

			case 'c':
                        	core_first = strtol(optarg, 0, 10);
				if(strstr(optarg,"-") == NULL)core_last = core_first;
				else core_last = strtol(strstr(optarg,"-") + 1, 0, 10);

				logi(TAG, "Cores: %d -> %d = %d threads\n", core_first, core_last, core_last - core_first + 1);

				if(core_last - core_first > MAX_THREADS){
					loge(TAG, "Too many cores, max is %d\n", MAX_THREADS);
					return -1;
				}

                        break;

			case 'd':
				ddr_bw_target = strtol(optarg, 0, 10);
                        break;

			case 'i':
				time_intervall = strtol(optarg, 0, 10);
				if(time_intervall < 1)time_intervall = 1;
				else if(time_intervall > 60)time_intervall = 60;
                        break;

			case 'l':
				log_setlevel(strtol(optarg, 0, 10));
                        break;

			case 't':
				tunealg = strtol(optarg, 0, 10);
                        break;

			case 'h':
			case '?':
				print_usage();
				return 0;
                        break;
		default:
			loge(TAG, "Error, strange command-line argument\n");
			return -1;
		}
	}

	pmu_ddr_init(&ddr);

//	printf("main mmap_ddr0 %p\n", mmap_ddr0);
//	pmu_ddr_rd(&mmap_ddr0, &mmap_ddr1);

	for(int tnum = 0; tnum <= (core_last - core_first); tnum++){
		gtinfo[tnum].core_id = core_first + tnum;
		pthread_create(&gtinfo[tnum].thread_id, NULL, &thread_start, &gtinfo[tnum]);
	}

	void * ret;
	pthread_join(gtinfo[0].thread_id, &ret);

	close(ddr.mem_file);

	printf("done!");

	return 0;
}

