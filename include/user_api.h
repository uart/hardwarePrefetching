#ifndef __USER_API_H
#define __USER_API_H

#include <stdint.h>
#define PROC_DEVICE "/proc/dpf_monitor"



int kernel_mode_init(void);
int kernel_core_range(uint32_t start, uint32_t end);
int kernel_set_core_weights(int count, int *core_priority);
int kernel_tuning_control(uint32_t tuning_status);
int kernel_set_ddr_bandwidth(uint32_t bandwidth);

#endif /* __USER_API_H */