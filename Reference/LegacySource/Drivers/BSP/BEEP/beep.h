
#ifndef __BEEP_H
#define __BEEP_H

#include "./SYSTEM/sys/sys.h"


/*********************************以下是通用定时器PWM输出相关宏定义*************************************/

// /* TIMX PWM输出定义 
//  * 默认是针对TIM2~TIM5
//  * 注意: 通过修改这几个宏定义,可以支持TIM1~TIM8任意一个定时器,任意一个IO口输出PWM
//  */
// #define GTIM_TIMX_PWM_CHY_GPIO_PORT         GPIOC
// #define GTIM_TIMX_PWM_CHY_GPIO_PIN          GPIO_PIN_6
// #define GTIM_TIMX_PWM_CHY_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)  /* PF口时钟使能 */
// #define GTIM_TIMX_PWM_CHY_GPIO_AF           GPIO_AF2_TIM3                               /* 端口复用到TIM4 */

// /* TIMX REMAP设置
//  * 因为我们LED0接在PF9上, 必须通过开启TIM14的部分重映射功能, 才能将TIM14_CH1输出到PF9上
//  */

// #define GTIM_TIMX_PWM                       TIM3                                        /* TIM14 */
// #define GTIM_TIMX_PWM_CHY                   TIM_CHANNEL_1                                /* 通道Y,  1<= Y <=4 */
// #define GTIM_TIMX_PWM_CHY_CCRX              TIM3->CCR1                                  /* 通道Y的输出比较寄存器 */
// #define GTIM_TIMX_PWM_CHY_CLK_ENABLE()      do{ __HAL_RCC_TIM3_CLK_ENABLE(); }while(0)  /* TIM14 时钟使能 */

/****************************************************************************************************/

/* BEEP GPIO定义 - 普通IO输出模式 */
#define BEEP_GPIO_PORT           GPIOC
#define BEEP_GPIO_PIN            GPIO_PIN_0
#define BEEP_GPIO_CLK_ENABLE()   do{__HAL_RCC_GPIOC_CLK_ENABLE();}while(0)


void beep_init(void);   /* 初始化蜂鸣器 */
void beep_start(void);
void beep_stop(void);

#endif

















