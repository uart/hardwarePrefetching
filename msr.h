#ifndef __MSR_H
#define __MSR_H

#include <stdint.h>

#include "atom_msr.h"

#define HWPF_MSR_FIELDS (5)
#define HWPF_MSR_BASE (0x1320)

int msr_corepmu_setup(int msr_file, int nr_events, uint64_t *event);
int msr_corepmu_read(int msr_file, int nr_events, uint64_t *result);

int msr_int(int core, union msr_u msr[]);
int msr_hwpf_write(int msr_file, union msr_u msr[]);

int msr_set_l2xq(union msr_u msr[], int value);
int msr_get_l2xq(union msr_u msr[]);
int msr_set_l3xq(union msr_u msr[], int value);
int msr_get_l3xq(union msr_u msr[]);

int msr_set_l2maxdist(union msr_u msr[], int value);
int msr_get_l2maxdist(union msr_u msr[]);
int msr_set_l3maxdist(union msr_u msr[], int value);
int msr_get_l3maxdist(union msr_u msr[]);

#endif


