#ifndef __KERNEL_PMU_DDR_H__
#define __KERNEL_PMU_DDR_H__

#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>

#include "../include/pmu_ddr.h"

 
// External variables
extern __u32 ddr_cpu_type;
extern __u32 num_ddr_controllers;
extern struct ddr_s ddr;

// Function prototypes
uint64_t kernel_pmu_ddr(struct ddr_s *ddr, int type);
int kernel_pmu_ddr_init_grr_srf(struct ddr_s *ddr, uint64_t ddr_bar);
int kernel_pmu_ddr_init_client(struct ddr_s *ddr, uint64_t ddr_bar);
int read_ddr_counters(uint64_t *read_bw, uint64_t *write_bw);

#endif /* __KERNEL_PMU_DDR_H__ */
