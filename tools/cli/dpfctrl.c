#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <assert.h>
#include <sys/types.h>

#include "../../include/user_api.h"
#include "../../kernelmod/kernel_common.h"
#include "dpfctrl.h"

#define VERSION "1.0.0"

// Convert number of PMU log entries to bytes
static size_t entries_to_bytes(size_t entries) {
    return entries * PMU_ENTRY_SIZE_BYTES;
}

// Get PMU counter name by index
static const char *get_pmu_counter_name(int index) {
    static const char *names[] = {
        "cycles",
        "instructions",
        "mem_load_uops",
        "l2_hit",
        "l3_hit",
        "drm_hit",
        "xq_promotion"
    };
    return (index >= 0 && index < PMU_COUNTERS) ? names[index] : "unknown";
}

// Parse command string to command type
static enum cmd_type parse_command(const char *cmd) {
    if (!cmd) {
        return CMD_UNKNOWN;
    }
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        return CMD_HELP;
    }
    if (strcmp(cmd, "start") == 0) {
        return CMD_START;
    }
    if (strcmp(cmd, "stop") == 0) {
        return CMD_STOP;
    }
    if (strcmp(cmd, "dump") == 0) {
        return CMD_DUMP;
    }

    return CMD_UNKNOWN;
}

// Parse format string to output format type
static enum output_format parse_format(const char *fmt) {
    if (!fmt) {
        return FMT_RAW;
    }
    return strcmp(fmt, "csv") == 0 ? FMT_CSV : FMT_RAW;
}

// Print version information
void print_version(void) {
    printf("dpfctrl version %s\n", VERSION);
}

// Print usage information
void print_usage(void) {
    printf("dpfctrl - DPF PMU Logging Control Tool %s\n\n", VERSION);
    printf("SYNOPSIS\n");
    printf("  dpfctrl [COMMAND] [OPTIONS]\n\n");
    printf("COMMANDS\n");
    printf("  start         Start PMU logging\n");
    printf("  stop          Stop PMU logging\n");
    printf("  dump          Dump PMU log data\n");

    printf("  help          Show this help message\n\n");
    printf("OPTIONS\n");
    printf("  For 'start' command:\n");
    printf("    -s, --size <entries>     Buffer size in entries (default: %d)\n", DEFAULT_BUFFER_ENTRIES);
    printf("    -m, --mode <reset|append> Logging mode (default: reset)\n\n");
    printf("  For 'dump' command:\n");
    printf("    -f, --file <filename>    Output file (required)\n");
    printf("    -o, --format <raw|csv>   Output format (default: raw)\n\n");

    printf("EXAMPLES\n");
    printf("  sudo dpfctrl start --size 10000 --mode reset\n");
    printf("  sudo dpfctrl start --mode append\n");
    printf("  sudo dpfctrl stop\n");
    printf("  sudo dpfctrl dump --file pmu_data.csv --format csv\n");

    printf("NOTE\n");
    printf("  Requires root privileges to access the kernel interface.\n");
    printf("  The DPF kernel module must be loaded.\n");
}

// Parse command-line options into opts structure
static int process_options(int argc, char *argv[], struct options_s *opts) {
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {"size", required_argument, 0, 's'},
        {"mode", required_argument, 0, 'm'},
        {"file", required_argument, 0, 'f'},
        {"format", required_argument, 0, 'o'},
        {0, 0, 0, 0}
    };

    int option_index	 = 0;
    int c;

    if (argc < 2) {
        print_usage();
        return -1;
    }

    opts->command = parse_command(argv[1]);
    if (opts->command == CMD_UNKNOWN) {
        fprintf(stderr, "Error: Unknown command '%s'\n", argv[1]);
        print_usage();
        return -1;
    }

    if (opts->command == CMD_HELP) {
        return 0;
    }

    while ((c = getopt_long(argc, argv, "hvs:m:f:o:e:", long_options, &option_index)) != -1) {
        switch (c) {
        case 'h':
            opts->command = CMD_HELP;
            return 0;
        case 'v':
            print_version();
            return 0;
        case 's':
            opts->buffer_entries = atoi(optarg);
            if (opts->buffer_entries <= 0) {
                fprintf(stderr, "Error: Buffer size must be a positive integer\n");
                return -1;
            }
            break;
        case 'm':
            if (strcmp(optarg, "reset") == 0) {
                opts->mode = 0;
            } else if (strcmp(optarg, "append") == 0) {
                opts->mode = 1;
            } else {
                fprintf(stderr, "Error: Invalid mode '%s'. Use 'reset' or 'append'\n", optarg);
                return -1;
            }
            break;
        case 'f':
            opts->output_file = optarg;
            break;
        case 'o':
            opts->format = parse_format(optarg);
            if (opts->format == FMT_RAW && strcmp(optarg, "raw") != 0) {
                fprintf(stderr, "Error: Invalid format '%s'. Use 'raw' or 'csv'\n", optarg);
                return -1;
            }
            break;
        default:
            fprintf(stderr, "Error: Unknown option\n");
            print_usage();
            return -1;
        }
    }

    return 0;
}

// Write PMU data to a CSV file
void dump_data_csv(FILE *fp, const char *buffer, uint64_t buffer_size) {
    if (!fp) {
        fprintf(stderr, "Error: Invalid file pointer\n");
        return;
    }

    if (buffer_size % PMU_ENTRY_SIZE_BYTES != 0) {
        fprintf(stderr, "Error: Invalid buffer size %zu, must be multiple of %zu\n",
                (size_t)buffer_size, (size_t)PMU_ENTRY_SIZE_BYTES);
        fprintf(fp, "# Note: Invalid PMU data size\n");
        return;
    }

    const dpf_pmu_log_entry_t *entries = (const dpf_pmu_log_entry_t *)buffer;
    uint64_t num_entries = buffer_size / PMU_ENTRY_SIZE_BYTES;

    if (num_entries == 0) {
        fprintf(fp, "# Note: No PMU data available\n");
        return;
    }

    // Write CSV header
    fprintf(fp, "timestamp,core_id");
    for (int i = 0; i < PMU_COUNTERS; i++) {
        fprintf(fp, ",%s", get_pmu_counter_name(i));
    }
    fprintf(fp, "\n");

    // Write data rows
    for (uint64_t i = 0; i < num_entries; i++) {
        fprintf(fp, "%llu,%u", (unsigned long long)entries[i].timestamp, entries[i].core_id);
        for (int j = 0; j < PMU_COUNTERS; j++) {
            fprintf(fp, ",%llu", (unsigned long long)entries[i].pmu_values[j]);
        }
        fprintf(fp, "\n");
    }

    if (fflush(fp) != 0) {
        perror("Warning: Error flushing CSV file");
    }
}

// Handle dump command to read and write PMU data
void handle_dump(const char *filename, enum output_format format) {
    if (!filename) {
        fprintf(stderr, "Error: Output file must be specified\n");
        return;
    }

    FILE *fp = NULL;
    char *buffer = NULL;
    uint64_t bytes_read = 0;  // Match kernel's __u64 type
    size_t buffer_size = entries_to_bytes(DEFAULT_BUFFER_ENTRIES);

    // Allocate buffer
    buffer = malloc(buffer_size);
    if (!buffer) {
        perror("Error: Failed to allocate buffer");
        goto cleanup;
    }

    // Read PMU data from kernel
    int ret = kernel_pmu_log_read(buffer, buffer_size, &bytes_read);
    if (ret != 0) {
        fprintf(stderr, "Error: Failed to read PMU data (error code: %d)\n", ret);
        goto cleanup;
    }
    
    if (bytes_read == 0) {
        fprintf(stderr, "Warning: No PMU data available\n");
        // Don't try to write to fp here since it hasn't been opened yet
        goto cleanup;
    }

    // Open file with appropriate mode
    const char *mode = (format == FMT_CSV) ? "w" : "wb";
    fp = fopen(filename, mode);
    if (!fp) {
        perror("Error: Failed to open output file");
        fprintf(stderr, "  File: %s\n", filename);
        fprintf(stderr, "  Mode: %s\n", mode);
        goto cleanup;
    }

    if (format == FMT_CSV) {
        dump_data_csv(fp, buffer, bytes_read);
    } else {
        fprintf(fp, "# Raw PMU data, size: %" PRIu64 " bytes\n", bytes_read);
        if (fwrite(buffer, 1, (size_t)bytes_read, fp) != (size_t)bytes_read) {
            perror("Warning: Incomplete write to raw file");
        }
    }

    printf("PMU data written to %s in %s format\n", filename, format == FMT_CSV ? "CSV" : "raw");

cleanup:
    if (fp && fclose(fp) != 0) {
        perror("Warning: Error closing output file");
    }
    free(buffer);
}

// Handle start command to initiate PMU logging
void handle_start(size_t buffer_entries, int mode) {
    printf("Starting PMU logging with %zu entries in %s mode...\n",
           buffer_entries, mode == 0 ? "reset" : "append");

    if (kernel_pmu_log_start(buffer_entries, mode) != 0) {
        fprintf(stderr, "Error: Failed to start PMU logging\n");
    }

    printf("PMU logging started successfully\n");
}

// Handle stop command to terminate PMU logging
void handle_stop(void) {
    printf("Stopping PMU logging...\n");

    if (kernel_pmu_log_stop() != 0) {
        fprintf(stderr, "Error: Failed to stop PMU logging\n");
    }

    printf("PMU logging stopped successfully\n");
}

// Main entry point for the dpfctrl tool
int main(int argc, char *argv[]) {
    struct options_s opts = {
        .command = CMD_UNKNOWN,
        .buffer_entries = DEFAULT_BUFFER_ENTRIES,
        .mode = DEFAULT_MODE_RESET,
        .output_file = NULL,
        .format = FMT_RAW
    };

    // Parse command-line options
    int ret = process_options(argc, argv, &opts);
    if (ret < 0) {
        return -1;
    }

    // Handle help command early
    if (opts.command == CMD_HELP) {
        print_usage();
        return 0;
    }

    // Initialize kernel interface
    if (kernel_mode_init() != 0) {
        fprintf(stderr, "Error: Failed to initialize kernel interface\n");
        fprintf(stderr, "Check if the DPF kernel module is loaded\n");
        return -1;
    }

    // Process commands
    switch (opts.command) {
    case CMD_START:
        handle_start(opts.buffer_entries, opts.mode);
        break;
    case CMD_STOP:
        handle_stop();
        break;
    case CMD_DUMP:
        if (!opts.output_file) {
            fprintf(stderr, "Error: Output file required for dump command (--file)\n");
            return -1;
        }
        handle_dump(opts.output_file, opts.format);
        break;

    default:
        fprintf(stderr, "Error: Invalid command\n");
        print_usage();
        return -1;
    }

    return 0;
}