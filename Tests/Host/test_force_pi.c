#include <assert.h>
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "Application/force_pi.h"

/* SYNTHETIC_INPUT: mathematical examples, never machine tuning parameters. */
static const ForcePiConfig s_math = {
    2.0f, 10.0f, -100.0f, 100.0f, -80.0f, 80.0f, 1.0f
};

static void Near(float actual, float expected)
{
    assert(fabsf(actual - expected) < 0.001f);
}

static void TestProportionalAndSignedError(void)
{
    ForcePiConfig config = s_math;
    ForcePiState state;
    ForcePiSnapshot snapshot;

    config.ki_mv_per_unit_s = 0.0f;
    assert(ForcePi_Initialize(&state, &config));
    assert(ForcePi_Update(&config, &state, 250.0f, 260.0f, 0.02f, true, &snapshot));
    Near(snapshot.error, -10.0f);
    Near(snapshot.proportional_mv, -20.0f);
    Near(snapshot.limited_output_mv, -20.0f);
    Near(snapshot.integral_mv, 0.0f);
    assert(ForcePi_Update(&config, &state, 250.0f, 230.0f, 0.0f, false, &snapshot));
    Near(snapshot.error, 20.0f);
    Near(snapshot.limited_output_mv, 40.0f);
    assert(!snapshot.saturated);
    Near(snapshot.target, 250.0f);
    Near(snapshot.measurement, 230.0f);
    assert(!snapshot.integration_allowed);
}

static void TestIntegralTimeFreezeAndReset(void)
{
    ForcePiState state;
    ForcePiSnapshot snapshot;
    ForcePiConfig config = s_math;

    assert(ForcePi_Initialize(&state, &config));
    assert(ForcePi_Update(&config, &state, 10.0f, 0.0f, 0.020f, true, &snapshot));
    Near(snapshot.integral_mv, 2.0f);
    Near(snapshot.limited_output_mv, 22.0f);
    Near(snapshot.dt_s, 0.020f);
    assert(ForcePi_Update(&config, &state, 10.0f, 0.0f, 0.035f, true, &snapshot));
    Near(snapshot.integral_mv, 5.5f);
    assert(ForcePi_Update(&config, &state, 10.0f, 10.0f, 0.020f, true, &snapshot));
    Near(snapshot.integral_mv, 5.5f); /* e=0 does not reset I. */
    assert(ForcePi_Update(&config, &state, 10.0f, 5.0f, 0.500f, false, &snapshot));
    Near(snapshot.integral_mv, 5.5f);
    Near(snapshot.limited_output_mv, 15.5f);
    assert(ForcePi_Reset(&state));
    Near(state.integral_mv, 0.0f);
    assert(ForcePi_Update(&config, &state, 10.0f, 0.0f, 0.1f, true, &snapshot));
    config.ki_mv_per_unit_s = 0.0f;
    assert(!ForcePi_Update(&config, &state, 10.0f, 0.0f, 0.1f, true, &snapshot));
    Near(state.integral_mv, 10.0f); /* Config changes must initialize. */
    assert(ForcePi_Initialize(&state, &config));
    assert(ForcePi_Update(&config, &state, 10.0f, 0.0f, 0.1f, true, &snapshot));
    Near(state.integral_mv, 0.0f);
}

static void TestSaturationBothDirections(void)
{
    ForcePiConfig config = s_math;
    ForcePiState state;
    ForcePiSnapshot snapshot;
    int sign;

    for (sign = -1; sign <= 1; sign += 2)
    {
        assert(ForcePi_Initialize(&state, &config));
        assert(ForcePi_Update(&config, &state, (float)sign * 60.0f, 0.0f,
                              0.1f, true, &snapshot));
        Near(snapshot.integral_mv, 0.0f); /* outward candidate rejected */
        Near(snapshot.unlimited_output_mv, (float)sign * 120.0f);
        Near(snapshot.limited_output_mv, (float)sign * 100.0f);
        assert(snapshot.saturated);
        assert(ForcePi_Update(&config, &state, (float)sign * 10.0f, 0.0f,
                              0.5f, true, &snapshot));
        Near(snapshot.integral_mv, (float)sign * 50.0f);
        assert(!snapshot.saturated);
        assert(ForcePi_Update(&config, &state, (float)sign * 30.0f, 0.0f,
                              0.1f, true, &snapshot));
        Near(snapshot.integral_mv, (float)sign * 50.0f);
        assert(snapshot.saturated);
        assert(ForcePi_Update(&config, &state, (float)-sign * 5.0f, 0.0f,
                              0.1f, true, &snapshot));
        Near(snapshot.integral_mv, (float)sign * 45.0f);
        assert(!snapshot.saturated);
    }

    /* Reverse increments must be admitted even while I keeps u saturated. */
    config.kp_mv_per_unit = 0.0f;
    config.integral_min_mv = -200.0f;
    config.integral_max_mv = 200.0f;
    for (sign = -1; sign <= 1; sign += 2)
    {
        assert(ForcePi_Initialize(&state, &config));
        state.integral_mv = (float)sign * 150.0f;
        assert(ForcePi_Update(&config, &state, (float)-sign * 10.0f, 0.0f,
                              0.1f, true, &snapshot));
        Near(state.integral_mv, (float)sign * 140.0f);
        assert(snapshot.saturated);
    }

    config = s_math;
    config.output_min_mv = -1000.0f;
    config.output_max_mv = 1000.0f;
    assert(ForcePi_Initialize(&state, &config));
    assert(ForcePi_Update(&config, &state, 50.0f, 0.0f, 1.0f, true, &snapshot));
    Near(state.integral_mv, 80.0f);
    assert(ForcePi_Update(&config, &state, -50.0f, 0.0f, 1.0f, true, &snapshot));
    Near(state.integral_mv, -80.0f);
}

static void InvalidUpdate(const ForcePiConfig *config, ForcePiState *state,
                           float target, float measurement, float dt, bool integrate)
{
    ForcePiState saved = *state;
    ForcePiSnapshot snapshot;
    ForcePiSnapshot zero;

    (void)memset(&snapshot, 0x55, sizeof(snapshot));
    (void)memset(&zero, 0, sizeof(zero));
    assert(!ForcePi_Update(config, state, target, measurement, dt,
                           integrate, &snapshot));
    assert(memcmp(&snapshot, &zero, sizeof(snapshot)) == 0);
    assert(memcmp(state, &saved, sizeof(saved)) == 0);
}

static void TestInvalidInputsAndFiniteArithmetic(void)
{
    ForcePiState state = {0};
    ForcePiState saved;
    ForcePiSnapshot snapshot;
    ForcePiConfig config = s_math;
    ForcePiConfig invalid;
    const float bad[] = {NAN, INFINITY, -INFINITY};
    size_t i;
    int field;

    InvalidUpdate(&config, &state, 1.0f, 0.0f, 0.02f, true);
    assert(!ForcePi_Reset(&state));
    assert(!ForcePi_Reset(NULL));
    assert(!ForcePi_Initialize(NULL, &config));
    assert(ForcePi_Initialize(&state, &config));
    assert(ForcePi_Update(&config, &state, 1.0f, 0.0f, 0.02f, true, &snapshot));
    InvalidUpdate(&config, &state, 1.0f, 0.0f, 0.0f, true);
    InvalidUpdate(&config, &state, 1.0f, 0.0f, -0.02f, false);
    InvalidUpdate(&config, &state, 1.0f, 0.0f, 1.001f, false);
    InvalidUpdate(&config, &state, 1.0f, 0.0f, 20.0f, true); /* not 20 ms */
    InvalidUpdate(NULL, &state, 1.0f, 0.0f, 0.02f, true);
    saved = state;
    assert(!ForcePi_Initialize(&state, NULL));
    assert(!ForcePi_Update(&config, &state, 1.0f, 0.0f, 0.02f, true, NULL));
    assert(memcmp(&state, &saved, sizeof(state)) == 0);
    assert(!ForcePi_Update(&config, NULL, 1.0f, 0.0f, 0.02f, true, &snapshot));
    Near(snapshot.limited_output_mv, 0.0f);

    for (i = 0U; i < sizeof(bad) / sizeof(bad[0]); ++i)
    {
        InvalidUpdate(&config, &state, bad[i], 0.0f, 0.02f, true);
        InvalidUpdate(&config, &state, 1.0f, bad[i], 0.02f, false);
        InvalidUpdate(&config, &state, 1.0f, 0.0f, bad[i], false);
        for (field = 0; field < 7; ++field)
        {
            invalid = config;
            switch (field)
            {
                case 0: invalid.kp_mv_per_unit = bad[i]; break;
                case 1: invalid.ki_mv_per_unit_s = bad[i]; break;
                case 2: invalid.output_min_mv = bad[i]; break;
                case 3: invalid.output_max_mv = bad[i]; break;
                case 4: invalid.integral_min_mv = bad[i]; break;
                case 5: invalid.integral_max_mv = bad[i]; break;
                default: invalid.maximum_dt_s = bad[i]; break;
            }
            saved = state;
            assert(!ForcePi_Initialize(&state, &invalid));
            assert(memcmp(&state, &saved, sizeof(state)) == 0);
            InvalidUpdate(&invalid, &state, 1.0f, 0.0f, 0.02f, true);
        }
    }
    for (field = 0; field < 6; ++field)
    {
        invalid = config;
        switch (field)
        {
            case 0: invalid.kp_mv_per_unit = -1.0f; break;
            case 1: invalid.ki_mv_per_unit_s = -1.0f; break;
            case 2: invalid.output_min_mv = 101.0f; break;
            case 3: invalid.integral_min_mv = 81.0f; break;
            case 4: invalid.maximum_dt_s = 0.0f; break;
            default: invalid.maximum_dt_s = -1.0f; break;
        }
        assert(!ForcePi_Initialize(&state, &invalid));
        InvalidUpdate(&invalid, &state, 1.0f, 0.0f, 0.02f, true);
    }
    InvalidUpdate(&config, &state, FLT_MAX, -FLT_MAX, 0.02f, false); /* error */
    InvalidUpdate(&config, &state, FLT_MAX, 0.0f, 0.02f, false); /* P */
    config.kp_mv_per_unit = 0.0f;
    config.ki_mv_per_unit_s = FLT_MAX;
    assert(ForcePi_Initialize(&state, &config));
    InvalidUpdate(&config, &state, 2.0f, 0.0f, 0.02f, true); /* Ki*e */
    config.maximum_dt_s = 2.0f;
    assert(ForcePi_Initialize(&state, &config));
    InvalidUpdate(&config, &state, 1.0f, 0.0f, 2.0f, true); /* delta */
    state.integral_mv = FLT_MAX;
    InvalidUpdate(&config, &state, 1.0f, 0.0f, 1.0f, true); /* I+delta */
    config.kp_mv_per_unit = FLT_MAX;
    config.ki_mv_per_unit_s = 1.0f;
    config.integral_min_mv = -FLT_MAX;
    config.integral_max_mv = FLT_MAX;
    assert(ForcePi_Initialize(&state, &config));
    state.integral_mv = FLT_MAX;
    InvalidUpdate(&config, &state, 1.0f, 0.0f, 0.02f, false); /* P+I */
    InvalidUpdate(&config, &state, 1.0f, 0.0f, 0.02f, true); /* candidate P+I */
    state.integral_mv = NAN;
    InvalidUpdate(&config, &state, 1.0f, 0.0f, 0.02f, false);
}

int main(void)
{
    TestProportionalAndSignedError();
    TestIntegralTimeFreezeAndReset();
    TestSaturationBothDirections();
    TestInvalidInputsAndFiniteArithmetic();
    puts("FORCE_PI_CORE=PASS SYNTHETIC_INPUT mathematical tests only");
    return 0;
}
