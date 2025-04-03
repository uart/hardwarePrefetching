#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "kernelmod/kernel_common.h"
#include "log.h"
#include "pcie.h"
#include "pmu_ddr.h"
#include "user_api.h"

#define TAG "KERNEL_API"
#define PROC_DEVICE "/proc/dpf_monitor"


// Initializes the kernel mode interface for hardware prefetching
// Arguments: No arguments.
// Returns: 0 on success, -1 on failure (unable to open device, read or write)
int kernel_mode_init(void)
{
	int fd;
	struct dpf_req_init req;
	struct dpf_resp_init resp;
	ssize_t ret;

	// Open proc interface
	fd = open(PROC_DEVICE, O_RDWR);
	if (fd < 0) {
		loge(TAG, "Failed to open kernel device\n");
		return -1;
	}

	// Prepare init request
	req.header.type = DPF_MSG_INIT;
	req.header.payload_size = sizeof(struct dpf_req_init);

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

	struct dpf_core_range req;
	struct dpf_resp_core_range resp;

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


//Sets priority weights for each core
// accepts: array length (count) and array of priority values (core_priority)
// Returns: 0 on success, and -1 if an error occured
int kernel_set_core_weights(int count, int *core_priority)
{
	int fd;
	ssize_t ret;

	if (count <= 0) {
		loge(TAG, "Invalid core count: %d\n", count);
		return -1;
	}

	size_t req_size = sizeof(struct dpf_core_weight)
		+ count * sizeof(uint32_t);
	size_t resp_size = sizeof(struct dpf_resp_core_weight)
		+ count * sizeof(uint32_t);

	struct dpf_core_weight *req = malloc(req_size);
	struct dpf_resp_core_weight *resp = malloc(resp_size);

	if (!req || !resp) {
		loge(TAG, "Memory allocation failed\n");
		free(req);
		free(resp);
		return -1;
	}

	req->header.type = DPF_MSG_CORE_WEIGHT;
	req->header.payload_size = req_size;
	req->count = count;

	for (int i = 0; i < count; i++) {
		req->weights[i] = core_priority[i];
	}

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
	for (int i = 0; i < count; i++) {
		logd(TAG, "Core %u: priority %u\n", i, resp->confirmed_weights[i]);
	}

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
	struct dpf_ddrbw_set req;
	struct dpf_resp_ddrbw_set resp;

	// Prepare DDR bandwidth request
	req.header.type = DPF_MSG_DDRBW_SET;
	req.header.payload_size = sizeof(struct dpf_ddrbw_set);
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
// Returns: 0 on success, -1 on failure (device access errors)
int kernel_tuning_control(uint32_t tuning_status)
{
	int fd;
	ssize_t ret;

	struct dpf_req_tuning req;
	struct dpf_resp_tuning resp;

	req.header.type = DPF_MSG_TUNING;
	req.header.payload_size = sizeof(req);
	req.enable = tuning_status;

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

	logd(TAG, "Tuning status: %u\n", resp.status);

	close(fd);

	return 0;
}


int kernel_msr_read(uint32_t core_id, uint64_t *msr_values)
{
	int fd;
	ssize_t ret;
	struct dpf_msr_read req;
	struct dpf_resp_msr_read resp;

	req.header.type = DPF_MSG_MSR_READ;
	req.header.payload_size = sizeof(struct dpf_msr_read);
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
//accept: Specific core id and array (pmu_values)
// Returns: 0 on success, and -1 on failure
int kernel_pmu_read(uint32_t core_id, uint64_t *pmu_values)
{
	int fd;
	ssize_t ret;
	struct dpf_pmu_read req;
	struct dpf_resp_pmu_read resp;

	req.header.type = DPF_MSG_PMU_READ;
	req.header.payload_size = sizeof(struct dpf_pmu_read);
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
	for (int i = 0; i < NR_OF_MSR; i++) {
		logi(TAG, "MSR %d: 0x%llx\n", i, msr_values[i]);
	}

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
	for (int i = 0; i < PMU_COUNTERS; i++) {
		logi(TAG, "PMU %d: %llu\n", i, pmu_values[i]);
	}

	return 0;
}

int kernel_set_ddr_config(struct ddr_s *ddr) {
	int fd;
	ssize_t ret;
	struct dpf_ddr_config req;
	struct dpf_resp_ddr_config resp;

	if (ddr->ddr_interface_type == DDR_NONE) {
		loge(TAG, "Failed to detect DDR configuration\n");
		return -1;
	}

	// Prepare DDR config request
	req.header.type = DPF_MSG_DDR_CONFIG;
	req.header.payload_size = sizeof(struct dpf_ddr_config);
	req.bar_address = ddr->bar_address;
	req.cpu_type = ddr->ddr_interface_type;

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
