#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "Application/bench_config.h"
#include "Application/runtime.h"
#include "Board/Motor/motor_executor.h"
#include "Board/Motor/motor_hw_real.h"
#include "Tests/Host/fake_motor_hw_real.h"
#include "Tests/Host/fake_motor_stop_timer.h"

static void StartFixture(ApplicationRuntime *runtime)
{
    FakeMotorHwReal_Reset();
    FakeMotorStopTimer_Reset();
    assert(MotorExecutor_Initialize() == MOTOR_RESULT_OK);
    ApplicationRuntime_Initialize(runtime,
                                  &g_sd700_bench_machine_config,
                                  0U);
    ApplicationRuntime_CompleteBoot(runtime, true, 1U);
    assert(runtime->machine.state == IDLE);
    assert(MotorExecutor_OutputIsDisabled());
}

static void FeedPressure(ApplicationRuntime *runtime,
                         uint16_t pressure,
                         uint64_t sequence,
                         uint32_t now_ms)
{
    PressureReceiverSnapshot snapshot = {
        .raw_pressure_counts = pressure,
        .sample_sequence = sequence,
        .received_at_ms = now_ms
    };

    assert(ApplicationRuntime_ServicePressure(runtime,
                                              &snapshot,
                                              now_ms));
}

static void SetTarget(MachineContext *machine,
                      int32_t target,
                      uint32_t now_ms)
{
    MachineCommand command = {CMD_SET_TARGET, target};

    assert(Machine_HandleCommand(machine, &command, now_ms) ==
           COMMAND_ACCEPTED);
    assert(machine->target_valid);
}

static void StartAuto(MachineContext *machine, uint32_t now_ms)
{
    MachineCommand command = {CMD_AUTO_START, 0};

    assert(Machine_HandleCommand(machine, &command, now_ms) ==
           COMMAND_ACCEPTED);
}

static void EnterPulse(ApplicationRuntime *runtime)
{
    FeedPressure(runtime, 500U, 1U, 2U);
    SetTarget(&runtime->machine, 500, 3U);
    StartAuto(&runtime->machine, 4U);
    assert(runtime->machine.state == AUTO_SETTLE);
    ApplicationRuntime_Tick(runtime, 14U);
    assert(runtime->machine.settle_phase == SETTLE_WAIT_SAMPLE);
    FeedPressure(runtime, 400U, 2U, 15U);
    assert(runtime->machine.state == AUTO_PULSE);
    assert(MotorExecutor_ActiveRequestIsValid());
    assert(!MotorHwReal_IsDisabled());
}

static void TestApproachContactPulseAndNormalCompletion(void)
{
    ApplicationRuntime runtime;
    const MotorExecutorSnapshot *motor;
    uint32_t disable_count;

    StartFixture(&runtime);
    FeedPressure(&runtime, 50U, 1U, 2U);
    SetTarget(&runtime.machine, 500, 3U);
    StartAuto(&runtime.machine, 4U);
    assert(runtime.machine.state == AUTO_APPROACH);
    assert(MotorExecutor_ActiveRequestIsValid());
    assert(!MotorHwReal_IsDisabled());

    disable_count = FakeMotorHwReal_GetState()->disable_count;
    assert(MotorExecutor_GuardOutput() == MOTOR_RESULT_OK);
    assert(FakeMotorHwReal_GetState()->disable_count == disable_count);
    assert(!MotorHwReal_IsDisabled());

    FeedPressure(&runtime, 100U, 2U, 5U);
    assert(runtime.machine.state == AUTO_SETTLE);
    assert(MotorExecutor_OutputIsDisabled());

    ApplicationRuntime_Tick(&runtime, 15U);
    FeedPressure(&runtime, 400U, 3U, 16U);
    assert(runtime.machine.state == AUTO_PULSE);
    assert(!MotorHwReal_IsDisabled());

    FakeMotorStopTimer_TriggerNormal();
    motor = MotorExecutor_GetSnapshot();
    assert(MotorHwReal_IsDisabled());
    assert(!MotorStopTimer_IsArmed());
    assert(motor->logical_active);
    assert(motor->last_completion == MOTOR_COMPLETION_NONE);

    ApplicationRuntime_ServiceSafety(&runtime, 36U);
    motor = MotorExecutor_GetSnapshot();
    assert(runtime.machine.state == AUTO_SETTLE);
    assert(!motor->logical_active);
    assert(motor->last_completion == MOTOR_COMPLETION_NORMAL);
    assert(MotorExecutor_OutputIsDisabled());
}

static void TestBackstopFault(void)
{
    ApplicationRuntime runtime;

    StartFixture(&runtime);
    EnterPulse(&runtime);
    FakeMotorStopTimer_TriggerBackstop();
    assert(MotorHwReal_IsDisabled());
    assert(MotorExecutor_GetSnapshot()->logical_active);

    ApplicationRuntime_ServiceSafety(&runtime, 35U);
    assert(runtime.machine.state == FAULT);
    assert(runtime.machine.fault == FAULT_MOTION_TIMEOUT);
    assert(runtime.machine.fault_detail == FAULT_DETAIL_PULSE_TIMEOUT);
    assert(MotorExecutor_GetSnapshot()->last_completion ==
           MOTOR_COMPLETION_BACKSTOP);
    assert(MotorExecutor_OutputIsDisabled());
}

static void TestTimerErrorFault(void)
{
    ApplicationRuntime runtime;

    StartFixture(&runtime);
    EnterPulse(&runtime);
    FakeMotorStopTimer_TriggerError();
    assert(MotorHwReal_IsDisabled());
    assert(MotorExecutor_GetSnapshot()->logical_active);

    ApplicationRuntime_ServiceSafety(&runtime, 35U);
    assert(runtime.machine.state == FAULT);
    assert(runtime.machine.fault == FAULT_MOTOR_FAULT);
    assert(runtime.machine.fault_detail ==
           FAULT_DETAIL_MOTOR_HARDWARE);
    assert(MotorExecutor_GetSnapshot()->last_completion ==
           MOTOR_COMPLETION_ERROR);
    assert(MotorExecutor_OutputIsDisabled());
}

static void TestApproachTimerErrorFault(void)
{
    ApplicationRuntime runtime;

    StartFixture(&runtime);
    FeedPressure(&runtime, 50U, 1U, 2U);
    SetTarget(&runtime.machine, 500, 3U);
    StartAuto(&runtime.machine, 4U);
    assert(runtime.machine.state == AUTO_APPROACH);
    assert(!MotorHwReal_IsDisabled());

    FakeMotorStopTimer_TriggerError();
    assert(MotorHwReal_IsDisabled());
    ApplicationRuntime_ServiceSafety(&runtime, 5U);
    assert(runtime.machine.state == FAULT);
    assert(runtime.machine.fault == FAULT_MOTOR_FAULT);
    assert(runtime.machine.fault_detail ==
           FAULT_DETAIL_MOTOR_HARDWARE);
    assert(MotorExecutor_GetSnapshot()->last_completion ==
           MOTOR_COMPLETION_ERROR);
    assert(MotorExecutor_OutputIsDisabled());
}

static void TestStopDuringActiveRequest(void)
{
    ApplicationRuntime runtime;
    MachineCommand stop = {CMD_STOP, 0};

    StartFixture(&runtime);
    FeedPressure(&runtime, 50U, 1U, 2U);
    SetTarget(&runtime.machine, 500, 3U);
    StartAuto(&runtime.machine, 4U);
    assert(runtime.machine.state == AUTO_APPROACH);
    assert(!MotorHwReal_IsDisabled());

    assert(Machine_HandleCommand(&runtime.machine, &stop, 5U) ==
           COMMAND_ACCEPTED);
    assert(runtime.machine.state == IDLE);
    assert(MotorExecutor_OutputIsDisabled());
}

static void TestPressureFaultDuringActiveRequest(void)
{
    ApplicationRuntime runtime;
    uint32_t fault_at_ms;

    StartFixture(&runtime);
    FeedPressure(&runtime, 50U, 1U, 2U);
    SetTarget(&runtime.machine, 500, 3U);
    StartAuto(&runtime.machine, 4U);
    assert(runtime.machine.state == AUTO_APPROACH);
    assert(!MotorHwReal_IsDisabled());

    fault_at_ms = 2U +
        g_sd700_bench_machine_config.pressure_freshness_ms + 1U;
    ApplicationRuntime_ServiceSafety(&runtime, fault_at_ms);
    assert(runtime.machine.state == FAULT);
    assert(runtime.machine.fault == FAULT_PRESSURE_SENSOR_FAULT);
    assert(runtime.machine.fault_detail ==
           FAULT_DETAIL_PRESSURE_TIMEOUT);
    assert(MotorExecutor_OutputIsDisabled());
}

int main(void)
{
    TestApproachContactPulseAndNormalCompletion();
    TestBackstopFault();
    TestTimerErrorFault();
    TestApproachTimerErrorFault();
    TestStopDuringActiveRequest();
    TestPressureFaultDuringActiveRequest();
    puts("Machine + Real MotorExecutor integration host test: PASS");
    return 0;
}
