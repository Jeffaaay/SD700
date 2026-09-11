#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "Application/direct_pulse_config.h"
#include "Application/machine.h"
#include "Application/motion_build_policy.h"
#include "Tests/Host/fake_motor_executor.h"

/* SYNTHETIC_INPUT: all pressures/gains below are artificial test inputs. */
static const MachineConfig s_machine = {
    1500, 100, 5, 10, 100U, 10U, 50U,
    6000U, 50U, 60U, 3000U, 25U, 35U,
    2000U, 10U, 20U, 500U, true, true, 1600U
}; /* Positional initialization deliberately preserves MachineConfig ABI. */
static const ForcePiConfig s_pi = {
    10.0f, 100.0f, -2000.0f, 2000.0f, -1500.0f, 1500.0f, 1.0f
};

static MachineCommandResult Command(MachineContext *machine,
                                    MachineCommandType type, uint32_t now)
{
    MachineCommand command = {type, 250};
    return Machine_HandleCommand(machine, &command, now);
}

static void Fixture(MachineContext *machine, bool release_enabled)
{
    MachineConfig config = s_machine;
    config.bench_release_correction_enabled = release_enabled;
    FakeMotorExecutor_Reset();
    Machine_Initialize(machine, &config, 0U);
    Machine_CompleteBoot(machine, true, 1U);
    assert(machine->state == IDLE);
    assert(!machine->force_pi_enabled);
    assert(Command(machine, CMD_SET_TARGET, 2U) == COMMAND_ACCEPTED);
}

static void FeedAt(MachineContext *machine, int32_t pressure, uint64_t sequence,
                    uint32_t received, uint32_t now)
{
    MachinePressureSample sample = {0};
    sample.sequence = sequence;
    sample.received_at_ms = received;
    sample.raw_pressure_counts = (pressure < 0) ? 0U : (uint32_t)pressure;
    sample.control_pressure_units = pressure;
    sample.frame_valid = true;
    sample.control_units_valid = true;
    Machine_HandlePressureSample(machine, &sample, now);
}

static void Feed(MachineContext *machine, int32_t pressure, uint64_t sequence,
                  uint32_t now)
{
    FeedAt(machine, pressure, sequence, now, now);
}

#if SD700_AUTOMATIC_COMMANDS_ENABLED
static void Near(float actual, float expected)
{
    assert(fabsf(actual - expected) < 0.001f);
}

static void Trace(const MachineContext *machine, const char *event)
{
    const ForcePiSnapshot *pi = &machine->force_pi_snapshot;
    const MotorExecutorSnapshot *motor = MotorExecutor_GetSnapshot();
    printf("FORCE_PI_TRACE,SYNTHETIC_INPUT,%s,%llu,%lu,%d,%.3f,%.3f,%.3f,"
           "%.6f,%.3f,%.3f,%.3f,%.3f,%d,%d,%d,%.3f,%ld,%lu,%d\n",
           event, (unsigned long long)machine->force_pi_sample_sequence,
           (unsigned long)machine->force_pi_received_at_ms, (int)machine->state,
           (double)pi->target, (double)pi->measurement, (double)pi->error,
           (double)pi->dt_s, (double)pi->proportional_mv, (double)pi->integral_mv,
           (double)pi->unlimited_output_mv, (double)pi->limited_output_mv,
           (int)pi->saturated, (int)pi->integration_allowed,
           (int)machine->force_pi_inhibit_reason,
           (double)machine->force_pi_blocked_suggestion_mv,
           (long)machine->force_pi_requested_signed_mv,
           (unsigned long)motor->request_sequence, (int)motor->logical_active);
}

static void AssertReset(const MachineContext *machine)
{
    ForcePiSnapshot zero;
    (void)memset(&zero, 0, sizeof(zero));
    Near(machine->force_pi_state.integral_mv, 0.0f);
    assert(memcmp(&machine->force_pi_snapshot, &zero, sizeof(zero)) == 0);
    assert(!machine->force_pi_time_valid);
    assert(machine->force_pi_sample_sequence == 0U);
    assert(machine->force_pi_requested_signed_mv == 0);
    Near(machine->force_pi_blocked_suggestion_mv, 0.0f);
}

static void StartSettled(MachineContext *machine, const ForcePiConfig *config)
{
    if (config != NULL)
    {
        assert(Machine_ConfigureForcePi(machine, config) == COMMAND_ACCEPTED);
    }
    Feed(machine, 200, 1U, 3U);
    assert(Command(machine, CMD_AUTO_START, 4U) == COMMAND_ACCEPTED);
    assert(machine->state == AUTO_SETTLE);
    Feed(machine, 200, 2U, 5U); /* First observation: no integration. */
    Machine_Tick(machine, 14U);
    assert(machine->settle_phase == SETTLE_WAIT_SAMPLE);
}

static void CompletePulse(MachineContext *machine, uint32_t now)
{
    assert(MotorExecutor_Service(now) == MOTOR_RESULT_OK);
    Machine_HandleMotorService(machine, now);
    assert(machine->state == AUTO_SETTLE);
    assert(MotorExecutor_OutputIsDisabled());
}

static void TestConfigurationAndResetBoundaries(void)
{
    MachineContext machine;
    ForcePiConfig config = s_pi;
    ForcePiState saved;
    int invalid_case;

    Fixture(&machine, true);
    assert(Machine_ConfigureForcePi(NULL, &config) == COMMAND_INVALID_VALUE);
    assert(Machine_ConfigureForcePi(&machine, NULL) == COMMAND_INVALID_VALUE);
    machine.state = BOOT_SAFE;
    assert(Machine_ConfigureForcePi(&machine, &config) == COMMAND_NOT_ALLOWED);
    machine.state = IDLE;
    /* Fake reset simulates an output which is not yet disabled. */
    FakeMotorExecutor_Reset();
    assert(Machine_ConfigureForcePi(&machine, &config) == COMMAND_NOT_READY);
    assert(MotorExecutor_Disable() == MOTOR_RESULT_OK);
    assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_PRESS, 10U, 10U, 20U, 0U) == MOTOR_RESULT_OK);
    assert(Machine_ConfigureForcePi(&machine, &config) == COMMAND_NOT_READY);
    assert(MotorExecutor_Disable() == MOTOR_RESULT_OK);
    assert(Machine_ConfigureForcePi(&machine, &config) == COMMAND_ACCEPTED);
    saved = machine.force_pi_state;
    for (invalid_case = 0; invalid_case < 7; ++invalid_case)
    {
        config = s_pi;
        switch (invalid_case)
        {
            case 0: config.output_max_mv = 24000.01f; break;
            case 1: config.output_min_mv = -24000.01f; break;
            case 2: config.output_min_mv = 0.01f; break;
            case 3: config.output_max_mv = -0.01f; break;
            case 4: config.output_max_mv = 1.0e30f; break;
            case 5: config.kp_mv_per_unit = NAN; break;
            default: config.ki_mv_per_unit_s = -1.0f; break;
        }
        assert(Machine_ConfigureForcePi(&machine, &config) == COMMAND_INVALID_VALUE);
        assert(memcmp(&saved, &machine.force_pi_state, sizeof(saved)) == 0);
    }
    config = s_pi;
    config.output_min_mv = -24000.0f;
    config.output_max_mv = 24000.0f;
    assert(Machine_ConfigureForcePi(&machine, &config) == COMMAND_ACCEPTED);
    /* Seed via core calculation to check every accepted lifecycle reset. */
    assert(ForcePi_Update(&machine.force_pi_config, &machine.force_pi_state,
                          10.0f, 0.0f, 0.1f, true, &machine.force_pi_snapshot));
    assert(Machine_ConfigureForcePi(&machine, &s_pi) == COMMAND_ACCEPTED);
    AssertReset(&machine);
    assert(ForcePi_Update(&machine.force_pi_config, &machine.force_pi_state,
                          10.0f, 0.0f, 0.1f, true, &machine.force_pi_snapshot));
    assert(Command(&machine, CMD_SET_TARGET, 2U) == COMMAND_ACCEPTED);
    AssertReset(&machine);
    assert(ForcePi_Update(&machine.force_pi_config, &machine.force_pi_state,
                          10.0f, 0.0f, 0.1f, true, &machine.force_pi_snapshot));
    Feed(&machine, 200, 1U, 3U);
    assert(Command(&machine, CMD_AUTO_START, 4U) == COMMAND_ACCEPTED);
    AssertReset(&machine);
    assert(Machine_ConfigureForcePi(&machine, &s_pi) == COMMAND_NOT_ALLOWED);
    assert(Command(&machine, CMD_STOP, 5U) == COMMAND_ACCEPTED);
    AssertReset(&machine);
}

static void TestFixedDefaultAndVariableAmplitude(void)
{
    MachineContext machine;
    ForcePiConfig config = s_pi;
    const int32_t pressures[] = {150, 230, 350, 270};
    const uint32_t amplitudes[] = {700U, 200U, 700U, 200U};
    size_t i;

    config.ki_mv_per_unit_s = 0.0f;
    config.output_min_mv = -700.0f;
    config.output_max_mv = 700.0f;
    for (i = 0U; i < 4U; ++i)
    {
        Fixture(&machine, true);
        StartSettled(&machine, NULL);
        Feed(&machine, pressures[i], 3U, 25U);
        assert(machine.state == AUTO_PULSE);
        assert(MotorExecutor_GetSnapshot()->command_mv == 2000U);
        Fixture(&machine, true);
        StartSettled(&machine, &config);
        Feed(&machine, pressures[i], 3U, 25U);
        assert(machine.state == AUTO_PULSE);
        assert(MotorExecutor_GetSnapshot()->command_mv == amplitudes[i]);
        assert(MotorExecutor_GetSnapshot()->direction ==
               ((pressures[i] < 250) ? MOTOR_DIRECTION_PRESS : MOTOR_DIRECTION_RELEASE));
        assert(MotorExecutor_GetSnapshot()->requested_duration_ms == 10U);
        assert(MotorExecutor_GetSnapshot()->logical_backstop_ms == 45U);
        assert(machine.motor_request_sequence == MotorExecutor_GetSnapshot()->request_sequence);
        assert(MotorExecutor_ActiveRequestIsValid());
    }
}

static void TestCompleteCallChainAndRealSampleIntervals(void)
{
    MachineContext machine;
    ForcePiSnapshot saved;

    Fixture(&machine, true);
    assert(Machine_ConfigureForcePi(&machine, &s_pi) == COMMAND_ACCEPTED);
    Feed(&machine, 50, 1U, 3U);
    assert(Command(&machine, CMD_AUTO_START, 4U) == COMMAND_ACCEPTED);
    assert(machine.state == AUTO_APPROACH);
    assert(MotorExecutor_GetSnapshot()->command_mv == 6000U);
    Feed(&machine, 80, 2U, 5U);
    Trace(&machine, "approach_first_observation");
    Near(machine.force_pi_snapshot.dt_s, 0.0f);
    Feed(&machine, 100, 3U, 10U);
    Trace(&machine, "contact_output_off");
    assert(machine.state == AUTO_SETTLE);
    assert(MotorExecutor_OutputIsDisabled());
    Feed(&machine, 180, 4U, 15U);
    Trace(&machine, "settle_delay");
    Near(machine.force_pi_state.integral_mv, 0.0f);
    Machine_Tick(&machine, 20U);
    /* now_ms differs from received_at_ms; integrate the 20 ms receive gap. */
    FeedAt(&machine, 200, 5U, 35U, 37U);
    Trace(&machine, "settled_press");
    assert(machine.state == AUTO_PULSE);
    Near(machine.force_pi_snapshot.dt_s, 0.020f);
    Near(machine.force_pi_state.integral_mv, 100.0f);
    assert(machine.force_pi_requested_signed_mv == 600);
    saved = machine.force_pi_snapshot;
    Feed(&machine, 200, 5U, 38U);
    assert(memcmp(&saved, &machine.force_pi_snapshot, sizeof(saved)) == 0);
    assert(FakeMotorExecutor_GetState()->start_pulse_count == 1U);
    Feed(&machine, 210, 6U, 40U);
    Trace(&machine, "pulse_busy");
    Near(machine.force_pi_state.integral_mv, 100.0f);
    assert(!machine.force_pi_snapshot.integration_allowed);
    assert(machine.force_pi_requested_signed_mv == 0);
    CompletePulse(&machine, 47U);
    Feed(&machine, 240, 7U, 50U);
    Trace(&machine, "after_completion_settle");
    Machine_Tick(&machine, 57U);
    Feed(&machine, 250, 8U, 60U);
    Trace(&machine, "synthetic_hold_entry");
    assert(machine.state == AUTO_HOLD);
    assert(MotorExecutor_OutputIsDisabled());
    Near(machine.force_pi_state.integral_mv, 100.0f);
    Feed(&machine, 245, 9U, 80U);
    Trace(&machine, "hold_inside_exit_band");
    assert(machine.state == AUTO_HOLD);
    Near(machine.force_pi_state.integral_mv, 100.0f);
    Feed(&machine, 230, 10U, 100U);
    Trace(&machine, "hold_press_correction");
    assert(machine.state == AUTO_PULSE);
    Near(machine.force_pi_snapshot.dt_s, 0.020f);
    Near(machine.force_pi_state.integral_mv, 140.0f);
    assert(machine.force_pi_requested_signed_mv == 340);
    assert(FakeMotorExecutor_GetState()->start_pulse_count == 2U);
    CompletePulse(&machine, 110U);
    Machine_Tick(&machine, 120U);
    Feed(&machine, 260, 11U, 121U);
    Trace(&machine, "wrong_press_inhibited");
    assert(machine.state == AUTO_SETTLE);
    assert(machine.force_pi_inhibit_reason == FORCE_PI_INHIBIT_WRONG_DIRECTION);
    Near(machine.force_pi_blocked_suggestion_mv, 40.0f);
    Near(machine.force_pi_state.integral_mv, 140.0f);
    assert(!machine.force_pi_snapshot.integration_allowed);
    assert(machine.force_pi_requested_signed_mv == 0);
}

static void TestDuplicateBatchBusyAndHysteresis(void)
{
    MachineContext machine;
    uint32_t starts;

    Fixture(&machine, true);
    StartSettled(&machine, &s_pi);
    Feed(&machine, 250, 3U, 25U);
    assert(machine.state == AUTO_HOLD);
    Feed(&machine, 260, 4U, 25U); /* same ms, new sequence, within exit band */
    assert(machine.state == AUTO_HOLD);
    Near(machine.force_pi_snapshot.dt_s, 0.0f);
    assert(!machine.force_pi_snapshot.integration_allowed);
    Feed(&machine, 230, 5U, 25U); /* same ms can request P-only correction */
    assert(machine.state == AUTO_PULSE);
    Near(machine.force_pi_state.integral_mv, 0.0f);
    assert(machine.force_pi_requested_signed_mv == 200);
    starts = FakeMotorExecutor_GetState()->start_pulse_count;
    Feed(&machine, 200, 6U, 25U);
    Feed(&machine, 200, 6U, 26U);
    assert(FakeMotorExecutor_GetState()->start_pulse_count == starts);
    Near(machine.force_pi_state.integral_mv, 0.0f);
    CompletePulse(&machine, 35U);
    Feed(&machine, 200, 7U, 40U);
    Machine_Tick(&machine, 45U);
    Feed(&machine, 200, 7U, 46U); /* gate waits for a NEW sequence */
    assert(machine.state == AUTO_SETTLE);
    Near(machine.force_pi_state.integral_mv, 0.0f);
    /* Executor busy despite a correction-ready machine: no integral/start. */
    assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_PRESS, 100U, 10U, 20U, 47U) == MOTOR_RESULT_OK);
    starts = FakeMotorExecutor_GetState()->start_pulse_count;
    Feed(&machine, 200, 8U, 50U);
    assert(machine.force_pi_inhibit_reason == FORCE_PI_INHIBIT_BUSY);
    Near(machine.force_pi_state.integral_mv, 0.0f);
    assert(FakeMotorExecutor_GetState()->start_pulse_count == starts);
    FakeMotorExecutor_Complete(MOTOR_COMPLETION_NORMAL);
    Feed(&machine, 200, 9U, 55U);
    Near(machine.force_pi_snapshot.dt_s, 0.005f);
    Near(machine.force_pi_state.integral_mv, 25.0f); /* no wait-time catch-up */
}

static void TestWrongDirectionBothSidesAndReleaseDisabled(void)
{
    MachineContext machine;
    ForcePiConfig config = s_pi;
    int sign;

    config.kp_mv_per_unit = 0.0f;
    for (sign = -1; sign <= 1; sign += 2)
    {
        Fixture(&machine, true);
        StartSettled(&machine, &config);
        Feed(&machine, 250 - sign * 50, 3U, 25U);
        Near(machine.force_pi_state.integral_mv, (float)sign * 100.0f);
        CompletePulse(&machine, 35U);
        Machine_Tick(&machine, 45U);
        Feed(&machine, 250, 4U, 46U);
        assert(machine.state == AUTO_HOLD);
        Feed(&machine, 250 + sign * 20, 5U, 66U);
        Trace(&machine, sign > 0 ? "hold_wrong_press_inhibited" :
                                  "hold_wrong_release_inhibited");
        assert(machine.state == AUTO_SETTLE);
        assert(machine.force_pi_inhibit_reason == FORCE_PI_INHIBIT_WRONG_DIRECTION);
        Near(machine.force_pi_blocked_suggestion_mv, (float)sign * 100.0f);
        assert(!machine.force_pi_snapshot.integration_allowed);
        assert(machine.force_pi_requested_signed_mv == 0);
        assert(MotorExecutor_OutputIsDisabled());
        assert(FakeMotorExecutor_GetState()->start_pulse_count == 1U);
    }
    Fixture(&machine, false);
    StartSettled(&machine, &s_pi);
    Near(machine.force_pi_config.output_min_mv, 0.0f);
    Feed(&machine, 280, 3U, 25U);
    Trace(&machine, "release_disabled");
    assert(machine.state == AUTO_SETTLE);
    assert(machine.force_pi_inhibit_reason == FORCE_PI_INHIBIT_RELEASE_DISABLED);
    Near(machine.force_pi_snapshot.limited_output_mv, 0.0f);
    Near(machine.force_pi_blocked_suggestion_mv, -300.0f);
    Near(machine.force_pi_state.integral_mv, 0.0f);
    assert(FakeMotorExecutor_GetState()->start_pulse_count == 0U);
    Machine_Tick(&machine, 35U);
    Feed(&machine, 250, 4U, 36U);
    assert(machine.state == AUTO_HOLD);
    Feed(&machine, 280, 5U, 56U);
    assert(machine.state == AUTO_SETTLE);
    assert(machine.force_pi_inhibit_reason == FORCE_PI_INHIBIT_RELEASE_DISABLED);
    assert(FakeMotorExecutor_GetState()->start_pulse_count == 0U);

    /* Release disabled AND positive residual integral above the upper bound. */
    Fixture(&machine, false);
    StartSettled(&machine, &config);
    Feed(&machine, 200, 3U, 25U);
    CompletePulse(&machine, 35U);
    Machine_Tick(&machine, 45U);
    Feed(&machine, 250, 4U, 46U);
    Feed(&machine, 280, 5U, 66U);
    assert(machine.force_pi_inhibit_reason == FORCE_PI_INHIBIT_WRONG_DIRECTION);
    assert(FakeMotorExecutor_GetState()->start_pulse_count == 1U);
    assert(machine.force_pi_requested_signed_mv == 0);
    Near(machine.force_pi_state.integral_mv, 100.0f);
    assert(MotorExecutor_OutputIsDisabled());
}

static void TestFinalDirectionAndRecontact(void)
{
    MachineContext machine;
    ForcePiConfig config = s_pi;
    int sign;

    /* A valid but unusual I clamp must never reverse the requested motion. */
    for (sign = -1; sign <= 1; sign += 2)
    {
        Fixture(&machine, true);
        config.integral_min_mv = (sign > 0) ? 1000.0f : -1500.0f;
        config.integral_max_mv = (sign > 0) ? 1500.0f : -1000.0f;
        StartSettled(&machine, &config);
        Feed(&machine, 250 + sign * 50, 3U, 25U);
        assert(machine.force_pi_inhibit_reason == FORCE_PI_INHIBIT_WRONG_DIRECTION);
        Near(machine.force_pi_blocked_suggestion_mv, (float)sign * 500.0f);
        Near(machine.force_pi_state.integral_mv, 0.0f);
        assert(!machine.force_pi_snapshot.integration_allowed);
        assert(FakeMotorExecutor_GetState()->start_pulse_count == 0U);
        assert(MotorExecutor_OutputIsDisabled());
    }
    Fixture(&machine, true);
    StartSettled(&machine, &s_pi);
    Feed(&machine, 250, 3U, 25U);
    Feed(&machine, 90, 4U, 45U);
    assert(machine.state == AUTO_APPROACH);
    assert(machine.approach_profile == APPROACH_RECONTACT);
    assert(MotorExecutor_GetSnapshot()->command_mv == 3000U);
    assert(!machine.force_pi_snapshot.integration_allowed);
    Near(machine.force_pi_state.integral_mv, 0.0f);
    Feed(&machine, 100, 5U, 50U);
    assert(machine.state == AUTO_SETTLE);
    assert(MotorExecutor_OutputIsDisabled());
}

static void TestZeroSmallRoundingAndBoundedWaiting(void)
{
    MachineContext machine;
    ForcePiConfig config = s_pi;
    const float gains[] = {0.0f, 0.01f, 0.0199f, 0.031f};
    size_t i;
    uint32_t now;
    uint64_t sequence;

    config.ki_mv_per_unit_s = 0.0f;
    for (i = 0U; i < 4U; ++i)
    {
        config.kp_mv_per_unit = gains[i];
        Fixture(&machine, true);
        StartSettled(&machine, &config);
        Feed(&machine, 200, 3U, 25U);
        if (i == 3U)
        {
            assert(machine.state == AUTO_PULSE);
            assert(machine.force_pi_requested_signed_mv == 2);
        }
        else
        {
            assert(machine.state == AUTO_SETTLE);
            assert(machine.settle_phase == SETTLE_WAIT_DELAY);
            assert(machine.force_pi_inhibit_reason == FORCE_PI_INHIBIT_BELOW_ONE_MV);
            assert(FakeMotorExecutor_GetState()->start_pulse_count == 0U);
            assert(MotorExecutor_OutputIsDisabled());
            Feed(&machine, 200, 4U, 26U);
            assert(machine.settle_phase == SETTLE_WAIT_DELAY);
            Machine_Tick(&machine, 35U);
            Machine_CheckPressureSafety(&machine, 85U);
            assert(machine.fault_detail == FAULT_DETAIL_SETTLE_FEEDBACK_TIMEOUT);
        }
    }
    config.kp_mv_per_unit = 0.0f;
    Fixture(&machine, true);
    StartSettled(&machine, &config);
    Feed(&machine, 250, 3U, 25U);
    Feed(&machine, 200, 4U, 45U); /* zero correction leaves HOLD, starts timeout */
    assert(machine.cycle_started_ms == 45U);
    sequence = 4U;
    for (now = 55U; now < 545U; now += 10U)
    {
        Machine_Tick(&machine, now);
        Feed(&machine, 200, ++sequence, now);
        assert(machine.state == AUTO_SETTLE);
    }
    Feed(&machine, 200, ++sequence, 545U);
    assert(machine.fault_detail == FAULT_DETAIL_CYCLE_TIMEOUT);
    assert(FakeMotorExecutor_GetState()->start_pulse_count == 0U);
}

static void TestStopsFaultsAndNoSubsequentMovement(void)
{
    MachineContext machine;
    uint32_t starts;
    int fault_case;

    for (fault_case = 0; fault_case < 7; ++fault_case)
    {
        Fixture(&machine, true);
        StartSettled(&machine, &s_pi);
        Feed(&machine, 200, 3U, 25U);
        Near(machine.force_pi_state.integral_mv, 100.0f);
        starts = FakeMotorExecutor_GetState()->start_pulse_count;
        switch (fault_case)
        {
            case 0:
                assert(Command(&machine, CMD_STOP, 26U) == COMMAND_ACCEPTED);
                assert(machine.state == IDLE);
                break;
            case 1:
                Machine_CheckPressureSafety(&machine, 126U);
                assert(machine.fault_detail == FAULT_DETAIL_PRESSURE_TIMEOUT);
                break;
            case 2:
                Feed(&machine, 1600, 4U, 26U);
                assert(machine.fault == FAULT_OVERPRESSURE);
                break;
            case 3:
                FakeMotorExecutor_Complete(MOTOR_COMPLETION_ERROR);
                Machine_HandleMotorService(&machine, 26U);
                assert(machine.fault == FAULT_MOTOR_FAULT);
                break;
            case 4:
                assert(MotorExecutor_Service(45U) == MOTOR_RESULT_OK);
                Machine_HandleMotorService(&machine, 45U);
                assert(machine.fault_detail == FAULT_DETAIL_PULSE_TIMEOUT);
                break;
            case 5:
                Feed(&machine, 200, 2U, 26U);
                assert(machine.fault_detail == FAULT_DETAIL_PRESSURE_ORDER_LOST);
                break;
            default:
                machine.force_pi_config.kp_mv_per_unit = NAN;
                Feed(&machine, 200, 4U, 26U);
                assert(machine.fault == FAULT_INTERNAL_FAULT);
                break;
        }
        AssertReset(&machine);
        assert(MotorExecutor_OutputIsDisabled());
        Feed(&machine, 200, 10U, 127U);
        Machine_Tick(&machine, 140U);
        Machine_HandleMotorService(&machine, 140U);
        AssertReset(&machine);
        assert(MotorExecutor_OutputIsDisabled());
        assert(FakeMotorExecutor_GetState()->start_pulse_count == starts);
    }
    Fixture(&machine, true);
    StartSettled(&machine, &s_pi);
    FakeMotorExecutor_FailNextStart();
    Feed(&machine, 200, 3U, 25U);
    assert(machine.fault_detail == FAULT_DETAIL_MOTOR_REQUEST_REJECTED);
    AssertReset(&machine);
    Feed(&machine, 200, 4U, 26U);
    assert(FakeMotorExecutor_GetState()->start_pulse_count == 1U);
    assert(MotorExecutor_OutputIsDisabled());
}

static void TestFreshnessInvalidTimingAndSafetyPriority(void)
{
    MachineContext machine;
    ForcePiConfig config = s_pi;
    MachinePressureSample invalid = {0};
    int sample_case;

    for (sample_case = 0; sample_case < 5; ++sample_case)
    {
        Fixture(&machine, true);
        config.maximum_dt_s = 0.015f;
        StartSettled(&machine, &config);
        switch (sample_case)
        {
            case 0:
                FeedAt(&machine, 200, 3U, 6U, 107U);
                assert(machine.fault_detail == FAULT_DETAIL_PRESSURE_TIMEOUT);
                break;
            case 1:
                Feed(&machine, 200, 3U, 25U);
                assert(machine.fault == FAULT_INTERNAL_FAULT);
                break;
            case 2:
                FeedAt(&machine, 200, 3U, 4U, 15U); /* backward time, new seq */
                assert(machine.fault == FAULT_INTERNAL_FAULT);
                break;
            case 3:
                machine.force_pi_config.kp_mv_per_unit = NAN;
                Feed(&machine, 1600, 3U, 25U);
                assert(machine.fault == FAULT_OVERPRESSURE); /* precedes PI */
                break;
            default:
                invalid.sequence = 3U;
                Machine_HandlePressureSample(&machine, &invalid, 25U);
                assert(machine.fault_detail == FAULT_DETAIL_PRESSURE_INVALID);
                break;
        }
        AssertReset(&machine);
        assert(MotorExecutor_OutputIsDisabled());
        assert(FakeMotorExecutor_GetState()->start_pulse_count == 0U);
    }
    /* Unsigned millisecond wrap is a valid small positive receive interval. */
    Fixture(&machine, true);
    assert(Machine_ConfigureForcePi(&machine, &s_pi) == COMMAND_ACCEPTED);
    Feed(&machine, 200, 1U, UINT32_MAX - 20U);
    assert(Command(&machine, CMD_AUTO_START, UINT32_MAX - 19U) == COMMAND_ACCEPTED);
    Feed(&machine, 200, 2U, UINT32_MAX - 10U);
    Machine_Tick(&machine, UINT32_MAX - 9U);
    Feed(&machine, 200, 3U, 9U);
    Near(machine.force_pi_snapshot.dt_s, 0.020f);
    Near(machine.force_pi_state.integral_mv, 100.0f);
    assert(machine.state == AUTO_PULSE);
}
#else
static void TestRealModesRejectPiAndKeepDirectProfile(void)
{
    MachineContext machine;
    int direction;

    Fixture(&machine, true);
    assert(Machine_ConfigureForcePi(&machine, &s_pi) == COMMAND_UNSUPPORTED);
    assert(!machine.force_pi_enabled);
    Feed(&machine, 200, 1U, 3U);
    assert(Command(&machine, CMD_AUTO_START, 4U) == COMMAND_UNSUPPORTED);
    for (direction = 0; direction < 2; ++direction)
    {
        Fixture(&machine, true);
        Feed(&machine, 200, 1U, 3U);
#if SD700_DIRECT_COMMANDS_ENABLED
        assert(Command(&machine, direction == 0 ? CMD_DIRECT_PRESS_PULSE :
                                                  CMD_DIRECT_RELEASE_PULSE, 4U) == COMMAND_ACCEPTED);
        assert(MotorExecutor_GetSnapshot()->command_mv == SD700_DIRECT_PULSE_COMMAND_MV);
        assert(MotorExecutor_GetSnapshot()->command_mv == 10000U);
        assert(MotorExecutor_GetSnapshot()->requested_duration_ms == 100U);
        assert(MotorExecutor_GetSnapshot()->logical_backstop_ms == 154U);
        assert(MotorExecutor_GetSnapshot()->direction ==
               (direction == 0 ? MOTOR_DIRECTION_PRESS : MOTOR_DIRECTION_RELEASE));
        assert(Machine_ConfigureForcePi(&machine, &s_pi) == COMMAND_UNSUPPORTED);
        assert(Command(&machine, CMD_STOP, 5U) == COMMAND_ACCEPTED);
        assert(MotorExecutor_OutputIsDisabled());
#else
        assert(Command(&machine, direction == 0 ? CMD_DIRECT_PRESS_PULSE :
                                                  CMD_DIRECT_RELEASE_PULSE, 4U) == COMMAND_UNSUPPORTED);
#endif
    }
}
#endif

int main(void)
{
#if SD700_AUTOMATIC_COMMANDS_ENABLED
    TestConfigurationAndResetBoundaries();
    TestFixedDefaultAndVariableAmplitude();
    TestCompleteCallChainAndRealSampleIntervals();
    TestDuplicateBatchBusyAndHysteresis();
    TestWrongDirectionBothSidesAndReleaseDisabled();
    TestFinalDirectionAndRecontact();
    TestZeroSmallRoundingAndBoundedWaiting();
    TestStopsFaultsAndNoSubsequentMovement();
    TestFreshnessInvalidTimingAndSafetyPriority();
    puts("MACHINE_FORCE_PI=PASS SYNTHETIC_INPUT Locked/Host pulse strategy only");
#else
    TestRealModesRejectPiAndKeepDirectProfile();
    puts("MACHINE_FORCE_PI_REAL_POLICY=PASS AUTO/PI unsupported; direct profile preserved");
#endif
    return 0;
}
