#ifndef __SYSDETECT_H
#define __SYSDETECT_H

// Struct to return first and last atom e-cores.
struct e_cores_layout_s
{
      int first_efficiency_core;
      int last_efficiency_core;
};


struct e_cores_layout_s get_efficient_core_ids();

#endif
