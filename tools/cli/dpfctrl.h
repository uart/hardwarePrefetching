#ifndef DPFCTRL_H
#define DPFCTRL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include "../../kernelmod/kernel_common.h"
#include "../../kernelmod/kernel_api.h"



// Default values
#define DEFAULT_BUFFER_ENTRIES (10000)
#define DEFAULT_MODE_RESET (1)
#define DEFAULT_OUTPUT_FORMAT "raw"

// Command types
enum cmd_type {
		CMD_UNKNOWN = -1,
		CMD_START,
		CMD_STOP,
		CMD_DUMP,
		CMD_HELP
};

// Output formats
enum output_format {
    FMT_UNKNOWN,
    FMT_RAW,
    FMT_CSV
};

// Options structure
struct options_s {
    enum cmd_type command;
    size_t buffer_entries;
    int mode;
    const char *output_file;
    enum output_format format;

};

 
// Function prototypes (all static since they're only used within dpfctrl.c)
static int process_options(int argc, char *argv[], struct options_s *opts);
static enum cmd_type parse_command(const char *cmd);
static enum output_format parse_format(const char *fmt);
static void print_usage(void);
static void print_version(void);
static void handle_start(size_t buffer_entries, int mode);
static void handle_stop(void);
static void handle_dump(const char *filename, enum output_format format);
static size_t entries_to_bytes(size_t entries);
static void dump_data_csv(FILE *fp, const char *buffer, uint64_t buffer_size);

#endif // DPFCTRL_H
