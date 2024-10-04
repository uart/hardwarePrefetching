#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>

#include "common.h"
#include "mab.h"

#define TAG "MAB SETUP"

// Function to read the entire file into a memory buffer
char* read_file(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (file == NULL) return NULL;

    fseek(file, 0, SEEK_END);
    size_t length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* data = (char*)malloc(length + 1);
    
    size_t read_bytes = fread(data, 1, length, file);
    if (read_bytes < length) {
        // Handle partial or failed read
        fprintf(stderr, "Failed to read the entire file. Expected %lu bytes, read %zu bytes.\n", length, read_bytes);
        free(data);
        fclose(file);
        return NULL;
    }
    data[length] = '\0';

    fclose(file);
    return data;
}

int map_algorithm_to_enum(const char* algorithm) {
    if (strcmp(algorithm, "E_GREEDY") == 0) return E_GREEDY;
    if (strcmp(algorithm, "UCB") == 0) return UCB;
    if (strcmp(algorithm, "DUCB") == 0) return DUCB;
    if (strcmp(algorithm, "RANDOM") == 0) return RANDOM;
    return -1; // Invalid algorithm
}

// Parse JSON and setup mab_state
void setup_mab_state_from_json(mab_state* mstate, const char* config_file) {
    char* data = read_file(config_file);
    if (data == NULL) {
        fprintf(stderr, "Failed to read the config file\n");
        exit(-1);
    }

    cJSON* json = cJSON_Parse(data);
    if (json == NULL) {
        fprintf(stderr, "Error before: [%s]\n", cJSON_GetErrorPtr());
        free(data);
        exit(-1);
    }

    // Parse and validate each field
    const cJSON* algorithm = cJSON_GetObjectItemCaseSensitive(json, "algorithm");
    const cJSON* arm_configuration = cJSON_GetObjectItemCaseSensitive(json, "arm_configuration");
    const cJSON* normalisation = cJSON_GetObjectItemCaseSensitive(json, "normalisation");
    const cJSON* epsilon = cJSON_GetObjectItemCaseSensitive(json, "epsilon");
    const cJSON* gamma = cJSON_GetObjectItemCaseSensitive(json, "gamma");
    const cJSON* c = cJSON_GetObjectItemCaseSensitive(json, "c");
    const cJSON* norm_freq = cJSON_GetObjectItemCaseSensitive(json, "norm_freq");
    const cJSON* dynamic_sd = cJSON_GetObjectItemCaseSensitive(json, "dynamic_sd");
    const cJSON* ipc_window_size = cJSON_GetObjectItemCaseSensitive(json, "ipc_window_size");
    const cJSON* sd_window_size = cJSON_GetObjectItemCaseSensitive(json, "sd_window_size");
    const cJSON* sd_mean_threshold = cJSON_GetObjectItemCaseSensitive(json, "sd_mean_threshold");

    // Ensure all configuration parameters are valid
    if (cJSON_IsString(algorithm) && algorithm->valuestring != NULL) {
        mstate->algorithm = map_algorithm_to_enum(algorithm->valuestring);
        if (mstate->algorithm == -1) {
            fprintf(stderr, "Invalid algorithm specified: %s\n", algorithm->valuestring);
            exit(-1);
        }
    }

    if (cJSON_IsNumber(arm_configuration) && arm_configuration->valueint >= 0 && arm_configuration->valueint <= 5) {
        mstate->arm_configuration = arm_configuration->valueint;
    } else {
        fprintf(stderr, "Invalid arm configuration specified.\n");
        exit(-1);
    }

    if (cJSON_IsNumber(norm_freq) && norm_freq->valueint > 0) {
        mstate->norm_freq = norm_freq->valueint;
    }

    if (cJSON_IsNumber(epsilon) && epsilon->valuedouble >= 0 && epsilon->valuedouble <= 1) {
        mstate->epsilon = (float)epsilon->valuedouble;
    }

    if (cJSON_IsNumber(gamma) && gamma->valuedouble >= 0 && gamma->valuedouble <= 1) {
        mstate->gamma = (float)gamma->valuedouble;
    }

    if (cJSON_IsNumber(c) && c->valuedouble > 0) {
        mstate->c = (float)c->valuedouble;
    }

    if (cJSON_IsNumber(normalisation) && normalisation->valueint >= 0) {
        mstate->normalise = normalisation->valueint;
    }

    if (cJSON_IsNumber(dynamic_sd) && dynamic_sd->valueint >= 0) {
        mstate->dynamic_sd = dynamic_sd->valueint;
    }

    if (cJSON_IsNumber(ipc_window_size) && ipc_window_size->valueint >= 0) {
        mstate->ipc_window_size = ipc_window_size->valueint;
    }

    if (cJSON_IsNumber(sd_window_size) && sd_window_size->valueint >= 0) {
        mstate->sd_window_size = sd_window_size->valueint;
    }

    if (cJSON_IsNumber(sd_mean_threshold) && sd_mean_threshold->valuedouble >= 0) {
        mstate->sd_mean_threshold = (float)sd_mean_threshold->valuedouble;
    }

    cJSON_Delete(json);
    free(data);
}


void populate_msr_u(union msr_u msr[]) {
    populate_msr1320(&msr[0]);
    populate_msr1321(&msr[0]);
    populate_msr1322(&msr[0]);
    populate_msr1323(&msr[0]);
}


// Functions to create various arm setups

// Creates 16 arms, one for each combination of activating or deactivating the prefetchers MLC, AMP, LLC and NLP
void create_16_arms(arms_t *arms) {
    for (int i = 0; i < 16; i++) {
        // Use the bits of 'i' to decide the on/off state of each prefetcher
        int mlc_disable = (i & 0x1) > 0; // Least significant bit
        int amp_disable = (i & 0x2) > 0; // Second least significant bit
        int llcoff = (i & 0x4) > 0;      // Third bit
        int nlpoff = (i & 0x8) > 0;      // Most significant bit of the 4 bits

        msr_set_mlc_disable(&arms->hwpf_msr_values[i][0], mlc_disable);
        msr_set_amp_disable(&arms->hwpf_msr_values[i][0], amp_disable);
        msr_set_llcoff(&arms->hwpf_msr_values[i][0], llcoff);
        msr_set_nlpoff(&arms->hwpf_msr_values[i][0], nlpoff);
    }
}

// Creates 4 arms, one for each combination of activating or deactivating the prefetchers MLC and AMP
void create_4_arms(arms_t *arms) {
    for (int i = 0; i < 4; i++) {
        // Use the bits of 'i' to decide the on/off state of MLC and AMP
        int mlc_disable = (i & 0x1) > 0; // MLC disable is determined by the second bit (more significant)
        int amp_disable = (i & 0x2) > 0; // AMP disable is determined by the first bit (less significant)

        msr_set_mlc_disable(&arms->hwpf_msr_values[i][0], mlc_disable);
        msr_set_amp_disable(&arms->hwpf_msr_values[i][0], amp_disable);
    }
}

// Creates 2 arms, alternatively activating or deactivating the MLC prefetcher
void create_2_arms(arms_t *arms) {
    for (int i = 0; i <= 1; i++) {

        msr_set_mlc_disable(&arms->hwpf_msr_values[i][0], i);
    }
}

// Creates 5 arms, 4 of which set the parameter L2 demand density, one of which turns off the MLC completely
void create_5_combos_l2dd(arms_t *arms) {
    int parameter_values[] = {0, 16, 64, 255};
    int num_values = sizeof(parameter_values) / sizeof(parameter_values[0]); // Number of values in the array

    for (int i = 0; i < num_values; i++) {
        int value = parameter_values[i];
        // Set L2 demand density value in the arm's MSR values
        msr_set_l2dd(&arms->hwpf_msr_values[i][0], value);
    }
    // Diable MLC in final arm 
    msr_set_mlc_disable(&arms->hwpf_msr_values[num_values][0], 1);
}

// Creates 6 arms, 5 of which set the parameter L2 XQ Threshold, one of which turns off the MLC completely
void create_6_combos_l2xq(arms_t *arms) {
    int parameter_values[] = {0, 4, 8, 16, 31}; // Array of parameter values
    int num_values = sizeof(parameter_values) / sizeof(parameter_values[0]); // Number of values in the array

    for (int i = 0; i < num_values; i++) {
        int value = parameter_values[i];
        // Set MLC and AMP disable states in the arm's MSR values
        msr_set_l2xq(&arms->hwpf_msr_values[i][0], value);
    }
    msr_set_mlc_disable(&arms->hwpf_msr_values[num_values][0], 1);
}



void create_arms(arms_t *arms, mab_state *mstate) {
    switch (mstate->arm_configuration) {
        case 0:
            create_16_arms(arms);
            mstate->num_arms = 16;
            break;
        case 1:
            create_4_arms(arms);
            mstate->num_arms = 4;
            break;
        case 2:
            create_5_combos_l2dd(arms);
            mstate->num_arms = 5;
            break;
        case 3:
            create_6_combos_l2xq(arms);
            mstate->num_arms = 6;
            break;
        case 4:
            create_2_arms(arms);
            mstate->num_arms = 2;
            break;
        default:
            fprintf(stderr, "Invalid arm configuration specified.\n");
            exit(-1);
    }

    for (size_t i = 0; i < mstate->num_arms; i++) {
        populate_msr_u(&arms->hwpf_msr_values[i][0]);
        arms->rewards[i] = 0.0;
        arms->nums[i] = 0;
        arms->ipcs[i] = 1;
    }
}

void init_mab_strategies(mab_state *mstate)
{
    if (mstate->algorithm == E_GREEDY)
    {
        mstate->next_arm_func = next_arm_max;
        mstate->update_func = update_selections_increment;
    }
    else if (mstate->algorithm == UCB)
    {
        mstate->next_arm_func = next_arm_potential;
        mstate->update_func = update_selections_increment;
    }
     else if (mstate->algorithm == DUCB)
    {
        mstate->next_arm_func = next_arm_potential;
        mstate->update_func = update_selections_discounted;
    }
    else
    {
        loge(TAG, "Error, unkown MAB algorithm\n");
    }
}

void allocate_buffers(size_t ipc_window_size, size_t sd_window_size) {
    mstate.ipc_buffer = (float *)calloc(ipc_window_size, sizeof(float));
    mstate.sd_buffer = (float *)calloc(sd_window_size, sizeof(float));
    if (!mstate.ipc_buffer || !mstate.sd_buffer) {
        perror("Memory allocation for buffers failed");
        exit(EXIT_FAILURE);
    }
    mstate.ipc_index = 0;
    mstate.sd_index = 0;
    mstate.current_ipc_mean = 0.0;
    mstate.current_ipc_M2 = 0.0;
    mstate.ipc_n = 0;
    mstate.current_sd_mean = 0.0;
    mstate.sd_n = 0;
}

void mab_init(mab_state *mstate, size_t active_threads) {
    mstate->num_total = 0;
    mstate->arm = 0;
    mstate->num_threads = active_threads;
    mstate->rr_counter = 0;
    mstate->normalise = ONCE;
    mstate->avg_reward = 1;
    mstate->iterations = 0;
    mstate->dynamic_sd = OFF;
    mstate->norm_freq = 1000;
    mstate->sd_mean_threshold = 0;
    mstate->sd_mean_min_threshold = 0.3;

    const char *config_file = MAB_CONFIG_FILE;
    setup_mab_state_from_json(mstate, config_file);

    if (mstate->dynamic_sd == ON || mstate->dynamic_sd == STEP) {
        allocate_buffers(mstate->ipc_window_size, mstate->sd_window_size);
    }

    init_mab_strategies(mstate);

    create_arms(&arms, mstate); // Pass the mstate to use arm_configuration
    
    srand((unsigned int)time(NULL)); // Initialise for random functions used in certain MAB algorithms
}