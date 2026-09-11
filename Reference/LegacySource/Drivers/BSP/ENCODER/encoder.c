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
#include "./BSP/ENCODER/encoder.h"
TIM_HandleTypeDef g_timx_encoder_handle;     /* 定时器x句柄 */
volatile int32_t tim4_encoder_overflow = 0;  /* 溢出计数 */
//static uint32_t encoder_last_count = 0;      /* 上次计数值（用于速度计算） */
uint32_t circle_num = 0;

int32_t total_count = 0;                    /*编码器总计数值*/
int32_t last_count = 0;                     /* 上次计数值 */
uint32_t last_time = 0;                     /* 上次更新时间 */
int32_t speed_rpm = 0;                      /* 转速 */ 


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
void timx_encoder_init(void)
{
    TIM_Encoder_InitTypeDef encoder_config = {0};

    g_timx_encoder_handle.Instance = ENCODER_TIMX;                 /* 定时器x */
    g_timx_encoder_handle.Init.Prescaler = 1 - 1;                     /* 预分频系数 */
    g_timx_encoder_handle.Init.CounterMode = TIM_COUNTERMODE_UP;    /* 递增计数模式 */
    g_timx_encoder_handle.Init.Period = 65535 ;                        /* 自动重装载值 */
    g_timx_encoder_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1; /* 采样频率，抗干扰 */
    g_timx_encoder_handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;


    //HAL_TIM_Base_Init(&g_timx_encoder_handle);

    encoder_config.EncoderMode = TIM_ENCODERMODE_TI12;               /* TI1和TI2边沿都计数 */

    encoder_config.IC1Filter = 0xF;                /*输入捕获过滤*/
    encoder_config.IC1Polarity = TIM_INPUTCHANNELPOLARITY_RISING;       /*上升沿捕获*/
    encoder_config.IC1Prescaler = TIM_ICPSC_DIV1;                   /*不分频*/
    encoder_config.IC1Selection = TIM_ICSELECTION_DIRECTTI;       

    encoder_config.IC2Filter = 0xF;                /*输入捕获过滤*/
    encoder_config.IC2Polarity = TIM_INPUTCHANNELPOLARITY_RISING;       /*上升沿捕获*/
    encoder_config.IC2Prescaler = TIM_ICPSC_DIV1;                   /*不分频*/
    encoder_config.IC2Selection = TIM_ICSELECTION_DIRECTTI;         

//    g_timx_pwm_chy_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
//    g_timx_pwm_chy_handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;


        /* 初始化 */
    HAL_TIM_Encoder_Init(&g_timx_encoder_handle, &encoder_config);

    HAL_TIM_Encoder_Start(&g_timx_encoder_handle,TIM_CHANNEL_ALL);
    HAL_NVIC_SetPriority(ENCODER_TIMX_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(ENCODER_TIMX_IRQn);
 
    last_time = HAL_GetTick();
        
}


/**
 * @brief       定时器底层驱动，时钟使能，引脚配置
                此函数会被HAL_TIM_PWM_Init()调用
 * @param       htim:定时器句柄
 * @retval      无
 */
void HAL_TIM_Encoder_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == ENCODER_TIMX)
    {
        GPIO_InitTypeDef gpio_init_struct = {0};
        ENCODER_TIMX_CLK_ENABLE();                          /* TIM4时钟使能 */
        ENCODER_A_GPIO_CLK_ENABLE();                 /* 使能A相 */
        ENCODER_B_GPIO_CLK_ENABLE();                 /* 使能B相 */
        ENCODER_Z_GPIO_CLK_ENABLE();                 /* 使能Z相 */

        gpio_init_struct.Pin = ENCODER_A_GPIO_PIN;                      /* A相引脚 */
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;                        /* 复用推完输出 */
        gpio_init_struct.Pull = GPIO_PULLUP;                            /* 上拉 */
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;                  /* 高速 */
        gpio_init_struct.Alternate = ENCODER_A_GPIO_AF;         /* IO口REMAP设置, 是否必要查看头文件配置的说明! */
        HAL_GPIO_Init(ENCODER_A_GPIO_PORT, &gpio_init_struct);

        gpio_init_struct.Pin = ENCODER_B_GPIO_PIN;              /* B相引脚 */
        gpio_init_struct.Alternate = ENCODER_B_GPIO_AF;         /* 复用 */
        HAL_GPIO_Init(ENCODER_B_GPIO_PORT, &gpio_init_struct);

        gpio_init_struct.Pin = ENCODER_Z_GPIO_PIN;              /* Z相引脚 */
        gpio_init_struct.Mode = GPIO_MODE_IT_FALLING;           /* 下降沿触发外部中断 */
        gpio_init_struct.Alternate = 0;                          /* 普通中断 */
        HAL_GPIO_Init(ENCODER_Z_GPIO_PORT, &gpio_init_struct);

    }
}

uint8_t Get_Encoder_Direction(void)
{
    return (__HAL_TIM_IS_TIM_COUNTING_DOWN(&g_timx_encoder_handle)) ? 0 : 1;
}

uint16_t Get_Encoder_Count(void)
{
    return __HAL_TIM_GET_COUNTER(&g_timx_encoder_handle);
}


void EXTI15_10_IRQHandler(void)
{
    if(__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_14) != RESET)  /* PD14中断 */
    {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_14);  /* 清除中断标志 */
        
        // 处理Z相信号
        tim4_encoder_overflow++;  /* 溢出计数增加 */
        total_count += 65535;     /* 加上溢出值 */
        circle_num++;

    }
}
