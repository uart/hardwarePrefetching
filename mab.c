#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "mab.h"
#include "msr.h"
#include "pmu.h"
#include "log.h"
#include "common.h"

#define TAG "MAB"

mab_state mstate;
arms_t arms;

// Next Arm Functions

size_t next_arm_rr(mab_state *mstate) {
    return mstate->rr_counter;
}

size_t next_arm_max(struct mab_state *mstate) {

    float epsilon = mstate->epsilon;
    size_t num_arms = mstate->num_arms;
    float r = (float)rand() / RAND_MAX;

    if (r < epsilon) {
        return (size_t)rand() % num_arms;
    } else {
        size_t max_index = 0;
        float max_reward = arms.rewards[0];

        for (size_t i = 1; i < mstate->num_arms; ++i) {
            if (arms.rewards[i] > max_reward) {
                max_reward = arms.rewards[i];
                max_index = i;
            }
        }

        return max_index;
    }
}

size_t next_arm_random(mab_state *mstate) {
    size_t arm = rand() / (RAND_MAX / mstate->num_arms);
    return arm;
}

static inline float exploration_factor(struct mab_state *mstate, size_t arm_num, float log_num_total) {
    return arms.rewards[arm_num] + mstate->c * sqrt(log_num_total / arms.nums[arm_num]);
}

size_t next_arm_potential(struct mab_state *mstate) {
    size_t num_arms = mstate->num_arms;

    float log_num_total = log(mstate->num_total);  // Precompute log value
    size_t max_index = 0;
    float max_reward = exploration_factor(mstate, 0, log_num_total);

    for (size_t i = 1; i < num_arms; ++i) {
        float arm_reward = exploration_factor(mstate, i, log_num_total);
        if (arm_reward > max_reward) {
            max_reward = arm_reward;
            max_index = i;
        }
    }

    return max_index;
}

size_t next_arm_default(mab_state *mstate) {
    (void)mstate;
    return 0;
}


// Update Selections Functions

void update_selections_rr(mab_state *mstate) {
    arms.nums[mstate->arm] = 1;
    mstate->num_total++;
    mstate->rr_counter++;
}

void update_selections_increment(mab_state *mstate) {
    arms.nums[mstate->arm] ++;
    mstate->num_total ++;
}

void update_selections_discounted(mab_state *mstate) {
    for (size_t i = 0; i < mstate->num_arms; ++i) {
        arms.nums[i] *= mstate->gamma;
    }
    arms.nums[mstate->arm] ++;
    mstate->num_total = (mstate->gamma * mstate->num_total) + 1;
}

void update_selections_none(mab_state *mstate) {
    (void)mstate;
    return;
}


// Update Reward Functions

float get_reward(int arm_num) {
    (void)arm_num; // Explicitly unused
    float reward = (double) gtinfo[1].instructions_retired / (double) gtinfo[1].cpu_cycles;

    if (mstate.mode == RR_RESTART || MAIN_LOOP_TRANSITION) {
        arms.ipcs[arm_num] = reward;
    }
    else {
        // Update raw IPC rolling average for normalisations
        arms.ipcs[arm_num] = (arms.ipcs[arm_num] * (arms.nums[arm_num] - 1) + reward) / arms.nums[arm_num];
    }

    return reward;
}

float update_reward(int arm_num) {
    float rstep = get_reward(arm_num);
    logv(TAG, "Raw reward: %.3f\n", rstep);

    // Calculate normalised reward and update reward rolling average
    float normalised_reward = rstep / mstate.avg_reward;
    logv(TAG, "Step reward: %.3f\n", normalised_reward);
    return (arms.rewards[arm_num] * (arms.nums[arm_num] - 1) + normalised_reward) / arms.nums[arm_num];
}


// Evaluation and setup functions

void set_msrs(mab_state *mstate, size_t arm_num) {
    if (arm_num != mstate->arm) {
        for(size_t i = 0; i < mstate->num_threads; i++){
            gtinfo[i].hwpf_msr_dirty = 1;
        }
        logv(TAG, "Switching to Arm %d\n", mstate->arm);
    }
}

void setup_arm(mab_state *mstate, next_arm_strategy_t next_arm_strategy, update_strategy_t update_strategy) {

    int prev_arm = mstate->arm;
    mstate->arm = next_arm_strategy(mstate);
    mstate->iterations++;
    set_msrs(mstate, prev_arm);
    update_strategy(mstate);
}

int evaluate_arm(mab_state *mstate, RewardUpdateFunc rewardFunc, const char *evaluationType) {
    size_t prev_arm = mstate->arm;
    arms.rewards[prev_arm] = rewardFunc(prev_arm);

    logv(TAG, "%s arm %d, av.reward: %.3f, arm_total: %.3f, num_total: %.1f\n",
         evaluationType, prev_arm, arms.rewards[prev_arm], arms.nums[prev_arm], mstate->num_total);
    return prev_arm;
}

void normalise_rewards() {
    float reward_total = 0;
    for (size_t i = 0; i < mstate.num_arms; i++) {
        reward_total += arms.ipcs[i];
    }
    float avg_reward = reward_total / (mstate.num_arms);
    
    for (int i = 0; i < core_last; i++) {
        arms.rewards[i] /= avg_reward;
    }
    mstate.avg_reward = avg_reward;

    logv(TAG, "Normalising rewards: IPC av. = %f\n", mstate.avg_reward);
}


// Update Standard Deviation functions

float update_and_fetch_sd_mean(mab_state *mstate, float new_ipc) {
    // Update IPC statistics
    float old_ipc = mstate->ipc_buffer[mstate->ipc_index];
    mstate->ipc_buffer[mstate->ipc_index] = new_ipc;
    mstate->ipc_index = (mstate->ipc_index + 1) % mstate->ipc_window_size;

    if (mstate->ipc_n < mstate->ipc_window_size) mstate->ipc_n++;
    
    float new_mean = mstate->current_ipc_mean + (new_ipc - old_ipc) / mstate->ipc_window_size;
    mstate->current_ipc_M2 += (new_ipc - old_ipc) * (new_ipc - new_mean + old_ipc - mstate->current_ipc_mean);
    mstate->current_ipc_mean = new_mean;

    if (mstate->ipc_n == mstate->ipc_window_size) {
        float new_variance = mstate->current_ipc_M2 / mstate->ipc_window_size;
        float new_sd = sqrt(new_variance);
        logv(TAG, "Current SD: %f\n", new_sd);

        // Update the SD buffer for averaging SDs
        float old_sd = mstate->sd_buffer[mstate->sd_index];
        mstate->sd_buffer[mstate->sd_index] = new_sd;
        mstate->sd_index = (mstate->sd_index + 1) % mstate->sd_window_size;

        if (mstate->sd_n < mstate->sd_window_size) {
            mstate->sd_n++;
            mstate->current_sd_mean += (new_sd - mstate->current_sd_mean) / mstate->sd_n;
        } else {
            // Adjust the mean based on the new and old values
            mstate->current_sd_mean += (new_sd - old_sd) / mstate->sd_window_size;
        }
        logv(TAG, "Current SD mean: %f\n", mstate->current_sd_mean);
    }

    return mstate->current_sd_mean;
}


// Main MAB algorithm

int mab(mab_state *mstate) {

    if (mstate->algorithm == RANDOM) {
        setup_arm(mstate, next_arm_random, update_selections_increment);
    }
    else {

        if ((mstate->normalise == PERIODIC) && ((mstate->iterations + 1) % mstate->norm_freq == 0)) {
            normalise_rewards();
        }

        if (mstate->mode == ROUND_ROBIN) {
            if (mstate->rr_counter != 0) {
                evaluate_arm(mstate, get_reward, "ROUND_ROBIN");
            }

            setup_arm(mstate, next_arm_rr, update_selections_rr);

            logv(TAG, "RR Counter: %d, Num arms: %d\n", mstate->rr_counter, mstate->num_arms);
            if (mstate->rr_counter == mstate->num_arms) {
                mstate->rr_counter = 0;
                mstate->mode = MAIN_LOOP_TRANSITION;
            }
        }
        else if (mstate->mode == MAIN_LOOP_TRANSITION) {
            evaluate_arm(mstate, get_reward, "FINAL ROUND ROBIN");
            if (mstate->normalise == ONCE || mstate->normalise == PERIODIC) {
                normalise_rewards();
            }
            setup_arm(mstate, mstate->next_arm_func, mstate->update_func);
            mstate->mode = MAIN_LOOP;
        }
        else { // mode == MAIN_LOOP
            evaluate_arm(mstate, update_reward, "MAIN LOOP");
            setup_arm(mstate, mstate->next_arm_func, mstate->update_func);
        }
    }

    return 0;
}
