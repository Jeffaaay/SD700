#include "Application/force_pi.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static bool ForcePi_ConfigIsValid(const ForcePiConfig *config)
{
    return (config != NULL) &&
           isfinite(config->kp_mv_per_unit) &&
           isfinite(config->ki_mv_per_unit_s) &&
           isfinite(config->output_min_mv) &&
           isfinite(config->output_max_mv) &&
           isfinite(config->integral_min_mv) &&
           isfinite(config->integral_max_mv) &&
           isfinite(config->maximum_dt_s) &&
           (config->kp_mv_per_unit >= 0.0f) &&
           (config->ki_mv_per_unit_s >= 0.0f) &&
           (config->output_min_mv <= config->output_max_mv) &&
           (config->integral_min_mv <= config->integral_max_mv) &&
           (config->maximum_dt_s > 0.0f);
}

static bool ForcePi_ConfigMatches(const ForcePiConfig *a,
                                  const ForcePiConfig *b)
{
    return (a->kp_mv_per_unit == b->kp_mv_per_unit) &&
           (a->ki_mv_per_unit_s == b->ki_mv_per_unit_s) &&
           (a->output_min_mv == b->output_min_mv) &&
           (a->output_max_mv == b->output_max_mv) &&
           (a->integral_min_mv == b->integral_min_mv) &&
           (a->integral_max_mv == b->integral_max_mv) &&
           (a->maximum_dt_s == b->maximum_dt_s);
}

static float ForcePi_Clamp(float value, float lower, float upper)
{
    return (value < lower) ? lower : ((value > upper) ? upper : value);
}

bool ForcePi_Initialize(ForcePiState *state, const ForcePiConfig *config)
{
    ForcePiState next = {0};

    if ((state == NULL) || (!ForcePi_ConfigIsValid(config)))
    {
        return false;
    }
    next.initialized_config = *config;
    next.initialized = true;
    *state = next;
    return true;
}

bool ForcePi_Reset(ForcePiState *state)
{
    if ((state == NULL) || (!state->initialized) ||
        (!ForcePi_ConfigIsValid(&state->initialized_config)))
    {
        return false;
    }
    state->integral_mv = 0.0f;
    return true;
}

bool ForcePi_Update(const ForcePiConfig *config,
                    ForcePiState *state,
                    float target,
                    float measurement,
                    float dt_s,
                    bool integration_allowed,
                    ForcePiSnapshot *snapshot)
{
    ForcePiSnapshot next = {0};
    float integral;
    float candidate;
    float delta;
    float candidate_output;

    if (snapshot == NULL)
    {
        return false;
    }
    (void)memset(snapshot, 0, sizeof(*snapshot));
    if ((state == NULL) || (!state->initialized) ||
        (!ForcePi_ConfigIsValid(config)) ||
        (!ForcePi_ConfigMatches(config, &state->initialized_config)) ||
        (!isfinite(state->integral_mv)) ||
        (!isfinite(target)) || (!isfinite(measurement)) ||
        (!isfinite(dt_s)) || (dt_s < 0.0f) ||
        ((dt_s == 0.0f) && integration_allowed) ||
        (dt_s > config->maximum_dt_s))
    {
        return false;
    }

    next.target = target;
    next.measurement = measurement;
    next.error = target - measurement;
    next.dt_s = dt_s;
    next.integration_allowed = integration_allowed;
    next.proportional_mv = config->kp_mv_per_unit * next.error;
    if ((!isfinite(next.error)) || (!isfinite(next.proportional_mv)))
    {
        return false;
    }

    /* Ki=0 is strictly P-only, including when integration is frozen. */
    integral = (config->ki_mv_per_unit_s == 0.0f) ?
               0.0f : state->integral_mv;
    if (integration_allowed && (config->ki_mv_per_unit_s > 0.0f))
    {
        delta = config->ki_mv_per_unit_s * next.error;
        if (!isfinite(delta))
        {
            return false;
        }
        delta *= dt_s;
        candidate = integral + delta;
        if ((!isfinite(delta)) || (!isfinite(candidate)))
        {
            return false;
        }
        candidate = ForcePi_Clamp(candidate,
                                  config->integral_min_mv,
                                  config->integral_max_mv);
        candidate_output = next.proportional_mv + candidate;
        if (!isfinite(candidate_output))
        {
            return false;
        }
        if (!(((candidate_output > config->output_max_mv) &&
               (next.error > 0.0f)) ||
              ((candidate_output < config->output_min_mv) &&
               (next.error < 0.0f))))
        {
            integral = candidate;
        }
    }
    next.integral_mv = integral;
    next.unlimited_output_mv = next.proportional_mv + integral;
    if (!isfinite(next.unlimited_output_mv))
    {
        return false;
    }
    next.limited_output_mv = ForcePi_Clamp(next.unlimited_output_mv,
                                          config->output_min_mv,
                                          config->output_max_mv);
    next.saturated = next.limited_output_mv != next.unlimited_output_mv;
    state->integral_mv = integral;
    *snapshot = next;
    return true;
}
