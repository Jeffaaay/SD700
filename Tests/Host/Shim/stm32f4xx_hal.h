#ifndef TESTS_HOST_SHIM_STM32F4XX_HAL_H
#define TESTS_HOST_SHIM_STM32F4XX_HAL_H

#include <stdint.h>

#include "stm32f4xx.h"

typedef enum
{
    HAL_OK = 0,
    HAL_ERROR = 1
} HAL_StatusTypeDef;

typedef struct
{
    uint32_t Prescaler;
    uint32_t CounterMode;
    uint32_t Period;
    uint32_t ClockDivision;
    uint32_t AutoReloadPreload;
} TIM_Base_InitTypeDef;

typedef struct
{
    TIM_TypeDef *Instance;
    TIM_Base_InitTypeDef Init;
} TIM_HandleTypeDef;

typedef struct
{
    uint32_t OCMode;
    uint32_t Pulse;
    uint32_t OCPolarity;
    uint32_t OCFastMode;
} TIM_OC_InitTypeDef;

typedef enum
{
    GPIO_PIN_RESET = 0,
    GPIO_PIN_SET
} GPIO_PinState;

#define TIM_COUNTERMODE_UP                 0U
#define TIM_CLOCKDIVISION_DIV1             0U
#define TIM_AUTORELOAD_PRELOAD_DISABLE     0U
#define TIM_OCMODE_PWM1                    (6UL << TIM_CCMR2_OC3M_Pos)
#define TIM_OCPOLARITY_HIGH                0U
#define TIM_OCFAST_DISABLE                 0U
#define TIM_CHANNEL_3                      8U

void FakeStm32Hal_SetCompare(TIM_HandleTypeDef *timer,
                            uint32_t compare);

#define __HAL_TIM_SET_COMPARE(handle, channel, compare) \
    do { \
        (void)(channel); \
        FakeStm32Hal_SetCompare((handle), (compare)); \
    } while (0)

uint32_t HAL_RCC_GetPCLK1Freq(void);
HAL_StatusTypeDef HAL_TIM_PWM_Init(TIM_HandleTypeDef *timer);
HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(
    TIM_HandleTypeDef *timer,
    const TIM_OC_InitTypeDef *channel,
    uint32_t selected_channel);
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *timer,
                                    uint32_t channel);
HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef *timer,
                                   uint32_t channel);
void HAL_GPIO_WritePin(GPIO_TypeDef *port,
                       uint32_t pins,
                       GPIO_PinState state);
void HAL_NVIC_SetPriority(int irq, uint32_t priority, uint32_t subpriority);
void HAL_NVIC_EnableIRQ(int irq);
void HAL_NVIC_DisableIRQ(int irq);
void HAL_NVIC_ClearPendingIRQ(int irq);

#endif
