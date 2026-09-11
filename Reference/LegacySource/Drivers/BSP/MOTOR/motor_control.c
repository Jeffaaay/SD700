/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    tim.c
  * @brief   This file provides code for the configuration
  *          of the TIM instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "./BSP/MOTOR/motor_control.h"
TIM_HandleTypeDef g_tim2;     /* 定时器x句柄 */
TIM_HandleTypeDef g_tim3;     /* 定时器x句柄 */

void TIM2_pwm(uint16_t arr, uint16_t psc);
void TIM3_pwm(uint16_t arr, uint16_t psc);
/**
 * @brief       通用定时器TIMX 通道Y PWM输出 初始化函数（使用PWM模式1）
 * @note
 *              通用定时器的时钟来自APB1,当PPRE1 ≥ 2分频的时候
 *              通用定时器的时钟为APB1时钟的2倍, 而APB1为42M, 所以定时器时钟 = 84Mhz
 *              定时器溢出时间计算方法: Tout = ((arr + 1) * (psc + 1)) / Ft us.
 *              Ft = 定时器工作频率,单位:Mhz
 *
 * @param       arr: 自动重装值
 * @param       psc: 预分频系数
 * @retval      无
 */
 
 
 void motor_init(uint16_t arr, uint16_t psc)
 {
    GPIO_InitTypeDef gpio_init_struct;
    
    SD1_GPIO_CLK_ENABLE();                                  /* DIR1时钟使能 */
    SD2_GPIO_CLK_ENABLE();                                  /* DIR2时钟使能 */
    
    gpio_init_struct.Pin = SD1_GPIO_PIN;                    /* DIR1引脚 */
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;             /* 推挽输出 */
    gpio_init_struct.Pull = GPIO_PULLUP;                     /* 上拉 */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;           /* 高速 */
    HAL_GPIO_Init(SD1_GPIO_PORT, &gpio_init_struct);        /* 初始化DIR1引脚 */
    
    gpio_init_struct.Pin = SD2_GPIO_PIN;                    /* DIR2引脚 */
    gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;             /* 推挽输出 */
    gpio_init_struct.Pull = GPIO_PULLUP;                     /* 上拉 */
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;           /* 高速 */
    HAL_GPIO_Init(SD2_GPIO_PORT, &gpio_init_struct);        /* 初始化DIR2引脚 */
     
    TIM2_pwm(arr,psc);
    TIM3_pwm(arr,psc);
     
    SD1(0);
    SD2(0);
 }
 
 
 
//配置控制信号（pulse、dir、ena）
void TIM3_pwm(uint16_t arr, uint16_t psc)
{
    TIM_OC_InitTypeDef tim3_oc_pwm_chy = {0};                /* 定时器输出句柄 */
    // tim3
    g_tim3.Instance = GTIM_TIM3_PWM;                 /* 定时器x */
    g_tim3.Init.Prescaler = psc;                     /* 预分频系数 */
    g_tim3.Init.CounterMode = TIM_COUNTERMODE_UP;    /* 递增计数模式 */
    g_tim3.Init.Period = arr;                        /* 自动重装载值 */
    HAL_TIM_PWM_Init(&g_tim3);                       /* 初始化PWM */

    tim3_oc_pwm_chy.OCMode = TIM_OCMODE_PWM1;                       /* 模式选择PWM1 */
    tim3_oc_pwm_chy.Pulse = 0;              //占空比=pulse/arr      /* 设置比较值,此值用来确定占空比 */

    tim3_oc_pwm_chy.OCPolarity = TIM_OCPOLARITY_HIGH;                                        /* 输出比较极性为高 */
    tim3_oc_pwm_chy.OCFastMode = TIM_OCFAST_DISABLE;  //计数值与ccr相等时是否立即响应，为保证周期完整一般不开
    HAL_TIM_PWM_ConfigChannel(&g_tim3, &tim3_oc_pwm_chy, GTIM_TIM3_PWM_CH3); /* 配置TIMx通道1 */
    HAL_TIM_PWM_Start(&g_tim3, GTIM_TIM3_PWM_CH3);                           /* 开启对应PWM通道 */
    
}



//配置控制信号（pulse、dir、ena）
void TIM2_pwm(uint16_t arr, uint16_t psc)
{
    TIM_OC_InitTypeDef tim2_oc_pwm_chy = {0};                /* 定时器输出句柄 */

    // tim2
    g_tim2.Instance = GTIM_TIM2_PWM;                 /* 定时器x */
    g_tim2.Init.Prescaler = psc;                     /* 预分频系数 */
    g_tim2.Init.CounterMode = TIM_COUNTERMODE_UP;    /* 递增计数模式 */
    g_tim2.Init.Period = arr;                        /* 自动重装载值 */
    HAL_TIM_PWM_Init(&g_tim2);                       /* 初始化PWM */

    tim2_oc_pwm_chy.OCMode = TIM_OCMODE_PWM1;                       /* 模式选择PWM1 */
    tim2_oc_pwm_chy.Pulse = 0;              //占空比=pulse/arr      /* 设置比较值,此值用来确定占空比 */

    tim2_oc_pwm_chy.OCPolarity = TIM_OCPOLARITY_HIGH;                                        /* 输出比较极性为高 */
    tim2_oc_pwm_chy.OCFastMode = TIM_OCFAST_DISABLE;  //计数值与ccr相等时是否立即响应，为保证周期完整一般不开
    HAL_TIM_PWM_ConfigChannel(&g_tim2, &tim2_oc_pwm_chy, GTIM_TIM2_PWM_CH3); /* 配置TIMx通道1 */
    HAL_TIM_PWM_Start(&g_tim2, GTIM_TIM2_PWM_CH3);                           /* 开启对应PWM通道 */

}

/**
 * @brief       定时器底层驱动，时钟使能，引脚配置
                此函数会被HAL_TIM_PWM_Init()调用
 * @param       htim:定时器句柄
 * @retval      无
 */

//底层io口初始化
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == GTIM_TIM2_PWM)
    {
        GPIO_InitTypeDef gpio_init_struct;
        GTIM_TIM2_PWM_CH3_GPIO_CLK_ENABLE();                            /* 开启通道y的CPIO时钟 */
        GTIM_TIM2_PWM_CHY_CLK_ENABLE();                                 /* 使能定时器时钟 */ //tim3

        gpio_init_struct.Pin = GTIM_TIM2_PWM_CH3_GPIO_PIN;              /* 通道y的CPIO口 */
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;                        /* 复用推完输出 */
        gpio_init_struct.Pull = GPIO_NOPULL;                            /* 上拉 */
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;                  /* 高速 */
        gpio_init_struct.Alternate = GTIM_TIM2_PWM_CH3_GPIO_AF;         /* IO口REMAP设置, 是否必要查看头文件配置的说明! */
        HAL_GPIO_Init(GTIM_TIM2_PWM_CH3_GPIO_PORT, &gpio_init_struct);
        
    }
    
    if(htim->Instance == GTIM_TIM3_PWM)
    {
        GPIO_InitTypeDef gpio_init_struct;
        GTIM_TIM3_PWM_CH3_GPIO_CLK_ENABLE();                            /* 开启通道y的CPIO时钟 */
        GTIM_TIM3_PWM_CHY_CLK_ENABLE();                                 /* 使能定时器时钟 */ //tim3

        gpio_init_struct.Pin = GTIM_TIM3_PWM_CH3_GPIO_PIN;              /* 通道y的CPIO口 */
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;                        /* 复用推完输出 */
        gpio_init_struct.Pull = GPIO_NOPULL;                            /* 上拉 */
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;                  /* 高速 */
        gpio_init_struct.Alternate = GTIM_TIM3_PWM_CH3_GPIO_AF;         /* IO口REMAP设置, 是否必要查看头文件配置的说明! */
        HAL_GPIO_Init(GTIM_TIM3_PWM_CH3_GPIO_PORT, &gpio_init_struct);
    }
}

//设置TIM3通道1的占空比
//compare:比较值
void TIM_SetTIM3Compare3(uint32_t compare)
{
    GTIM_TIM3_PWM_CH3_CCR3 = compare; 
}

//设置TIM3通道4的占空比
//compare:比较值
void TIM_SetTIM2Compare3(uint32_t compare)
{
    GTIM_TIM2_PWM_CH3_CCR3 = compare; 
}

// 修改预分频器值来改变频率
void Change_PWM_Frequency(uint32_t prescaler) {
    // 停止定时器
    HAL_TIM_PWM_Stop(&g_tim3, GTIM_TIM3_PWM_CH3);
    HAL_TIM_PWM_Stop(&g_tim2, GTIM_TIM2_PWM_CH3);
    
    // 修改预分频器
    __HAL_TIM_SET_PRESCALER(&g_tim3, prescaler);
    __HAL_TIM_SET_PRESCALER(&g_tim2, prescaler);
    // 重新启动定时器
    HAL_TIM_PWM_Start(&g_tim3, GTIM_TIM3_PWM_CH3);
    HAL_TIM_PWM_Start(&g_tim2, GTIM_TIM2_PWM_CH3);

}

