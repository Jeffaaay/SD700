
#ifndef __GMOTOR_CONTROL_H
#define __GMOTOR_CONTROL_H

#include "./SYSTEM/sys/sys.h"




/* TIMX PWM输出定义 
 * 这里输出的PWM控制DIR1(RED)的亮度
 * 默认是针对TIM2~TIM5
 * 注意: 通过修改这几个宏定义,可以支持TIM3~TIM8任意一个定时器,任意一个IO口输出PWM
 */
#define GTIM_TIM2_PWM_CH3_GPIO_PORT         GPIOB
#define GTIM_TIM2_PWM_CH3_GPIO_PIN          GPIO_PIN_10
#define GTIM_TIM2_PWM_CH3_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)  /* PA口时钟使能 */
#define GTIM_TIM2_PWM_CH3_GPIO_AF           GPIO_AF1_TIM2                               /* 端口复用到TIM2 */

#define GTIM_TIM3_PWM_CH3_GPIO_PORT         GPIOB
#define GTIM_TIM3_PWM_CH3_GPIO_PIN          GPIO_PIN_0
#define GTIM_TIM3_PWM_CH3_GPIO_CLK_ENABLE() do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)  /* PA口时钟使能 */
#define GTIM_TIM3_PWM_CH3_GPIO_AF           GPIO_AF2_TIM3                               /* 端口复用到TIM3 */

/* TIMX REMAP设置
 * 因为我们DIR1接在PF9上, 必须通过开启TIM34的部分重映射功能, 才能将TIM34_CH3输出到PF9上
 */

#define GTIM_TIM2_PWM                       TIM2                                        /* TIM2 */
#define GTIM_TIM2_PWM_CH3                   TIM_CHANNEL_3                               /* 通道3 */
#define GTIM_TIM2_PWM_CH3_CCR3              TIM2->CCR3                                  /* 通道1的输出比较寄存器 */
#define GTIM_TIM2_PWM_CHY_CLK_ENABLE()      do{ __HAL_RCC_TIM2_CLK_ENABLE(); }while(0)  /* TIM3 时钟使能 */

#define GTIM_TIM3_PWM                       TIM3                                        /* TIM3 */
#define GTIM_TIM3_PWM_CH3                   TIM_CHANNEL_3                               /* 通道3 */
#define GTIM_TIM3_PWM_CH3_CCR3              TIM3->CCR3                                  /* 通道1的输出比较寄存器 */
#define GTIM_TIM3_PWM_CHY_CLK_ENABLE()      do{ __HAL_RCC_TIM3_CLK_ENABLE(); }while(0)  /* TIM3 时钟使能 */

/*********************************************************************************************************************/
/*端口定义 */
#define SD1_GPIO_PORT                  GPIOB
#define SD1_GPIO_PIN                   GPIO_PIN_1
#define SD1_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)             /* PC口时钟使能 */

#define SD2_GPIO_PORT                  GPIOB
#define SD2_GPIO_PIN                   GPIO_PIN_2
#define SD2_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOB_CLK_ENABLE(); }while(0)             /* PA口时钟使能 */

#define SD1(x)   do{ x ? \
                      HAL_GPIO_WritePin(SD1_GPIO_PORT, SD1_GPIO_PIN, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(SD1_GPIO_PORT, SD1_GPIO_PIN, GPIO_PIN_RESET); \
                  }while(0)       /* DIR1 = RED */
#define SD2(x)   do{ x ? \
                      HAL_GPIO_WritePin(SD2_GPIO_PORT, SD2_GPIO_PIN, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(SD2_GPIO_PORT, SD2_GPIO_PIN, GPIO_PIN_RESET); \
                  }while(0)

void motor_init(uint16_t arr, uint16_t psc);
void TIM_SetTIM3Compare3(uint32_t compare);   /* 设置通道1的比较值 */
void TIM_SetTIM2Compare3(uint32_t compare);   /* 设置通道4的比较值 */
void Change_PWM_Frequency(uint32_t prescaler);
                    
#endif
