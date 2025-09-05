#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "kernelmod/kernel_api.h"
#include "kernelmod/kernel_common.h"
#include "log.h"
#include "pcie.h"
#include "pmu_ddr.h"
#include "user_api.h"

#define TAG "KERNEL_API"
#define PROC_DEVICE "/proc/dynamicPrefetch"

// PMU logging mode constants
#define PMU_LOG_MODE_RESET 0
#define PMU_LOG_MODE_APPEND 1



// Starts PMU event logging with the specified buffer size
// buffer_size: Size of the buffer to allocate for logging
// reset: If non-zero, resets the log buffer before starting
// Returns: 0 on success, -1 on failure
int kernel_pmu_log_start(size_t buffer_size, int reset)
{
	int fd;
	ssize_t ret;
	struct dpf_pmu_log_control_s req;
	struct dpf_resp_pmu_log_control_s resp;

	req.header.type = DPF_MSG_PMU_LOG_CONTROL;
	req.header.payload_size = sizeof(struct dpf_pmu_log_control_s);
	req.buffer_size = buffer_size;
	req.mode = reset;

	fd = open(PROC_DEVICE, O_RDWR);
	if (fd < 0) {
		loge(TAG, "Failed to open device file for PMU log control\n");
		return -1;
	}

	ret = write(fd, &req, sizeof(req));
	if (ret < 0) {
		loge(TAG, "Failed to write PMU log control request\n");
		close(fd);
		return -1;
	}

	ret = read(fd, &resp, sizeof(resp));
	if (ret < 0) {
		loge(TAG, "Failed to read PMU log control response\n");
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}


// Stops PMU event logging
// Returns: 0 on success, -1 on failure
int kernel_pmu_log_stop(void)
{
	int fd;
	ssize_t ret;
	struct dpf_pmu_log_stop_s req;
	struct dpf_resp_pmu_log_stop_s resp;

	req.header.type = DPF_MSG_PMU_LOG_STOP;
	req.header.payload_size = sizeof(struct dpf_pmu_log_stop_s);

	fd = open(PROC_DEVICE, O_RDWR);
	if (fd < 0) {
		loge(TAG, "Failed to open device file for PMU log stop\n");
		return -1;
	}

	ret = write(fd, &req, sizeof(req));
	if (ret < 0) {
		loge(TAG, "Failed to write PMU log stop request\n");
		close(fd);
		return -1;
	}

	ret = read(fd, &resp, sizeof(resp));
	if (ret < 0) {
		loge(TAG, "Failed to read PMU log stop response\n");
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}

// Reads logged PMU events into the provided buffer
// buffer: Pointer to buffer where the log data will be stored
// max_bytes: Maximum number of bytes that can be written to the buffer
// bytes_read: Output parameter that receives the actual number of bytes read
// Returns: 0 on success, -1 on failure

int kernel_pmu_log_read(char *buffer, size_t max_bytes, uint64_t *bytes_read)
{
	int fd;
	ssize_t ret;
	struct dpf_pmu_log_read_s req;
	struct dpf_resp_pmu_log_read_s *resp = NULL; // Pointer to response
	size_t resp_size;

	req.header.type = DPF_MSG_PMU_LOG_READ;
	req.header.payload_size = sizeof(struct dpf_pmu_log_read_s);
	req.max_bytes = max_bytes;

	fd = open(PROC_DEVICE, O_RDWR);
	if (fd < 0) {
		loge(TAG, "Failed to open device file for PMU log read\n");
		return -1;
	}

	ret = write(fd, &req, sizeof(req));
	if (ret < 0) {
		loge(TAG, "Failed to write PMU log read request\n");
		close(fd);
		return -1;
	}

	// First read the header and fixed part of response
	resp = malloc(sizeof(struct dpf_resp_pmu_log_read_s));
	if (!resp) {
		loge(TAG, "Failed to allocate response buffer\n");
		close(fd);
		return -1;
	}

	ret = read(fd, resp, sizeof(struct dpf_resp_pmu_log_read_s));
	if (ret < 0) {
		loge(TAG, "Failed to read PMU log response header\n");
		free(resp);
		close(fd);
		return -1;
	}

	// Calculate total response size including data
	resp_size = sizeof(struct dpf_resp_pmu_log_read_s) + resp->data_size;

	// Reallocate buffer to include data
	resp = realloc(resp, resp_size);
	if (!resp) {
		loge(TAG, "Failed to reallocate response buffer\n");
		close(fd);
		return -1;
	}

	// Read the remaining data
	ret = read(fd, resp->data, resp->data_size);
	if (ret < 0) {
		loge(TAG, "Failed to read PMU log data\n");
		free(resp);
		close(fd);
		return -1;
	}

	// Copy data to user buffer
	*bytes_read = resp->data_size;
	memcpy(buffer, resp->data, resp->data_size);

	free(resp);
	close(fd);
	return 0;
}

// Initializes the kernel mode interface for hardware prefetching
// Arguments: No arguments.
// Returns: 0 on success, -1 on failure (unable to open device, read or write)
int kernel_mode_init(void)
{
	int fd;
	struct dpf_req_init_s req;
	struct dpf_resp_init_s resp;
	ssize_t ret;

	// Open proc interface
	fd = open(PROC_DEVICE, O_RDWR);
	if (fd < 0) {
		loge(TAG, "Failed to open kernel device\n");
		return -1;
	}

	// Prepare init request
	req.header.type = DPF_MSG_INIT;
	req.header.payload_size = sizeof(struct dpf_req_init_s);

	// Send request
	ret = write(fd, &req, sizeof(req));
	if (ret < 0) {
		loge(TAG, "Failed to write to kernel device\n");
		close(fd);
		return -1;
	}

	// Read response
	ret = read(fd, &resp, sizeof(resp));
	if (ret < 0) {
		loge(TAG, "Failed to read from kernel device\n");
		close(fd);
		return -1;
	}

	logi(TAG, "API version: %u\n", resp.version);
	close(fd);
	return 0;
}

// Configures the range of CPU cores to be used for prefetching
// start: Starting core number, end: Ending core number in the range
// Returns: 0 on success, -1 on failure (device access errors)
int kernel_core_range(uint32_t start, uint32_t end)
{
	int fd;
	ssize_t ret;

	struct dpf_core_range_s req;
	struct dpf_resp_core_range_s resp;

	req.header.type = DPF_MSG_CORE_RANGE;
	req.header.payload_size = sizeof(req);
	req.core_start = start;
	req.core_end = end;

	fd = open(PROC_DEVICE, O_RDWR);
	if (fd < 0) {
		loge(TAG, "Failed to open device file\n");
		return -1;
	}

	ret = write(fd, &req, sizeof(req));
	if (ret < 0) {
		loge(TAG, "Failed to write core range request\n");
		return -1;
	}

	ret = read(fd, &resp, sizeof(resp));
	if (ret < 0) {
		loge(TAG, "Failed to read core range response\n");
		return -1;
	}

	logd(TAG, "Thread count: %u\n", resp.thread_count);

	close(fd);
	return 0;
}

// Sets priority weights for each core
// accepts: array length (count) and array of priority values (core_priority)
// Returns: 0 on success, and -1 if an error occurred
int kernel_set_core_weights(int count, int *core_priority)
{
	int fd;
	ssize_t ret;

	if (count <= 0) {
		loge(TAG, "Invalid core count: %d\n", count);
		return -1;
	}

	size_t req_size = sizeof(struct dpf_core_weight_s) + count * sizeof(uint32_t);
	size_t resp_size = sizeof(struct dpf_resp_core_weight_s) + count * sizeof(uint32_t);

	struct dpf_core_weight_s *req = malloc(req_size);
	struct dpf_resp_core_weight_s *resp = malloc(resp_size);

	if (!req || !resp) {
		loge(TAG, "Memory allocation failed\n");
		free(req);
		free(resp);
		return -1;
	}

	req->header.type = DPF_MSG_CORE_WEIGHT;
	req->header.payload_size = req_size;
	req->count = count;

	for (int i = 0; i < count; i++)
		req->weights[i] = core_priority[i];

	fd = open(PROC_DEVICE, O_RDWR);
	if (fd < 0) {
		loge(TAG, "Failed to open device file\n");
		return -1;
	}

	ret = write(fd, req, req_size);
	if (ret < 0) {
		loge(TAG, "Failed to write core weight message\n");
		free(req);
		free(resp);
		return -1;
	}

	ret = read(fd, resp, resp_size);
	if (ret < 0) {
		loge(TAG, "Failed to read core weight response\n");
		free(req);
		free(resp);
		return -1;
	}

	logd(TAG, "Confirmed Core weights set:\n");
	for (int i = 0; i < count; i++)
		logd(TAG, "Core %u: priority %u\n", i,
		     resp->confirmed_weights[i]);

	close(fd);
	free(req);
	free(resp);

	return 0;
}

// Sets specific DDR bandwidth value
// it accepts the requested bandwidth value
// Returns: 0 on success, and -1 on failure
int kernel_set_ddr_bandwidth(uint32_t bandwidth)
{
	int fd;
	ssize_t ret;
	struct dpf_ddrbw_set_s req;
	struct dpf_resp_ddrbw_set_s resp;

	// Prepare DDR bandwidth request
	req.header.type = DPF_MSG_DDRBW_SET;
	req.header.payload_size = sizeof(struct dpf_ddrbw_set_s);
	req.set_value = bandwidth;

	// Open proc interface
	fd = open(PROC_DEVICE, O_RDWR);
	if (fd < 0) {
		loge(TAG, "Failed to open device file for DDR bandwidth setting\n");
		return -1;
	}

	// Send request
	ret = write(fd, &req, sizeof(req));
	if (ret < 0) {
		loge(TAG, "Failed to write DDR bandwidth request\n");
		close(fd);
		return -1;
	}

	// Read response
	ret = read(fd, &resp, sizeof(resp));
	if (ret < 0) {
		loge(TAG, "Failed to read DDR bandwidth response\n");
		close(fd);
		return -1;
	}

	logd(TAG, "DDR bandwidth confirmed: %u MB/s\n", resp.confirmed_value);

	close(fd);
	return 0;
}

// Controls the tuning status of the API
// accept tuning_status: Enable (1) or disable (0)
// tunealg: The tuning algorithm to use
// aggr_factor: Aggressiveness factor (0.0 to 5.0), scaled by 10 in kernel
// Returns: 0 on success, -1 on failure (device access errors)
int kernel_tuning_control(uint32_t tuning_status, uint32_t tunealg, float aggr_factor)
{
	int fd;
	ssize_t ret;

	struct dpf_req_tuning_s req;
	struct dpf_resp_tuning_s resp;

	uint32_t aggr_scaled;

	// Validate aggressiveness factor range
	if (aggr_factor < 0.0f || aggr_factor > 5.0f) {
		loge(TAG, "Aggressiveness factor out of range (0.0-5.0): %.1f\n", aggr_factor);
		return -1;
	}

	// Scale aggressiveness factor by 10 for kernel (0.1 -> 1, 1.0 -> 10, 5.0 -> 50)
	aggr_scaled = (uint32_t)(aggr_factor * 10.0f);

	req.header.type = DPF_MSG_TUNING;
	req.header.payload_size = sizeof(req);
	req.enable = tuning_status;
	req.tunealg = tunealg;
	req.aggr = aggr_scaled;

	fd = open(PROC_DEVICE, O_RDWR);
	if (fd < 0) {
		loge(TAG, "Failed to open device file\n");
		return -1;
	}

	ret = write(fd, &req, sizeof(req));
	if (ret < 0) {
		loge(TAG, "Failed to write tuning control request\n");
		return -1;
	}

	ret = read(fd, &resp, sizeof(resp));
	if (ret < 0) {
		loge(TAG, "Failed to read tuning control response\n");
		return -1;
	}

	logd(TAG, "Tuning status: %u, Algorithm: %u, Aggressiveness: %u\n",
	     resp.status, resp.confirmed_tunealg, resp.confirmed_aggr);

	close(fd);

	return 0;
}

// Read MSR values
// accept: Specific core id and array (msr_values)
// Returns: 0 on success, and -1 on failure
int kernel_msr_read(uint32_t core_id, uint64_t *msr_values)
{
	int fd;
	ssize_t ret;
	struct dpf_msr_read_s req;
	struct dpf_resp_msr_read_s resp;

	req.header.type = DPF_MSG_MSR_READ;
	req.header.payload_size = sizeof(struct dpf_msr_read_s);
	req.core_id = core_id;

	fd = open(PROC_DEVICE, O_RDWR);
	if (fd < 0) {
		loge(TAG, "Failed to open %s for MSR read\n", PROC_DEVICE);
		return -1;
	}

	ret = write(fd, &req, sizeof(req));
	if (ret < 0) {
		loge(TAG, "Failed to write MSR read request for core %u\n", core_id);
		close(fd);
		return -1;
	}

	ret = read(fd, &resp, sizeof(resp));
	if (ret < 0 || ret != sizeof(resp)) {
		loge(TAG, "Failed to read MSR values for core %u\n", core_id);
		close(fd);
		return -1;
	}

	memcpy(msr_values, resp.msr_values, NR_OF_MSR * sizeof(uint64_t));
	close(fd);

	return 0;
}

// Read PMU values
// accept: Specific core id and array (pmu_values)
// Returns: 0 on success, and -1 on failure
int kernel_pmu_read(uint32_t core_id, uint64_t *pmu_values)
{
	int fd;
	ssize_t ret;
	struct dpf_pmu_read_s req;
	struct dpf_resp_pmu_read_s resp;

	req.header.type = DPF_MSG_PMU_READ;
	req.header.payload_size = sizeof(struct dpf_pmu_read_s);
	req.core_id = core_id;

	fd = open(PROC_DEVICE, O_RDWR);
	if (fd < 0) {
		loge(TAG, "Failed to open %s for PMU read\n", PROC_DEVICE);
		return -1;
	}

	ret = write(fd, &req, sizeof(req));
	if (ret < 0) {
		loge(TAG, "Failed to write PMU read request for core %u\n", core_id);
		close(fd);
		return -1;
	}

	ret = read(fd, &resp, sizeof(resp));
	if (ret < 0 || ret != sizeof(resp)) {
		loge(TAG, "Failed to read PMU values for core %u (ret = %zd, expected = %zu)\n", core_id, ret, sizeof(resp));
		close(fd);
		return -1;
	}

	memcpy(pmu_values, resp.pmu_values, PMU_COUNTERS * sizeof(uint64_t));
	close(fd);

	return 0;
}

// Read DDR bandwidth values
// accept: Pointers to read_bw and write_bw
// Returns: 0 on success, -1 on failure
int kernel_ddr_bw_read(uint64_t *read_bw, uint64_t *write_bw)
{
	int fd;
	ssize_t ret;
	struct dpf_ddr_bw_read_s req;
	struct dpf_resp_ddr_bw_read_s resp;

	req.header.type = DPF_MSG_DDR_BW_READ;
	req.header.payload_size = sizeof(struct dpf_ddr_bw_read_s);

	fd = open(PROC_DEVICE, O_RDWR);
	if (fd < 0) {
		loge(TAG, "Failed to open %s for DDR bandwidth read\n", PROC_DEVICE);
		return -1;
	}

	ret = write(fd, &req, sizeof(req));
	if (ret < 0) {
		loge(TAG, "Failed to write DDR bandwidth read request\n");
		close(fd);
		return -1;
	}

	ret = read(fd, &resp, sizeof(resp));
	if (ret < 0 || ret != sizeof(resp)) {
		loge(TAG, "Failed to read DDR bandwidth values (ret = %zd, expected = %zu)\n",
		     ret, sizeof(resp));
		close(fd);
		return -1;
	}

	*read_bw = resp.read_bw;
	*write_bw = resp.write_bw;

	close(fd);
	return 0;
}

// Logs MSR values for a specific core
// core_id: The CPU core to read from
// Returns: 0 on success, -1 on failure
int kernel_log_msr_values(uint32_t core_id)
{
	uint64_t msr_values[NR_OF_MSR];

	if (kernel_msr_read(core_id, msr_values) < 0) {
		loge(TAG, "Failed to read MSR values for core %d\n", core_id);
		return -1;
	}

	logi(TAG, "MSR values for core %d:\n", core_id);

	for (int i = 0; i < NR_OF_MSR; i++)
		logi(TAG, "MSR %d: 0x%llx\n", i, msr_values[i]);

	return 0;
}

// Logs PMU values for a specific core
// core_id: The CPU core to read from
// Returns: 0 on success, -1 on failure
int kernel_log_pmu_values(uint32_t core_id)
{
	uint64_t pmu_values[PMU_COUNTERS];

	if (kernel_pmu_read(core_id, pmu_values) < 0) {
		loge(TAG, "Failed to read PMU values for core %d\n", core_id);
		return -1;
	}

	logi(TAG, "PMU values for core %d:\n", core_id);

	for (int i = 0; i < PMU_COUNTERS; i++)
		logi(TAG, "PMU %d: %llu\n", i, pmu_values[i]);

	return 0;
}

// Logs DDR bandwidth values
// Returns: 0 on success, -1 on failure
int kernel_log_ddr_bw(void)
{
	uint64_t read_bw, write_bw;

	if (kernel_ddr_bw_read(&read_bw, &write_bw) < 0) {
		loge(TAG, "Failed to read DDR bandwidth values\n");
		return -1;
	}

	double read_mbps = (double)read_bw / (1024 * 1024);
	double write_mbps = (double)write_bw / (1024 * 1024);

	logi(TAG, "DDR Bandwidth: Read=%llu bytes (%.2f MB/s), Write=%llu bytes (%.2f MB/s)\n",
	     read_bw, read_mbps, write_bw, write_mbps);

	return 0;
}

// Sets the DDR configuration
// ddr: The ddr_s struct containing the configuration information
// Returns: 0 on success, -1 on failure (device access errors)
int kernel_set_ddr_config(struct ddr_s *ddr)
{
	int fd;
	ssize_t ret;
	struct dpf_ddr_config_s req;
	struct dpf_resp_ddr_config_s resp;

	if (ddr->ddr_interface_type == DDR_NONE) {
		loge(TAG, "Failed to detect DDR configuration\n");
		return -1;
	}

	// Prepare DDR config request
	req.header.type = DPF_MSG_DDR_CONFIG;
	req.header.payload_size = sizeof(struct dpf_ddr_config_s);
	req.bar_address = ddr->bar_address;
	req.cpu_type = ddr->ddr_interface_type;
	req.num_controllers = ddr->num_ddr_controllers;

	// Open proc interface
	fd = open(PROC_DEVICE, O_RDWR);
	if (fd < 0) {
		loge(TAG, "Failed to open device file for DDR config\n");
		return -1;
	}

	ret = write(fd, &req, sizeof(req));
	if (ret < 0) {
		loge(TAG, "Failed to write DDR config request\n");
		close(fd);
		return -1;
	}

	ret = read(fd, &resp, sizeof(resp));
	if (ret < 0) {
		loge(TAG, "Failed to read DDR config response\n");
		close(fd);
		return -1;
	}

	logd(TAG, "DDR config confirmed: BAR=0x%llx, Type=%u\n",
	     resp.confirmed_bar, resp.confirmed_type);

	close(fd);

	return 0;
}
