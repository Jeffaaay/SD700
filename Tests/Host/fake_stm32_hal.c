#include "Tests/Host/fake_stm32_hal.h"

#include <assert.h>
#include <string.h>

#include "Board/Motor/motor_hw_force_disable.h"
#include "Board/Motor/motor_hw_map.h"
#include "Board/Motor/motor_hw_real.h"

TIM_TypeDef g_fake_tim2;
TIM_TypeDef g_fake_tim3;
TIM_TypeDef g_fake_tim5;
GPIO_TypeDef g_fake_gpiob;
RCC_TypeDef g_fake_rcc;

static FakeStm32HalState s_hal;

static void FakeStm32Hal_ObserveCcrSafety(void)
{
    if ((TIM2->CCR3 != 0U) && (TIM3->CCR3 != 0U))
    {
        s_hal.both_ccr_nonzero_violation = true;
    }
}

void FakeStm32Hal_Reset(void)
{
    (void)memset(&g_fake_tim2, 0, sizeof(g_fake_tim2));
    (void)memset(&g_fake_tim3, 0, sizeof(g_fake_tim3));
    (void)memset(&g_fake_tim5, 0, sizeof(g_fake_tim5));
    (void)memset(&g_fake_gpiob, 0, sizeof(g_fake_gpiob));
    (void)memset(&g_fake_rcc, 0, sizeof(g_fake_rcc));
    (void)memset(&s_hal, 0, sizeof(s_hal));
    s_hal.pclk1_hz = 48000000U;
    s_hal.output_arming_allowed = true;
    g_fake_rcc.CFGR = 4UL << 10U;
}

FakeStm32HalState *FakeStm32Hal_GetState(void)
{
    return &s_hal;
}

void FakeStm32Hal_AssertMotorDisabled(void)
{
    assert((GPIOB->ODR & MOTOR_HW_SD_GPIO_PIN_MASK) == 0U);
    assert(TIM2->CCR3 == 0U);
    assert(TIM3->CCR3 == 0U);
    assert((TIM2->CCER & TIM_CCER_CC3E) == 0U);
    assert((TIM3->CCER & TIM_CCER_CC3E) == 0U);
    assert((TIM2->CR1 & TIM_CR1_CEN) == 0U);
    assert((TIM3->CR1 & TIM_CR1_CEN) == 0U);
}

uint32_t HAL_RCC_GetPCLK1Freq(void)
{
    return s_hal.pclk1_hz;
}

HAL_StatusTypeDef HAL_TIM_PWM_Init(TIM_HandleTypeDef *timer)
{
    if (timer->Instance == s_hal.fail_pwm_init_instance)
    {
        return HAL_ERROR;
    }
    timer->Instance->PSC = timer->Init.Prescaler;
    timer->Instance->ARR = timer->Init.Period;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(
    TIM_HandleTypeDef *timer,
    const TIM_OC_InitTypeDef *channel,
    uint32_t selected_channel)
{
    assert(selected_channel == TIM_CHANNEL_3);
    if (timer->Instance == s_hal.fail_channel_instance)
    {
        return HAL_ERROR;
    }
    timer->Instance->CCER &= ~(TIM_CCER_CC3E | TIM_CCER_CC3P);
    timer->Instance->CCMR2 =
        (timer->Instance->CCMR2 &
         ~(TIM_CCMR2_OC3M | TIM_CCMR2_OC3FE)) |
        channel->OCMode | channel->OCFastMode | TIM_CCMR2_OC3PE;
    timer->Instance->CCR3 = channel->Pulse;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *timer,
                                    uint32_t channel)
{
    assert(channel == TIM_CHANNEL_3);
    ++s_hal.pwm_start_call_count;
    if (timer->Instance == TIM2)
    {
        ++s_hal.tim2_pwm_start_count;
        s_hal.tim2_last_start_order = s_hal.pwm_start_call_count;
    }
    else
    {
        assert(timer->Instance == TIM3);
        ++s_hal.tim3_pwm_start_count;
        s_hal.tim3_last_start_order = s_hal.pwm_start_call_count;
    }
    if (timer->Instance == s_hal.fail_start_instance)
    {
        return HAL_ERROR;
    }
    timer->Instance->CCER |= TIM_CCER_CC3E;
    timer->Instance->CR1 |= TIM_CR1_CEN;
    if (s_hal.break_zero_carrier_verify &&
        (timer->Instance == TIM3))
    {
        TIM2->CR1 &= ~TIM_CR1_CEN;
    }
    FakeStm32Hal_ObserveCcrSafety();
    return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef *timer,
                                   uint32_t channel)
{
    assert(channel == TIM_CHANNEL_3);
    if (timer->Instance == s_hal.fail_stop_instance)
    {
        return HAL_ERROR;
    }
    timer->Instance->CCER &= ~TIM_CCER_CC3E;
    timer->Instance->CR1 &= ~TIM_CR1_CEN;
    FakeStm32Hal_ObserveCcrSafety();
    return HAL_OK;
}

void FakeStm32Hal_SetCompare(TIM_HandleTypeDef *timer,
                            uint32_t compare)
{
    timer->Instance->CCR3 = compare;
    if (s_hal.break_zero_compare_commit &&
        (timer->Instance == TIM3) && (compare == 0U))
    {
        timer->Instance->CCR3 = 1U;
        s_hal.break_zero_compare_commit = false;
    }
    if (s_hal.break_active_compare_verify && (compare != 0U))
    {
        timer->Instance->CCR3 = 0U;
        s_hal.break_active_compare_verify = false;
    }
    FakeStm32Hal_ObserveCcrSafety();
}

void HAL_GPIO_WritePin(GPIO_TypeDef *port,
                       uint32_t pins,
                       GPIO_PinState state)
{
    assert(port == GPIOB);
    if (state == GPIO_PIN_SET)
    {
        ++s_hal.sd_enable_call_count;
        s_hal.tim2_ccr3_at_sd_enable = TIM2->CCR3;
        s_hal.tim3_ccr3_at_sd_enable = TIM3->CCR3;
        s_hal.tim2_cc3e_at_sd_enable =
            (TIM2->CCER & TIM_CCER_CC3E) != 0U;
        s_hal.tim2_cen_at_sd_enable =
            (TIM2->CR1 & TIM_CR1_CEN) != 0U;
        s_hal.tim3_cc3e_at_sd_enable =
            (TIM3->CCER & TIM_CCER_CC3E) != 0U;
        s_hal.tim3_cen_at_sd_enable =
            (TIM3->CR1 & TIM_CR1_CEN) != 0U;
        s_hal.both_zero_carriers_running_at_sd_enable =
            (s_hal.tim2_ccr3_at_sd_enable == 0U) &&
            (s_hal.tim3_ccr3_at_sd_enable == 0U) &&
            s_hal.tim2_cc3e_at_sd_enable &&
            s_hal.tim2_cen_at_sd_enable &&
            s_hal.tim3_cc3e_at_sd_enable &&
            s_hal.tim3_cen_at_sd_enable;
        if (s_hal.break_driver_enable_verify)
        {
            port->ODR |= pins & (1UL << 1U);
            FakeStm32Hal_ObserveCcrSafety();
            return;
        }
        port->ODR |= pins;
    }
    else
    {
        port->ODR &= ~pins;
    }
    FakeStm32Hal_ObserveCcrSafety();
}

void HAL_NVIC_SetPriority(int irq, uint32_t priority, uint32_t subpriority)
{
    (void)irq;
    (void)priority;
    (void)subpriority;
}

void HAL_NVIC_EnableIRQ(int irq)
{
    assert(irq == TIM5_IRQn);
    s_hal.tim5_irq_enabled = true;
}

void HAL_NVIC_DisableIRQ(int irq)
{
    assert(irq == TIM5_IRQn);
    s_hal.tim5_irq_enabled = false;
}

void HAL_NVIC_ClearPendingIRQ(int irq)
{
    assert(irq == TIM5_IRQn);
    s_hal.tim5_irq_pending = false;
}

void MotorHw_ForceDisableImmediate(void)
{
    ++s_hal.force_disable_count;
    if (s_hal.force_disable_count == s_hal.fail_disable_on_call)
    {
        GPIOB->ODR |= MOTOR_HW_SD_GPIO_PIN_MASK;
        TIM3->CCR3 = 1U;
        TIM3->CCER |= TIM_CCER_CC3E;
        return;
    }
    GPIOB->ODR &= ~MOTOR_HW_SD_GPIO_PIN_MASK;
    TIM2->CCR3 = 0U;
    TIM3->CCR3 = 0U;
    TIM2->CCER &= ~TIM_CCER_CC3E;
    TIM3->CCER &= ~TIM_CCER_CC3E;
}

void MotorHw_ForceDisable(void)
{
    MotorHw_ForceDisableImmediate();
}

bool MotorHwReal_OutputArmingAllowed(void)
{
    return s_hal.output_arming_allowed;
}
