#ifndef __SYSDETECT_H
#define __SYSDETECT_H

#define MAX_DMI_MEM_CH (128)

#define DMI_TYPE17_VERSION_ONE (21)
#define DMI_TYPE17_VERSION_TWO (27)
#define DMI_TYPE17_VERSION_THREE (34)
#define DMI_TYPE17_VERSION_FOUR (40)
#define DMI_TYPE17_VERSION_FIVE (84)
#define DMI_TYPE17_VERSION_SIX (92)

#define MEGABYTE (1024 * 1024)
#define BWTEST_ARRAY_SIZE ((150 * 1024 * 1024) / 8) //150 MB
#define NTIMES (10)

#define DMI_FILE "/sys/firmware/dmi/tables/DMI"

// Struct to return first and last atom e-cores.
struct e_cores_layout_s {
	int first_efficiency_core;
	int last_efficiency_core;
};

struct dmi_type_header_s {
	uint8_t type;
	uint8_t length;
};


struct __attribute__((packed)) type17_s {
	uint8_t type;
	uint8_t length;
	uint16_t handle;
	uint16_t physical_memory;
	uint16_t memory_error_handle;
	uint16_t total_width;
	uint16_t data_width;
	uint16_t size;
	uint8_t form_factor;
	uint8_t device_set;
	uint8_t device_locator;
	uint8_t bank_locator;
	uint8_t memory_type;
	uint16_t type_detail;
	uint16_t speed;
	uint8_t manufacturer;
	uint8_t serial_number;
	uint8_t asset_tag;
	uint8_t part_number;
	uint8_t attributes;
	uint32_t extended_size;
	uint16_t configured_speed;
	uint16_t minimum_voltage;
	uint16_t maximum_voltage;
	uint16_t configured_voltage;
	uint8_t memory_technology;
	uint16_t operating_mode_capacity;
	uint8_t firmware_version;
	uint16_t manufacturer_id;
	uint16_t product_id;
	uint16_t memory_subsystem_controller_manufacturer_id;
	uint16_t memory_subsystem_controller_product_id;
	uint64_t non_volatile_size;
	uint64_t volatile_size;
	uint64_t cache_size;
	uint64_t logical_size;
	uint32_t extended_speed;
	uint32_t extended_configured_memory_speed;
	uint16_t pmic0_manufacturer_id;
	uint16_t pmic0_revision_number;
	uint16_t rcd_manufacturer_id;
	uint16_t rcd_revision_number;
};



struct e_cores_layout_s get_efficient_core_ids(void);
int dmi_get_bandwidth(void);
int ddrmembw_init(void);
int ddrmembw_deinit(void);
int ddrmembw_measurement(void);
#endif
