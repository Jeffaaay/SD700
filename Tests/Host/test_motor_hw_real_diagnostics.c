#include <assert.h>
#include <stdio.h>

#include "Board/Motor/motor_hw_map.h"
#include "Board/Motor/motor_hw_real.h"
#include "Tests/Host/fake_stm32_hal.h"

static void AssertInitFailure(TIM_TypeDef *init_instance,
                              TIM_TypeDef *channel_instance,
                              MotorFailureStage expected)
{
    FakeStm32Hal_Reset();
    FakeStm32Hal_GetState()->fail_pwm_init_instance = init_instance;
    FakeStm32Hal_GetState()->fail_channel_instance = channel_instance;
    assert(!MotorHwReal_InitializeDisabled());
    assert(MotorHwReal_GetLastFailureStage() == expected);
    FakeStm32Hal_AssertMotorDisabled();
}

static void StartInitializedFixture(void)
{
    FakeStm32Hal_Reset();
    assert(MotorHwReal_InitializeDisabled());
    assert(MotorHwReal_GetLastFailureStage() ==
           MOTOR_FAILURE_STAGE_NONE);
    FakeStm32Hal_AssertMotorDisabled();
}

static void AssertZeroCarrierEnableSnapshot(void)
{
    const FakeStm32HalState *hal = FakeStm32Hal_GetState();

    assert(hal->sd_enable_call_count > 0U);
    assert(hal->tim2_ccr3_at_sd_enable == 0U);
    assert(hal->tim3_ccr3_at_sd_enable == 0U);
    assert(hal->tim2_cc3e_at_sd_enable);
    assert(hal->tim2_cen_at_sd_enable);
    assert(hal->tim3_cc3e_at_sd_enable);
    assert(hal->tim3_cen_at_sd_enable);
    assert(hal->both_zero_carriers_running_at_sd_enable);
    assert(hal->tim2_pwm_start_count == hal->tim3_pwm_start_count);
    assert(hal->tim2_last_start_order + 1U ==
           hal->tim3_last_start_order);
    assert(!hal->both_ccr_nonzero_violation);
}

static void AssertActiveCarrierState(bool press, uint16_t duty_counts)
{
    assert((GPIOB->ODR & MOTOR_HW_SD_GPIO_PIN_MASK) ==
           MOTOR_HW_SD_GPIO_PIN_MASK);
    assert(TIM2->CCR3 == (press ? 0U : duty_counts));
    assert(TIM3->CCR3 == (press ? duty_counts : 0U));
    assert((TIM2->CCER & TIM_CCER_CC3E) != 0U);
    assert((TIM3->CCER & TIM_CCER_CC3E) != 0U);
    assert((TIM2->CR1 & TIM_CR1_CEN) != 0U);
    assert((TIM3->CR1 & TIM_CR1_CEN) != 0U);
    assert(!((TIM2->CCR3 != 0U) && (TIM3->CCR3 != 0U)));
    assert(!FakeStm32Hal_GetState()->both_ccr_nonzero_violation);
}

static void AssertApplyFailure(MotorFailureStage expected)
{
    assert(!MotorHwReal_ApplyPress(100U));
    assert(MotorHwReal_GetLastFailureStage() == expected);
    FakeStm32Hal_AssertMotorDisabled();
}

static void TestInitializationStages(void)
{
    AssertInitFailure(TIM2, NULL,
                      MOTOR_FAILURE_STAGE_HW_INIT_TIM2_PWM);
    AssertInitFailure(NULL, TIM2,
                      MOTOR_FAILURE_STAGE_HW_INIT_TIM2_CHANNEL);
    AssertInitFailure(TIM3, NULL,
                      MOTOR_FAILURE_STAGE_HW_INIT_TIM3_PWM);
    AssertInitFailure(NULL, TIM3,
                      MOTOR_FAILURE_STAGE_HW_INIT_TIM3_CHANNEL);

    FakeStm32Hal_Reset();
    FakeStm32Hal_GetState()->fail_disable_on_call = 2U;
    assert(!MotorHwReal_InitializeDisabled());
    assert(MotorHwReal_GetLastFailureStage() ==
           MOTOR_FAILURE_STAGE_HW_INIT_DISABLED_VERIFY);
    FakeStm32Hal_AssertMotorDisabled();
}

static void TestApplyStages(void)
{
    assert(MOTOR_FAILURE_STAGE_HW_APPLY_ZERO_COMPARE_COMMIT == 45);
    assert(MOTOR_FAILURE_STAGE_HW_APPLY_START_TIM2_ZERO_CARRIER == 46);
    assert(MOTOR_FAILURE_STAGE_HW_APPLY_START_TIM3_ZERO_CARRIER == 47);
    assert(MOTOR_FAILURE_STAGE_HW_APPLY_ZERO_CARRIER_VERIFY == 48);
    assert(MOTOR_FAILURE_STAGE_HW_APPLY_DRIVER_ENABLE_VERIFY == 49);
    assert(MOTOR_FAILURE_STAGE_HW_APPLY_ACTIVE_COMPARE_VERIFY == 50);

    StartInitializedFixture();
    FakeStm32Hal_GetState()->output_arming_allowed = false;
    AssertApplyFailure(MOTOR_FAILURE_STAGE_HW_APPLY_ARMING_DENIED);

    StartInitializedFixture();
    FakeStm32Hal_GetState()->fail_stop_instance = TIM2;
    AssertApplyFailure(MOTOR_FAILURE_STAGE_HW_APPLY_STOP_TIM2);

    StartInitializedFixture();
    FakeStm32Hal_GetState()->fail_stop_instance = TIM3;
    AssertApplyFailure(MOTOR_FAILURE_STAGE_HW_APPLY_STOP_TIM3);

    StartInitializedFixture();
    FakeStm32Hal_GetState()->break_zero_compare_commit = true;
    AssertApplyFailure(
        MOTOR_FAILURE_STAGE_HW_APPLY_ZERO_COMPARE_COMMIT);

    StartInitializedFixture();
    FakeStm32Hal_GetState()->fail_start_instance = TIM2;
    AssertApplyFailure(
        MOTOR_FAILURE_STAGE_HW_APPLY_START_TIM2_ZERO_CARRIER);

    StartInitializedFixture();
    FakeStm32Hal_GetState()->fail_start_instance = TIM3;
    AssertApplyFailure(
        MOTOR_FAILURE_STAGE_HW_APPLY_START_TIM3_ZERO_CARRIER);

    StartInitializedFixture();
    FakeStm32Hal_GetState()->break_zero_carrier_verify = true;
    AssertApplyFailure(
        MOTOR_FAILURE_STAGE_HW_APPLY_ZERO_CARRIER_VERIFY);

    StartInitializedFixture();
    FakeStm32Hal_GetState()->break_driver_enable_verify = true;
    AssertApplyFailure(
        MOTOR_FAILURE_STAGE_HW_APPLY_DRIVER_ENABLE_VERIFY);

    StartInitializedFixture();
    FakeStm32Hal_GetState()->break_active_compare_verify = true;
    AssertApplyFailure(
        MOTOR_FAILURE_STAGE_HW_APPLY_ACTIVE_COMPARE_VERIFY);
}

static void TestDirectionMapping(void)
{
    StartInitializedFixture();
    assert(MotorHwReal_ApplyPress(100U));
    AssertZeroCarrierEnableSnapshot();
    AssertActiveCarrierState(true, 100U);
    MotorHwReal_DisableImmediate();
    FakeStm32Hal_AssertMotorDisabled();

    StartInitializedFixture();
    assert(MotorHwReal_ApplyRelease(100U));
    AssertZeroCarrierEnableSnapshot();
    AssertActiveCarrierState(false, 100U);
    MotorHwReal_DisableImmediate();
    FakeStm32Hal_AssertMotorDisabled();
}

static void TestPressDisableReleaseTransition(void)
{
    uint32_t disable_count;

    StartInitializedFixture();
    assert(MotorHwReal_ApplyPress(125U));
    AssertActiveCarrierState(true, 125U);

    disable_count = FakeStm32Hal_GetState()->force_disable_count;
    MotorHwReal_DisableImmediate();
    assert(FakeStm32Hal_GetState()->force_disable_count ==
           disable_count + 1U);
    FakeStm32Hal_AssertMotorDisabled();

    assert(MotorHwReal_ApplyRelease(125U));
    AssertZeroCarrierEnableSnapshot();
    AssertActiveCarrierState(false, 125U);
    assert(!FakeStm32Hal_GetState()->both_ccr_nonzero_violation);

    MotorHwReal_DisableImmediate();
    FakeStm32Hal_AssertMotorDisabled();
}

int main(void)
{
    TestInitializationStages();
    TestApplyStages();
    TestDirectionMapping();
    TestPressDisableReleaseTransition();
    puts("Actual real motor HW diagnostic host tests: PASS");
    return 0;
}
