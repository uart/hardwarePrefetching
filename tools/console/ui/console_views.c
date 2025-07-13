#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "metrics_interface.h"
#include "console.h"

#define TITLEBAR_COLOR (1)

int num_metrics = 50;
int scroll_offset;

// Update system metrics from kernel
// Return 0 on success
//
// Populates global snapshot and sysinfo structs
int update_metrics(void)
{
	if ((collect_sysinfo(&sysinfo) == 0) &&
	    (update_console_snapshot(&snapshot) == 0)) {
		num_metrics = sysinfo.last_core - sysinfo.first_core + 1;
	};

	return 0;
}

// Initialize color pairs for UI
// Color Scheme:
// - TITLEBAR_COLOR: Black on White
int init_colors(void)
{
	start_color();
	init_pair(TITLEBAR_COLOR, COLOR_BLACK, COLOR_WHITE);

	return 0;
}

// Renders the column title bar inside main_win
// - titles:    Array of column header strings
// - size:      Number of columns
// - pos_x:     Initial X position for each column
// - space:     Spacing between columns
int title_bar(const char *titles[], size_t size, int pos_x, int space)
{
	werase(titlebar_win);
	wbkgd(titlebar_win, COLOR_PAIR(TITLEBAR_COLOR));

	// print titles in bold style
	wattron(titlebar_win, A_BOLD);
	mvwprintw(titlebar_win, 0, 2, "Core");
	for (int i = 1; i < size; i++) {
		mvwprintw(titlebar_win, 0, pos_x, "%s", titles[i]);
		pos_x += space;
	}
	wattroff(titlebar_win, A_BOLD);

	wrefresh(titlebar_win);

	return 0;
}

// Displays system information.
int header(void)
{
	double ddr_write_mbps;
	double ddr_read_mbps;

	ddr_write_mbps = (double)snapshot.ddr_write_bw / (1024 * 1024);
	ddr_read_mbps = (double)snapshot.ddr_read_bw / (1024 * 1024);

	werase(header_win);
	box(header_win, 0, 0);
	wattron(header_win, A_BOLD | A_UNDERLINE);
	mvwprintw(header_win, 1, 2, "dPF Monitor");
	wattroff(header_win, A_BOLD | A_UNDERLINE);

	mvwprintw(header_win, 2, 2, "Active Cores: %d",
		sysinfo.last_core - sysinfo.first_core + 1);

	mvwprintw(header_win, 3, 2, "Tuning: %-3s",
		  snapshot.tuning_enabled ? "ON" : "OFF");

	mvwprintw(header_win, 4, 2, "DDR BW: Read=%lu bytes( %.2f MB/s),"
				    " Write=%lu bytes ( %.2f MB/s)",
		  snapshot.ddr_read_bw,
		  ddr_read_mbps, snapshot.ddr_write_bw, ddr_write_mbps);

	mvwprintw(header_win, 5, 2, "DDR Config: BAR=%lx, Type=%s",
		  sysinfo.confirmed_bar, sysinfo.confirmed_ddr_type == -1 ?
		  "None" : sysinfo.confirmed_ddr_type == 1 ? "Client" :
		  sysinfo.confirmed_ddr_type == 2 ?
		  "Grand Ridge / Sierra Forest" : "Unknown");

	mvwprintw(header_win, 6, 2, "Processor Type: %s",
		sysinfo.is_hybrid ? "Hybrid" : "Non-Hybrid");

	mvwprintw(header_win, 7, 2,
		  "First Atom E-Core: CPU(%d) \tLast Atom E-Core: CPU(%d)",
		  sysinfo.first_core, sysinfo.last_core);

	mvwprintw(header_win, 8, 2, "Total memory BW: %d MB/s",
		  sysinfo.theoretical_bw);

	refresh();
	wrefresh(header_win);

	return 0;
}

// Display PMU counters view
// Return 0 on success
//
// Shows performance counters for:
// - Cache hits
// - Memory accesses
// - Instruction counts
int pmu_view(void)
{
	int max_rows, cols;
	int viewable_rows;
	int pos_x, space;
	size_t size;
	struct core_metrics *core;

	const char *pmu_headers[] = {"Core", "All Loads", "L2 Hit", "L3 Hit",
				"DRAM Hit", "XQ Promo", "Cycles", "Instr"};

	size = sizeof(pmu_headers) / sizeof(pmu_headers[0]);

	werase(main_win);
	box(main_win, 0, 0);

	getmaxyx(main_win, max_rows, cols);
	viewable_rows = max_rows - 8;
	(void)cols;

	wattron(main_win, A_BOLD);
	mvwprintw(main_win, 1, 2, "PMU Counters");
	wattroff(main_win, A_BOLD);

	pos_x = 12;
	space = 16;

	title_bar(pmu_headers, size, pos_x, space);

	// Print pmu values
	for (int i = 0; i < snapshot.core_count && i < viewable_rows;
	     ++i) {
		core = &snapshot.cores[i];

		mvwprintw(main_win, 4 + i, 2, "Core %-2d",
			  core->core_id);

		pos_x = 12;
		for (int j = 0; j < NUM_PMU; j++) {
			mvwprintw(main_win, 4 + i, pos_x, "%-15lu",
				  core->pmu_values[j]);
			pos_x += space;
		}
	}

	mvwprintw(main_win, max_rows - 2, 2, "Showing %d-%d of %d",
		scroll_offset + 1, (scroll_offset + viewable_rows >
		num_metrics) ? num_metrics : scroll_offset +
		viewable_rows, num_metrics);

	// usage guide
	mvwprintw(main_win, max_rows - 1, 2,
		  " Press [s] Start tuning, [t] Stop tuning, [p] ");
	wattron(main_win, A_BOLD);
	wattron(main_win, COLOR_PAIR(TITLEBAR_COLOR));
	mvwprintw(main_win, max_rows - 1, 48, "PMU");
	wattroff(main_win, A_BOLD);
	wattroff(main_win, COLOR_PAIR(TITLEBAR_COLOR));
	mvwprintw(main_win, max_rows - 1, 51, ", [m] MSR, [q] quit ");

	refresh();
	wrefresh(main_win);

	return 0;
}

// Display MSR registers view
// Return 0 on success
//
// Shows model-specific registers including:
// - Power control (0x1320-0x1324)
// - Feature control (0x1a4)
int msr_view(void)
{
	int max_rows, cols;
	int viewable_rows;
	int pos_x, space;
	size_t size;
	struct core_metrics *core;

	const char *msr_headers[] = {"Core", "0x1320", "0x1321", "0x1322",
				     "0x1323", "0x1324", "0x1a4"};

	size = sizeof(msr_headers) / sizeof(msr_headers[0]);

	werase(main_win);
	box(main_win, 0, 0);

	getmaxyx(main_win, max_rows, cols);
	(void)cols;
	viewable_rows = max_rows - 8;

	wattron(main_win, A_BOLD);
	mvwprintw(main_win, 1, 2, "MSR Settings");
	wattroff(main_win, A_BOLD);

	pos_x = 12;
	space = 18;

	title_bar(msr_headers, size, pos_x, space);

	// Print msr values
	for (int i = 0; i < snapshot.core_count && i < viewable_rows;
	     ++i) {
		core = &snapshot.cores[i];
		mvwprintw(main_win, 4 + i, 2, "Core %-2d",
			  core->core_id);

		pos_x = 12;
		for (int j = 0; j < NUM_MSR; j++) {
			mvwprintw(main_win, 4 + i, pos_x, "%-35lx",
				  core->msr_values[j]);
			pos_x += space;
		}
	}

	mvwprintw(main_win, max_rows - 2, 2, "Showing %d-%d of %d",
		  scroll_offset + 1, (scroll_offset + viewable_rows >
			num_metrics) ? num_metrics : scroll_offset +
			viewable_rows, num_metrics);

	// usage guide
	mvwprintw(main_win, max_rows - 1, 2,
		  " Press [s] Start tuning, [t] Stop tuning, [p] PMU, [m] ");
	wattron(main_win, COLOR_PAIR(TITLEBAR_COLOR));
	wattron(main_win, A_BOLD);
	mvwprintw(main_win, max_rows - 1, 57, "MSR");
	wattroff(main_win, A_BOLD);
	wattroff(main_win, COLOR_PAIR(TITLEBAR_COLOR));
	mvwprintw(main_win, max_rows - 1, 60, ", [q] quit ");

	refresh();
	wrefresh(main_win);

	return 0;
}

// Handle terminal resize
// - sig: Signal number (unused)
//
// Recreates windows with new dimensions
// Maintains titlebar as subwindow of main_win
void handle_resize(int sig)
{
	int rows, cols;
	(void)sig;
	const char *pmu_headers[] = {"Core", "All Loads", "L2 Hit",
				     "L3 Hit", "DRAM Hit", "XQ Promo",
				     "Cycles", "Instr"};

	const char *msr_headers[] = {"Core", "0x1320", "0x1321", "0x1322",
				     "0x1323", "0x1324", "0x1a4"};

	endwin();
	refresh();
	clear();

	getmaxyx(stdscr, rows, cols);

	if (header_win)
		delwin(header_win);
	if (main_win)
		delwin(main_win);
	if (titlebar_win)
		delwin(titlebar_win);

	header_win = newwin(11, cols, 0, 0);
	main_win = newwin(rows - 10, cols, 10, 0);
	titlebar_win = derwin(main_win, 1, cols - 2, 3, 1);
	box(main_win, 0, 0);

	header();

	if (current_view == 0) {
		title_bar(pmu_headers,
			  sizeof(pmu_headers) / sizeof(pmu_headers[0]), 12, 16);
		pmu_view();

	} else {
		title_bar(msr_headers,
			  sizeof(msr_headers) / sizeof(msr_headers[0]), 12, 18);
		msr_view();
	}
}

// Handles content scrolling
// - ch: Input character (KEY_UP/KEY_DOWN)
//
// implements
// - Arrow keys (↑/↓) for line-by-line
// - 'j'/'k' vi-style bindings
// - Dynamic range checking
//
// Updates scroll_offset global
int scrollable(int ch)
{
	int max_rows, cols;
	int viewable_rows;

	getmaxyx(main_win, max_rows, cols);
	(void)cols;
	viewable_rows = max_rows - 4;

	if ((ch == KEY_DOWN || ch == 'j') &&
	    scroll_offset + viewable_rows < num_metrics) {
		scroll_offset++;

	} else if ((ch == KEY_UP || ch == 'k') && scroll_offset > 0) {
		scroll_offset--;
	}

	return 0;
}
