#ifndef __METRICS_H
#define __METRICS_H

#include <stdint.h>

int read_pmu(int core_id, uint64_t *pmu_values);
int read_msr(int core_id, uint64_t *msr_values);
int read_ddr_bw(uint64_t *read_bw, uint64_t *write_bw);
int kernel_ddr_bw_read(uint64_t *read_bw, uint64_t *write_bw);

#endif /* __METRICS_H */
