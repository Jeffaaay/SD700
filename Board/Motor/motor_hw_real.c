#include "Board/Motor/motor_hw_real.h"

#include "Board/Motor/motor_hw_force_disable.h"
#include "Board/Motor/motor_hw_map.h"
#include "stm32f4xx_hal.h"

static bool s_initialized;
static TIM_HandleTypeDef s_tim2;
static TIM_HandleTypeDef s_tim3;
static MotorFailureStage s_last_failure_stage;

static uint32_t MotorHwReal_TimerClockHz(void)
{
    uint32_t clock_hz = HAL_RCC_GetPCLK1Freq();

    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1)
    {
        clock_hz *= 2U;
    }
    return clock_hz;
}

static void MotorHwReal_ConfigureShutdownPins(void)
{
    GPIOB->BSRR = MOTOR_HW_SD_GPIO_PIN_MASK << 16U;
    GPIOB->OTYPER &= ~MOTOR_HW_SD_GPIO_PIN_MASK;
    GPIOB->OSPEEDR |= MOTOR_HW_SD_MODE_FIELD_MASK;
    GPIOB->PUPDR =
        (GPIOB->PUPDR & ~MOTOR_HW_SD_MODE_FIELD_MASK) |
        MOTOR_HW_SD_PULLDOWN_BITS;
    GPIOB->MODER =
        (GPIOB->MODER & ~MOTOR_HW_SD_MODE_FIELD_MASK) |
        MOTOR_HW_SD_OUTPUT_MODE_BITS;
}

static void MotorHwReal_ConfigureAlternatePin(uint32_t pin_number,
                                              uint32_t alternate)
{
    uint32_t mode_shift = pin_number * 2U;
    uint32_t afr_index = pin_number / 8U;
    uint32_t afr_shift = (pin_number % 8U) * 4U;
    uint32_t mode_mask = 3UL << mode_shift;

    GPIOB->MODER = (GPIOB->MODER & ~mode_mask) |
                   (2UL << mode_shift);
    GPIOB->OTYPER &= ~(1UL << pin_number);
    GPIOB->OSPEEDR = (GPIOB->OSPEEDR & ~mode_mask) |
                     (3UL << mode_shift);
    GPIOB->PUPDR &= ~mode_mask;
    GPIOB->AFR[afr_index] =
        (GPIOB->AFR[afr_index] & ~(0xFUL << afr_shift)) |
        (alternate << afr_shift);
}

static bool MotorHwReal_ConfigurePwmTimer(TIM_HandleTypeDef *timer,
                                          TIM_TypeDef *instance,
                                          MotorFailureStage init_stage,
                                          MotorFailureStage channel_stage)
{
    TIM_OC_InitTypeDef channel = {0};

    *timer = (TIM_HandleTypeDef){0};
    timer->Instance = instance;
    timer->Init.Prescaler = MOTOR_HW_PWM_PRESCALER;
    timer->Init.CounterMode = TIM_COUNTERMODE_UP;
    timer->Init.Period = MOTOR_HW_PWM_PERIOD_COUNTS;
    timer->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    timer->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    channel.OCMode = TIM_OCMODE_PWM1;
    channel.Pulse = 0U;
    channel.OCPolarity = TIM_OCPOLARITY_HIGH;
    channel.OCFastMode = TIM_OCFAST_DISABLE;

    if (HAL_TIM_PWM_Init(timer) != HAL_OK)
    {
        s_last_failure_stage = init_stage;
        return false;
    }
    if (HAL_TIM_PWM_ConfigChannel(timer,
                                 &channel,
                                 TIM_CHANNEL_3) != HAL_OK)
    {
        s_last_failure_stage = channel_stage;
        return false;
    }
    return true;
}

static bool MotorHwReal_PwmConfigurationIsValid(void)
{
    uint32_t required_clock_hz =
        MOTOR_HW_PWM_FREQUENCY_HZ *
        (MOTOR_HW_PWM_PERIOD_COUNTS + 1U) *
        (MOTOR_HW_PWM_PRESCALER + 1U);

    return (MotorHwReal_TimerClockHz() == required_clock_hz) &&
           (TIM2->PSC == MOTOR_HW_PWM_PRESCALER) &&
           (TIM3->PSC == MOTOR_HW_PWM_PRESCALER) &&
           (TIM2->ARR == MOTOR_HW_PWM_PERIOD_COUNTS) &&
           (TIM3->ARR == MOTOR_HW_PWM_PERIOD_COUNTS) &&
           ((TIM2->CCMR2 & TIM_CCMR2_OC3M) == TIM_OCMODE_PWM1) &&
           ((TIM3->CCMR2 & TIM_CCMR2_OC3M) == TIM_OCMODE_PWM1) &&
           ((TIM2->CCER & TIM_CCER_CC3P) == 0U) &&
           ((TIM3->CCER & TIM_CCER_CC3P) == 0U) &&
           ((TIM2->CCMR2 & TIM_CCMR2_OC3FE) == 0U) &&
           ((TIM3->CCMR2 & TIM_CCMR2_OC3FE) == 0U);
}

void MotorHwReal_DisableImmediate(void)
{
    MotorHw_ForceDisableImmediate();
    TIM2->CR1 &= ~TIM_CR1_CEN;
    TIM3->CR1 &= ~TIM_CR1_CEN;
    TIM2->SR = 0U;
    TIM3->SR = 0U;
    __DSB();
}

bool MotorHwReal_IsDisabled(void)
{
    return ((GPIOB->ODR & MOTOR_HW_SD_GPIO_PIN_MASK) == 0U) &&
           (TIM2->CCR3 == 0U) &&
           (TIM3->CCR3 == 0U) &&
           ((TIM2->CCER & TIM_CCER_CC3E) == 0U) &&
           ((TIM3->CCER & TIM_CCER_CC3E) == 0U) &&
           ((TIM2->CR1 & TIM_CR1_CEN) == 0U) &&
           ((TIM3->CR1 & TIM_CR1_CEN) == 0U);
}

bool MotorHwReal_InitializeDisabled(void)
{
    volatile uint32_t clock_enable_readback;

    s_last_failure_stage = MOTOR_FAILURE_STAGE_NONE;
    s_initialized = false;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN | RCC_APB1ENR_TIM3EN;
    clock_enable_readback = RCC->AHB1ENR;
    clock_enable_readback ^= RCC->APB1ENR;
    (void)clock_enable_readback;

    MotorHwReal_DisableImmediate();
    MotorHwReal_ConfigureShutdownPins();
    MotorHwReal_ConfigureAlternatePin(10U, MOTOR_HW_TIM2_GPIO_AF);
    MotorHwReal_ConfigureAlternatePin(0U, MOTOR_HW_TIM3_GPIO_AF);
    if (!MotorHwReal_ConfigurePwmTimer(
            &s_tim2,
            TIM2,
            MOTOR_FAILURE_STAGE_HW_INIT_TIM2_PWM,
            MOTOR_FAILURE_STAGE_HW_INIT_TIM2_CHANNEL))
    {
        MotorHwReal_DisableImmediate();
        return false;
    }
    if (!MotorHwReal_ConfigurePwmTimer(
            &s_tim3,
            TIM3,
            MOTOR_FAILURE_STAGE_HW_INIT_TIM3_PWM,
            MOTOR_FAILURE_STAGE_HW_INIT_TIM3_CHANNEL))
    {
        MotorHwReal_DisableImmediate();
        return false;
    }
    MotorHwReal_DisableImmediate();

    if (!MotorHwReal_PwmConfigurationIsValid())
    {
        s_last_failure_stage =
            MOTOR_FAILURE_STAGE_HW_INIT_CONFIG_VERIFY;
        MotorHwReal_DisableImmediate();
        return false;
    }
    if (!MotorHwReal_IsDisabled())
    {
        s_last_failure_stage =
            MOTOR_FAILURE_STAGE_HW_INIT_DISABLED_VERIFY;
        MotorHwReal_DisableImmediate();
        return false;
    }
    s_initialized = true;
    return true;
}

static bool MotorHwReal_FailApply(MotorFailureStage stage)
{
    s_last_failure_stage = stage;
    MotorHwReal_DisableImmediate();
    return false;
}

static bool MotorHwReal_ZeroCompareCommitIsValid(void)
{
    return ((GPIOB->ODR & MOTOR_HW_SD_GPIO_PIN_MASK) == 0U) &&
           (TIM2->CCR3 == 0U) &&
           (TIM3->CCR3 == 0U) &&
           (TIM2->CNT == 0U) &&
           (TIM3->CNT == 0U) &&
           ((TIM2->CCER & TIM_CCER_CC3E) == 0U) &&
           ((TIM3->CCER & TIM_CCER_CC3E) == 0U) &&
           ((TIM2->CR1 & TIM_CR1_CEN) == 0U) &&
           ((TIM3->CR1 & TIM_CR1_CEN) == 0U);
}

static bool MotorHwReal_ZeroCarriersAreRunning(void)
{
    return ((GPIOB->ODR & MOTOR_HW_SD_GPIO_PIN_MASK) == 0U) &&
           (TIM2->CCR3 == 0U) &&
           (TIM3->CCR3 == 0U) &&
           ((TIM2->CCER & TIM_CCER_CC3E) != 0U) &&
           ((TIM3->CCER & TIM_CCER_CC3E) != 0U) &&
           ((TIM2->CR1 & TIM_CR1_CEN) != 0U) &&
           ((TIM3->CR1 & TIM_CR1_CEN) != 0U);
}

static bool MotorHwReal_ActiveCompareIsValid(bool press,
                                              uint16_t duty_counts)
{
    uint32_t expected_tim2 = press ? 0U : duty_counts;
    uint32_t expected_tim3 = press ? duty_counts : 0U;

    return ((GPIOB->ODR & MOTOR_HW_SD_GPIO_PIN_MASK) ==
            MOTOR_HW_SD_GPIO_PIN_MASK) &&
           ((TIM2->CCER & TIM_CCER_CC3E) != 0U) &&
           ((TIM3->CCER & TIM_CCER_CC3E) != 0U) &&
           ((TIM2->CR1 & TIM_CR1_CEN) != 0U) &&
           ((TIM3->CR1 & TIM_CR1_CEN) != 0U) &&
           (TIM2->CCR3 == expected_tim2) &&
           (TIM3->CCR3 == expected_tim3) &&
           !((TIM2->CCR3 != 0U) && (TIM3->CCR3 != 0U));
}

static bool MotorHwReal_Apply(bool press, uint16_t duty_counts)
{
    HAL_StatusTypeDef tim2_stop_result;
    HAL_StatusTypeDef tim3_stop_result;

    s_last_failure_stage = MOTOR_FAILURE_STAGE_NONE;
    MotorHwReal_DisableImmediate();
    if (!s_initialized)
    {
        return MotorHwReal_FailApply(
            MOTOR_FAILURE_STAGE_HW_APPLY_NOT_INITIALIZED);
    }
    if ((duty_counts == 0U) ||
        (duty_counts > MOTOR_HW_PWM_PERIOD_COUNTS))
    {
        return MotorHwReal_FailApply(
            MOTOR_FAILURE_STAGE_HW_APPLY_INVALID_DUTY);
    }

    if (!MotorHwReal_OutputArmingAllowed())
    {
        return MotorHwReal_FailApply(
            MOTOR_FAILURE_STAGE_HW_APPLY_ARMING_DENIED);
    }

    /* Immediate shutdown does not update HAL channel state.  Stop in main
       context before the next HAL start so both private handles are ready. */
    tim2_stop_result = HAL_TIM_PWM_Stop(&s_tim2, TIM_CHANNEL_3);
    tim3_stop_result = HAL_TIM_PWM_Stop(&s_tim3, TIM_CHANNEL_3);
    if ((tim2_stop_result != HAL_OK) ||
        (tim3_stop_result != HAL_OK))
    {
        return MotorHwReal_FailApply(
            (tim2_stop_result != HAL_OK) ?
                MOTOR_FAILURE_STAGE_HW_APPLY_STOP_TIM2 :
                MOTOR_FAILURE_STAGE_HW_APPLY_STOP_TIM3);
    }

    __HAL_TIM_SET_COMPARE(&s_tim2, TIM_CHANNEL_3, 0U);
    __HAL_TIM_SET_COMPARE(&s_tim3, TIM_CHANNEL_3, 0U);
    TIM2->CNT = 0U;
    TIM3->CNT = 0U;
    TIM2->EGR = TIM_EGR_UG;
    TIM3->EGR = TIM_EGR_UG;
    TIM2->SR = 0U;
    TIM3->SR = 0U;
    __DSB();
    if (!MotorHwReal_ZeroCompareCommitIsValid())
    {
        return MotorHwReal_FailApply(
            MOTOR_FAILURE_STAGE_HW_APPLY_ZERO_COMPARE_COMMIT);
    }

    if (HAL_TIM_PWM_Start(&s_tim2, TIM_CHANNEL_3) != HAL_OK)
    {
        return MotorHwReal_FailApply(
            MOTOR_FAILURE_STAGE_HW_APPLY_START_TIM2_ZERO_CARRIER);
    }
    if (HAL_TIM_PWM_Start(&s_tim3, TIM_CHANNEL_3) != HAL_OK)
    {
        return MotorHwReal_FailApply(
            MOTOR_FAILURE_STAGE_HW_APPLY_START_TIM3_ZERO_CARRIER);
    }
    __DSB();
    if (!MotorHwReal_ZeroCarriersAreRunning())
    {
        return MotorHwReal_FailApply(
            MOTOR_FAILURE_STAGE_HW_APPLY_ZERO_CARRIER_VERIFY);
    }

    TIM2->CNT = 0U;
    TIM3->CNT = 0U;
    TIM2->SR = ~((uint32_t)TIM_SR_UIF);
    TIM3->SR = ~((uint32_t)TIM_SR_UIF);
    HAL_GPIO_WritePin(GPIOB,
                      MOTOR_HW_SD_GPIO_PIN_MASK,
                      GPIO_PIN_SET);
    __DSB();
    if ((GPIOB->ODR & MOTOR_HW_SD_GPIO_PIN_MASK) !=
        MOTOR_HW_SD_GPIO_PIN_MASK)
    {
        return MotorHwReal_FailApply(
            MOTOR_FAILURE_STAGE_HW_APPLY_DRIVER_ENABLE_VERIFY);
    }

    if (press)
    {
        __HAL_TIM_SET_COMPARE(&s_tim2, TIM_CHANNEL_3, 0U);
        __HAL_TIM_SET_COMPARE(&s_tim3, TIM_CHANNEL_3, duty_counts);
    }
    else
    {
        __HAL_TIM_SET_COMPARE(&s_tim2, TIM_CHANNEL_3, duty_counts);
        __HAL_TIM_SET_COMPARE(&s_tim3, TIM_CHANNEL_3, 0U);
    }
    __DSB();
    if (!MotorHwReal_ActiveCompareIsValid(press, duty_counts))
    {
        return MotorHwReal_FailApply(
            MOTOR_FAILURE_STAGE_HW_APPLY_ACTIVE_COMPARE_VERIFY);
    }
    return true;
}

bool MotorHwReal_ApplyPress(uint16_t duty_counts)
{
    return MotorHwReal_Apply(true, duty_counts);
}

bool MotorHwReal_ApplyRelease(uint16_t duty_counts)
{
    return MotorHwReal_Apply(false, duty_counts);
}

MotorFailureStage MotorHwReal_GetLastFailureStage(void)
{
    return s_last_failure_stage;
}
