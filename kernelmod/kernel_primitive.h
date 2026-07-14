#ifndef __KERNEL_PRIMITIVE__
#define __KERNEL_PRIMITIVE__

// Compute module and mode definitions used by tunealg 1 logic
#define CORES_PER_COMPUTE_MODULE (4)
#define BENEFITING_CORES (1) // signed vote threshold

#define MODE_BOOT_DEFAULT (-1)
#define MODE_IDLE (0)
#define MODE_AGGRESSIVE (1)
#define MODE_UNINITIALIZED (2)

// Idle-core threshold for tunealg 1, normalized by interval length in ms.
// If cycles_per_ms is below this threshold, the core is treated as idle.
#define IDLE_CYCLES_THRESHOLD (500000ULL)

int kernel_basicalg(int tunealg, int aggr);

#endif

