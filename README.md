# hardwarePrefetching
Code repository for work on hardware prefetching and controlling hardware prefetchers.

# Multi-armed Bandit Tuning Algorithms

This section documents the Multi-Armed Bandit (MAB) tuning algorithms contributed to the dynamic prefetching program. Detailed explanations of the algorithms, hyperparameters, and configurations are provided below. The implementation and analysis of these algorithms can be found in the bachelor's thesis by Daniel Brown, conducted at UART at Uppsala University in collaboration with Intel, which also includes research conducted using the DUCB algorithm. For more details, please contact [daniel.brown@it.uu.se](mailto:daniel.brown@it.uu.se).

## Multi-Armed Bandit (MAB) Algorithms

The following MAB algorithms are implemented in this program:

1. **E-greedy**
2. **UCB (Upper Confidence Bound)**
3. **DUCB (Discounted UCB)**
4. **RANDOM**

### Algorithm Descriptions

#### E-greedy
- **Description**: Selects a random arm with probability `epsilon` and the arm with the highest reward otherwise.
- **Hyperparameters**:
  - `epsilon` (float): Probability of selecting a random arm. Set via the configuration file.

#### UCB (Upper Confidence Bound)
- **Description**: Selects the arm that maximizes a confidence bound for the reward.
- **Hyperparameters**:
  - `c` (float): Constant used to scale the exploration term. Set via the configuration file.

#### DUCB (Discounted UCB)
- **Description**: Similar to UCB but uses a discounted sum of rewards.
- **Hyperparameters**:
  - `gamma` (float): Discount factor applied to past rewards. Set via the configuration file.
  - `c` (float): Constant used to scale the exploration term. Set via the configuration file.

#### RANDOM
- **Description**: Randomly selects an arm at each time interval.
- **Hyperparameters**: None.

### SD Filtering

SD filtering can be applied to any of the above algorithms with two modes: `ON` and `STEP`.

- **ON**: Pauses the algorithm and runs with default settings while the standard deviation of the IPC is over a specific threshold.
- **STEP**: Increases the time interval to `MAX_TIME_INTERVAL` while the standard deviation of the IPC is over the threshold.

#### SD Filtering Hyperparameters

- `dynamic_sd` (int): SD filtering mode (0 = OFF, 1 = ON, 2 = STEP). Set via the configuration file.
- `ipc_window_size` (int): Size of the time window over which IPC standard deviation is calculated. Set via the configuration file.
- `sd_window_size` (int): Size of the time window over which the average SD is calculated. Set via the configuration file.
- `sd_mean_threshold` (float): SD threshold above which filtering is activated. Set via the configuration file.

### Normalisation

Normalisation can be applied to any of the MAB algorithms. The reward values can be normalised with three settings:
- **0 (Never)**: No normalisation.
- **1 (Once)**: Normalise after the initial round robin.
- **3 (Periodic)**: Normalise after the initial round robin and at periodic intervals defined by `norm_freq`.

#### Normalisation Hyperparameters

- `normalisation` (int): Normalisation mode. Set via the configuration file.
- `norm_freq` (int): Number of iterations between periodic normalisations. Set via the configuration file.

### Arm Configurations

The arm configuration determines the set of prefetcher settings used by the algorithm. The available configurations are:

0. **16 Arms**: Each combination of activating/deactivating the prefetchers MLC, AMP, LLC, and NLP.
1. **4 Arms**: Each combination of activating/deactivating the prefetchers MLC and AMP.
2. **2 Arms**: Activating or deactivating the MLC prefetcher.
3. **5 Arms**: Four combinations of the L2 demand density parameter, plus one arm with MLC off.
4. **6 Arms**: Five combinations of the L2 XQ Threshold parameter, plus one arm with MLC off.

## Configuration File (mab_config.json)

The following parameters are set in the configuration file:

- `algorithm` (string): The MAB algorithm to use (`E_GREEDY`, `UCB`, `DUCB`, `RANDOM`).
- `arm_configuration` (int): The arm configuration to use (0-4).
- `epsilon` (float): Epsilon value for E-greedy.
- `gamma` (float): Discount factor for DUCB.
- `c` (float): Exploration constant for UCB/DUCB.
- `normalisation` (int): Normalisation mode (0 = Never, 1 = Once, 3 = Periodic).
- `norm_freq` (int): Frequency of periodic normalisation.
- `dynamic_sd` (int): SD filtering mode (0 = OFF, 1 = ON, 2 = STEP).
- `ipc_window_size` (int): Window size for IPC standard deviation calculation.
- `sd_window_size` (int): Window size for average SD calculation.
- `sd_mean_threshold` (float): SD threshold for filtering.

### Command Line Parameters

- `time_interval` (int): Set from the command line. Determines the time interval for algorithm execution.

## Default Settings

The default settings for the prefetcher parameters (not algorithm hyperparameters) are provided as macros in the `msr.h` file. These settings are currently configured for the Intel Alderlake chip (12th generation). It is crucial to set these appropriately for the specific machine on which the algorithm is run.

For further details on the algorithms and the research conducted using the DUCB algorithm, please refer to the bachelor's thesis by Daniel Brown, which documents the implementation and analysis of these algorithms extensively. Thesis link: *awaiting publication*.
