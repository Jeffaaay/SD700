#include "Board/Motor/motor_hw_force_disable.h"

#include "Board/Motor/motor_hw_map.h"

void MotorHw_ForceDisableImmediate(void)
{
    volatile uint32_t clock_enable_readback;

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    clock_enable_readback = RCC->AHB1ENR;
    (void)clock_enable_readback;

    GPIOB->BSRR = MOTOR_HW_SD_GPIO_PIN_MASK << 16U;
    GPIOB->PUPDR =
        (GPIOB->PUPDR & ~MOTOR_HW_SD_MODE_FIELD_MASK) |
        MOTOR_HW_SD_PULLDOWN_BITS;
    GPIOB->OTYPER &= ~MOTOR_HW_SD_GPIO_PIN_MASK;
    GPIOB->MODER =
        (GPIOB->MODER & ~MOTOR_HW_SD_MODE_FIELD_MASK) |
        MOTOR_HW_SD_OUTPUT_MODE_BITS;

    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN | RCC_APB1ENR_TIM3EN;
    clock_enable_readback = RCC->APB1ENR;
    (void)clock_enable_readback;

    TIM2->CCR3 = 0U;
    TIM3->CCR3 = 0U;
    TIM2->CCER &= ~TIM_CCER_CC3E;
    TIM3->CCER &= ~TIM_CCER_CC3E;
    __DSB();
}

void MotorHw_ForceDisable(void)
{
    MotorHw_ForceDisableImmediate();
}
