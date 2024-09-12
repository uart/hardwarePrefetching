#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <cpuid.h>
#include <sys/sysinfo.h>
#include <sched.h>

#include "log.h"
#include "sysdetect.h"

#define TAG "SYSDETECT"


// Function to determine if a cpu core is an atom e-core type or not.
// Arguments: Cpu core's id number.
// Returns core_value. If 0x20, then core is an e-core.
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

        __cpuid_count(leaf, subleaf, values[0], values[1], values[2], values
                                                                        [3]);

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


// Function to detect the processor is a hybrid or not.
// Arguments: No arguments.
// Returns 1 if hybrid.
static int get_hybridflag()
{
        unsigned int values[4];
        unsigned int hybrid;

        unsigned int leaf = 0x07;
        unsigned int subleaf = 0;

        __cpuid_count(leaf, subleaf, values[0], values[1], values[2], values
                                                                        [3]);

        //Hybrid detection.
        hybrid = (values[3] >> 15) & 0x01;

        return hybrid;       
}


// Function to get the first and the last efficiency core id's.
// Arguments: No arguments.
// Returns a struct containing the first and last e-core id's.
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
                                        core_locations.first_efficiency_core = 
                                                                        i;
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
                        core_locations.last_efficiency_core = available_cores - 
                                                                        1;
                }
        }

        //Results
        logv(TAG, "First Atom E-Core: CPU(%d)\n", 
                core_locations.first_efficiency_core);
        logv(TAG, "Last Atom E-Core: CPU(%d)\n", 
                core_locations.last_efficiency_core);

        return core_locations;
}







/*

DDR BANDWIDTH FROM BIOS/DMI SETTINGS

*/



// Function to read a type 17 field from the file
// it accept a pointer, sizeof, and a file pointer
// return -1 if an error occured and 1 upon success
static int dmi_read_type17_field(void *field, size_t size, FILE *file) 
{
        if (fread(field, size, 1, file) != 1){
                return -1;
        }
        
        return 1;
}


// read Type 17 structure from the file based on length
// It accept the length of type 17, type 17 struct pointer, and a file pointer
// It returns -1 if an error occured and 1 upon success
static int dmi_read_type17(int length, struct type17_s *type17, FILE *file) 
{
        // Common fields to all lengths
        if (!dmi_read_type17_field(&type17->handle, sizeof(uint16_t), file) ||
            !dmi_read_type17_field(&type17->physical_memory, 
                                sizeof(uint16_t), file) ||
            !dmi_read_type17_field(&type17->memory_error_handle, 
                                        sizeof(uint16_t), file) ||
            !dmi_read_type17_field(&type17->total_width, 
                                sizeof(uint16_t), file) ||
            !dmi_read_type17_field(&type17->data_width, 
                                sizeof(uint16_t), file) ||
            !dmi_read_type17_field(&type17->size, sizeof(uint16_t), file) ||
            !dmi_read_type17_field(&type17->form_factor, 
                                        sizeof(uint8_t), file) ||
            !dmi_read_type17_field(&type17->device_set, 
                                sizeof(uint8_t), file) ||
            !dmi_read_type17_field(&type17->device_locator, 
                                sizeof(uint8_t), file) ||
            !dmi_read_type17_field(&type17->bank_locator, 
                                sizeof(uint8_t), file) ||
            !dmi_read_type17_field(&type17->memory_type, 
                                sizeof(uint8_t), file) ||
            !dmi_read_type17_field(&type17->type_detail, 
                                sizeof(uint16_t), file) ||
            !dmi_read_type17_field(&type17->speed, sizeof(uint16_t), file)){
                loge(TAG, "Error reading common fields\n");
                return -1;
        }

        // Additional fields for length greater than or equal to 27
        if (length >= DMI_TYPE17_VERSION_TWO){
                if (!dmi_read_type17_field(&type17->manufacturer, 
                                        sizeof(uint8_t), file) ||
                    !dmi_read_type17_field(&type17->serial_number, 
                                        sizeof(uint8_t), file) ||
                    !dmi_read_type17_field(&type17->asset_tag, 
                                        sizeof(uint8_t), file) ||
                    !dmi_read_type17_field(&type17->part_number, 
                                        sizeof(uint8_t), file) ||
                    !dmi_read_type17_field(&type17->attributes, 
                                        sizeof(uint8_t), file)){

                        loge(TAG, "Error reading fields for length >= 27\n");
                        return -1;
                }
        }

        // Additional fields for length greater than or equal to 34
        if (length >= DMI_TYPE17_VERSION_THREE){
                if (!dmi_read_type17_field(&type17->extended_size, 
                                sizeof(uint32_t), file) ||
                        !dmi_read_type17_field(&type17->configured_speed, 
                                                sizeof(uint16_t), file) ||
                        !dmi_read_type17_field(&type17->minimum_voltage, 
                                                sizeof(uint16_t), file)) {
                        loge(TAG, "Error reading fields for length >=34\n");
                        return -1;
                }
        }

        // Additional fields for length greater than or equal to 40
        if (length >= DMI_TYPE17_VERSION_FOUR){
                if (!dmi_read_type17_field(&type17->maximum_voltage, 
                                        sizeof(uint16_t), file) ||
                    !dmi_read_type17_field(&type17->configured_voltage, 
                                        sizeof(uint16_t), file) || 
                    !dmi_read_type17_field(&type17->memory_technology, 
                                        sizeof(uint8_t), file)) {
                        loge(TAG, "Error reading fields for length >= 40\n");
                        return -1;
                }
        }

        // Additional fields for length greater than or equal to 84
        if (length >= DMI_TYPE17_VERSION_FIVE){
                if (!dmi_read_type17_field(&type17->operating_mode_capacity, 
                                                sizeof(uint16_t), file) ||
                    !dmi_read_type17_field(&type17->firmware_version, 
                                                sizeof(uint8_t), file) ||
                    !dmi_read_type17_field(&type17->manufacturer_id, 
                                                sizeof(uint16_t), file) ||
                    !dmi_read_type17_field(&type17->product_id, 
                                                sizeof(uint16_t), file) ||
                    !dmi_read_type17_field(&
                        type17->memory_subsystem_controller_manufacturer_id, 
                                                sizeof(uint16_t), file) ||
                    !dmi_read_type17_field(&
                                type17->memory_subsystem_controller_product_id, 
                                                sizeof(uint16_t), file) ||
                    !dmi_read_type17_field(&type17->non_volatile_size, 
                                                sizeof(uint64_t), file) ||
                    !dmi_read_type17_field(&type17->volatile_size, 
                                                sizeof(uint64_t), file) ||
                    !dmi_read_type17_field(&type17->cache_size, 
                                                sizeof(uint64_t), file) ||
                    !dmi_read_type17_field(&type17->logical_size, 
                                                sizeof(uint64_t), file) || 
                    !dmi_read_type17_field(&type17->extended_speed, 
                                                sizeof(uint32_t), file)){
                        loge(TAG, "Error reading fields for length >= 84\n");
                        return -1;
                }
        }

        // Additional fields for length greater than or equal to 92
        if (length >= DMI_TYPE17_VERSION_SIX){
                if (!dmi_read_type17_field(&
                                type17->extended_configured_memory_speed, 
                                                sizeof(uint32_t), file) ||
                        !dmi_read_type17_field(&type17->pmic0_manufacturer_id,  
                                                sizeof(uint16_t), file)){
                        loge(TAG, "Error reading fields for length >= 92\n");
                        return -1;
                }
        }

        return 0;
}


//  check if all the respective sizes are equal
//  call with pointer to an array and number of elements
//  It returns a negative number if not equal and positive when equal
static int dmi_check_same_size(int type17_total, struct type17_s *type17) 
{
        int memory_size = type17[0].size;
        
        if (memory_size == 0) memory_size = type17[1].size;

        int count = 0;

        for (int i = 1; i < type17_total; i++){
                if(type17[i].size == 0)continue;

                if (type17[i].size == memory_size){
                        count++;
                }else{
                        logv(TAG, "Memory sizes are not equal\n");
                        return -1;
                }
        }

        return count;
}


//  check if all the respective speeds are equal
//  It accept the total number of type 17 and type17 struct pointer / array
//  It returns a negative number if not equal and positive when equal
static int dmi_check_same_speed(int type17_total, struct type17_s *type17) 
{
        int memory_speed = type17[0].speed;

        if (memory_speed == 0) memory_speed = type17[1].speed;

        int count = 0;

        for (int i = 1; i < type17_total; i++){
                if(type17[i].speed == 0)continue;

                if (type17[i].speed == memory_speed){
                        count++;
                }else{
                        return -1;
                }
        }

        return count;
}



// Reads all the various DMI types from a file and then calculates the total 
// memory bandwidth based on type 17 (memory device) data and returns the result
// It handles errors during file access, data reading, and ensures bandwidth is 
// valid. 
//  Returns the total calculated bandwidth, or -1 if an error occurs.
int dmi_get_bandwidth() 
{
        struct type17_s type17[MAX_DMI_MEM_CH];
        struct dmi_type_header_s current_header;
        int type17_count = 0;
        FILE *fp;


        // Open the file in binary read mode
        fp = fopen(DMI_FILE, "rb");
        if (fp == NULL){
                loge(TAG, "Error opening file\n");
                return -1;
        }

        // Record all type 17 entries (memory controller)
        while (fread(&current_header, sizeof(struct dmi_type_header_s), 1, fp)){
                int type = current_header.type;
                int length = current_header.length;

                if (length < 1){
                        loge(TAG, "Invalid type17 entry length encountered\n");
                        fclose(fp);
                        return -1;
                }

                if (type == 17){
                        type17[type17_count].type = type;
                        type17[type17_count].length = length;

                        int type17_version = dmi_read_type17(length, &type17
                                                        [type17_count], fp);

                        if (type17_version < 0) {
                                loge(TAG, "Error reading type 17 fields\n");
                                fclose(fp);
                                return -1;
                        }

                        //Log information about the found type17 entry: speed 
                        // and size
                        logv(TAG, "Found type17 #%d Speed = %d Size = %d\n", 
                                        type17_count +1, type17[type17_count].
                                        speed, type17[type17_count].size);

                        type17_count++;
                }else{
                        // Move to the next bytes based on the length
                        if (fseek(fp, length - 2, SEEK_CUR) != 0){
                                loge(TAG, "Error seeking in file\n");
                                fclose(fp);
                                return -1;
                        }
                }

                // Search for the 00 00 sequence
                uint8_t byte1, byte2;
                int zero_ret;

                zero_ret = fread(&byte1, sizeof(uint8_t), 1, fp);
                if (zero_ret != 1){
                        loge(TAG, "Error reading zero sequence\n");
                        fclose(fp);
                        return -1;                        
                }
                
                zero_ret = fread(&byte2, sizeof(uint8_t), 1, fp);
                if (zero_ret != 1){
                        loge(TAG, "Error reading zero sequence\n");
                        fclose(fp);
                        return -1;                        
                }

                while (!(byte1 == 0x00 && byte2 == 0x00)){
                        byte1 = byte2;
                        zero_ret = fread(&byte2, sizeof(uint8_t), 1, fp);
                        
                        if (zero_ret != 1){
                                loge(TAG, "Error reading zero sequence\n");
                                fclose(fp);
                                return -1;                        
                        }
                }
        }
        
        fclose(fp);

        int memory_speed = type17[0].speed;
        int total_bandwidth = 0;

        // An array to keep track of all the bandwidth
        int bandwidth_values[type17_count];

        // Get the lowest speed
        for (int i = 0; i < type17_count; i++){
                if (type17[i].speed == 0) continue;
                
                if ((type17[i].speed < memory_speed) || memory_speed == 0) {
                        memory_speed = type17[i].speed;
                }
        }

        int equal_speed = dmi_check_same_speed(type17_count, type17);
        if (equal_speed < 0) logv(TAG, "Memory speed are not equal\n");

        // Calculate the theoretical bandwidth for the various type 17
        for (int i = 0; i < type17_count; i++){
                //  bandwidth = memory speed * 8 bytes per transaction * number 
                // of channels
                bandwidth_values[i] = memory_speed * 8 * type17_count;
        }

        // If check_same_size returns a value greater than zero, then all 
        // devices have same size
        // For equal bandwidth, add all the respective bandwidth together
        // Else report the lowest bandwidth
        
        if (dmi_check_same_size(type17_count, type17) > 0){
                for (int i = 0; i < type17_count; i++){
                        total_bandwidth += bandwidth_values[i];
                }
        }else{
                total_bandwidth = bandwidth_values[0];
        }


        if(total_bandwidth <= 0){
                loge(TAG, "bandwidth is %d\n", total_bandwidth);
                return -1;
        }

        logv(TAG, "Total memory BW: %d MB/s @ %d ch\n", total_bandwidth, 
                                                        type17_count);

        return total_bandwidth;
}       
