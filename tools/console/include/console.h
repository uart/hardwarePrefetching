#ifndef __CONSOLE_H
#define __CONSOLE_H

// .......................................................
// console.h
// NCurses interface for CPU performance monitoring
// .......................................................

// Global state
extern int current_view;	// 0=PMU view, 1=MSR view
extern int core_first;
extern int core_last;

extern struct dpf_console_snapshot snapshot;
extern struct dpf_console_sysinfo sysinfo;

// NCurses windows
extern WINDOW *header_win;	// System info window
extern WINDOW *main_win;	// Main metrics display
extern WINDOW *titlebar_win;	// Column headers (subwindow of main_win)

int init_colors(void);
int title_bar(const char *titles[], size_t size, int pos_x, int space);
int header(void);
int pmu_view(void);
int msr_view(void);
void handle_resize(int sig);
int scrollable(int ch);
int update_metrics(void);

#endif // __CONSOLE_H
