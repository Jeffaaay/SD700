#ifndef APPLICATION_FORCE_PI_H
#define APPLICATION_FORCE_PI_H

#include <stdbool.h>

/* Mathematical control units, NOT calibrated/certified Newtons. */
typedef struct
{
    float kp_mv_per_unit;
    float ki_mv_per_unit_s;
    float output_min_mv;
    float output_max_mv;
    float integral_min_mv;
    float integral_max_mv;
    float maximum_dt_s;
} ForcePiConfig;

typedef struct
{
    ForcePiConfig initialized_config;
    float integral_mv;
    bool initialized;
} ForcePiState;

/* Calculation suggestion only; this does not describe an actuator request. */
typedef struct
{
    float target;
    float measurement;
    float error;
    float dt_s;
    float proportional_mv;
    float integral_mv;
    float unlimited_output_mv;
    float limited_output_mv;
    bool saturated;
    bool integration_allowed;
} ForcePiSnapshot;

/* Initialize/reconfigure transactionally, always starting with zero integral.
 * Update rejects a changed config until Initialize is called again.
 * Reset retains the initialized config. Failed calls do not mutate state.
 * Update zeros the snapshot on failure (when snapshot is non-NULL).
 * State, config and snapshot must be distinct, non-overlapping objects. */
bool ForcePi_Initialize(ForcePiState *state, const ForcePiConfig *config);
bool ForcePi_Reset(ForcePiState *state);
bool ForcePi_Update(const ForcePiConfig *config,
                    ForcePiState *state,
                    float target,
                    float measurement,
                    float dt_s,
                    bool integration_allowed,
                    ForcePiSnapshot *snapshot);

#endif
