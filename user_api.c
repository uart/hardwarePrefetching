
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "log.h"
#include "user_api.h"
#include "kernelmod/kernel_common.h"

#define TAG "KERNEL_API"


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
		perror("Memory allocation failed");
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