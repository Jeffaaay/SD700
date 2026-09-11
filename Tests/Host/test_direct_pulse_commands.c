#include <assert.h>
#include <stdio.h>

#include "Application/direct_pulse_config.h"
#include "Application/machine.h"
#include "Tests/Host/fake_motor_executor.h"

static const MachineConfig s_config = {
    .maximum_target_pressure_units = 1000,
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
    .recontact_duration_ms = 20U,
    .recontact_backstop_ms = 30U,
    .pulse_command_mv = 2000U,
    .pulse_duration_ms = 10U,
    .pulse_backstop_ms = 20U,
    .automatic_cycle_timeout_ms = 500U,
    .bench_release_correction_enabled = false,
    .raw_overpressure_enabled = false,
    .raw_overpressure_limit_counts = 0U
};

static void StartFixture(MachineContext *machine)
{
    FakeMotorExecutor_Reset();
    Machine_Initialize(machine, &s_config, 0U);
    Machine_CompleteBoot(machine, true, 1U);
    assert(machine->state == IDLE);
}

static MachineCommandResult Command(MachineContext *machine,
                                    MachineCommandType type,
                                    uint32_t now_ms)
{
    MachineCommand command = {type, 0};

    return Machine_HandleCommand(machine, &command, now_ms);
}

static void TestMotorFaultDetailInvariant(void)
{
    MachineContext machine;

    StartFixture(&machine);
    Machine_ReportFault(&machine,
                        FAULT_MOTOR_FAULT,
                        FAULT_DETAIL_NONE,
                        2U);
    assert(machine.state == FAULT);
    assert(machine.fault == FAULT_MOTOR_FAULT);
    assert(machine.fault_detail == FAULT_DETAIL_MOTOR_HARDWARE);
}

#if defined(SD700_MOTOR_MODE_SCOPE_TEST)
static void AssertActiveRequest(const MachineContext *machine,
                                MotorDirection direction,
                                MachineState state)
{
    const FakeMotorExecutorState *motor =
        FakeMotorExecutor_GetState();

    assert(machine->state == state);
    assert(motor->start_pulse_count == 1U);
    assert(motor->start_run_count == 0U);
    assert(motor->snapshot.direction == direction);
    assert(motor->snapshot.command_mv ==
           SD700_DIRECT_PULSE_COMMAND_MV);
    assert(motor->snapshot.requested_duration_ms ==
           SD700_DIRECT_PULSE_DURATION_MS);
    assert(motor->snapshot.logical_backstop_ms -
           machine->state_entered_ms ==
           SD700_DIRECT_PULSE_BACKSTOP_MS);
}

static void TestScopeDirectPulses(void)
{
    MachineContext machine;
    uint32_t request_count;

    StartFixture(&machine);
    assert(Command(&machine, CMD_DIRECT_PRESS_PULSE, 2U) ==
           COMMAND_ACCEPTED);
    AssertActiveRequest(&machine,
                        MOTOR_DIRECTION_PRESS,
                        DIRECT_PRESS_PULSE);
    FakeMotorExecutor_Complete(MOTOR_COMPLETION_NORMAL);
    Machine_HandleMotorService(&machine, 2U + SD700_DIRECT_PULSE_DURATION_MS);
    assert(machine.state == IDLE);

    StartFixture(&machine);
    assert(Command(&machine, CMD_DIRECT_RELEASE_PULSE, 2U) ==
           COMMAND_ACCEPTED);
    AssertActiveRequest(&machine,
                        MOTOR_DIRECTION_RELEASE,
                        DIRECT_RELEASE_PULSE);
    FakeMotorExecutor_Complete(MOTOR_COMPLETION_NORMAL);
    Machine_HandleMotorService(&machine, 2U + SD700_DIRECT_PULSE_DURATION_MS);
    assert(machine.state == IDLE);

    StartFixture(&machine);
    assert(Command(&machine, CMD_DIRECT_PRESS_PULSE, 2U) ==
           COMMAND_ACCEPTED);
    FakeMotorExecutor_Complete(MOTOR_COMPLETION_BACKSTOP);
    Machine_HandleMotorService(&machine, 2U + SD700_DIRECT_PULSE_BACKSTOP_MS);
    assert(machine.state == FAULT);
    assert(machine.fault == FAULT_MOTION_TIMEOUT);
    assert(machine.fault_detail ==
           FAULT_DETAIL_DIRECT_PULSE_TIMEOUT);

    StartFixture(&machine);
    assert(Command(&machine, CMD_DIRECT_RELEASE_PULSE, 2U) ==
           COMMAND_ACCEPTED);
    FakeMotorExecutor_Complete(MOTOR_COMPLETION_ERROR);
    Machine_HandleMotorService(&machine, 3U);
    assert(machine.state == FAULT);
    assert(machine.fault == FAULT_MOTOR_FAULT);
    assert(machine.fault_detail != FAULT_DETAIL_NONE);

    StartFixture(&machine);
    assert(Command(&machine, CMD_DIRECT_PRESS_PULSE, 2U) ==
           COMMAND_ACCEPTED);
    assert(Command(&machine, CMD_STOP, 3U) == COMMAND_ACCEPTED);
    assert(machine.state == IDLE);
    assert(MotorExecutor_OutputIsDisabled());

    StartFixture(&machine);
    assert(Command(&machine, CMD_DIRECT_RELEASE_PULSE, 2U) ==
           COMMAND_ACCEPTED);
    assert(Command(&machine, CMD_STOP, 3U) == COMMAND_ACCEPTED);
    assert(machine.state == IDLE);
    assert(MotorExecutor_OutputIsDisabled());

    StartFixture(&machine);
    assert(Command(&machine, CMD_DIRECT_PRESS_PULSE, 2U) ==
           COMMAND_ACCEPTED);
    request_count = FakeMotorExecutor_GetState()->start_pulse_count;
    assert(Command(&machine, CMD_DIRECT_RELEASE_PULSE, 3U) ==
           COMMAND_BUSY);
    assert(FakeMotorExecutor_GetState()->start_pulse_count ==
           request_count);
    assert(FakeMotorExecutor_GetState()->snapshot.direction ==
           MOTOR_DIRECTION_PRESS);

    assert(Command(&machine, CMD_AUTO_START, 4U) ==
           COMMAND_UNSUPPORTED);
    assert(FakeMotorExecutor_GetState()->start_run_count == 0U);
}
#endif

#if defined(SD700_MOTOR_MODE_REAL_BENCH)
static void Feed(MachineContext *machine,
                 uint64_t sequence,
                 uint32_t received_at_ms,
                 bool valid,
                 uint32_t now_ms)
{
    MachinePressureSample sample = {
        .sequence = sequence,
        .received_at_ms = received_at_ms,
        .raw_pressure_counts = 50U,
        .control_pressure_units = 50,
        .frame_valid = valid,
        .control_units_valid = valid
    };

    Machine_HandlePressureSample(machine, &sample, now_ms);
}

static void TestRealBenchPressurePolicy(void)
{
    MachineContext machine;

    StartFixture(&machine);
    assert(Command(&machine, CMD_DIRECT_PRESS_PULSE, 2U) ==
           COMMAND_NOT_READY);
    assert(FakeMotorExecutor_GetState()->start_pulse_count == 0U);

    Feed(&machine, 1U, 2U, true, 2U);
    assert(Command(&machine, CMD_DIRECT_PRESS_PULSE, 3U) ==
           COMMAND_ACCEPTED);
    assert(FakeMotorExecutor_GetState()->start_pulse_count == 1U);

    StartFixture(&machine);
    Feed(&machine, 1U, 2U, true, 2U);
    assert(Command(&machine, CMD_DIRECT_PRESS_PULSE, 3U) ==
           COMMAND_ACCEPTED);
    Feed(&machine, 2U, 4U, false, 4U);
    assert(machine.state == FAULT);
    assert(machine.fault == FAULT_PRESSURE_SENSOR_FAULT);
    assert(machine.fault_detail == FAULT_DETAIL_PRESSURE_INVALID);
    assert(MotorExecutor_OutputIsDisabled());

    StartFixture(&machine);
    Feed(&machine, 2U, 2U, true, 2U);
    assert(Command(&machine, CMD_DIRECT_RELEASE_PULSE, 3U) ==
           COMMAND_ACCEPTED);
    Feed(&machine, 1U, 4U, true, 4U);
    assert(machine.state == FAULT);
    assert(machine.fault_detail == FAULT_DETAIL_PRESSURE_ORDER_LOST);
    assert(MotorExecutor_OutputIsDisabled());

    StartFixture(&machine);
    Feed(&machine, 1U, 2U, true, 2U);
    assert(Command(&machine, CMD_DIRECT_RELEASE_PULSE, 3U) ==
           COMMAND_ACCEPTED);
    Machine_CheckPressureSafety(&machine, 103U);
    assert(machine.state == FAULT);
    assert(machine.fault_detail == FAULT_DETAIL_PRESSURE_TIMEOUT);
    assert(MotorExecutor_OutputIsDisabled());

    StartFixture(&machine);
    Feed(&machine, 1U, 2U, true, 2U);
    assert(Command(&machine, CMD_AUTO_START, 3U) ==
           COMMAND_UNSUPPORTED);
    assert(FakeMotorExecutor_GetState()->start_run_count == 0U);
    assert(FakeMotorExecutor_GetState()->start_pulse_count == 0U);
}
#endif

#if !defined(SD700_MOTOR_MODE_SCOPE_TEST) && \
    !defined(SD700_MOTOR_MODE_REAL_BENCH)
static void TestUnsupportedPolicy(void)
{
    MachineContext machine;

    StartFixture(&machine);
    assert(Command(&machine, CMD_DIRECT_PRESS_PULSE, 2U) ==
           COMMAND_UNSUPPORTED);
    assert(Command(&machine, CMD_DIRECT_RELEASE_PULSE, 3U) ==
           COMMAND_UNSUPPORTED);
    assert(FakeMotorExecutor_GetState()->start_pulse_count == 0U);
#if defined(SD700_MOTOR_MODE_REAL_COMPILE_CHECK)
    assert(Command(&machine, CMD_AUTO_START, 4U) ==
           COMMAND_UNSUPPORTED);
    assert(FakeMotorExecutor_GetState()->start_run_count == 0U);
#endif
}
#endif

int main(void)
{
    TestMotorFaultDetailInvariant();
#if defined(SD700_MOTOR_MODE_SCOPE_TEST)
    TestScopeDirectPulses();
#elif defined(SD700_MOTOR_MODE_REAL_BENCH)
    TestRealBenchPressurePolicy();
#else
    TestUnsupportedPolicy();
#endif
    puts("Direct pulse Machine policy host tests: PASS");
    return 0;
}
