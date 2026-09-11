#include "Board/Motor/motor_hw_init_disabled.h"

#include "Board/Motor/motor_hw_force_disable.h"
#include "Board/Motor/motor_hw_map.h"
#include "stm32f4xx_hal.h"

static void MotorHw_ConfigureShutdownPins(void)
{
    GPIO_InitTypeDef gpio = {0};

    GPIOB->BSRR = MOTOR_HW_SD_GPIO_PIN_MASK << 16U;

    gpio.Pin = MOTOR_HW_SD_GPIO_PIN_MASK;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);
}

static void MotorHw_ConfigurePwmPin(uint32_t pin, uint32_t alternate)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = pin;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = alternate;
    HAL_GPIO_Init(GPIOB, &gpio);
}

static void MotorHw_ConfigurePwmTimer(TIM_TypeDef *instance)
{
    TIM_HandleTypeDef timer = {0};
    TIM_OC_InitTypeDef channel = {0};

    timer.Instance = instance;
    timer.Init.Prescaler = MOTOR_HW_PWM_PRESCALER;
    timer.Init.CounterMode = TIM_COUNTERMODE_UP;
    timer.Init.Period = MOTOR_HW_PWM_PERIOD_COUNTS;
    timer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    channel.OCMode = TIM_OCMODE_PWM1;
    channel.Pulse = 0U;
    channel.OCPolarity = TIM_OCPOLARITY_HIGH;
    channel.OCFastMode = TIM_OCFAST_DISABLE;

    (void)HAL_TIM_PWM_Init(&timer);
    (void)HAL_TIM_PWM_ConfigChannel(&timer, &channel, TIM_CHANNEL_3);
}

void MotorHw_InitDisabled(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();

    MotorHw_ForceDisableImmediate();
    MotorHw_ConfigureShutdownPins();
    MotorHw_ConfigurePwmPin(MOTOR_HW_TIM2_GPIO_PIN, MOTOR_HW_TIM2_GPIO_AF);
    MotorHw_ConfigurePwmPin(MOTOR_HW_TIM3_GPIO_PIN, MOTOR_HW_TIM3_GPIO_AF);
    MotorHw_ConfigurePwmTimer(MOTOR_HW_TIM2_INSTANCE);
    MotorHw_ConfigurePwmTimer(MOTOR_HW_TIM3_INSTANCE);
    MotorHw_ForceDisable();
}
