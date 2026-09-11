#ifndef BOARD_MOTOR_MOTOR_HW_MAP_H
#define BOARD_MOTOR_MOTOR_HW_MAP_H

#include "Board/Motor/motor_plan_config.h"
#include "stm32f4xx.h"

#define MOTOR_HW_PWM_PERIOD_COUNTS        MOTOR_PLAN_PWM_PERIOD_COUNTS
#define MOTOR_HW_PWM_PRESCALER            0U
#define MOTOR_HW_PWM_FREQUENCY_HZ         20000U
#define MOTOR_HW_SUPPLY_REFERENCE_MV      MOTOR_PLAN_SUPPLY_REFERENCE_MV

#define MOTOR_HW_TIM2_INSTANCE            TIM2
#define MOTOR_HW_TIM2_CHANNEL             8U
#define MOTOR_HW_TIM2_GPIO_PORT           GPIOB
#define MOTOR_HW_TIM2_GPIO_PIN            (1UL << 10U)
#define MOTOR_HW_TIM2_GPIO_AF             1U

#define MOTOR_HW_TIM3_INSTANCE            TIM3
#define MOTOR_HW_TIM3_CHANNEL             8U
#define MOTOR_HW_TIM3_GPIO_PORT           GPIOB
#define MOTOR_HW_TIM3_GPIO_PIN            (1UL << 0U)
#define MOTOR_HW_TIM3_GPIO_AF             2U

#define MOTOR_HW_SD1_GPIO_PORT            GPIOB
#define MOTOR_HW_SD1_GPIO_PIN_NUMBER      1U
#define MOTOR_HW_SD1_GPIO_PIN             (1UL << MOTOR_HW_SD1_GPIO_PIN_NUMBER)
#define MOTOR_HW_SD2_GPIO_PORT            GPIOB
#define MOTOR_HW_SD2_GPIO_PIN_NUMBER      2U
#define MOTOR_HW_SD2_GPIO_PIN             (1UL << MOTOR_HW_SD2_GPIO_PIN_NUMBER)
#define MOTOR_HW_SD_GPIO_PIN_MASK         (MOTOR_HW_SD1_GPIO_PIN | MOTOR_HW_SD2_GPIO_PIN)
#define MOTOR_HW_SHUTDOWN_DISABLED_LEVEL  0U

#define MOTOR_HW_SD_MODE_FIELD_MASK \
    ((3UL << (MOTOR_HW_SD1_GPIO_PIN_NUMBER * 2U)) | \
     (3UL << (MOTOR_HW_SD2_GPIO_PIN_NUMBER * 2U)))
#define MOTOR_HW_SD_OUTPUT_MODE_BITS \
    ((1UL << (MOTOR_HW_SD1_GPIO_PIN_NUMBER * 2U)) | \
     (1UL << (MOTOR_HW_SD2_GPIO_PIN_NUMBER * 2U)))
#define MOTOR_HW_SD_PULLDOWN_BITS \
    ((2UL << (MOTOR_HW_SD1_GPIO_PIN_NUMBER * 2U)) | \
     (2UL << (MOTOR_HW_SD2_GPIO_PIN_NUMBER * 2U)))

typedef enum
{
    MOTOR_HW_LEGACY_POSITIVE_PRESS_DOWN = 1,
    MOTOR_HW_LEGACY_NEGATIVE_RELEASE_UP = -1
} MotorHwLegacyDirection;

/*
 * Legacy positive/press/down: TIM2_CH3 compare 0, TIM3_CH3 compare duty.
 * Legacy negative/release/up: TIM2_CH3 compare duty, TIM3_CH3 compare 0.
 * This is evidence only; Phase 1 deliberately exposes no motion API.
 */

#endif
