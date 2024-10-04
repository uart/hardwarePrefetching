#ifndef __RDT_MBM_H
#define __RDT_MBM_H

#include <stdint.h>
#include "msr.h"

#define PQOS_CPUID_MON_L3_OCCUP_BIT 	0x1 /**< LLC occupancy supported bit */
#define PQOS_CPUID_MON_TMEM_BW_BIT  	0x2 /**< TMEM B/W supported bit */
#define PQOS_CPUID_MON_LMEM_BW_BIT  	0x4 /**< LMEM B/W supported bit */
#define MIN_MBM_COUNTER_LEN		24
#define RETVAL_OK			0

/**<  Measure DDR BW usage of all cores */
#define DDR_BW_ALL_CORES (1)

/**
 * Available types of monitored events
 * (matches CPUID enumeration)
 */
enum pqos_mon_event {
        PQOS_MON_EVENT_LMEM_BW = 2,  /**< Local memory bandwidth */
        PQOS_MON_EVENT_TMEM_BW = 4,  /**< Total memory bandwidth */
};

/**
 * Results of CPUID operation are stored in this structure.
 * It consists of 4x32bits IA registers: EAX, EBX, ECX and EDX.
 */
struct cpuid_out {
        uint32_t eax;
        uint32_t ebx;
        uint32_t ecx;
        uint32_t edx;
};

struct mbm_data_st {
	uint32_t rmid;
	uint64_t delta;
	uint64_t old_count;
};

/* Check if memory bandwidth measurement using RDT is supported */
int rdt_mbm_support_check(void);

/* Initialize RDT MBM by associating RMID for each core */
int rdt_mbm_init(void);

/* Reset/Disable memory bandwidth measurement */
int rdt_mbm_reset(void);

/* Measure DDR bandwithd */
uint64_t rdt_mbm_bw_get(void);

/* Set RMID on MSR */
int rdt_mbm_set_rmid(const unsigned core, const unsigned rmid);
#endif
