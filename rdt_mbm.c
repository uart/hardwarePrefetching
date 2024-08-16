#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#if 0
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdatomic.h>
#include <sys/mman.h>
#endif
#include "log.h"
#include "rdt_mbm.h"

#define TAG "RDT_MBM"

struct mbm_data_st mbm_data[MAX_NUM_CORES];
uint64_t total_mbt = 0;
uint32_t scale_factor = 0;
uint32_t counter_length = 0;
uint32_t max_rmid = 0;
const uint32_t min_rmid = 1;
uint32_t num_cores = 0;

extern int core_first;
extern int core_last;

int lcpuid(const unsigned leaf, const unsigned subleaf, struct cpuid_out *out);

int
rdt_mbm_support_check(void)
{
	/* Refer Intel Software developers manual, section 18.18.2 Enabling Monitoring: Usage Flow */
	struct cpuid_out res;
        /**
         * Run CPUID.0x7.0 to check
         * for quality monitoring capability (bit 12 of ebx)
         */
        lcpuid(0x7, 0x0, &res);
        if (!(res.ebx & (1 << 12))) {
                logd(TAG, "CPUID.0x7.0: Memory Monitoring capability not supported!\n");
                return -1;
        }

        /**
         * We can go to CPUID.0xf.0 for further
         * exploration of monitoring capabilities
         */
        lcpuid(0xf, 0x0, &res);
        if (!(res.edx & (1 << 1))) {
                loge(TAG, "CPUID.0xf.0: Monitoring capability not supported!\n");
                return -2;
        }

       /**
         * MAX_RMID for the socket
         */
        max_rmid = (unsigned)res.ebx + 1;
	logd(TAG, "rdt_mbm_support_check(): max_rmid %u\n", max_rmid);

	/** Query resource monitoring. Refer section 18.18.5.2  */
	lcpuid(0xf, 1, &res);
	if (!(res.edx & PQOS_CPUID_MON_TMEM_BW_BIT)) {
		loge(TAG, "CPUID.0xf.1: Total memory BW not supported!\n");
                return -3;
	}
	if (!(res.edx & PQOS_CPUID_MON_LMEM_BW_BIT)) {
		loge(TAG, "CPUID.0xf.1: Local memory BW not supported!\n");
                return -4;
	}
	scale_factor = res.ebx;
        counter_length = (res.eax & 0x7f) + MIN_MBM_COUNTER_LEN;
	logd(TAG, "counter_length %u, scale_factor %u\n",
		counter_length, scale_factor);

	num_cores = sysconf(_SC_NPROCESSORS_ONLN);
	logd(TAG, "Number of cores %d\n", num_cores);

	return 0;
}

/* Allocated RMID for each cores.
    TODO: Update RMID allocation - refer hw_mon_start_counter() in intel-cmt-cat
*/

unsigned core2rmid(unsigned core)
{
	unsigned rmid;
	rmid = core + 1;
	if(rmid > max_rmid)
		rmid = min_rmid;
	return rmid;
}
int rdt_mbm_reset(void)
{
        /* reset core assoc */
	unsigned core;
	int retval;
#if DDR_BW_ALL_CORES
	// measure all cores
	int core_first = 0;
	int core_last;
	core_last = num_cores - 1;
#endif
	for (core = (unsigned)core_first; core <= (unsigned)core_last; core++) {
		retval = msr_set_rmid(core, 0);
		if (retval != 0)
                        return -1;;
		close(msr_file_id[core]);
	}
	printf("Resetting RDT MBM done\n");
	return 0;
}

unsigned
get_event_id(const enum pqos_mon_event event)
{
        switch (event) {
        case PQOS_MON_EVENT_LMEM_BW:
                return 3;
                break;
        case PQOS_MON_EVENT_TMEM_BW:
                return 2;
                break;
        default:
		loge(TAG, "error: unsupported MBM event\n");
		rdt_mbm_reset();
                break;
        }
        return 0;
}

int rdt_mbm_init(void)
{
	uint32_t rmid;
	uint32_t core;
	uint32_t mon_count = 0;
	int ret;

#if DDR_BW_ALL_CORES
	// measure all cores
	int core_first = 0;
	int core_last;
	core_last = num_cores - 1;
#endif

	/*
	ToDo: Update RMID allocation - refer hw_mon_start_counter() in intel-cmt-cat
	*/

	// Map rmid
	for (core = (uint32_t)core_first; core <= (uint32_t)core_last; core++) {
		rmid = core2rmid(core);
		// If MSR file is not opened, open it here
		if (msr_file_id[core] == 0)
			msr_file_id[core] = msr_open(core);
		ret = rdt_mbm_set_rmid(core, core2rmid(core));
		if (ret) {
			loge(TAG, "Warning: RDT bandwidth monitoring not possible on core %u\n", core);
			continue;
		}
		mbm_data[core].rmid = rmid;
		mon_count++;
	}
	if (!mon_count) {
		loge(TAG, "Error: RDT bandwidth monitoring not possible on any of the cores\n");
		return -1;
	}
	logd(TAG, "RMID mapping done\n\n");
	rdt_mbm_bw_get(); //first read can be spiky, flush it.
	rdt_mbm_bw_get(); //let's do another clean just to be safe
	return 0;
}

//Set RMID on Allocation & Monitoring association MSR
int rdt_mbm_set_rmid(const unsigned core, const unsigned rmid)
{
        int ret = 0;
        uint64_t val = 0;

        ret = msr_get_rmid(core, &val);
        if (ret != 0) {
		loge(TAG, "Couldn't get RMID for core %d\n", core);
                return -1;
        }

        val &= PQOS_MSR_ASSOC_QECOS_MASK;
        val |= (uint64_t)(rmid & PQOS_MSR_ASSOC_RMID_MASK);

        ret = msr_set_rmid(core, val);
        if (ret != 0)
                loge(TAG, "rdt_mbm_set_rmid(): error writing msr. lcore %u, reg 0x%X, val 0x%lX\n",
			core, PQOS_MSR_ASSOC, val);
	logd(TAG, "rdt_mbm_set_rmid(): lcore %u, reg 0x%X, val 0x%lX\n",
		core, PQOS_MSR_ASSOC, val);

        return ret;
}

// Read Memory Bandwidth Monitoring data from RDT MSR
int rdt_mbm_bw_count(const unsigned core,
            const unsigned rmid,
            const unsigned event,
            uint64_t *value)
{
        int retries = 0, retval = -1;
        uint64_t val = 0;
        uint64_t val_evtsel = 0;
        int flag_wrt = 1;

        /**
         * Set event selection register (RMID + event id)
         */
        val_evtsel = ((uint64_t)rmid) & PQOS_MSR_MON_EVTSEL_RMID_MASK;
        val_evtsel <<= PQOS_MSR_MON_EVTSEL_RMID_SHIFT;
        val_evtsel |= ((uint64_t)event) & PQOS_MSR_MON_EVTSEL_EVTID_MASK;

        for (retries = 0; retries < 4; retries++) {
                if (flag_wrt) {
                        if (msr_set_evtsel(core, val_evtsel) != 0)
                                break;
                }
                if (msr_get_mon_count(core, &val) != 0)
                        break;
                if ((val & PQOS_MSR_MON_QMC_ERROR) != 0ULL) {
                 /* Read back IA32_QM_EVTSEL register
                         * to check for content change.
                         */
                        if (msr_get_evtsel(core, &val) != 0)
                                break;
                        if (val != val_evtsel) {
                                flag_wrt = 1;
                                continue;
                        }
                }
                if ((val & PQOS_MSR_MON_QMC_UNAVAILABLE) != 0ULL) {
                        /**
                         * Waiting for monitoring data
                         */
                        flag_wrt = 0;
                        continue;
                }
                retval = 0;
                break;
        }
        /**
         * Store event value
         */
        if (retval == 0) {
                *value = (val & PQOS_MSR_MON_QMC_DATA_MASK);
		logd(TAG, "rdt_mbm_bw_count(): rmid %u, val_evtsel 0x%lX, "
			"val %lu, event %u\n",
			rmid, val_evtsel, val, event);
        }
        else
                loge(TAG, "Error reading event %u on core %u (RMID%u)!\n",
			event, core, (unsigned)rmid);

        return retval;
}

uint64_t rdt_mbm_bw_get(void)
{
	uint64_t band_width;
	enum pqos_mon_event event = PQOS_MON_EVENT_TMEM_BW;
	uint64_t bw_count = 0;
	uint32_t core;
	float bw_mbps;
        int ret;
#if DDR_BW_ALL_CORES
	// measure all cores
	int core_first = 0;
	int core_last;
	core_last = num_cores - 1;
#endif

        if((event != PQOS_MON_EVENT_LMEM_BW) &&
               (event != PQOS_MON_EVENT_TMEM_BW))
               loge(TAG, "Unsupported event\n");

	total_mbt = 0;

	logv(TAG, "Core\tBW[MB/s]\n");
	for (core = (uint32_t)core_first; core <= (uint32_t)core_last; core++) {
		if(!mbm_data[core].rmid) {
			logi(TAG, "Warning: skipping DDR BW measurement on core %d\n", core);
			continue;
		}
		ret = rdt_mbm_bw_count(core, mbm_data[core].rmid,
			get_event_id(event), &bw_count);
                if (ret != RETVAL_OK)
                        return ret;
		mbm_data[core].delta = bw_count - mbm_data[core].old_count;
		bw_mbps = (mbm_data[core].delta * scale_factor) / (1024.0 * 1024.0);

		logd(TAG, "rdt_mbm_bw_get(): core %02u, count %lu, "
			"old %lu, delta %lu, scale %u, BW[MB/s]: %f\n",
			core, bw_count, mbm_data[core].old_count,
			mbm_data[core].delta, scale_factor,
			bw_mbps);
		logv(TAG, "%02u\t%.2f\n", core, bw_mbps);
		mbm_data[core].old_count = bw_count;
		total_mbt += mbm_data[core].delta;
	}
	logv(TAG, "rdt_mbm_bw_get(): Total BW[MBps]: %f\n",
		(float)(total_mbt * scale_factor) / (1024.0 * 1024.0));

        band_width = total_mbt * scale_factor;

	return band_width;
}
