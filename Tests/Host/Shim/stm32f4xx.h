#ifndef TESTS_HOST_SHIM_STM32F4XX_H
#define TESTS_HOST_SHIM_STM32F4XX_H

#include <stdint.h>

typedef struct
{
    volatile uint32_t CR1;
    volatile uint32_t CR2;
    volatile uint32_t SMCR;
    volatile uint32_t DIER;
    volatile uint32_t SR;
    volatile uint32_t EGR;
    volatile uint32_t CCMR1;
    volatile uint32_t CCMR2;
    volatile uint32_t CCER;
    volatile uint32_t CNT;
    volatile uint32_t PSC;
    volatile uint32_t ARR;
    volatile uint32_t RCR;
    volatile uint32_t CCR1;
    volatile uint32_t CCR2;
    volatile uint32_t CCR3;
    volatile uint32_t CCR4;
} TIM_TypeDef;

typedef struct
{
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
    volatile uint32_t BSRR;
    volatile uint32_t LCKR;
    volatile uint32_t AFR[2];
} GPIO_TypeDef;

typedef struct
{
    volatile uint32_t CR;
    volatile uint32_t PLLCFGR;
    volatile uint32_t CFGR;
    volatile uint32_t CIR;
    volatile uint32_t AHB1RSTR;
    volatile uint32_t AHB2RSTR;
    volatile uint32_t reserved0[2];
    volatile uint32_t APB1RSTR;
    volatile uint32_t APB2RSTR;
    volatile uint32_t reserved1[2];
    volatile uint32_t AHB1ENR;
    volatile uint32_t AHB2ENR;
    volatile uint32_t reserved2[2];
    volatile uint32_t APB1ENR;
} RCC_TypeDef;

extern TIM_TypeDef g_fake_tim2;
extern TIM_TypeDef g_fake_tim3;
extern TIM_TypeDef g_fake_tim5;
extern GPIO_TypeDef g_fake_gpiob;
extern RCC_TypeDef g_fake_rcc;

#define TIM2 (&g_fake_tim2)
#define TIM3 (&g_fake_tim3)
#define TIM5 (&g_fake_tim5)
#define GPIOB (&g_fake_gpiob)
#define RCC (&g_fake_rcc)

#define RCC_AHB1ENR_GPIOBEN      (1UL << 1U)
#define RCC_APB1ENR_TIM2EN       (1UL << 0U)
#define RCC_APB1ENR_TIM3EN       (1UL << 1U)
#define RCC_APB1ENR_TIM5EN       (1UL << 3U)
#define RCC_CFGR_PPRE1           (7UL << 10U)
#define RCC_CFGR_PPRE1_DIV1      0U

#define TIM_CR1_CEN              (1UL << 0U)
#define TIM_CR1_URS              (1UL << 2U)
#define TIM_CR1_OPM              (1UL << 3U)
#define TIM_DIER_UIE             (1UL << 0U)
#define TIM_DIER_CC1IE           (1UL << 1U)
#define TIM_SR_UIF               (1UL << 0U)
#define TIM_SR_CC1IF             (1UL << 1U)
#define TIM_SR_CC1OF             (1UL << 9U)
#define TIM_EGR_UG               (1UL << 0U)
#define TIM_CCER_CC3E            (1UL << 8U)
#define TIM_CCER_CC3P            (1UL << 9U)
#define TIM_CCMR2_OC3FE          (1UL << 2U)
#define TIM_CCMR2_OC3M_Pos       4U
#define TIM_CCMR2_OC3M           (7UL << TIM_CCMR2_OC3M_Pos)
#define TIM_CCMR2_OC3PE          (1UL << 3U)

#define TIM5_IRQn                50

#define __DSB()                  ((void)0)

#endif
