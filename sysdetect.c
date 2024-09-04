#define _GNU_SOURCE

#include <stdio.h>
#include <cpuid.h>
#include <sys/sysinfo.h>
#include <sched.h>

#include "log.h"
#include "sysdetect.h"

#define TAG "SYSDETECT"


/*
-  Function to determine if a cpu core is an atom e-core type or not.
-  Arguments: Cpu core's id number.
-  Returns core_value. If 0x20, then core is an e-core.
*/
static int get_core_type(int core_number)
{
        cpu_set_t cpu_set;
        cpu_set_t original_set;

        // Get current cpu affinity.
        if (sched_getaffinity(0, sizeof(cpu_set_t), &original_set) == -1){
                loge(TAG, "Error getting current CPU affinity\n");
                return -1;
        }

        // Reset & init cpu affinity.
        CPU_ZERO(&cpu_set);
        CPU_SET(core_number, &cpu_set);         
        
        // Set cpu affinity for the next task.
        if (sched_setaffinity(0, sizeof(cpu_set_t), &cpu_set) == -1){
                loge(TAG, "Error setting CPU affinity\n");
                return -2;
        }       
        
        // Get core type.
        unsigned int values[4];
        unsigned int leaf = 0x1a;
        unsigned int subleaf = 0;

        __cpuid_count(leaf, subleaf, values[0], values[1], values[2], values[3]);

        // Get core value. If 0x20, core is an e-core.
        int core_value = (values[0] >> 24);

        //Clear cpu from set
        CPU_CLR(core_number, &cpu_set);
        
        // Restore original cpu affinity.
        if (sched_setaffinity(0, sizeof(cpu_set_t), &original_set) == -1){
                loge(TAG, "Error restoring original CPU affinity\n");
                return -3;
        }       
        
        return core_value;
}


/*
-  Function to detect the processor is a hybrid or not.
-  Arguments: No arguments.
-  Returns 1 if hybrid.
*/
static int get_hybridflag()
{
        unsigned int values[4];
        unsigned int hybrid;

        unsigned int leaf = 0x07;
        unsigned int subleaf = 0;

        __cpuid_count(leaf, subleaf, values[0], values[1], values[2], values[3]);

        //Hybrid detection.
        hybrid = (values[3] >> 15) & 0x01;

        return hybrid;       
}


/*
-  Function to get the first and the last efficiency core id's.
-  Arguments: No arguments.
-  Returns a struct containing the first and last e-core id's.
*/
struct e_cores_layout_s get_efficient_core_ids()
{
        struct e_cores_layout_s core_locations;

        //Initialise first/last e-core location values.
        core_locations.first_efficiency_core = -1;
        core_locations.last_efficiency_core = -1;

        //E-core count.
        int efficient_count = 0;

        //Get available cores from OS.
        int total_cores = get_nprocs_conf();
        int available_cores = get_nprocs();

        //Warning (if there are unavailable cores).
        if (total_cores > available_cores) {
                logi(TAG, "%u core(s) might be busy or unavailable.\n",
                        total_cores - available_cores);
        }
        
        //If hybrid
        if (get_hybridflag() == 1){
                logv(TAG, "Hybrid processor detected.\n");

                for (int i = 0; i < available_cores; i++){
                        //If get_core_type returns 0x20, core is an e-core.
                        if (get_core_type(i) == 0x20) {
                                if (efficient_count == 0){
                                        core_locations.first_efficiency_core = i;
                                }

                                core_locations.last_efficiency_core = i;
                                efficient_count++;
                        }
                }
        }
        else { //All Cores are of the same type.
                logv(TAG, "Non-hybrid processor detected.\n");

                if (get_core_type(0) == 0x20){
                        //All Efficient Cores.
                        core_locations.first_efficiency_core = 0;
                        core_locations.last_efficiency_core = available_cores - 1;
                }
        }

        //Results
        logv(TAG, "First Atom E-Core: CPU(%d)\n", core_locations.first_efficiency_core);
        logv(TAG, "Last Atom E-Core: CPU(%d)\n", core_locations.last_efficiency_core);

        return core_locations;
}
