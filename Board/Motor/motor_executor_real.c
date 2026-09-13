#include "Board/Motor/motor_executor.h"

#include "Application/motion_build_policy.h"
#if SD700_FORCE_SERVO_ENABLED
#include "Board/Motor/motor_atomic.h"
#include "Application/force_servo.h"
static volatile bool s_servo_open;
static uint32_t s_servo_generation;
static uint64_t s_servo_sequence;
static bool s_servo_has_sequence;
static int32_t s_servo_committed;
static int s_servo_last_sign;
static uint32_t s_servo_off_ms;
#endif
#include <stddef.h>
#include <string.h>

#include "Board/Motor/motor_hw_real.h"
#include "Board/Motor/motor_plan_config.h"
#include "Board/Motor/motor_stop_timer.h"

static MotorExecutorSnapshot s_motor;
static bool s_initialized;
/* TIM5 is one-shot: the ISR is the single producer and main is the
   single consumer of at most one pending completion event. */
static volatile MotorStopTimerEvent s_pending_completion_event;
static volatile bool s_completion_event_pending;

static void MotorExecutor_ClearFailure(void)
{
    s_motor.last_failure_result = MOTOR_RESULT_OK;
    s_motor.last_failure_stage = MOTOR_FAILURE_STAGE_NONE;
}

static void MotorExecutor_LatchFailure(MotorResult result,
                                       MotorFailureStage specific_stage,
                                       MotorFailureStage fallback_stage)
{
    MotorFailureStage stage;

    if ((result != MOTOR_RESULT_TIMER_ERROR) &&
        (result != MOTOR_RESULT_HARDWARE_ERROR))
    {
        return;
    }
    if (s_motor.last_failure_result != MOTOR_RESULT_OK)
    {
        return;
    }

    stage = (specific_stage != MOTOR_FAILURE_STAGE_NONE) ?
        specific_stage : fallback_stage;
    if (stage == MOTOR_FAILURE_STAGE_NONE)
    {
        stage = MOTOR_FAILURE_STAGE_UNSPECIFIED;
    }
    s_motor.last_failure_result = result;
    s_motor.last_failure_stage = stage;
}

static bool MotorExecutor_DirectionIsValid(MotorDirection direction)
{
    return (direction == MOTOR_DIRECTION_PRESS) ||
           (direction == MOTOR_DIRECTION_RELEASE);
}

static bool MotorExecutor_TimeReached(uint32_t now_ms,
                                      uint32_t deadline_ms)
{
    return ((int32_t)(now_ms - deadline_ms) >= 0);
}

static void MotorExecutor_SetPhysicalStatus(void)
{
    s_motor.physical_output_locked =
        !MotorHwReal_OutputArmingAllowed();
    s_motor.physical_output_disabled = MotorHwReal_IsDisabled();
}

static void MotorExecutor_ClearActivePlan(void)
{
    s_motor.planned_tim2_ccr3 = 0U;
    s_motor.planned_tim3_ccr3 = 0U;
    s_motor.logical_active = false;
}

static void MotorExecutor_ClearPendingCompletion(void)
{
    s_completion_event_pending = false;
    s_pending_completion_event = MOTOR_STOP_TIMER_ERROR;
}

static void MotorExecutor_LatchCompletion(MotorStopTimerEvent event)
{
    s_pending_completion_event = event;
    s_completion_event_pending = true;
}

static bool MotorExecutor_TakeCompletion(MotorStopTimerEvent *event)
{
    if ((event == NULL) || (!s_completion_event_pending))
    {
        return false;
    }

    *event = s_pending_completion_event;
    s_completion_event_pending = false;
    return true;
}

static MotorCompletion MotorExecutor_MapCompletion(
    MotorStopTimerEvent event)
{
    if (event == MOTOR_STOP_TIMER_NORMAL)
    {
        return MOTOR_COMPLETION_NORMAL;
    }
    if (event == MOTOR_STOP_TIMER_BACKSTOP)
    {
        return MOTOR_COMPLETION_BACKSTOP;
    }
    return MOTOR_COMPLETION_ERROR;
}

static void MotorExecutor_StopTimerExpiredFromIsr(
    MotorStopTimerEvent event)
{
    /* ISR ordering: output off, timer cleared, minimal event latched. */
    MotorHwReal_DisableImmediate();
    MotorStopTimer_Cancel();
#if SD700_FORCE_SERVO_ENABLED
    s_servo_open = false;
#endif
    MotorExecutor_LatchCompletion(event);
}

static MotorResult MotorExecutor_PublishCompletion(
    MotorStopTimerEvent event)
{
    MotorResult result = MOTOR_RESULT_OK;

    MotorExecutor_SetPhysicalStatus();
    if (!s_motor.physical_output_disabled)
    {
        MotorHwReal_DisableImmediate();
        MotorExecutor_SetPhysicalStatus();
        event = MOTOR_STOP_TIMER_ERROR;
        result = MOTOR_RESULT_HARDWARE_ERROR;
        MotorExecutor_LatchFailure(
            result,
            MotorHwReal_GetLastFailureStage(),
            MOTOR_FAILURE_STAGE_EXECUTOR_COMPLETION_OUTPUT_NOT_DISABLED);
    }
    else if (event == MOTOR_STOP_TIMER_ERROR)
    {
        result = MOTOR_RESULT_TIMER_ERROR;
        MotorExecutor_LatchFailure(
            result,
            MotorStopTimer_GetLastFailureStage(),
            MOTOR_FAILURE_STAGE_UNSPECIFIED);
    }

#if SD700_FORCE_SERVO_ENABLED
    if (s_motor.last_action == MOTOR_ACTION_CONTINUOUS && event == MOTOR_STOP_TIMER_NORMAL)
        s_motor.last_completion = MOTOR_COMPLETION_LEASE;
    else
#endif
    s_motor.last_completion = MotorExecutor_MapCompletion(event);
    MotorExecutor_ClearActivePlan();
    return result;
}

MotorResult MotorExecutor_PlanCommand(MotorDirection direction,
                                      uint32_t command_mv,
                                      uint16_t *planned_tim2_ccr3,
                                      uint16_t *planned_tim3_ccr3)
{
    uint32_t duty;

#if SD700_FORCE_SERVO_COMMISSIONING
    if ((direction==MOTOR_DIRECTION_PRESS && command_mv>FS_PRESS_PROFILE_CEILING) ||
        (direction==MOTOR_DIRECTION_RELEASE && command_mv>FS_RELEASE_PROFILE_CEILING)) return MOTOR_RESULT_INVALID;
#endif
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

MotorResult MotorExecutor_Initialize(void)
{
    MotorHwReal_DisableImmediate();
    (void)memset(&s_motor, 0, sizeof(s_motor));
    s_initialized = false;
#if SD700_FORCE_SERVO_ENABLED
    s_servo_open = false;
#endif
    MotorExecutor_ClearPendingCompletion();
    s_motor.last_action = MOTOR_ACTION_DISABLED;
    MotorExecutor_ClearFailure();
    MotorExecutor_SetPhysicalStatus();

    if (!MotorHwReal_InitializeDisabled())
    {
        MotorHwReal_DisableImmediate();
        MotorExecutor_SetPhysicalStatus();
        MotorExecutor_LatchFailure(
            MOTOR_RESULT_HARDWARE_ERROR,
            MotorHwReal_GetLastFailureStage(),
            MOTOR_FAILURE_STAGE_UNSPECIFIED);
        return MOTOR_RESULT_HARDWARE_ERROR;
    }
    if (!MotorStopTimer_Initialize(
            MotorExecutor_StopTimerExpiredFromIsr))
    {
        MotorHwReal_DisableImmediate();
        MotorExecutor_SetPhysicalStatus();
        MotorExecutor_LatchFailure(
            MOTOR_RESULT_TIMER_ERROR,
            MotorStopTimer_GetLastFailureStage(),
            MOTOR_FAILURE_STAGE_UNSPECIFIED);
        return MOTOR_RESULT_TIMER_ERROR;
    }

    s_initialized = true;
    MotorExecutor_SetPhysicalStatus();
    if (!s_motor.physical_output_disabled)
    {
        MotorExecutor_LatchFailure(
            MOTOR_RESULT_HARDWARE_ERROR,
            MOTOR_FAILURE_STAGE_NONE,
            MOTOR_FAILURE_STAGE_EXECUTOR_INIT_OUTPUT_NOT_DISABLED);
        return MOTOR_RESULT_HARDWARE_ERROR;
    }
    return MOTOR_RESULT_OK;
}

#if SD700_FORCE_SERVO_ENABLED
static MotorResult MotorExecutor_DisableInternal(void)
#else
MotorResult MotorExecutor_Disable(void)
#endif
{
    bool output_disabled;
    bool timer_armed;

    MotorHwReal_DisableImmediate();
    MotorStopTimer_Cancel();
    MotorExecutor_ClearPendingCompletion();
    s_motor.last_action = MOTOR_ACTION_DISABLED;
    s_motor.command_mv = 0U;
    s_motor.requested_duration_ms = 0U;
    s_motor.logical_deadline_ms = 0U;
    s_motor.logical_backstop_ms = 0U;
    MotorExecutor_ClearActivePlan();
    MotorExecutor_SetPhysicalStatus();
    output_disabled = s_motor.physical_output_disabled;
    timer_armed = MotorStopTimer_IsArmed();
    if (!output_disabled)
    {
        MotorExecutor_LatchFailure(
            MOTOR_RESULT_HARDWARE_ERROR,
            MOTOR_FAILURE_STAGE_NONE,
            MOTOR_FAILURE_STAGE_EXECUTOR_DISABLE_OUTPUT_NOT_DISABLED);
        MotorHwReal_DisableImmediate();
        MotorExecutor_SetPhysicalStatus();
        return MOTOR_RESULT_HARDWARE_ERROR;
    }
    if (timer_armed)
    {
        MotorExecutor_LatchFailure(
            MOTOR_RESULT_TIMER_ERROR,
            MOTOR_FAILURE_STAGE_NONE,
            MOTOR_FAILURE_STAGE_EXECUTOR_DISABLE_TIMER_STILL_ARMED);
        MotorStopTimer_Cancel();
        return MOTOR_RESULT_TIMER_ERROR;
    }
    return MOTOR_RESULT_OK;
}

#if SD700_FORCE_SERVO_ENABLED
MotorResult MotorExecutor_Disable(void)
{
    uint32_t key = MotorAtomic_Enter();
    s_servo_open = false;
    MotorResult r = MotorExecutor_DisableInternal();
    MotorAtomic_Leave(key);
    return r;
}
#endif

static MotorResult MotorExecutor_FailStart(MotorResult result)
{
    MotorHwReal_DisableImmediate();
    MotorStopTimer_Cancel();
    MotorExecutor_ClearPendingCompletion();
    MotorExecutor_ClearActivePlan();
    MotorExecutor_SetPhysicalStatus();
    return result;
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

#if SD700_FORCE_SERVO_ENABLED
    if (s_servo_open) return MOTOR_RESULT_BUSY;
#endif
#if SD700_FORCE_SERVO_COMMISSIONING
    /* No inherited 10000 mV approach or unmonitored pulse/run entry in this build. */
    return MOTOR_RESULT_INVALID;
#endif
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

    MotorExecutor_ClearFailure();

    /* This is also the break-before-make boundary between directions. */
    MotorHwReal_DisableImmediate();
    MotorStopTimer_Cancel();
    MotorExecutor_SetPhysicalStatus();
    if ((!s_initialized) || (!s_motor.physical_output_disabled))
    {
        MotorExecutor_LatchFailure(
            MOTOR_RESULT_HARDWARE_ERROR,
            MOTOR_FAILURE_STAGE_NONE,
            MOTOR_FAILURE_STAGE_EXECUTOR_START_PRECONDITION);
        return MotorExecutor_FailStart(MOTOR_RESULT_HARDWARE_ERROR);
    }

    if (!MotorHwReal_OutputArmingAllowed())
    {
        MotorExecutor_LatchFailure(
            MOTOR_RESULT_HARDWARE_ERROR,
            MOTOR_FAILURE_STAGE_NONE,
            MOTOR_FAILURE_STAGE_EXECUTOR_START_OUTPUT_ARMING);
        return MotorExecutor_FailStart(MOTOR_RESULT_HARDWARE_ERROR);
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
    MotorExecutor_ClearPendingCompletion();

    if (!MotorStopTimer_Arm(duration_ms, backstop_ms))
    {
        MotorExecutor_LatchFailure(
            MOTOR_RESULT_TIMER_ERROR,
            MotorStopTimer_GetLastFailureStage(),
            MOTOR_FAILURE_STAGE_EXECUTOR_START_TIMER_ARM);
        return MotorExecutor_FailStart(MOTOR_RESULT_TIMER_ERROR);
    }

    if (direction == MOTOR_DIRECTION_PRESS)
    {
        result = MotorHwReal_ApplyPress(tim3_plan) ?
                 MOTOR_RESULT_OK : MOTOR_RESULT_HARDWARE_ERROR;
    }
    else
    {
        result = MotorHwReal_ApplyRelease(tim2_plan) ?
                 MOTOR_RESULT_OK : MOTOR_RESULT_HARDWARE_ERROR;
    }
    MotorExecutor_SetPhysicalStatus();
    if ((result != MOTOR_RESULT_OK) ||
        s_motor.physical_output_disabled)
    {
        MotorExecutor_LatchFailure(
            MOTOR_RESULT_HARDWARE_ERROR,
            MotorHwReal_GetLastFailureStage(),
            MOTOR_FAILURE_STAGE_EXECUTOR_START_HW_APPLY);
        return MotorExecutor_FailStart(MOTOR_RESULT_HARDWARE_ERROR);
    }

    if (!MotorStopTimer_CommitArm())
    {
        MotorExecutor_LatchFailure(
            MOTOR_RESULT_TIMER_ERROR,
            MotorStopTimer_GetLastFailureStage(),
            MOTOR_FAILURE_STAGE_EXECUTOR_START_TIMER_COMMIT);
        if (s_completion_event_pending)
        {
            (void)MotorExecutor_Service(now_ms);
            return MOTOR_RESULT_TIMER_ERROR;
        }
        return MotorExecutor_FailStart(MOTOR_RESULT_TIMER_ERROR);
    }
    if (!s_motor.logical_active)
    {
        MotorExecutor_LatchFailure(
            MOTOR_RESULT_TIMER_ERROR,
            MotorStopTimer_GetLastFailureStage(),
            MOTOR_FAILURE_STAGE_EXECUTOR_START_TIMER_COMMIT);
        return MOTOR_RESULT_TIMER_ERROR;
    }
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
    MotorStopTimerEvent event;

    if (MotorExecutor_TakeCompletion(&event))
    {
        return MotorExecutor_PublishCompletion(event);
    }

    MotorExecutor_SetPhysicalStatus();
    if (MotorExecutor_TakeCompletion(&event))
    {
        return MotorExecutor_PublishCompletion(event);
    }
    if (!s_motor.logical_active)
    {
        if (!s_motor.physical_output_disabled)
        {
            MotorHwReal_DisableImmediate();
            MotorStopTimer_Cancel();
            MotorExecutor_SetPhysicalStatus();
            s_motor.last_completion = MOTOR_COMPLETION_ERROR;
            MotorExecutor_LatchFailure(
                MOTOR_RESULT_HARDWARE_ERROR,
                MOTOR_FAILURE_STAGE_NONE,
                MOTOR_FAILURE_STAGE_EXECUTOR_GUARD_UNEXPECTED_OUTPUT);
            return MOTOR_RESULT_HARDWARE_ERROR;
        }
        if (MotorStopTimer_IsArmed())
        {
            MotorHwReal_DisableImmediate();
            MotorStopTimer_Cancel();
            s_motor.last_completion = MOTOR_COMPLETION_ERROR;
            MotorExecutor_LatchFailure(
                MOTOR_RESULT_TIMER_ERROR,
                MOTOR_FAILURE_STAGE_NONE,
                MOTOR_FAILURE_STAGE_EXECUTOR_GUARD_UNEXPECTED_TIMER);
            return MOTOR_RESULT_TIMER_ERROR;
        }
        if (!MotorStopTimer_IsHealthy())
        {
            if (MotorExecutor_TakeCompletion(&event))
            {
                return MotorExecutor_PublishCompletion(event);
            }
            MotorExecutor_LatchFailure(
                MOTOR_RESULT_TIMER_ERROR,
                MotorStopTimer_GetLastFailureStage(),
                MOTOR_FAILURE_STAGE_EXECUTOR_SERVICE_TIMER_UNHEALTHY);
            return MOTOR_RESULT_TIMER_ERROR;
        }
        return MOTOR_RESULT_OK;
    }

    if (!MotorStopTimer_IsHealthy())
    {
        if (MotorExecutor_TakeCompletion(&event))
        {
            return MotorExecutor_PublishCompletion(event);
        }
        MotorExecutor_LatchFailure(
            MOTOR_RESULT_TIMER_ERROR,
            MotorStopTimer_GetLastFailureStage(),
            MOTOR_FAILURE_STAGE_EXECUTOR_SERVICE_TIMER_UNHEALTHY);
        MotorHwReal_DisableImmediate();
        MotorStopTimer_Cancel();
        return MotorExecutor_PublishCompletion(
            MOTOR_STOP_TIMER_ERROR);
    }
    if (!MotorStopTimer_IsArmed())
    {
        if (MotorExecutor_TakeCompletion(&event))
        {
            return MotorExecutor_PublishCompletion(event);
        }
        MotorExecutor_LatchFailure(
            MOTOR_RESULT_TIMER_ERROR,
            MOTOR_FAILURE_STAGE_NONE,
            MOTOR_FAILURE_STAGE_EXECUTOR_SERVICE_TIMER_NOT_ARMED);
        MotorHwReal_DisableImmediate();
        MotorStopTimer_Cancel();
        return MotorExecutor_PublishCompletion(
            MOTOR_STOP_TIMER_ERROR);
    }
    if (MotorExecutor_TimeReached(now_ms,
                                  s_motor.logical_backstop_ms))
    {
        if (MotorExecutor_TakeCompletion(&event))
        {
            return MotorExecutor_PublishCompletion(event);
        }
        MotorExecutor_LatchFailure(
            MOTOR_RESULT_TIMER_ERROR,
            MOTOR_FAILURE_STAGE_NONE,
            MOTOR_FAILURE_STAGE_EXECUTOR_SERVICE_LOGICAL_BACKSTOP);
        MotorHwReal_DisableImmediate();
        MotorStopTimer_Cancel();
        return MotorExecutor_PublishCompletion(
            MOTOR_STOP_TIMER_ERROR);
    }
    if (s_motor.physical_output_disabled
#if SD700_FORCE_SERVO_ENABLED
        && !(s_servo_open && s_motor.last_action == MOTOR_ACTION_CONTINUOUS && s_servo_committed == 0)
#endif
       )
    {
        if (MotorExecutor_TakeCompletion(&event))
        {
            return MotorExecutor_PublishCompletion(event);
        }
        MotorExecutor_LatchFailure(
            MOTOR_RESULT_HARDWARE_ERROR,
            MOTOR_FAILURE_STAGE_NONE,
            MOTOR_FAILURE_STAGE_EXECUTOR_SERVICE_OUTPUT_DROPPED);
        MotorHwReal_DisableImmediate();
        MotorStopTimer_Cancel();
        (void)MotorExecutor_PublishCompletion(
            MOTOR_STOP_TIMER_ERROR);
        return MOTOR_RESULT_HARDWARE_ERROR;
    }
    return MOTOR_RESULT_OK;
}

static MotorResult MotorExecutor_GuardPendingCompletion(void)
{
    /* Reinforce the ISR shutdown, but leave its event entirely to Service. */
    MotorHwReal_DisableImmediate();
    MotorStopTimer_Cancel();
    MotorExecutor_SetPhysicalStatus();
    if (!s_motor.physical_output_disabled)
    {
        MotorExecutor_LatchFailure(
            MOTOR_RESULT_HARDWARE_ERROR,
            MOTOR_FAILURE_STAGE_NONE,
            MOTOR_FAILURE_STAGE_EXECUTOR_GUARD_UNEXPECTED_OUTPUT);
        return MOTOR_RESULT_HARDWARE_ERROR;
    }
    if (MotorStopTimer_IsArmed())
    {
        MotorExecutor_LatchFailure(
            MOTOR_RESULT_TIMER_ERROR,
            MOTOR_FAILURE_STAGE_NONE,
            MOTOR_FAILURE_STAGE_EXECUTOR_GUARD_UNEXPECTED_TIMER);
        return MOTOR_RESULT_TIMER_ERROR;
    }
    return MOTOR_RESULT_OK;
}

MotorResult MotorExecutor_GuardOutput(void)
{
    bool physical_was_active;
    bool timer_was_armed;

    if (s_completion_event_pending)
    {
        return MotorExecutor_GuardPendingCompletion();
    }
    MotorExecutor_SetPhysicalStatus();
    if (s_completion_event_pending)
    {
        return MotorExecutor_GuardPendingCompletion();
    }
    if (s_motor.logical_active)
    {
        if (MotorExecutor_ActiveRequestIsValid())
        {
            if (s_completion_event_pending)
            {
                return MotorExecutor_GuardPendingCompletion();
            }
            return MOTOR_RESULT_OK;
        }
        if (s_completion_event_pending)
        {
            return MotorExecutor_GuardPendingCompletion();
        }

        MotorHwReal_DisableImmediate();
        MotorStopTimer_Cancel();
        MotorExecutor_SetPhysicalStatus();
        if (s_completion_event_pending)
        {
            return MotorExecutor_GuardPendingCompletion();
        }
        /* No ISR event: report the real invalid request without fabricating
           an event that could overwrite a racing ISR's classification. */
        MotorExecutor_LatchFailure(
            MOTOR_RESULT_HARDWARE_ERROR,
            MOTOR_FAILURE_STAGE_NONE,
            MOTOR_FAILURE_STAGE_EXECUTOR_GUARD_ACTIVE_REQUEST_INVALID);
        return MOTOR_RESULT_HARDWARE_ERROR;
    }

    physical_was_active = !s_motor.physical_output_disabled;
    timer_was_armed = MotorStopTimer_IsArmed();
    MotorHwReal_DisableImmediate();
    if (!s_motor.logical_active)
    {
        MotorStopTimer_Cancel();
    }
    MotorExecutor_SetPhysicalStatus();
    if (s_completion_event_pending)
    {
        return MotorExecutor_GuardPendingCompletion();
    }
    if (!s_motor.physical_output_disabled)
    {
        MotorExecutor_LatchFailure(
            MOTOR_RESULT_HARDWARE_ERROR,
            MOTOR_FAILURE_STAGE_NONE,
            MOTOR_FAILURE_STAGE_EXECUTOR_GUARD_UNEXPECTED_OUTPUT);
        return MOTOR_RESULT_HARDWARE_ERROR;
    }
    if (physical_was_active)
    {
        MotorExecutor_LatchFailure(
            MOTOR_RESULT_HARDWARE_ERROR,
            MOTOR_FAILURE_STAGE_NONE,
            MOTOR_FAILURE_STAGE_EXECUTOR_GUARD_UNEXPECTED_OUTPUT);
        return MOTOR_RESULT_HARDWARE_ERROR;
    }
    if (timer_was_armed)
    {
        MotorExecutor_LatchFailure(
            MOTOR_RESULT_TIMER_ERROR,
            MOTOR_FAILURE_STAGE_NONE,
            MOTOR_FAILURE_STAGE_EXECUTOR_GUARD_UNEXPECTED_TIMER);
        return MOTOR_RESULT_TIMER_ERROR;
    }
    return MOTOR_RESULT_OK;
}

bool MotorExecutor_ActiveRequestIsValid(void)
{
#if SD700_FORCE_SERVO_ENABLED
    if (s_motor.last_action == MOTOR_ACTION_CONTINUOUS && !s_completion_event_pending) {
        bool valid = s_servo_open && s_motor.logical_active && MotorStopTimer_IsHealthy() &&
            MotorStopTimer_IsArmed() && MotorHwReal_OutputArmingAllowed() &&
            (MotorHwReal_IsDisabled() == (s_servo_committed == 0)) &&
            MotorHwReal_MatchesPlan(s_motor.planned_tim2_ccr3,s_motor.planned_tim3_ccr3);
        if (!s_completion_event_pending) return valid && s_servo_open;
    }
#endif

    MotorExecutor_SetPhysicalStatus();
    if (!s_motor.logical_active)
    {
        return false;
    }
    if ((!s_completion_event_pending) &&
        (!s_motor.physical_output_locked) &&
        (!s_motor.physical_output_disabled) &&
        MotorStopTimer_IsArmed() &&
        MotorStopTimer_IsHealthy() &&
        (!s_completion_event_pending))
    {
        return true;
    }

    /* TIM5 may complete during the validation queries above. Recheck the
       handoff and physical state, including when the event was already
       pending. Only Service consumes/publishes the event classification. */
    if (s_completion_event_pending)
    {
        MotorExecutor_SetPhysicalStatus();
        return s_motor.physical_output_disabled &&
               (!MotorStopTimer_IsArmed());
    }
    return false;
}

bool MotorExecutor_OutputIsDisabled(void)
{
    MotorExecutor_SetPhysicalStatus();
    return s_motor.physical_output_disabled;
}

bool MotorExecutor_IsHealthy(void)
{
    MotorExecutor_SetPhysicalStatus();
    if ((!s_initialized) || (!MotorStopTimer_IsHealthy()))
    {
        return false;
    }
    if (s_motor.logical_active)
    {
        return MotorExecutor_ActiveRequestIsValid();
    }
    return s_motor.physical_output_disabled &&
           (!MotorStopTimer_IsArmed());
}

const MotorExecutorSnapshot *MotorExecutor_GetStatus(void)
{
    MotorExecutor_SetPhysicalStatus();
    return &s_motor;
}

const MotorExecutorSnapshot *MotorExecutor_GetSnapshot(void)
{
    MotorExecutor_SetPhysicalStatus();
    return &s_motor;
}

#if SD700_FORCE_SERVO_ENABLED
MotorResult MotorExecutor_BeginContinuous(uint32_t *token)
{
    uint32_t key=MotorAtomic_Enter();
    MotorResult r=MOTOR_RESULT_INVALID;
    if (token && s_initialized && !s_servo_open && !s_motor.logical_active &&
        !s_completion_event_pending && MotorHwReal_IsDisabled() &&
        !MotorStopTimer_IsArmed() && MotorStopTimer_IsHealthy() && MotorHwReal_OutputArmingAllowed()) {
        if (++s_servo_generation==0) ++s_servo_generation;
        *token=s_servo_generation; s_servo_open=true; s_servo_has_sequence=false;
        s_servo_committed=0; s_servo_last_sign=0;
        s_motor.last_action=MOTOR_ACTION_CONTINUOUS; s_motor.last_completion=MOTOR_COMPLETION_NONE;
        r=MOTOR_RESULT_OK;
    }
    MotorAtomic_Leave(key); return r;
}

MotorResult MotorExecutor_UpdateContinuous(uint32_t token, uint64_t sequence,
    uint32_t received_ms, uint32_t now_ms, uint32_t lease_ms, uint32_t max_age_ms,
    uint32_t deadtime_ms, int32_t requested_mv, int32_t *committed_mv, bool *interlocked)
{
    uint32_t key=MotorAtomic_Enter();
    MotorResult result=MOTOR_RESULT_INVALID;
    uint16_t t2=0,t3=0;
    now_ms=MotorAtomic_Now(now_ms);
    uint32_t age=now_ms-received_ms;
    uint64_t distance=sequence-s_servo_sequence;
    /* Rejected old tokens/duplicates never mutate or renew the current session. */
    if (!s_servo_open || token!=s_servo_generation || !committed_mv || !interlocked)
        goto done;
    if (s_servo_has_sequence && (distance==0 || distance>=(UINT64_C(1)<<63))) goto done;
#if SD700_FORCE_SERVO_COMMISSIONING
    if (requested_mv>(int32_t)FS_PRESS_PROFILE_CEILING ||
        requested_mv<-(int32_t)FS_RELEASE_PROFILE_CEILING ||
        lease_ms>FORCE_SERVO_COMMISSIONING_LEASE_MS || max_age_ms>FORCE_SERVO_COMMISSIONING_AGE_MS ||
        deadtime_ms<FORCE_SERVO_COMMISSIONING_DEADTIME_MS) goto fail;
#endif
    if (s_completion_event_pending || lease_ms<3 ||
#if !SD700_FORCE_SERVO_COMMISSIONING
        lease_ms>100 ||
#endif
        max_age_ms>=lease_ms ||
        age>max_age_ms || age>=lease_ms-1 || deadtime_ms<1 || deadtime_ms>100 ||
#if !SD700_FORCE_SERVO_COMMISSIONING
        requested_mv>5000 || requested_mv<-800 ||
#endif
        !MotorHwReal_OutputArmingAllowed()) goto fail;
    if (s_servo_has_sequence &&
        (!MotorStopTimer_IsArmed() || !MotorExecutor_ActiveRequestIsValid() ||
         (int32_t)(now_ms-s_motor.logical_deadline_ms)>=0)) goto fail;
    *interlocked=false;
    int sign=requested_mv>0 ? 1 : requested_mv<0 ? -1 : 0;
    int32_t actual=requested_mv;
    if (s_servo_committed!=0 && sign!=0 && sign!=s_servo_last_sign) {
        MotorHwReal_DisableImmediate();
        if (!MotorHwReal_IsDisabled()) goto fail;
        s_servo_committed=0; s_servo_off_ms=now_ms;
        actual=0; *interlocked=true;
    } else if (sign!=0 && s_servo_last_sign!=0 && sign!=s_servo_last_sign &&
               (uint32_t)(now_ms-s_servo_off_ms)<deadtime_ms) {
        actual=0; *interlocked=true;
    }
    /* Arm before enabling; renew while live without clearing any pending expiry.
     * Failure at any later stage revokes the lease and forces OFF. */
    if (!(s_servo_has_sequence ? MotorStopTimer_RenewLease(lease_ms-age) :
                                MotorStopTimer_ArmLease(lease_ms-age))) goto fail;
    if (!s_servo_open || s_completion_event_pending) goto fail;
    if (actual==0) {
        if (s_servo_committed!=0) s_servo_off_ms=now_ms;
        MotorHwReal_DisableImmediate();
        if (!MotorHwReal_IsDisabled()) goto fail;
    } else {
        MotorDirection dir=actual>0 ? MOTOR_DIRECTION_PRESS : MOTOR_DIRECTION_RELEASE;
        uint32_t magnitude=(uint32_t)(actual>0 ? actual : -actual);
        if (MotorExecutor_PlanCommand(dir,magnitude,&t2,&t3)!=MOTOR_RESULT_OK) goto fail;
        bool ok = s_servo_committed!=0 ? MotorHwReal_Update(actual>0,actual>0?t3:t2) :
            (actual>0 ? MotorHwReal_ApplyPress(t3) : MotorHwReal_ApplyRelease(t2));
        if (!ok || MotorHwReal_IsDisabled()) goto fail;
        s_servo_last_sign=actual>0 ? 1 : -1;
    }
    if (!MotorStopTimer_CommitArm() || !s_servo_open || s_completion_event_pending) goto fail;
    s_servo_sequence=sequence; s_servo_has_sequence=true; s_servo_committed=actual;
    s_motor.last_action=MOTOR_ACTION_CONTINUOUS; s_motor.logical_active=true;
    s_motor.last_completion=MOTOR_COMPLETION_NONE;
    s_motor.direction=actual<0 ? MOTOR_DIRECTION_RELEASE : MOTOR_DIRECTION_PRESS;
    s_motor.command_mv=(uint32_t)(actual<0 ? -actual : actual);
    s_motor.requested_duration_ms=0;
    s_motor.planned_tim2_ccr3=t2; s_motor.planned_tim3_ccr3=t3;
    s_motor.logical_deadline_ms=received_ms+lease_ms;
    s_motor.logical_backstop_ms=s_motor.logical_deadline_ms;
    ++s_motor.request_sequence;
    *committed_mv=actual; result=MOTOR_RESULT_OK; goto done;
fail:
    s_servo_open=false;
    MotorHwReal_DisableImmediate(); MotorStopTimer_Cancel();
    /* Preserve the ISR's event classification for Service. */
    if (!s_completion_event_pending) MotorExecutor_LatchCompletion(MOTOR_STOP_TIMER_ERROR);
    result=MOTOR_RESULT_HARDWARE_ERROR;
done:
    MotorExecutor_SetPhysicalStatus();
    MotorAtomic_Leave(key); return result;
}
#endif

#if SD700_FORCE_SERVO_ENABLED
bool MotorExecutor_ContinuousExpired(void)
{
    return s_motor.last_action==MOTOR_ACTION_CONTINUOUS &&
           (!s_servo_open || s_completion_event_pending);
}
#endif
