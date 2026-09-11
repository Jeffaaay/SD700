#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "Board/Motor/motor_executor.h"
#include "Board/Motor/motor_plan_config.h"
#include "Tests/Host/fake_motor_hw.h"

static void AssertLockedOff(void)
{
    const MotorExecutorSnapshot *snapshot =
        MotorExecutor_GetSnapshot();

    assert(snapshot != NULL);
    assert(snapshot->physical_output_locked);
    assert(snapshot->physical_output_disabled);
}

static void TestPlanner(void)
{
    uint16_t tim2;
    uint16_t tim3;

    assert(MotorExecutor_PlanCommand(MOTOR_DIRECTION_PRESS,
                                     6000U,
                                     &tim2,
                                     &tim3) == MOTOR_RESULT_OK);
    assert(tim2 == 0U);
    assert(tim3 > 0U);
    assert(tim3 <= MOTOR_PLAN_PWM_PERIOD_COUNTS);

    assert(MotorExecutor_PlanCommand(MOTOR_DIRECTION_RELEASE,
                                     6000U,
                                     &tim2,
                                     &tim3) == MOTOR_RESULT_OK);
    assert(tim2 > 0U);
    assert(tim3 == 0U);

    assert(MotorExecutor_PlanCommand(MOTOR_DIRECTION_PRESS,
                                     1U,
                                     &tim2,
                                     &tim3) == MOTOR_RESULT_OK);
    assert(tim3 == 1U);
    assert(MotorExecutor_PlanCommand(MOTOR_DIRECTION_PRESS,
                                     24000U,
                                     &tim2,
                                     &tim3) == MOTOR_RESULT_OK);
    assert(tim3 == MOTOR_PLAN_PWM_PERIOD_COUNTS);
    assert(MotorExecutor_PlanCommand(MOTOR_DIRECTION_PRESS,
                                     24001U,
                                     &tim2,
                                     &tim3) == MOTOR_RESULT_INVALID);
}

static void TestRunBusyDisableAndSequence(void)
{
    const MotorExecutorSnapshot *snapshot;
    uint32_t first_sequence;

    FakeMotorHw_Reset();
    assert(MotorExecutor_Initialize() == MOTOR_RESULT_OK);
    AssertLockedOff();
    assert(FakeMotorHw_GetDisableCount() > 0U);

    assert(MotorExecutor_StartRun(MOTOR_DIRECTION_PRESS,
                                  6000U,
                                  50U,
                                  60U,
                                  10U) == MOTOR_RESULT_OK);
    snapshot = MotorExecutor_GetSnapshot();
    first_sequence = snapshot->request_sequence;
    assert(snapshot->last_action == MOTOR_ACTION_PRESS_RUN);
    assert(snapshot->logical_active);
    assert(snapshot->planned_tim2_ccr3 == 0U);
    assert(snapshot->planned_tim3_ccr3 > 0U);
    AssertLockedOff();

    assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_PRESS,
                                    2000U,
                                    10U,
                                    20U,
                                    11U) == MOTOR_RESULT_BUSY);
    assert(MotorExecutor_Disable() == MOTOR_RESULT_OK);
    assert(MotorExecutor_Disable() == MOTOR_RESULT_OK);
    assert(!MotorExecutor_GetSnapshot()->logical_active);
    AssertLockedOff();

    assert(MotorExecutor_StartRun(MOTOR_DIRECTION_RELEASE,
                                  3000U,
                                  25U,
                                  35U,
                                  20U) == MOTOR_RESULT_OK);
    assert(MotorExecutor_GetSnapshot()->request_sequence ==
           first_sequence + 1U);
    assert(MotorExecutor_GetSnapshot()->last_action ==
           MOTOR_ACTION_RELEASE_RUN);
}

static void TestCompletionAndBackstop(void)
{
    assert(MotorExecutor_Initialize() == MOTOR_RESULT_OK);
    assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_PRESS,
                                    2000U,
                                    10U,
                                    20U,
                                    100U) == MOTOR_RESULT_OK);
    assert(MotorExecutor_Service(109U) == MOTOR_RESULT_OK);
    assert(MotorExecutor_GetSnapshot()->logical_active);
    assert(MotorExecutor_Service(110U) == MOTOR_RESULT_OK);
    assert(!MotorExecutor_GetSnapshot()->logical_active);
    assert(MotorExecutor_GetSnapshot()->last_completion ==
           MOTOR_COMPLETION_NORMAL);
    AssertLockedOff();

    assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_RELEASE,
                                    2000U,
                                    10U,
                                    20U,
                                    200U) == MOTOR_RESULT_OK);
    assert(MotorExecutor_Service(220U) == MOTOR_RESULT_OK);
    assert(!MotorExecutor_GetSnapshot()->logical_active);
    assert(MotorExecutor_GetSnapshot()->last_completion ==
           MOTOR_COMPLETION_BACKSTOP);
    AssertLockedOff();
}

static void TestInvalidRequests(void)
{
    assert(MotorExecutor_Initialize() == MOTOR_RESULT_OK);
    assert(MotorExecutor_StartRun((MotorDirection)99,
                                  1000U,
                                  10U,
                                  20U,
                                  0U) == MOTOR_RESULT_INVALID);
    assert(MotorExecutor_StartRun(MOTOR_DIRECTION_PRESS,
                                  0U,
                                  10U,
                                  20U,
                                  0U) == MOTOR_RESULT_INVALID);
    assert(MotorExecutor_StartRun(MOTOR_DIRECTION_PRESS,
                                  1000U,
                                  10U,
                                  10U,
                                  0U) == MOTOR_RESULT_INVALID);
    assert(!MotorExecutor_GetSnapshot()->logical_active);
    AssertLockedOff();
}

int main(void)
{
    TestPlanner();
    TestRunBusyDisableAndSequence();
    TestCompletionAndBackstop();
    TestInvalidRequests();
    puts("Locked MotorExecutor host tests: PASS");
    return 0;
}
