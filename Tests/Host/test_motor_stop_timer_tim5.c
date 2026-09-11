#include <assert.h>
#include <stdio.h>

#include "Board/Motor/motor_stop_timer.h"
#include "Tests/Host/fake_stm32_hal.h"

static MotorStopTimerEvent s_event;
static uint32_t s_callback_count;

static void RecordEvent(MotorStopTimerEvent event)
{
    s_event = event;
    ++s_callback_count;
}

static void StartFixture(void)
{
    FakeStm32Hal_Reset();
    s_event = MOTOR_STOP_TIMER_ERROR;
    s_callback_count = 0U;
    assert(MotorStopTimer_Initialize(RecordEvent));
    assert(MotorStopTimer_GetLastFailureStage() ==
           MOTOR_FAILURE_STAGE_NONE);
}

static void ArmAndCommit(void)
{
    assert(MotorStopTimer_Arm(20U, 50U));
    assert(MotorStopTimer_IsArmed());
    assert(TIM5->PSC == 9599U);
    assert(TIM5->CCR1 == 200U);
    assert(TIM5->ARR == 499U);
    assert((TIM5->CR1 & (TIM_CR1_OPM | TIM_CR1_URS |
                         TIM_CR1_CEN)) ==
           (TIM_CR1_OPM | TIM_CR1_URS | TIM_CR1_CEN));
    assert(!FakeStm32Hal_GetState()->tim5_irq_enabled);
    assert(MotorStopTimer_CommitArm());
    assert(FakeStm32Hal_GetState()->tim5_irq_enabled);
}

static void TriggerAndAssert(uint32_t status,
                             MotorStopTimerEvent expected_event,
                             MotorFailureStage expected_stage)
{
    TIM5->SR = status;
    MotorStopTimer_IrqHandler();
    assert(s_callback_count == 1U);
    assert(s_event == expected_event);
    assert(MotorStopTimer_GetLastFailureStage() == expected_stage);
    assert(!MotorStopTimer_IsArmed());
    assert(!FakeStm32Hal_GetState()->tim5_irq_enabled);
}

static void TestInitializeFailures(void)
{
    FakeStm32Hal_Reset();
    assert(!MotorStopTimer_Initialize(NULL));
    assert(MotorStopTimer_GetLastFailureStage() ==
           MOTOR_FAILURE_STAGE_TIMER_INIT_NULL_HANDLER);

    FakeStm32Hal_Reset();
    FakeStm32Hal_GetState()->pclk1_hz = 12345U;
    assert(!MotorStopTimer_Initialize(RecordEvent));
    assert(MotorStopTimer_GetLastFailureStage() ==
           MOTOR_FAILURE_STAGE_TIMER_INIT_CLOCK_INVALID);
}

static void TestNormalBackstopAndErrors(void)
{
    StartFixture();
    ArmAndCommit();
    TriggerAndAssert(TIM_SR_CC1IF,
                     MOTOR_STOP_TIMER_NORMAL,
                     MOTOR_FAILURE_STAGE_NONE);

    StartFixture();
    ArmAndCommit();
    TriggerAndAssert(TIM_SR_UIF,
                     MOTOR_STOP_TIMER_BACKSTOP,
                     MOTOR_FAILURE_STAGE_NONE);

    StartFixture();
    ArmAndCommit();
    TriggerAndAssert(TIM_SR_CC1OF,
                     MOTOR_STOP_TIMER_ERROR,
                     MOTOR_FAILURE_STAGE_TIMER_IRQ_COMPARE_OVERCAPTURE);
    MotorStopTimer_Cancel();
    assert(MotorStopTimer_GetLastFailureStage() ==
           MOTOR_FAILURE_STAGE_TIMER_IRQ_COMPARE_OVERCAPTURE);

    StartFixture();
    ArmAndCommit();
    TriggerAndAssert(TIM_SR_CC1OF | TIM_SR_UIF | TIM_SR_CC1IF,
                     MOTOR_STOP_TIMER_ERROR,
                     MOTOR_FAILURE_STAGE_TIMER_IRQ_COMPARE_OVERCAPTURE);

    StartFixture();
    ArmAndCommit();
    TriggerAndAssert(0U,
                     MOTOR_STOP_TIMER_ERROR,
                     MOTOR_FAILURE_STAGE_TIMER_IRQ_UNKNOWN_STATUS);
}

static void TestUnexpectedIrqAndCommitPending(void)
{
    StartFixture();
    MotorStopTimer_IrqHandler();
    assert(s_callback_count == 1U);
    assert(s_event == MOTOR_STOP_TIMER_ERROR);
    assert(MotorStopTimer_GetLastFailureStage() ==
           MOTOR_FAILURE_STAGE_TIMER_IRQ_NOT_ARMED);
    assert(!MotorStopTimer_IsArmed());

    FakeStm32Hal_Reset();
    s_callback_count = 0U;
    assert(!MotorStopTimer_Initialize(NULL));
    MotorStopTimer_IrqHandler();
    assert(s_callback_count == 0U);
    assert(MotorStopTimer_GetLastFailureStage() ==
           MOTOR_FAILURE_STAGE_TIMER_IRQ_MISSING_HANDLER);
    assert(!MotorStopTimer_IsArmed());

    StartFixture();
    assert(MotorStopTimer_Arm(20U, 50U));
    TIM5->SR = TIM_SR_CC1IF;
    assert(!MotorStopTimer_CommitArm());
    assert(s_callback_count == 1U);
    assert(!MotorStopTimer_IsArmed());
    assert(MotorStopTimer_GetLastFailureStage() ==
           MOTOR_FAILURE_STAGE_TIMER_COMMIT_PENDING_EVENT);
}

int main(void)
{
    TestInitializeFailures();
    TestNormalBackstopAndErrors();
    TestUnexpectedIrqAndCommitPending();
    puts("Concrete TIM5 stop timer host tests: PASS");
    return 0;
}
