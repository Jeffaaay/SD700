#include "Board/Motor/motor_executor.h"

#include <stddef.h>
#include <string.h>

#include "Application/bench_config.h"
#include "Board/Motor/motor_hw_force_disable.h"
#include "Board/Motor/motor_plan_config.h"

#if SD700_PHYSICAL_MOTOR_OUTPUT_LOCKED != 1
#error "Physical motor output unlock is outside this PR"
#endif

static MotorExecutorSnapshot s_motor;

static bool MotorExecutor_DirectionIsValid(MotorDirection direction)
{
    return (direction == MOTOR_DIRECTION_PRESS) ||
           (direction == MOTOR_DIRECTION_RELEASE);
}

static bool MotorExecutor_TimeReached(uint32_t now_ms, uint32_t deadline_ms)
{
    return ((int32_t)(now_ms - deadline_ms) >= 0);
}

MotorResult MotorExecutor_PlanCommand(MotorDirection direction,
                                      uint32_t command_mv,
                                      uint16_t *planned_tim2_ccr3,
                                      uint16_t *planned_tim3_ccr3)
{
    uint32_t duty;

    if ((!MotorExecutor_DirectionIsValid(direction)) ||
        (command_mv == 0U) ||
        (command_mv > MOTOR_PLAN_SUPPLY_REFERENCE_MV) ||
        (planned_tim2_ccr3 == NULL) ||
        (planned_tim3_ccr3 == NULL))
    {
        return MOTOR_RESULT_INVALID;
    }

    duty = (uint32_t)((((uint64_t)command_mv *
                        MOTOR_PLAN_PWM_PERIOD_COUNTS) +
                       (MOTOR_PLAN_SUPPLY_REFERENCE_MV - 1U)) /
                      MOTOR_PLAN_SUPPLY_REFERENCE_MV);
    if (duty > MOTOR_PLAN_PWM_PERIOD_COUNTS)
    {
        duty = MOTOR_PLAN_PWM_PERIOD_COUNTS;
    }

    if (direction == MOTOR_DIRECTION_PRESS)
    {
        *planned_tim2_ccr3 = 0U;
        *planned_tim3_ccr3 = (uint16_t)duty;
    }
    else
    {
        *planned_tim2_ccr3 = (uint16_t)duty;
        *planned_tim3_ccr3 = 0U;
    }
    return MOTOR_RESULT_OK;
}

static MotorResult MotorExecutor_Start(MotorDirection direction,
                                       uint32_t command_mv,
                                       uint32_t duration_ms,
                                       uint32_t backstop_ms,
                                       uint32_t now_ms,
                                       bool pulse)
{
    uint16_t tim2_plan;
    uint16_t tim3_plan;
    MotorResult result;

    MotorHw_ForceDisableImmediate();
    if (s_motor.logical_active)
    {
        return MOTOR_RESULT_BUSY;
    }
    if ((duration_ms == 0U) || (backstop_ms <= duration_ms))
    {
        return MOTOR_RESULT_INVALID;
    }

    result = MotorExecutor_PlanCommand(direction,
                                       command_mv,
                                       &tim2_plan,
                                       &tim3_plan);
    if (result != MOTOR_RESULT_OK)
    {
        return result;
    }

    ++s_motor.request_sequence;
    s_motor.direction = direction;
    s_motor.last_action = (direction == MOTOR_DIRECTION_PRESS) ?
        (pulse ? MOTOR_ACTION_PRESS_PULSE : MOTOR_ACTION_PRESS_RUN) :
        (pulse ? MOTOR_ACTION_RELEASE_PULSE : MOTOR_ACTION_RELEASE_RUN);
    s_motor.last_completion = MOTOR_COMPLETION_NONE;
    s_motor.command_mv = command_mv;
    s_motor.requested_duration_ms = duration_ms;
    s_motor.logical_deadline_ms = now_ms + duration_ms;
    s_motor.logical_backstop_ms = now_ms + backstop_ms;
    s_motor.planned_tim2_ccr3 = tim2_plan;
    s_motor.planned_tim3_ccr3 = tim3_plan;
    s_motor.logical_active = true;
    s_motor.physical_output_locked = true;
    s_motor.physical_output_disabled = true;
    return MOTOR_RESULT_OK;
}

MotorResult MotorExecutor_Initialize(void)
{
    (void)memset(&s_motor, 0, sizeof(s_motor));
    s_motor.last_action = MOTOR_ACTION_DISABLED;
    s_motor.physical_output_locked = true;
    s_motor.physical_output_disabled = true;
    s_motor.last_failure_result = MOTOR_RESULT_OK;
    s_motor.last_failure_stage = MOTOR_FAILURE_STAGE_NONE;
    MotorHw_ForceDisableImmediate();
    return MOTOR_RESULT_OK;
}

MotorResult MotorExecutor_Disable(void)
{
    MotorHw_ForceDisableImmediate();
    s_motor.last_action = MOTOR_ACTION_DISABLED;
    s_motor.command_mv = 0U;
    s_motor.requested_duration_ms = 0U;
    s_motor.logical_deadline_ms = 0U;
    s_motor.logical_backstop_ms = 0U;
    s_motor.planned_tim2_ccr3 = 0U;
    s_motor.planned_tim3_ccr3 = 0U;
    s_motor.logical_active = false;
    s_motor.physical_output_locked = true;
    s_motor.physical_output_disabled = true;
    return MOTOR_RESULT_OK;
}

MotorResult MotorExecutor_StartRun(MotorDirection direction,
                                   uint32_t command_mv,
                                   uint32_t stop_after_ms,
                                   uint32_t hard_stop_after_ms,
                                   uint32_t now_ms)
{
    return MotorExecutor_Start(direction,
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
    return MotorExecutor_Start(direction,
                               command_mv,
                               duration_ms,
                               backstop_ms,
                               now_ms,
                               true);
}

MotorResult MotorExecutor_Service(uint32_t now_ms)
{
    MotorHw_ForceDisableImmediate();
    if (!s_motor.logical_active)
    {
        return MOTOR_RESULT_OK;
    }

    if (MotorExecutor_TimeReached(now_ms, s_motor.logical_backstop_ms))
    {
        s_motor.last_completion = MOTOR_COMPLETION_BACKSTOP;
        s_motor.logical_active = false;
    }
    else if (MotorExecutor_TimeReached(now_ms, s_motor.logical_deadline_ms))
    {
        s_motor.last_completion = MOTOR_COMPLETION_NORMAL;
        s_motor.logical_active = false;
    }

    if (!s_motor.logical_active)
    {
        s_motor.planned_tim2_ccr3 = 0U;
        s_motor.planned_tim3_ccr3 = 0U;
        s_motor.physical_output_disabled = true;
    }
    return MOTOR_RESULT_OK;
}

MotorResult MotorExecutor_GuardOutput(void)
{
    MotorHw_ForceDisableImmediate();
    s_motor.physical_output_locked = true;
    s_motor.physical_output_disabled = true;
    return MOTOR_RESULT_OK;
}

bool MotorExecutor_ActiveRequestIsValid(void)
{
    return s_motor.logical_active &&
           s_motor.physical_output_locked &&
           s_motor.physical_output_disabled;
}

bool MotorExecutor_OutputIsDisabled(void)
{
    return s_motor.physical_output_disabled;
}

bool MotorExecutor_IsHealthy(void)
{
    return s_motor.physical_output_locked &&
           s_motor.physical_output_disabled;
}

const MotorExecutorSnapshot *MotorExecutor_GetStatus(void)
{
    return &s_motor;
}

const MotorExecutorSnapshot *MotorExecutor_GetSnapshot(void)
{
    return &s_motor;
}
