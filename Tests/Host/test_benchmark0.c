#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "Application/machine.h"
#include "Board/Motor/motor_executor.h"
#include "Tests/Host/fake_motor_executor.h"

static const MachineConfig s_config = {
    .maximum_target_pressure_units = 1500,
    .contact_threshold_units = 100,
    .hold_enter_tolerance_units = 5,
    .hold_exit_tolerance_units = 10,
    .pressure_freshness_ms = 100U,
    .settle_delay_ms = 10U,
    .settle_feedback_timeout_ms = 50U,
    .first_approach_command_mv = 6000U,
    .first_approach_duration_ms = 50U,
    .first_approach_backstop_ms = 60U,
    .recontact_command_mv = 3000U,
    .recontact_duration_ms = 25U,
    .recontact_backstop_ms = 35U,
    .pulse_command_mv = 2000U,
    .pulse_duration_ms = 10U,
    .pulse_backstop_ms = 20U,
    .automatic_cycle_timeout_ms = 500U,
    .bench_release_correction_enabled = true,
    .raw_overpressure_enabled = true,
    .raw_overpressure_limit_counts = 1600U
};

static void StartFixture(MachineContext *context)
{
    FakeMotorExecutor_Reset();
    Machine_Initialize(context, &s_config, 0U);
    Machine_CompleteBoot(context, true, 1U);
    assert(context->state == IDLE);
    assert(!FakeMotorExecutor_GetState()->output_enabled);
}

static void SetTarget(MachineContext *context, int32_t target, uint32_t now_ms)
{
    MachineCommand command = {CMD_SET_TARGET, target};

    assert(Machine_HandleCommand(context, &command, now_ms) ==
           COMMAND_ACCEPTED);
}

static MachineCommandResult StartAuto(MachineContext *context,
                                      uint32_t now_ms)
{
    MachineCommand command = {CMD_AUTO_START, 0};

    return Machine_HandleCommand(context, &command, now_ms);
}

static void Stop(MachineContext *context, uint32_t now_ms)
{
    MachineCommand command = {CMD_STOP, 0};

    assert(Machine_HandleCommand(context, &command, now_ms) ==
           COMMAND_ACCEPTED);
}

static void Feed(MachineContext *context,
                 uint32_t pressure,
                 uint64_t sequence,
                 uint32_t now_ms)
{
    MachinePressureSample sample = {
        .sequence = sequence,
        .received_at_ms = now_ms,
        .raw_pressure_counts = pressure,
        .control_pressure_units = (int32_t)pressure,
        .frame_valid = true,
        .control_units_valid = true
    };

    Machine_HandlePressureSample(context, &sample, now_ms);
}

static void EnterSettleWaitSample(MachineContext *context,
                                  uint32_t *now_ms,
                                  uint64_t *sequence)
{
    Feed(context, 100U, ++(*sequence), ++(*now_ms));
    assert(context->state == AUTO_SETTLE);
    Machine_Tick(context, *now_ms + s_config.settle_delay_ms);
    *now_ms += s_config.settle_delay_ms;
    assert(context->settle_phase == SETTLE_WAIT_SAMPLE);
}

static void TestBootTargetAndAdmission(void)
{
    MachineContext context;
    MachineCommand target = {CMD_SET_TARGET, 500};

    FakeMotorExecutor_Reset();
    Machine_Initialize(&context, &s_config, 0U);
    assert(context.state == BOOT_SAFE);
    assert(FakeMotorExecutor_GetState()->first_call ==
           FAKE_MOTOR_CALL_DISABLE);
    assert(!FakeMotorExecutor_GetState()->output_enabled);
    assert(Machine_HandleCommand(&context, &target, 0U) ==
           COMMAND_NOT_ALLOWED);

    Machine_CompleteBoot(&context, true, 1U);
    assert(context.state == IDLE);
    SetTarget(&context, 500, 2U);
    assert(StartAuto(&context, 3U) == COMMAND_NOT_READY);
}

static void TestApproachContactAndSettleGate(void)
{
    MachineContext context;

    StartFixture(&context);
    SetTarget(&context, 500, 2U);
    Feed(&context, 50U, 1U, 3U);
    assert(StartAuto(&context, 4U) == COMMAND_ACCEPTED);
    assert(context.state == AUTO_APPROACH);
    assert(MotorExecutor_GetSnapshot()->last_action ==
           MOTOR_ACTION_PRESS_RUN);

    Feed(&context, 100U, 2U, 5U);
    assert(context.state == AUTO_SETTLE);
    assert(context.settle_phase == SETTLE_WAIT_DELAY);
    assert(!MotorExecutor_GetSnapshot()->logical_active);

    Feed(&context, 400U, 3U, 6U);
    assert(context.state == AUTO_SETTLE);
    assert(FakeMotorExecutor_GetState()->start_pulse_count == 0U);
    Machine_Tick(&context, 15U);
    assert(context.settle_phase == SETTLE_WAIT_SAMPLE);
    assert(context.settle_gate_sequence == 3U);
    Feed(&context, 400U, 4U, 16U);
    assert(context.state == AUTO_PULSE);
    assert(MotorExecutor_GetSnapshot()->last_action ==
           MOTOR_ACTION_PRESS_PULSE);
}

static void TestSettledReleaseAndHold(void)
{
    MachineContext context;
    uint32_t now = 4U;
    uint64_t sequence = 1U;

    StartFixture(&context);
    SetTarget(&context, 500, 2U);
    Feed(&context, 50U, sequence, 3U);
    assert(StartAuto(&context, now) == COMMAND_ACCEPTED);
    EnterSettleWaitSample(&context, &now, &sequence);
    Feed(&context, 520U, ++sequence, ++now);
    assert(context.state == AUTO_PULSE);
    assert(MotorExecutor_GetSnapshot()->last_action ==
           MOTOR_ACTION_RELEASE_PULSE);

    StartFixture(&context);
    SetTarget(&context, 500, 2U);
    Feed(&context, 500U, 1U, 3U);
    assert(StartAuto(&context, 4U) == COMMAND_ACCEPTED);
    assert(context.state == AUTO_SETTLE);
    Machine_Tick(&context, 14U);
    Feed(&context, 500U, 2U, 15U);
    assert(context.state == AUTO_HOLD);
    assert(!MotorExecutor_GetSnapshot()->logical_active);

    Feed(&context, 480U, 2U, 16U);
    assert(context.state == AUTO_HOLD);

    Feed(&context, 493U, 3U, 17U);
    assert(context.state == AUTO_HOLD);
    Feed(&context, 480U, 4U, 18U);
    assert(context.state == AUTO_PULSE);
    assert(MotorExecutor_GetSnapshot()->last_action ==
           MOTOR_ACTION_PRESS_PULSE);

    StartFixture(&context);
    SetTarget(&context, 500, 2U);
    Feed(&context, 500U, 1U, 3U);
    assert(StartAuto(&context, 4U) == COMMAND_ACCEPTED);
    Machine_Tick(&context, 14U);
    Feed(&context, 500U, 2U, 15U);
    Feed(&context, 520U, 3U, 16U);
    assert(context.state == AUTO_PULSE);
    assert(MotorExecutor_GetSnapshot()->last_action ==
           MOTOR_ACTION_RELEASE_PULSE);
}

static void TestRecontactAndPulseCompletion(void)
{
    MachineContext context;

    StartFixture(&context);
    SetTarget(&context, 500, 2U);
    Feed(&context, 500U, 1U, 3U);
    assert(StartAuto(&context, 4U) == COMMAND_ACCEPTED);
    Machine_Tick(&context, 14U);
    Feed(&context, 500U, 2U, 15U);
    Feed(&context, 50U, 3U, 16U);
    assert(context.state == AUTO_APPROACH);
    assert(context.approach_profile == APPROACH_RECONTACT);

    StartFixture(&context);
    SetTarget(&context, 500, 2U);
    Feed(&context, 500U, 1U, 3U);
    assert(StartAuto(&context, 4U) == COMMAND_ACCEPTED);
    Machine_Tick(&context, 14U);
    Feed(&context, 400U, 2U, 15U);
    assert(context.state == AUTO_PULSE);
    (void)MotorExecutor_Service(25U);
    Machine_HandleMotorService(&context, 25U);
    assert(context.state == AUTO_SETTLE);
    assert(!MotorExecutor_GetSnapshot()->logical_active);
}

static void TestHoldIgnoresAutomaticCycleTimeout(void)
{
    MachineContext context;
    uint32_t now_ms;
    uint64_t sequence = 2U;

    StartFixture(&context);
    SetTarget(&context, 500, 2U);
    Feed(&context, 500U, 1U, 3U);
    assert(StartAuto(&context, 4U) == COMMAND_ACCEPTED);
    Machine_Tick(&context, 14U);
    Feed(&context, 500U, sequence, 15U);
    assert(context.state == AUTO_HOLD);
    assert(context.cycle_started_ms == 0U);

    for (now_ms = 100U; now_ms <= 1090U; now_ms += 90U)
    {
        Feed(&context, 500U, ++sequence, now_ms);
        Machine_CheckPressureSafety(&context, now_ms);
        assert(context.state == AUTO_HOLD);
        assert(context.fault == FAULT_NONE);
        assert(!MotorExecutor_GetSnapshot()->logical_active);
        assert(!FakeMotorExecutor_GetState()->output_enabled);
    }
    assert(now_ms > s_config.automatic_cycle_timeout_ms);
}

static void TestPressureAndTimeoutFaults(void)
{
    MachineContext context;
    MachinePressureSample invalid = {
        .sequence = 2U,
        .received_at_ms = 5U,
        .frame_valid = false,
        .control_units_valid = false
    };

    StartFixture(&context);
    SetTarget(&context, 500, 2U);
    Feed(&context, 50U, 1U, 3U);
    assert(StartAuto(&context, 4U) == COMMAND_ACCEPTED);
    Machine_HandlePressureSample(&context, &invalid, 5U);
    assert(context.state == FAULT);
    assert(context.fault_detail == FAULT_DETAIL_PRESSURE_INVALID);

    StartFixture(&context);
    SetTarget(&context, 500, 2U);
    Feed(&context, 50U, 1U, 3U);
    assert(StartAuto(&context, 4U) == COMMAND_ACCEPTED);
    Machine_CheckPressureSafety(&context, 104U);
    assert(context.state == FAULT);
    assert(context.fault_detail == FAULT_DETAIL_PRESSURE_TIMEOUT);

    StartFixture(&context);
    SetTarget(&context, 500, 2U);
    Feed(&context, 500U, 1U, 3U);
    assert(StartAuto(&context, 4U) == COMMAND_ACCEPTED);
    Machine_Tick(&context, 14U);
    Machine_CheckPressureSafety(&context, 64U);
    assert(context.state == FAULT);
    assert(context.fault_detail ==
           FAULT_DETAIL_SETTLE_FEEDBACK_TIMEOUT);

    StartFixture(&context);
    SetTarget(&context, 500, 2U);
    Feed(&context, 50U, 1U, 3U);
    assert(StartAuto(&context, 4U) == COMMAND_ACCEPTED);
    (void)MotorExecutor_Service(54U);
    Machine_HandleMotorService(&context, 54U);
    assert(context.state == FAULT);
    assert(context.fault_detail == FAULT_DETAIL_APPROACH_TIMEOUT);

    StartFixture(&context);
    SetTarget(&context, 500, 2U);
    Feed(&context, 500U, 1U, 3U);
    assert(StartAuto(&context, 4U) == COMMAND_ACCEPTED);
    Machine_Tick(&context, 14U);
    Feed(&context, 400U, 2U, 15U);
    (void)MotorExecutor_Service(35U);
    Machine_HandleMotorService(&context, 35U);
    assert(context.state == FAULT);
    assert(context.fault_detail == FAULT_DETAIL_PULSE_TIMEOUT);

    StartFixture(&context);
    SetTarget(&context, 500, 2U);
    Feed(&context, 500U, 1U, 3U);
    assert(StartAuto(&context, 4U) == COMMAND_ACCEPTED);
    Feed(&context, 500U, 2U, 503U);
    Machine_CheckPressureSafety(&context, 504U);
    assert(context.state == FAULT);
    assert(context.fault_detail == FAULT_DETAIL_CYCLE_TIMEOUT);
}

static void TestStopFaultRejectionOrderingAndImpossibleState(void)
{
    MachineContext context;
    MachineCommand stop = {CMD_STOP, 0};
    MachineState state;

    for (state = AUTO_APPROACH; state <= AUTO_HOLD; ++state)
    {
        StartFixture(&context);
        context.state = state;
        assert(Machine_HandleCommand(&context, &stop, 10U) ==
               COMMAND_ACCEPTED);
        assert(context.state == IDLE);
        assert(!MotorExecutor_GetSnapshot()->logical_active);
    }

    StartFixture(&context);
    Machine_ReportFault(&context, FAULT_INTERNAL_FAULT,
                        FAULT_DETAIL_INTERNAL_STATE, 2U);
    Stop(&context, 3U);
    assert(context.state == FAULT);
    assert(context.fault == FAULT_INTERNAL_FAULT);

    StartFixture(&context);
    SetTarget(&context, 500, 2U);
    Feed(&context, 50U, 1U, 3U);
    FakeMotorExecutor_FailNextStart();
    assert(StartAuto(&context, 4U) == COMMAND_EXECUTOR_FAILED);
    assert(context.state == FAULT);

    StartFixture(&context);
    SetTarget(&context, 500, 2U);
    Feed(&context, 500U, 1U, 3U);
    assert(StartAuto(&context, 4U) == COMMAND_ACCEPTED);
    Machine_Tick(&context, 14U);
    Feed(&context, 400U, 1U, 15U);
    assert(context.state == AUTO_SETTLE);
    Feed(&context, 400U, 2U, 16U);
    assert(context.state == AUTO_PULSE);
    Feed(&context, 401U, 1U, 17U);
    assert(context.state == FAULT);
    assert(context.fault_detail == FAULT_DETAIL_PRESSURE_ORDER_LOST);

    StartFixture(&context);
    context.state = (MachineState)99;
    Machine_Tick(&context, 2U);
    assert(context.state == FAULT);
    assert(context.fault == FAULT_INTERNAL_FAULT);
    assert(!MotorExecutor_GetSnapshot()->logical_active);
}

static void TestQualifiedRawOverpressure(void)
{
    MachineContext context;

    StartFixture(&context);
    Feed(&context, 1600U, 1U, 2U);
    assert(context.state == FAULT);
    assert(context.fault == FAULT_OVERPRESSURE);
}

int main(void)
{
    TestBootTargetAndAdmission();
    TestApproachContactAndSettleGate();
    TestSettledReleaseAndHold();
    TestRecontactAndPulseCompletion();
    TestHoldIgnoresAutomaticCycleTimeout();
    TestPressureAndTimeoutFaults();
    TestStopFaultRejectionOrderingAndImpossibleState();
    TestQualifiedRawOverpressure();
    puts("Benchmark 0 / Machine M01-M20 host tests: PASS");
    return 0;
}
