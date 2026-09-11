#include "Tests/Host/fake_motor_stop_timer.h"

#include <assert.h>
#include <string.h>

#include "Board/Motor/motor_hw_real.h"
#include "Tests/Host/fake_motor_hw_real.h"

static FakeMotorStopTimerState s_timer;
static void FakeMotorStopTimer_Trigger(MotorStopTimerEvent event);

void FakeMotorStopTimer_Reset(void)
{
    (void)memset(&s_timer, 0, sizeof(s_timer));
}

void FakeMotorStopTimer_FailNextArm(void)
{
    s_timer.fail_next_arm = true;
}

void FakeMotorStopTimer_FailNextCommit(void)
{
    s_timer.fail_next_commit = true;
}

void FakeMotorStopTimer_FailNextCancelVerification(void)
{
    s_timer.fail_next_cancel = true;
}

void FakeMotorStopTimer_ForceUnhealthy(void)
{
    s_timer.healthy = false;
}

void FakeMotorStopTimer_HoldArmedOnCancel(bool hold)
{
    s_timer.hold_armed_on_cancel = hold;
}

const FakeMotorStopTimerState *FakeMotorStopTimer_GetState(void)
{
    return &s_timer;
}

bool MotorStopTimer_Initialize(MotorStopTimerHandler handler)
{
    s_timer.last_failure_stage = MOTOR_FAILURE_STAGE_NONE;
    if (handler == NULL)
    {
        s_timer.last_failure_stage =
            MOTOR_FAILURE_STAGE_TIMER_INIT_NULL_HANDLER;
        return false;
    }
    s_timer.handler = handler;
    s_timer.initialized = true;
    s_timer.healthy = true;
    s_timer.armed = false;
    return true;
}

bool MotorStopTimer_Arm(uint32_t normal_duration_ms,
                        uint32_t backstop_ms)
{
    s_timer.last_failure_stage = MOTOR_FAILURE_STAGE_NONE;
    ++s_timer.arm_count;
    if (s_timer.fail_next_arm)
    {
        s_timer.fail_next_arm = false;
        s_timer.armed = false;
        s_timer.last_failure_stage =
            MOTOR_FAILURE_STAGE_TIMER_ARM_START_VERIFY;
        return false;
    }
    if ((!s_timer.initialized) || (!s_timer.healthy) ||
        (normal_duration_ms == 0U) ||
        (backstop_ms <= normal_duration_ms))
    {
        s_timer.armed = false;
        s_timer.last_failure_stage =
            MOTOR_FAILURE_STAGE_TIMER_ARM_PRECONDITION;
        return false;
    }
    s_timer.normal_duration_ms = normal_duration_ms;
    s_timer.backstop_ms = backstop_ms;
    s_timer.armed = true;
    return true;
}

bool MotorStopTimer_CommitArm(void)
{
    s_timer.last_failure_stage = MOTOR_FAILURE_STAGE_NONE;
    if (s_timer.fail_next_commit)
    {
        s_timer.fail_next_commit = false;
        s_timer.healthy = false;
        s_timer.last_failure_stage =
            MOTOR_FAILURE_STAGE_TIMER_COMMIT_PENDING_EVENT;
        s_timer.trigger_in_progress = true;
        s_timer.handler(MOTOR_STOP_TIMER_ERROR);
        s_timer.trigger_in_progress = false;
        return false;
    }
    if ((!s_timer.armed) || (!s_timer.healthy))
    {
        s_timer.last_failure_stage =
            MOTOR_FAILURE_STAGE_TIMER_COMMIT_PRECONDITION;
        return false;
    }
    return true;
}

void MotorStopTimer_Cancel(void)
{
    if (s_timer.trigger_in_progress)
    {
        s_timer.expiry_cancel_saw_output_disabled =
            MotorHwReal_IsDisabled();
    }
    ++s_timer.cancel_count;
    if (s_timer.fail_next_cancel || s_timer.hold_armed_on_cancel)
    {
        s_timer.fail_next_cancel = false;
        s_timer.armed = true;
        return;
    }
    s_timer.armed = false;
}

bool MotorStopTimer_IsArmed(void)
{
    const bool was_armed = s_timer.armed;

    ++s_timer.armed_query_count;
    if (s_timer.trigger_on_next_armed_query)
    {
        /* One-shot ISR injection at the validation race boundary. */
        s_timer.trigger_on_next_armed_query = false;
        FakeMotorStopTimer_Trigger(s_timer.next_armed_query_event);
        if (s_timer.armed_query_returns_previous_state)
        {
            s_timer.armed_query_returns_previous_state = false;
            return was_armed;
        }
    }
    return s_timer.armed;
}

bool MotorStopTimer_IsHealthy(void)
{
    if (s_timer.trigger_on_next_healthy_query)
    {
        s_timer.trigger_on_next_healthy_query = false;
        FakeMotorStopTimer_Trigger(s_timer.next_healthy_query_event);
    }
    return s_timer.initialized && s_timer.healthy;
}

MotorFailureStage MotorStopTimer_GetLastFailureStage(void)
{
    return s_timer.last_failure_stage;
}

void MotorStopTimer_IrqHandler(void)
{
}

static void FakeMotorStopTimer_Trigger(MotorStopTimerEvent event)
{
    assert(s_timer.armed);
    assert(s_timer.handler != NULL);
    ++s_timer.expiry_count;
    s_timer.trigger_in_progress = true;
    if (event == MOTOR_STOP_TIMER_ERROR)
    {
        s_timer.healthy = false;
        s_timer.last_failure_stage =
            MOTOR_FAILURE_STAGE_TIMER_IRQ_COMPARE_OVERCAPTURE;
    }
    s_timer.handler(event);
    s_timer.trigger_in_progress = false;
}

void FakeMotorStopTimer_TriggerNormal(void)
{
    FakeMotorStopTimer_Trigger(MOTOR_STOP_TIMER_NORMAL);
}

void FakeMotorStopTimer_TriggerBackstop(void)
{
    FakeMotorStopTimer_Trigger(MOTOR_STOP_TIMER_BACKSTOP);
}

void FakeMotorStopTimer_TriggerError(void)
{
    FakeMotorStopTimer_Trigger(MOTOR_STOP_TIMER_ERROR);
}

void FakeMotorStopTimer_TriggerOnNextArmedQuery(MotorStopTimerEvent event)
{
    assert(s_timer.armed);
    assert(!s_timer.trigger_on_next_armed_query);
    s_timer.next_armed_query_event = event;
    s_timer.trigger_on_next_armed_query = true;
}

void FakeMotorStopTimer_TriggerNormalOnNextArmedQuery(void)
{
    FakeMotorStopTimer_TriggerOnNextArmedQuery(MOTOR_STOP_TIMER_NORMAL);
}

void FakeMotorStopTimer_TriggerOnNextArmedQueryReturningPreviousState(
    MotorStopTimerEvent event)
{
    FakeMotorStopTimer_TriggerOnNextArmedQuery(event);
    s_timer.armed_query_returns_previous_state = true;
}

void FakeMotorStopTimer_TriggerOnNextHealthyQuery(MotorStopTimerEvent event)
{
    assert(s_timer.armed);
    assert(!s_timer.trigger_on_next_healthy_query);
    s_timer.next_healthy_query_event = event;
    s_timer.trigger_on_next_healthy_query = true;
}
