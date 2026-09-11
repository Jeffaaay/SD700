#include "Tests/Host/fake_motor_hw_real.h"

#include <string.h>
#include <assert.h>

#include "Board/Motor/motor_hw_real.h"
#include "Board/Motor/motor_plan_config.h"
#include "Board/Motor/motor_real_config.h"
#include "Tests/Host/fake_motor_stop_timer.h"

static FakeMotorHwRealState s_hw;
static void (*s_disabled_query_hook)(void);
static uint32_t s_disabled_query_countdown;
static void (*s_disable_hook)(void);

void FakeMotorHwReal_Reset(void)
{
    (void)memset(&s_hw, 0, sizeof(s_hw));
    s_disabled_query_hook = NULL;
    s_disabled_query_countdown = 0U;
    s_disable_hook = NULL;
}

void FakeMotorHwReal_HoldOutputEnabled(bool hold)
{
    s_hw.hold_output_enabled = hold;
}

void FakeMotorHwReal_OnDisabledQuery(uint32_t query_count, void (*hook)(void))
{
    assert(query_count > 0U);
    assert(s_disabled_query_hook == NULL);
    s_disabled_query_countdown = query_count;
    s_disabled_query_hook = hook;
}

void FakeMotorHwReal_OnNextDisable(void (*hook)(void))
{
    assert(s_disable_hook == NULL);
    s_disable_hook = hook;
}

void FakeMotorHwReal_FailNextApply(void)
{
    s_hw.fail_next_apply = true;
}

void FakeMotorHwReal_FailNextDisableVerification(void)
{
    s_hw.fail_next_disable = true;
}

void FakeMotorHwReal_ForceOutputDropped(void)
{
    s_hw.tim2_ccr3 = 0U;
    s_hw.tim3_ccr3 = 0U;
    s_hw.tim2_enabled = false;
    s_hw.tim3_enabled = false;
    s_hw.shutdown_enabled = false;
    s_hw.driver_enabled = false;
}

void FakeMotorHwReal_ForceUnexpectedPressOutput(uint16_t duty_counts)
{
    s_hw.shutdown_enabled = true;
    s_hw.driver_enabled = true;
    s_hw.tim3_ccr3 = duty_counts;
    s_hw.tim2_enabled = true;
    s_hw.tim3_enabled = true;
}

const FakeMotorHwRealState *FakeMotorHwReal_GetState(void)
{
    return &s_hw;
}

void MotorHwReal_DisableImmediate(void)
{
    if (s_disable_hook != NULL)
    {
        void (*hook)(void) = s_disable_hook;
        s_disable_hook = NULL;
        hook();
    }
    ++s_hw.disable_count;
    if (s_hw.fail_next_disable || s_hw.hold_output_enabled)
    {
        s_hw.fail_next_disable = false;
        s_hw.shutdown_enabled = true;
        s_hw.driver_enabled = true;
        s_hw.tim3_ccr3 = 1U;
        s_hw.tim3_enabled = true;
        return;
    }
    s_hw.tim2_ccr3 = 0U;
    s_hw.tim3_ccr3 = 0U;
    s_hw.tim2_enabled = false;
    s_hw.tim3_enabled = false;
    s_hw.shutdown_enabled = false;
    s_hw.driver_enabled = false;
}

bool MotorHwReal_IsDisabled(void)
{
    const bool disabled = (!s_hw.shutdown_enabled) && (!s_hw.driver_enabled) &&
           (!s_hw.tim2_enabled) && (!s_hw.tim3_enabled) &&
           (s_hw.tim2_ccr3 == 0U) && (s_hw.tim3_ccr3 == 0U);

    if ((s_disabled_query_hook != NULL) &&
        (--s_disabled_query_countdown == 0U))
    {
        void (*hook)(void) = s_disabled_query_hook;
        s_disabled_query_hook = NULL;
        hook();
    }
    /* Model an ISR after the hardware was sampled but before return. */
    return disabled;
}

bool MotorHwReal_OutputArmingAllowed(void)
{
    return SD700_REAL_OUTPUT_ARMING_ENABLED != 0;
}

bool MotorHwReal_InitializeDisabled(void)
{
    s_hw.last_failure_stage = MOTOR_FAILURE_STAGE_NONE;
    MotorHwReal_DisableImmediate();
    s_hw.initialized = true;
    return true;
}

static bool FakeMotorHwReal_Apply(bool press, uint16_t duty_counts)
{
    bool started_disabled = MotorHwReal_IsDisabled();

    s_hw.last_failure_stage = MOTOR_FAILURE_STAGE_NONE;
    MotorHwReal_DisableImmediate();
    ++s_hw.apply_count;
    s_hw.last_apply_started_disabled = started_disabled;
    s_hw.last_apply_saw_timer_armed =
        FakeMotorStopTimer_GetState()->armed;
    if (s_hw.fail_next_apply)
    {
        s_hw.fail_next_apply = false;
        s_hw.last_failure_stage =
            MOTOR_FAILURE_STAGE_HW_APPLY_PWM_START;
        return false;
    }
    if ((!s_hw.initialized) || (duty_counts == 0U) ||
        (duty_counts > MOTOR_PLAN_PWM_PERIOD_COUNTS))
    {
        s_hw.last_failure_stage = (!s_hw.initialized) ?
            MOTOR_FAILURE_STAGE_HW_APPLY_NOT_INITIALIZED :
            MOTOR_FAILURE_STAGE_HW_APPLY_INVALID_DUTY;
        return false;
    }

    s_hw.shutdown_enabled = true;
    s_hw.driver_enabled = true;
    s_hw.tim2_enabled = true;
    s_hw.tim3_enabled = true;
    s_hw.tim2_ccr3 = 0U;
    s_hw.tim3_ccr3 = 0U;
    if (press)
    {
        s_hw.tim3_ccr3 = duty_counts;
    }
    else
    {
        s_hw.tim2_ccr3 = duty_counts;
    }
    if ((s_hw.tim2_ccr3 > 0U) && (s_hw.tim3_ccr3 > 0U))
    {
        s_hw.both_legs_active_violation = true;
    }
    return true;
}

bool MotorHwReal_ApplyPress(uint16_t duty_counts)
{
    return FakeMotorHwReal_Apply(true, duty_counts);
}

bool MotorHwReal_ApplyRelease(uint16_t duty_counts)
{
    return FakeMotorHwReal_Apply(false, duty_counts);
}

MotorFailureStage MotorHwReal_GetLastFailureStage(void)
{
    return s_hw.last_failure_stage;
}
