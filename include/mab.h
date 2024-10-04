#ifndef __MAB_H
#define __MAB_H

#include "msr.h"
#include "atom_msr.h"

#define MAB_CONFIG_FILE "mab_config.json"

#define MAX_ARMS 1000
#define MAX_ITERATIONS 2000000

// Tuning Algorithms
#define MAB (2)

// MAB modes
#define ROUND_ROBIN (0)
#define MAIN_LOOP (1)
#define MAIN_LOOP_TRANSITION (2)
#define RR_RESTART (3)
#define SLEEP (4)

// MAB variants
#define E_GREEDY (0)
#define UCB (1)
#define DUCB (2)
#define RANDOM (3)

// Normalisation variants
#define NEVER (0)
#define ONCE (1)
#define ALWAYS (2)
#define PERIODIC (3)

// Dynamic SD Activation modes
#define OFF (0)
#define ON (1)
#define STEP (2)

#define MAX_TIME_INTERVAL (0.1)
#define MIN_TIME_INTERVAL (0.01)

typedef struct mab_state mab_state;
extern mab_state mstate;

typedef float (*RewardUpdateFunc)(int);
typedef size_t (*next_arm_strategy_t)(mab_state *mstate);
typedef void (*update_strategy_t)(mab_state *mstate);

typedef struct mab_state {
    int mode;
    int algorithm;
    float num_total;
    size_t arm;
    size_t num_arms;
    int arm_configuration;
    float epsilon;
    float gamma;
    float c;
    float avg_reward;
    int normalise;
    size_t norm_freq;
    size_t num_threads;
    size_t rr_counter;
    next_arm_strategy_t next_arm_func;
    update_strategy_t update_func;
    size_t iterations;

    int dynamic_sd;
    float *ipc_buffer;  // Circular buffer to store the recent IPC values
    size_t ipc_index;  // Current index in the IPC circular buffer
    float current_ipc_mean;  // Current mean of IPC values
    float current_ipc_M2;  // Sum of squares of differences from the current mean
    size_t ipc_n;  // Current count of IPC values in the buffer
    size_t ipc_window_size;

    float *sd_buffer;  // Circular buffer to store the recent SD values
    size_t sd_index;  // Current index in the SD circular buffer
    float current_sd_mean;  // Current mean of SD values
    size_t sd_n;  // Current count of SD values in the buffer
    size_t sd_window_size;  // Size of the SD window

    float sd_mean_threshold;
    float sd_mean_min_threshold;
} mab_state;

typedef struct arms {
    union msr_u hwpf_msr_values[MAX_ARMS][HWPF_MSR_FIELDS];
    float rewards[MAX_ARMS];
    float ipcs[MAX_ARMS];
    float nums[MAX_ARMS];
    float exploration_factors[MAX_ARMS];
} arms_t;
extern arms_t arms;

void mab_init(mab_state *mstate, size_t active_threads);
int mab(mab_state *mstate);
void print_arm_details(union msr_u msr[]);
void setup_mab_state_from_json(mab_state* mstate, const char* config_file);
float update_and_fetch_sd_mean(mab_state *mstate, float new_ipc);
void setup_arm(mab_state *mstate, next_arm_strategy_t next_arm_strategy, update_strategy_t update_strategy);

size_t next_arm_max(struct mab_state *mstate);
size_t next_arm_potential(struct mab_state *mstate);
size_t next_arm_default(struct mab_state *mstate);
void update_selections_increment(mab_state *mstate);
void update_selections_discounted(mab_state *mstate);
void update_selections_none(mab_state *mstate);

#endif