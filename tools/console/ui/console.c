#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "console.h"
#include "log.h"
#include "metrics_interface.h"
#include "sysdetect.h"
#include "user_api.h"

int current_view;
int core_first = -1;
int core_last = -1;

WINDOW *header_win;
WINDOW *main_win;
WINDOW *titlebar_win;

struct dpf_console_snapshot_s snapshot;
struct dpf_console_sysinfo_s sysinfo;

// Detect CPU core topology
// Sets global core_first/core_last to efficient core range
// Return 0 on success, exits on failure
int detect_cores(void)
{
	struct e_cores_layout_s e_cores;

	if (core_first == -1 || core_last == -1) {
		e_cores = get_efficient_core_ids();
		core_first = e_cores.first_efficiency_core;
		core_last = e_cores.last_efficiency_core;

		if (core_first == -1 || core_last == -1) {
			fprintf(stderr,
				"Error: No efficient cores detected.\n");

			exit(1);
		}
	}

	return 0;
}

// Main application entry
//
// Initializes:
// 1. Kernel interface
// 2. NCurses UI
// 3. Event loop
//
// Key controls:
// - s: Start tuning
// - t: Terminate tuning
// - p: PMU view
// - m: MSR view
// - q: Quit
// - ↑/↓: Scroll
int main(void)
{
	int rows, cols, ch, ret;
	time_t last_update = 0;
	time_t now;
	bool needs_refresh = true; // Flag to force initial refresh

	log_setlevel(2);

	ret = kernel_mode_init();
	if (ret < 0) {
		fprintf(stderr, "Error: Kernel module not inserted\n");
		exit(1);
	}

	ret = detect_cores();
	if (ret < 0) {
		fprintf(stderr, "Error: Failed to detect CPU cores.\n");
		exit(1);
	}

	ret = kernel_core_range(core_first, core_last);
	if (ret < 0) {
		fprintf(stderr, "Error: Failed to set core range.\n");
		exit(1);
	}

	update_metrics();

	// initialize ncurses
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	nodelay(stdscr, TRUE);
	signal(SIGWINCH, handle_resize);
	curs_set(0);

	init_colors();

	refresh();

	// create windows
	getmaxyx(stdscr, rows, cols);
	header_win = newwin(11, cols, 0, 0);
	main_win = newwin(rows - 10, cols, 10, 0);
	titlebar_win = derwin(main_win, 1, cols - 2, 3, 1);

	while (1) {
		if (update_console_snapshot(&snapshot) == 0) {
			now = time(NULL);

			// Check if 1 sec has passed or we need forced refresh
			if (needs_refresh || difftime(now, last_update) >= 1.0) {
				header();
				if (current_view == 0)
					pmu_view();
				else
					msr_view();

				last_update = now;
				needs_refresh = false;
			}

			ch = getch();

			if (ch != ERR) {
				if (ch == 'q')
					break;

				switch (ch) {
				case 's':
					snapshot.tuning_enabled = 1;
					break;

				case 't':
					snapshot.tuning_enabled = 0;
					break;

				case 'p':
					current_view = 0;
					needs_refresh = true;
					break;

				case 'm':
					current_view = 1;
					needs_refresh = true;
					break;

				default:
					break;
				}

				scrollable(ch);
			}
		} else {
			mvwprintw(main_win, 4, 2,
				  "Failed to read PMU metrics.");
		}

		napms(50);
	}

	delwin(main_win);
	delwin(titlebar_win);
	delwin(header_win);
	endwin();

	return 0;
}
