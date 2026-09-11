#include "Tests/Host/fake_motor_executor.h"

#include <stddef.h>
#include <string.h>

static FakeMotorExecutorState s_fake_motor;

static void FakeMotorExecutor_RecordFirstCall(FakeMotorCall call)
{
    if (s_fake_motor.first_call == FAKE_MOTOR_CALL_NONE)
    {
        s_fake_motor.first_call = call;
    }
}

static MotorResult FakeMotorExecutor_Start(MotorDirection direction,
                                           uint32_t command_mv,
                                           uint32_t duration_ms,
                                           uint32_t backstop_ms,
                                           uint32_t now_ms,
                                           bool pulse)
{
    uint16_t tim2_plan;
    uint16_t tim3_plan;

    if (s_fake_motor.fail_next_start)
    {
        s_fake_motor.fail_next_start = false;
        s_fake_motor.output_enabled = false;
        s_fake_motor.snapshot.last_failure_result =
            MOTOR_RESULT_TIMER_ERROR;
        s_fake_motor.snapshot.last_failure_stage =
            MOTOR_FAILURE_STAGE_EXECUTOR_START_TIMER_ARM;
        return MOTOR_RESULT_TIMER_ERROR;
    }
    if (s_fake_motor.snapshot.logical_active)
    {
        return MOTOR_RESULT_BUSY;
    }
    if ((duration_ms == 0U) || (backstop_ms <= duration_ms) ||
        (MotorExecutor_PlanCommand(direction,
                                   command_mv,
                                   &tim2_plan,
                                   &tim3_plan) != MOTOR_RESULT_OK))
    {
        return MOTOR_RESULT_INVALID;
    }

    ++s_fake_motor.snapshot.request_sequence;
    s_fake_motor.snapshot.direction = direction;
    s_fake_motor.snapshot.last_action =
        (direction == MOTOR_DIRECTION_PRESS) ?
        (pulse ? MOTOR_ACTION_PRESS_PULSE : MOTOR_ACTION_PRESS_RUN) :
        (pulse ? MOTOR_ACTION_RELEASE_PULSE : MOTOR_ACTION_RELEASE_RUN);
    s_fake_motor.snapshot.last_completion = MOTOR_COMPLETION_NONE;
    s_fake_motor.snapshot.command_mv = command_mv;
    s_fake_motor.snapshot.requested_duration_ms = duration_ms;
    s_fake_motor.snapshot.logical_deadline_ms = now_ms + duration_ms;
    s_fake_motor.snapshot.logical_backstop_ms = now_ms + backstop_ms;
    s_fake_motor.snapshot.planned_tim2_ccr3 = tim2_plan;
    s_fake_motor.snapshot.planned_tim3_ccr3 = tim3_plan;
    s_fake_motor.snapshot.logical_active = true;
    s_fake_motor.output_enabled = true;
    return MOTOR_RESULT_OK;
}

void FakeMotorExecutor_Reset(void)
{
    (void)memset(&s_fake_motor, 0, sizeof(s_fake_motor));
    s_fake_motor.output_enabled = true;
    s_fake_motor.snapshot.last_action = MOTOR_ACTION_DISABLED;
    s_fake_motor.snapshot.physical_output_locked = true;
    s_fake_motor.snapshot.physical_output_disabled = true;
    s_fake_motor.snapshot.last_failure_result = MOTOR_RESULT_OK;
    s_fake_motor.snapshot.last_failure_stage =
        MOTOR_FAILURE_STAGE_NONE;
}

void FakeMotorExecutor_FailNextStart(void)
{
    s_fake_motor.fail_next_start = true;
}

void FakeMotorExecutor_Complete(MotorCompletion completion)
{
    s_fake_motor.snapshot.last_completion = completion;
    s_fake_motor.snapshot.logical_active = false;
    s_fake_motor.snapshot.planned_tim2_ccr3 = 0U;
    s_fake_motor.snapshot.planned_tim3_ccr3 = 0U;
    s_fake_motor.snapshot.physical_output_disabled = true;
    s_fake_motor.output_enabled = false;
    if (completion == MOTOR_COMPLETION_ERROR)
    {
        s_fake_motor.snapshot.last_failure_result =
            MOTOR_RESULT_TIMER_ERROR;
        s_fake_motor.snapshot.last_failure_stage =
            MOTOR_FAILURE_STAGE_TIMER_IRQ_UNKNOWN_STATUS;
    }
}

const FakeMotorExecutorState *FakeMotorExecutor_GetState(void)
{
    return &s_fake_motor;
}

MotorResult MotorExecutor_Initialize(void)
{
    FakeMotorExecutor_Reset();
    return MotorExecutor_Disable();
}

MotorResult MotorExecutor_Disable(void)
{
    FakeMotorExecutor_RecordFirstCall(FAKE_MOTOR_CALL_DISABLE);
    ++s_fake_motor.disable_count;
    s_fake_motor.output_enabled = false;
    s_fake_motor.snapshot.last_action = MOTOR_ACTION_DISABLED;
    s_fake_motor.snapshot.command_mv = 0U;
    s_fake_motor.snapshot.requested_duration_ms = 0U;
    s_fake_motor.snapshot.planned_tim2_ccr3 = 0U;
    s_fake_motor.snapshot.planned_tim3_ccr3 = 0U;
    s_fake_motor.snapshot.logical_active = false;
    s_fake_motor.snapshot.physical_output_locked = true;
    s_fake_motor.snapshot.physical_output_disabled = true;
    return MOTOR_RESULT_OK;
}

MotorResult MotorExecutor_PlanCommand(MotorDirection direction,
                                      uint32_t command_mv,
                                      uint16_t *planned_tim2_ccr3,
                                      uint16_t *planned_tim3_ccr3)
{
    uint16_t duty;

    if (((direction != MOTOR_DIRECTION_PRESS) &&
         (direction != MOTOR_DIRECTION_RELEASE)) ||
        (command_mv == 0U) || (command_mv > 24000U) ||
        (planned_tim2_ccr3 == NULL) ||
        (planned_tim3_ccr3 == NULL))
    {
        return MOTOR_RESULT_INVALID;
    }
    duty = (uint16_t)((command_mv + 4U) / 5U);
    if (direction == MOTOR_DIRECTION_PRESS)
    {
        *planned_tim2_ccr3 = 0U;
        *planned_tim3_ccr3 = duty;
    }
    else
    {
        *planned_tim2_ccr3 = duty;
        *planned_tim3_ccr3 = 0U;
    }
    return MOTOR_RESULT_OK;
}

MotorResult MotorExecutor_StartRun(MotorDirection direction,
                                   uint32_t command_mv,
                                   uint32_t stop_after_ms,
                                   uint32_t hard_stop_after_ms,
                                   uint32_t now_ms)
{
    FakeMotorExecutor_RecordFirstCall(FAKE_MOTOR_CALL_START_RUN);
    ++s_fake_motor.start_run_count;
    return FakeMotorExecutor_Start(direction,
                                   command_mv,
                                   stop_after_ms,
                                   hard_stop_after_ms,
                                   now_ms,
                                   false);
}

MotorResult MotorExecutor_StartPulse(MotorDirection direction,
                                     uint32_t command_mv,
                                     uint32_t duration_ms,
                                     uint32_t backstop_ms,
                                     uint32_t now_ms)
{
    FakeMotorExecutor_RecordFirstCall(FAKE_MOTOR_CALL_START_PULSE);
    ++s_fake_motor.start_pulse_count;
    return FakeMotorExecutor_Start(direction,
                                   command_mv,
                                   duration_ms,
                                   backstop_ms,
                                   now_ms,
                                   true);
}

MotorResult MotorExecutor_Service(uint32_t now_ms)
{
    FakeMotorExecutor_RecordFirstCall(FAKE_MOTOR_CALL_SERVICE);
    ++s_fake_motor.service_count;
    if (s_fake_motor.snapshot.logical_active &&
        ((int32_t)(now_ms -
                   s_fake_motor.snapshot.logical_backstop_ms) >= 0))
    {
        s_fake_motor.snapshot.logical_active = false;
        s_fake_motor.snapshot.last_completion =
            MOTOR_COMPLETION_BACKSTOP;
        s_fake_motor.output_enabled = false;
    }
    else if (s_fake_motor.snapshot.logical_active &&
             ((int32_t)(now_ms -
                        s_fake_motor.snapshot.logical_deadline_ms) >= 0))
    {
        s_fake_motor.snapshot.logical_active = false;
        s_fake_motor.snapshot.last_completion =
            MOTOR_COMPLETION_NORMAL;
        s_fake_motor.output_enabled = false;
    }
    return MOTOR_RESULT_OK;
}

MotorResult MotorExecutor_GuardOutput(void)
{
    s_fake_motor.snapshot.physical_output_locked = true;
    s_fake_motor.snapshot.physical_output_disabled = true;
    s_fake_motor.output_enabled = false;
    return MOTOR_RESULT_OK;
}

bool MotorExecutor_ActiveRequestIsValid(void)
{
    return s_fake_motor.snapshot.logical_active &&
           s_fake_motor.snapshot.physical_output_locked &&
           s_fake_motor.snapshot.physical_output_disabled;
}

bool MotorExecutor_OutputIsDisabled(void)
{
    return s_fake_motor.snapshot.physical_output_disabled &&
           (!s_fake_motor.output_enabled);
}

bool MotorExecutor_IsHealthy(void)
{
    return s_fake_motor.snapshot.physical_output_locked &&
           s_fake_motor.snapshot.physical_output_disabled;
}

const MotorExecutorSnapshot *MotorExecutor_GetStatus(void)
{
    return &s_fake_motor.snapshot;
}

const MotorExecutorSnapshot *MotorExecutor_GetSnapshot(void)
{
    return &s_fake_motor.snapshot;
}
