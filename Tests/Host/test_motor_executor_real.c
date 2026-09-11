#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Board/Motor/motor_executor.h"
#include "Board/Motor/motor_hw_real.h"
#include "Tests/Host/fake_motor_hw_real.h"
#include "Tests/Host/fake_motor_stop_timer.h"

static void StartFixture(void)
{
    FakeMotorHwReal_Reset();
    FakeMotorStopTimer_Reset();
    assert(MotorExecutor_Initialize() == MOTOR_RESULT_OK);
    assert(MotorExecutor_IsHealthy());
    assert(MotorHwReal_IsDisabled());
}

static void AssertBothLegsSafe(void)
{
    const FakeMotorHwRealState *hw = FakeMotorHwReal_GetState();

    assert(!(hw->tim2_ccr3 > 0U && hw->tim3_ccr3 > 0U));
    assert(!hw->both_legs_active_violation);
}

static void TestDirectionMappingAndBreakBeforeMake(void)
{
    const FakeMotorHwRealState *hw;
    const MotorExecutorSnapshot *snapshot;
    uint32_t disable_count;

    StartFixture();
    assert(MotorExecutor_StartRun(MOTOR_DIRECTION_PRESS,
                                  6000U,
                                  50U,
                                  60U,
                                  10U) == MOTOR_RESULT_OK);
    hw = FakeMotorHwReal_GetState();
    assert(hw->shutdown_enabled);
    assert(hw->driver_enabled);
    assert(hw->tim2_enabled);
    assert(hw->tim2_ccr3 == 0U);
    assert(hw->tim3_enabled);
    assert(hw->tim3_ccr3 > 0U);
    assert(hw->last_apply_saw_timer_armed);
    assert(hw->last_apply_started_disabled);
    assert(MotorExecutor_ActiveRequestIsValid());
    AssertBothLegsSafe();

    disable_count = hw->disable_count;
    assert(MotorExecutor_GuardOutput() == MOTOR_RESULT_OK);
    assert(FakeMotorHwReal_GetState()->disable_count == disable_count);
    assert(!MotorHwReal_IsDisabled());

    FakeMotorStopTimer_TriggerNormal();
    snapshot = MotorExecutor_GetSnapshot();
    assert(MotorHwReal_IsDisabled());
    assert(!MotorStopTimer_IsArmed());
    assert(snapshot->logical_active);
    assert(snapshot->last_completion == MOTOR_COMPLETION_NONE);
    assert(MotorExecutor_Service(60U) == MOTOR_RESULT_OK);
    assert(!MotorExecutor_GetSnapshot()->logical_active);
    assert(MotorExecutor_GetSnapshot()->last_completion ==
           MOTOR_COMPLETION_NORMAL);
    assert(MotorExecutor_StartRun(MOTOR_DIRECTION_RELEASE,
                                  6000U,
                                  50U,
                                  60U,
                                  70U) == MOTOR_RESULT_OK);
    hw = FakeMotorHwReal_GetState();
    assert(hw->shutdown_enabled);
    assert(hw->driver_enabled);
    assert(hw->tim2_enabled);
    assert(hw->tim2_ccr3 > 0U);
    assert(hw->tim3_enabled);
    assert(hw->tim3_ccr3 == 0U);
    assert(hw->last_apply_saw_timer_armed);
    assert(hw->last_apply_started_disabled);
    AssertBothLegsSafe();
}

static void TestInvalidAndTimerArmFailureStayDisabled(void)
{
    uint32_t apply_count;

    StartFixture();
    apply_count = FakeMotorHwReal_GetState()->apply_count;
    assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_PRESS,
                                    0U,
                                    10U,
                                    20U,
                                    0U) == MOTOR_RESULT_INVALID);
    assert(FakeMotorHwReal_GetState()->apply_count == apply_count);
    assert(MotorHwReal_IsDisabled());

    assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_PRESS,
                                    1000U,
                                    20U,
                                    20U,
                                    0U) == MOTOR_RESULT_INVALID);
    assert(FakeMotorHwReal_GetState()->apply_count == apply_count);
    assert(MotorHwReal_IsDisabled());

    FakeMotorStopTimer_FailNextArm();
    assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_PRESS,
                                    2000U,
                                    10U,
                                    20U,
                                    0U) == MOTOR_RESULT_TIMER_ERROR);
    assert(FakeMotorHwReal_GetState()->apply_count == apply_count);
    assert(MotorHwReal_IsDisabled());
    assert(!MotorStopTimer_IsArmed());
}

static void TestExpiryOrdering(void)
{
    const FakeMotorStopTimerState *timer;
    const MotorExecutorSnapshot *snapshot;

    StartFixture();
    assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_PRESS,
                                    2000U,
                                    10U,
                                    20U,
                                    100U) == MOTOR_RESULT_OK);
    FakeMotorStopTimer_TriggerNormal();
    timer = FakeMotorStopTimer_GetState();
    snapshot = MotorExecutor_GetSnapshot();
    assert(timer->expiry_cancel_saw_output_disabled);
    assert(!timer->armed);
    assert(snapshot->last_completion == MOTOR_COMPLETION_NONE);
    assert(snapshot->logical_active);
    assert(snapshot->physical_output_disabled);
    assert(MotorExecutor_Service(110U) == MOTOR_RESULT_OK);
    snapshot = MotorExecutor_GetSnapshot();
    assert(snapshot->last_completion == MOTOR_COMPLETION_NORMAL);
    assert(!snapshot->logical_active);

    assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_RELEASE,
                                    2000U,
                                    10U,
                                    20U,
                                    200U) == MOTOR_RESULT_OK);
    FakeMotorStopTimer_TriggerBackstop();
    timer = FakeMotorStopTimer_GetState();
    snapshot = MotorExecutor_GetSnapshot();
    assert(timer->expiry_cancel_saw_output_disabled);
    assert(!timer->armed);
    assert(snapshot->last_completion == MOTOR_COMPLETION_NONE);
    assert(snapshot->logical_active);
    assert(snapshot->physical_output_disabled);
    assert(MotorExecutor_Service(220U) == MOTOR_RESULT_OK);
    snapshot = MotorExecutor_GetSnapshot();
    assert(snapshot->last_completion == MOTOR_COMPLETION_BACKSTOP);
    assert(!snapshot->logical_active);
}

static void TestDisableBusyAndApplyFailure(void)
{
    uint32_t apply_count;

    StartFixture();
    assert(MotorExecutor_StartRun(MOTOR_DIRECTION_PRESS,
                                  6000U,
                                  50U,
                                  60U,
                                  0U) == MOTOR_RESULT_OK);
    apply_count = FakeMotorHwReal_GetState()->apply_count;
    assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_RELEASE,
                                    2000U,
                                    10U,
                                    20U,
                                    1U) == MOTOR_RESULT_BUSY);
    assert(FakeMotorHwReal_GetState()->apply_count == apply_count);
    assert(!MotorHwReal_IsDisabled());
    AssertBothLegsSafe();

    assert(MotorExecutor_Disable() == MOTOR_RESULT_OK);
    assert(MotorExecutor_Disable() == MOTOR_RESULT_OK);
    assert(MotorHwReal_IsDisabled());
    assert(FakeMotorHwReal_GetState()->tim2_ccr3 == 0U);
    assert(FakeMotorHwReal_GetState()->tim3_ccr3 == 0U);
    assert(!FakeMotorHwReal_GetState()->tim2_enabled);
    assert(!FakeMotorHwReal_GetState()->tim3_enabled);

    FakeMotorHwReal_FailNextApply();
    assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_PRESS,
                                    2000U,
                                    10U,
                                    20U,
                                    100U) == MOTOR_RESULT_HARDWARE_ERROR);
    assert(MotorHwReal_IsDisabled());
    assert(!MotorStopTimer_IsArmed());
}

static void TestTimerCommitErrorDisablesOutput(void)
{
    const MotorExecutorSnapshot *snapshot;

    StartFixture();
    FakeMotorStopTimer_FailNextCommit();
    assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_PRESS,
                                    2000U,
                                    10U,
                                    20U,
                                    100U) == MOTOR_RESULT_TIMER_ERROR);
    snapshot = MotorExecutor_GetSnapshot();
    assert(MotorHwReal_IsDisabled());
    assert(!MotorStopTimer_IsArmed());
    assert(!snapshot->logical_active);
    assert(snapshot->physical_output_disabled);
    assert(snapshot->last_completion == MOTOR_COMPLETION_ERROR);
    assert(!MotorExecutor_IsHealthy());
    assert(snapshot->last_failure_result == MOTOR_RESULT_TIMER_ERROR);
    assert(snapshot->last_failure_stage ==
           MOTOR_FAILURE_STAGE_TIMER_COMMIT_PENDING_EVENT);
    assert(FakeMotorStopTimer_GetState()
               ->expiry_cancel_saw_output_disabled);
}

static void TestInactiveGuardReportsUnexpectedOutput(void)
{
    const MotorExecutorSnapshot *snapshot;

    StartFixture();
    snapshot = MotorExecutor_GetSnapshot();
    assert(!snapshot->logical_active);
    FakeMotorHwReal_ForceUnexpectedPressOutput(100U);
    assert(!MotorHwReal_IsDisabled());

    assert(MotorExecutor_GuardOutput() ==
           MOTOR_RESULT_HARDWARE_ERROR);
    assert(MotorHwReal_IsDisabled());
    assert(!MotorStopTimer_IsArmed());
    assert(!MotorExecutor_GetSnapshot()->logical_active);
    assert(MotorExecutor_GetSnapshot()->last_failure_result ==
           MOTOR_RESULT_HARDWARE_ERROR);
    assert(MotorExecutor_GetSnapshot()->last_failure_stage ==
           MOTOR_FAILURE_STAGE_EXECUTOR_GUARD_UNEXPECTED_OUTPUT);
}

static void TestInactiveGuardReportsUnexpectedTimer(void)
{
    const MotorExecutorSnapshot *snapshot;

    StartFixture();
    snapshot = MotorExecutor_GetSnapshot();
    assert(!snapshot->logical_active);
    assert(MotorStopTimer_Arm(10U, 20U));
    assert(MotorStopTimer_IsArmed());
    assert(MotorHwReal_IsDisabled());

    assert(MotorExecutor_GuardOutput() == MOTOR_RESULT_TIMER_ERROR);
    assert(!MotorStopTimer_IsArmed());
    assert(MotorHwReal_IsDisabled());
    assert(!MotorExecutor_GetSnapshot()->logical_active);
    assert(MotorExecutor_GetSnapshot()->last_failure_result ==
           MOTOR_RESULT_TIMER_ERROR);
    assert(MotorExecutor_GetSnapshot()->last_failure_stage ==
           MOTOR_FAILURE_STAGE_EXECUTOR_GUARD_UNEXPECTED_TIMER);
}

static void TestFailureDiagnosticsAndPreservation(void)
{
    const MotorExecutorSnapshot *snapshot;
    MotorFailureStage preserved_stage;

    StartFixture();
    FakeMotorStopTimer_FailNextArm();
    assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_PRESS,
                                    2000U,
                                    10U,
                                    20U,
                                    0U) == MOTOR_RESULT_TIMER_ERROR);
    snapshot = MotorExecutor_GetSnapshot();
    assert(snapshot->last_failure_result == MOTOR_RESULT_TIMER_ERROR);
    assert(snapshot->last_failure_stage ==
           MOTOR_FAILURE_STAGE_TIMER_ARM_START_VERIFY);
    preserved_stage = snapshot->last_failure_stage;
    assert(MotorExecutor_Disable() == MOTOR_RESULT_OK);
    assert(MotorExecutor_GetSnapshot()->last_failure_stage ==
           preserved_stage);

    assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_PRESS,
                                    2000U,
                                    10U,
                                    20U,
                                    30U) == MOTOR_RESULT_OK);
    snapshot = MotorExecutor_GetSnapshot();
    assert(snapshot->last_failure_result == MOTOR_RESULT_OK);
    assert(snapshot->last_failure_stage == MOTOR_FAILURE_STAGE_NONE);
    assert(MotorExecutor_Disable() == MOTOR_RESULT_OK);

    FakeMotorHwReal_FailNextApply();
    assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_RELEASE,
                                    2000U,
                                    10U,
                                    20U,
                                    50U) == MOTOR_RESULT_HARDWARE_ERROR);
    snapshot = MotorExecutor_GetSnapshot();
    assert(snapshot->last_failure_result ==
           MOTOR_RESULT_HARDWARE_ERROR);
    assert(snapshot->last_failure_stage ==
           MOTOR_FAILURE_STAGE_HW_APPLY_PWM_START);
    assert(MotorExecutor_Disable() == MOTOR_RESULT_OK);
    assert(MotorExecutor_GetSnapshot()->last_failure_stage ==
           MOTOR_FAILURE_STAGE_HW_APPLY_PWM_START);
}

static void TestServiceAndDisableDiagnostics(void)
{
    const MotorExecutorSnapshot *snapshot;
    MotorFailureStage busy_stage;

    StartFixture();
    assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_PRESS,
                                    2000U,
                                    10U,
                                    20U,
                                    0U) == MOTOR_RESULT_OK);
    FakeMotorHwReal_ForceOutputDropped();
    assert(MotorExecutor_Service(1U) == MOTOR_RESULT_HARDWARE_ERROR);
    snapshot = MotorExecutor_GetSnapshot();
    assert(snapshot->last_failure_stage ==
           MOTOR_FAILURE_STAGE_EXECUTOR_SERVICE_OUTPUT_DROPPED);
    assert(MotorExecutor_OutputIsDisabled());

    StartFixture();
    assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_PRESS,
                                    2000U,
                                    10U,
                                    20U,
                                    0U) == MOTOR_RESULT_OK);
    FakeMotorStopTimer_ForceUnhealthy();
    assert(MotorExecutor_GuardOutput() ==
           MOTOR_RESULT_HARDWARE_ERROR);
    busy_stage = MotorExecutor_GetSnapshot()->last_failure_stage;
    assert(busy_stage ==
           MOTOR_FAILURE_STAGE_EXECUTOR_GUARD_ACTIVE_REQUEST_INVALID);
    assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_RELEASE,
                                    2000U,
                                    10U,
                                    20U,
                                    1U) == MOTOR_RESULT_BUSY);
    assert(MotorExecutor_GetSnapshot()->last_failure_stage ==
           busy_stage);

    StartFixture();
    assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_PRESS,
                                    2000U,
                                    10U,
                                    20U,
                                    0U) == MOTOR_RESULT_OK);
    FakeMotorHwReal_FailNextDisableVerification();
    assert(MotorExecutor_Disable() == MOTOR_RESULT_HARDWARE_ERROR);
    assert(MotorExecutor_GetSnapshot()->last_failure_stage ==
           MOTOR_FAILURE_STAGE_EXECUTOR_DISABLE_OUTPUT_NOT_DISABLED);

    StartFixture();
    assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_RELEASE,
                                    2000U,
                                    10U,
                                    20U,
                                    0U) == MOTOR_RESULT_OK);
    FakeMotorStopTimer_FailNextCancelVerification();
    assert(MotorExecutor_Disable() == MOTOR_RESULT_TIMER_ERROR);
    assert(MotorExecutor_GetSnapshot()->last_failure_stage ==
           MOTOR_FAILURE_STAGE_EXECUTOR_DISABLE_TIMER_STILL_ARMED);
}

static void StartValidationPulse(MotorDirection direction)
{
    StartFixture();
    assert(!MotorExecutor_ActiveRequestIsValid());
    assert(MotorExecutor_StartPulse(direction, 10000U, 100U, 150U,
                                    0U) == MOTOR_RESULT_OK);
    assert(MotorExecutor_ActiveRequestIsValid());
}

static void TestInvalidWithoutCompletion(MotorDirection direction)
{
    StartValidationPulse(direction);
    FakeMotorHwReal_ForceOutputDropped();
    assert(!MotorExecutor_ActiveRequestIsValid());
    assert(FakeMotorStopTimer_GetState()->expiry_count == 0U);
    assert(MotorExecutor_Service(1U) == MOTOR_RESULT_HARDWARE_ERROR);
    assert(MotorExecutor_GetSnapshot()->last_failure_stage ==
           MOTOR_FAILURE_STAGE_EXECUTOR_SERVICE_OUTPUT_DROPPED);

    StartValidationPulse(direction);
    MotorStopTimer_Cancel();
    assert(!MotorExecutor_ActiveRequestIsValid());
    assert(!MotorHwReal_IsDisabled());
    assert(FakeMotorStopTimer_GetState()->expiry_count == 0U);
    assert(MotorExecutor_Service(1U) == MOTOR_RESULT_TIMER_ERROR);
    assert(MotorExecutor_GetSnapshot()->last_failure_stage ==
           MOTOR_FAILURE_STAGE_EXECUTOR_SERVICE_TIMER_NOT_ARMED);

    StartValidationPulse(direction);
    FakeMotorStopTimer_ForceUnhealthy();
    assert(!MotorExecutor_ActiveRequestIsValid());
    assert(FakeMotorStopTimer_GetState()->expiry_count == 0U);
    assert(MotorExecutor_Service(1U) == MOTOR_RESULT_TIMER_ERROR);
    assert(MotorExecutor_GetSnapshot()->last_failure_stage ==
           MOTOR_FAILURE_STAGE_EXECUTOR_SERVICE_TIMER_UNHEALTHY);
}

static void TestUnsafePendingCompletion(MotorDirection direction)
{
    const MotorExecutorSnapshot *motor;
    uint32_t disable_count;
    uint32_t cancel_count;

    StartValidationPulse(direction);
    motor = MotorExecutor_GetSnapshot();
    FakeMotorStopTimer_TriggerNormal();
    FakeMotorHwReal_ForceUnexpectedPressOutput(100U);
    disable_count = FakeMotorHwReal_GetState()->disable_count;
    cancel_count = FakeMotorStopTimer_GetState()->cancel_count;
    assert(!MotorExecutor_ActiveRequestIsValid());
    assert(FakeMotorHwReal_GetState()->disable_count == disable_count);
    assert(FakeMotorStopTimer_GetState()->cancel_count == cancel_count);
    assert(motor->logical_active);
    assert(motor->last_completion == MOTOR_COMPLETION_NONE);
    /* Only Service may consume the event and diagnose the unsafe output. */
    assert(MotorExecutor_Service(100U) == MOTOR_RESULT_HARDWARE_ERROR);
    assert(motor->last_failure_stage ==
           MOTOR_FAILURE_STAGE_EXECUTOR_COMPLETION_OUTPUT_NOT_DISABLED);
    assert(motor->last_completion == MOTOR_COMPLETION_ERROR);

    StartValidationPulse(direction);
    FakeMotorStopTimer_FailNextCancelVerification();
    FakeMotorStopTimer_TriggerNormal();
    assert(MotorHwReal_IsDisabled());
    assert(MotorStopTimer_IsArmed());
    assert(!MotorExecutor_ActiveRequestIsValid());
    assert(motor->logical_active);
    assert(motor->last_completion == MOTOR_COMPLETION_NONE);
    /* Repair only the fake timer state; validation must have kept the event. */
    MotorStopTimer_Cancel();
    assert(MotorExecutor_ActiveRequestIsValid());
    assert(MotorExecutor_Service(100U) == MOTOR_RESULT_OK);
    assert(motor->last_completion == MOTOR_COMPLETION_NORMAL);

    /* Also reject unsafe physical output when the event arrives mid-query. */
    StartValidationPulse(direction);
    FakeMotorHwReal_FailNextDisableVerification();
    FakeMotorStopTimer_TriggerNormalOnNextArmedQuery();
    assert(!MotorExecutor_ActiveRequestIsValid());
    assert(!MotorHwReal_IsDisabled());
    assert(!MotorStopTimer_IsArmed());
    assert(motor->logical_active);
    assert(motor->last_completion == MOTOR_COMPLETION_NONE);
    assert(MotorExecutor_Service(100U) == MOTOR_RESULT_HARDWARE_ERROR);
    assert(motor->last_completion == MOTOR_COMPLETION_ERROR);

    /* A failed ISR cancel must not pass through the ordinary active checks. */
    StartValidationPulse(direction);
    FakeMotorStopTimer_FailNextCancelVerification();
    FakeMotorStopTimer_TriggerNormalOnNextArmedQuery();
    assert(!MotorExecutor_ActiveRequestIsValid());
    assert(MotorHwReal_IsDisabled());
    assert(MotorStopTimer_IsArmed());
    assert(motor->logical_active);
    assert(motor->last_completion == MOTOR_COMPLETION_NONE);
    MotorStopTimer_Cancel();
    assert(MotorExecutor_ActiveRequestIsValid());
    assert(MotorExecutor_Service(100U) == MOTOR_RESULT_OK);
    assert(motor->last_completion == MOTOR_COMPLETION_NORMAL);
}

static void TestCompletionHandoff(MotorDirection direction,
                                  MotorStopTimerEvent event,
                                  bool during_query)
{
    const MotorExecutorSnapshot *motor;
    const MotorCompletion expected = (event == MOTOR_STOP_TIMER_NORMAL) ?
        MOTOR_COMPLETION_NORMAL : ((event == MOTOR_STOP_TIMER_BACKSTOP) ?
            MOTOR_COMPLETION_BACKSTOP : MOTOR_COMPLETION_ERROR);

    StartValidationPulse(direction);
    motor = MotorExecutor_GetSnapshot();
    if (during_query)
    {
        FakeMotorStopTimer_TriggerOnNextArmedQuery(event);
    }
    else if (event == MOTOR_STOP_TIMER_NORMAL)
    {
        FakeMotorStopTimer_TriggerNormal();
    }
    else if (event == MOTOR_STOP_TIMER_BACKSTOP)
    {
        FakeMotorStopTimer_TriggerBackstop();
    }
    else
    {
        FakeMotorStopTimer_TriggerError();
    }
    assert(MotorExecutor_ActiveRequestIsValid());
    assert(MotorExecutor_ActiveRequestIsValid());
    assert(motor->physical_output_disabled);
    assert(MotorHwReal_IsDisabled());
    assert(!MotorStopTimer_IsArmed());
    assert(motor->logical_active);
    assert(motor->last_completion == MOTOR_COMPLETION_NONE);
    assert(motor->last_failure_result == MOTOR_RESULT_OK);
    assert(FakeMotorStopTimer_GetState()->expiry_count == 1U);

    assert(MotorExecutor_Service(100U) ==
           ((event == MOTOR_STOP_TIMER_ERROR) ? MOTOR_RESULT_TIMER_ERROR :
                                               MOTOR_RESULT_OK));
    assert(motor->last_completion == expected);
    assert(!motor->logical_active);
    assert(!MotorExecutor_ActiveRequestIsValid());
    if (event == MOTOR_STOP_TIMER_ERROR)
    {
        assert(motor->last_failure_result == MOTOR_RESULT_TIMER_ERROR);
        assert(motor->last_failure_stage ==
               MOTOR_FAILURE_STAGE_TIMER_IRQ_COMPARE_OVERCAPTURE);
    }
}

static void TestGuardCompletionRaceAndStop(MotorDirection direction)
{
    StartValidationPulse(direction);
    FakeMotorStopTimer_TriggerNormalOnNextArmedQuery();
    assert(MotorExecutor_GuardOutput() == MOTOR_RESULT_OK);
    assert(MotorHwReal_IsDisabled());
    assert(!MotorStopTimer_IsArmed());
    assert(MotorExecutor_GetSnapshot()->last_completion == MOTOR_COMPLETION_NONE);
    assert(MotorExecutor_Service(100U) == MOTOR_RESULT_OK);
    assert(MotorExecutor_GetSnapshot()->last_completion == MOTOR_COMPLETION_NORMAL);

    StartValidationPulse(direction);
    FakeMotorStopTimer_TriggerNormal();
    assert(MotorExecutor_ActiveRequestIsValid());
    assert(MotorExecutor_Disable() == MOTOR_RESULT_OK);
    assert(!MotorExecutor_ActiveRequestIsValid());
    assert(MotorExecutor_Service(100U) == MOTOR_RESULT_OK);
    assert(MotorExecutor_GetSnapshot()->last_completion == MOTOR_COMPLETION_NONE);
    assert(FakeMotorHwReal_GetState()->apply_count == 1U);
}

static void TestActiveRequestRegressions(void)
{
    static const MotorDirection directions[] = {
        MOTOR_DIRECTION_PRESS, MOTOR_DIRECTION_RELEASE
    };
    static const MotorStopTimerEvent events[] = {
        MOTOR_STOP_TIMER_NORMAL, MOTOR_STOP_TIMER_BACKSTOP, MOTOR_STOP_TIMER_ERROR
    };
    unsigned direction;
    unsigned event;

    for (direction = 0U; direction < 2U; ++direction)
    {
        TestInvalidWithoutCompletion(directions[direction]);
        TestUnsafePendingCompletion(directions[direction]);
        TestGuardCompletionRaceAndStop(directions[direction]);
        for (event = 0U; event < 3U; ++event)
        {
            TestCompletionHandoff(directions[direction], events[event], false);
            TestCompletionHandoff(directions[direction], events[event], true);
        }
    }
}

typedef enum
{
    SERVICE_PHYSICAL_QUERY,
    SERVICE_ARMED_QUERY,
    SERVICE_HEALTHY_QUERY,
    SERVICE_OUTPUT_DROPPED_QUERY,
    GUARD_PENDING_ENTRY,
    GUARD_PHYSICAL_QUERY,
    GUARD_ACTIVE_CHECK_FAILURE,
    GUARD_AFTER_DISABLE
} CompletionWindow;

static MotorStopTimerEvent s_injected_event;

static void TriggerInjectedCompletion(void)
{
    if (s_injected_event == MOTOR_STOP_TIMER_NORMAL)
    {
        FakeMotorStopTimer_TriggerNormal();
    }
    else if (s_injected_event == MOTOR_STOP_TIMER_BACKSTOP)
    {
        FakeMotorStopTimer_TriggerBackstop();
    }
    else
    {
        FakeMotorStopTimer_TriggerError();
    }
}

static void TestRemainingCompletionWindow(MotorDirection direction,
                                           MotorStopTimerEvent event,
                                           CompletionWindow window)
{
    const MotorExecutorSnapshot *motor;
    const bool guard = window >= GUARD_PENDING_ENTRY;
    const MotorResult service_result = (event == MOTOR_STOP_TIMER_ERROR) ?
        MOTOR_RESULT_TIMER_ERROR : MOTOR_RESULT_OK;
    const MotorCompletion completion = (event == MOTOR_STOP_TIMER_NORMAL) ?
        MOTOR_COMPLETION_NORMAL : ((event == MOTOR_STOP_TIMER_BACKSTOP) ?
            MOTOR_COMPLETION_BACKSTOP : MOTOR_COMPLETION_ERROR);
    uint32_t armed_queries;

    StartValidationPulse(direction);
    motor = MotorExecutor_GetSnapshot();
    s_injected_event = event;
    switch (window)
    {
        case SERVICE_PHYSICAL_QUERY:
        case GUARD_PHYSICAL_QUERY:
            FakeMotorHwReal_OnDisabledQuery(1U, TriggerInjectedCompletion);
            break;
        case SERVICE_ARMED_QUERY:
            FakeMotorStopTimer_TriggerOnNextArmedQuery(event);
            break;
        case SERVICE_HEALTHY_QUERY:
            FakeMotorStopTimer_TriggerOnNextHealthyQuery(event);
            break;
        case SERVICE_OUTPUT_DROPPED_QUERY:
            FakeMotorHwReal_ForceOutputDropped();
            FakeMotorStopTimer_TriggerOnNextArmedQueryReturningPreviousState(event);
            break;
        case GUARD_PENDING_ENTRY:
            FakeMotorStopTimer_FailNextCancelVerification();
            TriggerInjectedCompletion();
            FakeMotorHwReal_ForceUnexpectedPressOutput(100U);
            break;
        case GUARD_ACTIVE_CHECK_FAILURE:
            FakeMotorStopTimer_FailNextCancelVerification();
            FakeMotorStopTimer_TriggerOnNextArmedQuery(event);
            break;
        case GUARD_AFTER_DISABLE:
            FakeMotorHwReal_ForceOutputDropped();
            FakeMotorHwReal_OnNextDisable(TriggerInjectedCompletion);
            break;
    }
    printf("COMPLETION_WINDOW direction=%u event=%u window=%u\n",
           (unsigned)direction, (unsigned)event, (unsigned)window);
    (void)fflush(stdout);
    if (guard)
    {
        assert(MotorExecutor_GuardOutput() == MOTOR_RESULT_OK);
        assert(motor->logical_active);
        assert(motor->last_completion == MOTOR_COMPLETION_NONE);
        assert(MotorExecutor_GuardOutput() == MOTOR_RESULT_OK);
        assert(motor->logical_active);
        assert(motor->last_completion == MOTOR_COMPLETION_NONE);
    }
    assert(MotorExecutor_Service(100U) == service_result);
    assert(motor->last_completion == completion);
    assert(!motor->logical_active);
    assert(MotorHwReal_IsDisabled());
    assert(!MotorStopTimer_IsArmed());
    assert(FakeMotorStopTimer_GetState()->expiry_count == 1U);
    assert(FakeMotorStopTimer_GetState()->arm_count == 1U);
    assert(FakeMotorHwReal_GetState()->apply_count == 1U);
    assert(motor->request_sequence == 1U);
    if (event == MOTOR_STOP_TIMER_ERROR)
    {
        assert(motor->last_failure_result == MOTOR_RESULT_TIMER_ERROR);
        assert(motor->last_failure_stage == MOTOR_FAILURE_STAGE_TIMER_IRQ_COMPARE_OVERCAPTURE);
    }
    else
    {
        assert(motor->last_failure_result == MOTOR_RESULT_OK);
        assert(motor->last_failure_stage == MOTOR_FAILURE_STAGE_NONE);
        armed_queries = FakeMotorStopTimer_GetState()->armed_query_count;
        assert(MotorExecutor_Service(101U) == MOTOR_RESULT_OK);
        assert(FakeMotorStopTimer_GetState()->armed_query_count == armed_queries + 1U);
        assert(motor->last_completion == completion);
    }
}

static void TestGuardPersistentFailures(MotorDirection direction)
{
    const MotorExecutorSnapshot *motor;

    StartValidationPulse(direction);
    FakeMotorStopTimer_TriggerBackstop();
    motor = MotorExecutor_GetSnapshot();
    FakeMotorHwReal_ForceUnexpectedPressOutput(100U);
    FakeMotorHwReal_HoldOutputEnabled(true);
    assert(MotorExecutor_GuardOutput() == MOTOR_RESULT_HARDWARE_ERROR);
    assert(!MotorHwReal_IsDisabled());
    assert(motor->logical_active);
    assert(motor->last_completion == MOTOR_COMPLETION_NONE);
    FakeMotorHwReal_HoldOutputEnabled(false);
    assert(MotorExecutor_GuardOutput() == MOTOR_RESULT_OK);
    assert(MotorExecutor_Service(100U) == MOTOR_RESULT_OK);
    assert(motor->last_completion == MOTOR_COMPLETION_BACKSTOP);

    StartValidationPulse(direction);
    FakeMotorStopTimer_HoldArmedOnCancel(true);
    FakeMotorStopTimer_TriggerNormal();
    assert(MotorExecutor_GuardOutput() == MOTOR_RESULT_TIMER_ERROR);
    assert(MotorHwReal_IsDisabled());
    assert(MotorStopTimer_IsArmed());
    assert(motor->logical_active);
    assert(motor->last_completion == MOTOR_COMPLETION_NONE);
    FakeMotorStopTimer_HoldArmedOnCancel(false);
    assert(MotorExecutor_GuardOutput() == MOTOR_RESULT_OK);
    assert(MotorExecutor_Service(100U) == MOTOR_RESULT_OK);
    assert(motor->last_completion == MOTOR_COMPLETION_NORMAL);

    StartValidationPulse(direction);
    FakeMotorHwReal_ForceOutputDropped();
    assert(MotorExecutor_GuardOutput() == MOTOR_RESULT_HARDWARE_ERROR);
    assert(motor->last_failure_stage == MOTOR_FAILURE_STAGE_EXECUTOR_GUARD_ACTIVE_REQUEST_INVALID);
    assert(FakeMotorStopTimer_GetState()->expiry_count == 0U);

    StartValidationPulse(direction);
    MotorStopTimer_Cancel();
    assert(MotorExecutor_GuardOutput() == MOTOR_RESULT_HARDWARE_ERROR);
    assert(motor->last_failure_stage == MOTOR_FAILURE_STAGE_EXECUTOR_GUARD_ACTIVE_REQUEST_INVALID);
    assert(FakeMotorStopTimer_GetState()->expiry_count == 0U);
}

static void TestServiceGuardWindows(MotorDirection direction, bool guard)
{
    static const MotorStopTimerEvent events[] = {
        MOTOR_STOP_TIMER_NORMAL, MOTOR_STOP_TIMER_BACKSTOP, MOTOR_STOP_TIMER_ERROR
    };
    unsigned event;
    unsigned window;
    const unsigned first = guard ? GUARD_PENDING_ENTRY : SERVICE_PHYSICAL_QUERY;
    const unsigned last = guard ? GUARD_AFTER_DISABLE : SERVICE_OUTPUT_DROPPED_QUERY;

    for (event = 0U; event < 3U; ++event)
    {
        for (window = first; window <= last; ++window)
        {
            TestRemainingCompletionWindow(direction, events[event], (CompletionWindow)window);
        }
    }
    if (guard)
    {
        TestGuardPersistentFailures(direction);
    }
}

int main(int argc, char **argv)
{
    if (argc == 2)
    {
        const bool press = strstr(argv[1], "press") != NULL;
        const bool guard = strncmp(argv[1], "guard-", 6U) == 0;
        assert((strcmp(argv[1], "service-press") == 0) ||
               (strcmp(argv[1], "service-release") == 0) ||
               (strcmp(argv[1], "guard-press") == 0) ||
               (strcmp(argv[1], "guard-release") == 0));
        TestServiceGuardWindows(press ? MOTOR_DIRECTION_PRESS : MOTOR_DIRECTION_RELEASE, guard);
        puts("Service/Guard race regressions: PASS");
        return 0;
    }
    assert(argc == 1);
    TestDirectionMappingAndBreakBeforeMake();
    TestInvalidAndTimerArmFailureStayDisabled();
    TestExpiryOrdering();
    TestDisableBusyAndApplyFailure();
    TestTimerCommitErrorDisablesOutput();
    TestInactiveGuardReportsUnexpectedOutput();
    TestInactiveGuardReportsUnexpectedTimer();
    TestFailureDiagnosticsAndPreservation();
    TestServiceAndDisableDiagnostics();
    TestActiveRequestRegressions();
    TestServiceGuardWindows(MOTOR_DIRECTION_PRESS, false);
    TestServiceGuardWindows(MOTOR_DIRECTION_RELEASE, false);
    TestServiceGuardWindows(MOTOR_DIRECTION_PRESS, true);
    TestServiceGuardWindows(MOTOR_DIRECTION_RELEASE, true);
    puts("Real MotorExecutor host tests: PASS");
    return 0;
}
