
#ifndef __ENCODER_H
#define __ENCODER_H

#include "./SYSTEM/sys/sys.h"




/* TIMX PWM输出定义 
 * 这里输出的PWM控制LED0(RED)的亮度
 * 默认是针对TIM2~TIM5
 * 注意: 通过修改这几个宏定义,可以支持TIM1~TIM8任意一个定时器,任意一个IO口输出PWM
 */
#define ENCODER_A_GPIO_PORT         GPIOD
#define ENCODER_A_GPIO_PIN          GPIO_PIN_12
#define ENCODER_A_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOD_CLK_ENABLE(); }while(0)  /* PB口时钟使能 */
#define ENCODER_A_GPIO_AF           GPIO_AF2_TIM4                               /* 端口复用到TIM4 */


#define ENCODER_B_GPIO_PORT         GPIOD
#define ENCODER_B_GPIO_PIN          GPIO_PIN_13
#define ENCODER_B_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOD_CLK_ENABLE(); }while(0)  /* PB口时钟使能 */
#define ENCODER_B_GPIO_AF           GPIO_AF2_TIM4  


#define ENCODER_Z_GPIO_PORT         GPIOD
#define ENCODER_Z_GPIO_PIN          GPIO_PIN_14
#define ENCODER_Z_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOD_CLK_ENABLE(); }while(0)  /* PB口时钟使能 */
//#define ENCODER_Z_GPIO_AF           GPIO_AF2_TIM4 

/* TIMX REMAP设置
 * 因为我们LED0接在PF9上, 必须通过开启TIM14的部分重映射功能, 才能将TIM14_CH1输出到PF9上
 */

#define ENCODER_TIMX                   TIM4                                        /* TIM4 */
#define ENCODER_TIMX_CLK_ENABLE()      do{ __HAL_RCC_TIM4_CLK_ENABLE(); }while(0)  /* TIM4 时钟使能 */
#define ENCODER_TIMX_IRQn              EXTI15_10_IRQn
#define ENCODER_TIMX_IRQHandler        EXTI15_10_IRQHandler
//#define ENCODER_TIMX_REMAP_ENABLE()    do{ __HAL_REMAPTIM4_ENABLE(); }while(0)                 /* TIM4完全重映射 */

void timx_encoder_init(void);    /* 通用定时器 PWM初始化函数 */
uint8_t Get_Encoder_Direction(void);
uint16_t Get_Encoder_Count(void);

#endif

