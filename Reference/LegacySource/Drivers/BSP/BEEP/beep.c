
#include "./BSP/BEEP/beep.h"


void beep_init(void) {
    GPIO_InitTypeDef gpio_init_struct = {0};
    
    BEEP_GPIO_CLK_ENABLE();
    
    gpio_init_struct.Pin   = BEEP_GPIO_PIN;
    gpio_init_struct.Mode  = GPIO_MODE_OUTPUT_PP; // 推挽输出
    gpio_init_struct.Pull  = GPIO_NOPULL;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_LOW; // 蜂鸣器不需要高速
    
    HAL_GPIO_Init(BEEP_GPIO_PORT, &gpio_init_struct);
    beep_stop(); // 初始化后关闭蜂鸣器
}

void beep_start(void) {
    HAL_GPIO_WritePin(BEEP_GPIO_PORT, BEEP_GPIO_PIN, GPIO_PIN_RESET);
}
void beep_stop(void) {
    HAL_GPIO_WritePin(BEEP_GPIO_PORT, BEEP_GPIO_PIN, GPIO_PIN_SET);
}

// //pwm驱动蜂鸣器，用于调节音量（占空比）和音调（频率）
// TIM_HandleTypeDef g_tim4_pwm_ch4_handle;     /* 定时器x句柄 */

// /**
//  * @brief       BEEP相关IO口,通用定时器TIMX 通道Y PWM输出 初始化函数（使用PWM模式1）
//  * @note
//  *              通用定时器的时钟来自APB1,当PPRE1 ≥ 2分频的时候
//  *              通用定时器的时钟为APB1时钟的2倍, 而APB1为42M, 所以定时器时钟 = 84Mhz
//  *              定时器溢出时间计算方法: Tout = ((arr + 1) * (psc + 1)) / Ft us.
//  *              Ft = 定时器工作频率,单位:Mhz
//  *
//  * @param       arr: 自动重装值
//  * @param       psc: 预分频系数
//  * @retval      无
//  */
// void beep_init(void)
// {
//     TIM_OC_InitTypeDef timx_oc_pwm_chy = {0};                       /* 定时器输出句柄 */
    
//     g_tim4_pwm_ch4_handle.Instance = GTIM_TIMX_PWM;                 /* 定时器x */
//     g_tim4_pwm_ch4_handle.Init.Prescaler = 96 - 1;                     /* 预分频系数 */
//     g_tim4_pwm_ch4_handle.Init.CounterMode = TIM_COUNTERMODE_UP;    /* 递增计数模式 */
//     g_tim4_pwm_ch4_handle.Init.Period = 1000 - 1;                        /* 自动重装载值 */
//     HAL_TIM_PWM_Init(&g_tim4_pwm_ch4_handle);                       /* 初始化PWM */

//     GPIO_InitTypeDef gpio_init_struct;
//     GTIM_TIMX_PWM_CHY_GPIO_CLK_ENABLE();                            /* 开启通道y的CPIO时钟 */
//     GTIM_TIMX_PWM_CHY_CLK_ENABLE();                                 /* 使能定时器时钟 */

//     gpio_init_struct.Pin = GTIM_TIMX_PWM_CHY_GPIO_PIN;              /* 通道y的CPIO口 */
//     gpio_init_struct.Mode = GPIO_MODE_AF_PP;                        /* 复用推完输出 */
//     gpio_init_struct.Pull = GPIO_PULLUP;                            /* 上拉 */
//     gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;                  /* 高速 */
//     gpio_init_struct.Alternate = GTIM_TIMX_PWM_CHY_GPIO_AF;         /* IO口REMAP设置 */
//     HAL_GPIO_Init(GTIM_TIMX_PWM_CHY_GPIO_PORT, &gpio_init_struct);
    
//     timx_oc_pwm_chy.OCMode = TIM_OCMODE_PWM1;                       /* 模式选择PWM1 */
//     timx_oc_pwm_chy.Pulse = 999 / 2;                                /* 设置比较值,此值用来确定占空比 */

//     timx_oc_pwm_chy.OCPolarity = TIM_OCPOLARITY_LOW;                                        /* 输出比较极性为低 */
//     HAL_TIM_PWM_ConfigChannel(&g_tim4_pwm_ch4_handle, &timx_oc_pwm_chy, GTIM_TIMX_PWM_CHY); /* 配置TIMx通道y */
//     HAL_TIM_PWM_Stop(&g_tim4_pwm_ch4_handle, GTIM_TIMX_PWM_CHY);    /* 先不开启对应PWM通道 */
// }
///**
// * @brief       定时器底层驱动，时钟使能，引脚配置
//                此函数会被HAL_TIM_PWM_Init()调用
// * @param       htim:定时器句柄
// * @retval      无
// */
//void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
//{
//    if (htim->Instance == GTIM_TIMX_PWM)
//    {
//        GPIO_InitTypeDef gpio_init_struct;
//        GTIM_TIMX_PWM_CHY_GPIO_CLK_ENABLE();                            /* 开启通道y的CPIO时钟 */
//        GTIM_TIMX_PWM_CHY_CLK_ENABLE();                                 /* 使能定时器时钟 */

//        gpio_init_struct.Pin = GTIM_TIMX_PWM_CHY_GPIO_PIN;              /* 通道y的CPIO口 */
//        gpio_init_struct.Mode = GPIO_MODE_AF_PP;                        /* 复用推完输出 */
//        gpio_init_struct.Pull = GPIO_PULLUP;                            /* 上拉 */
//        gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;                  /* 高速 */
//        gpio_init_struct.Alternate = GTIM_TIMX_PWM_CHY_GPIO_AF;         /* IO口REMAP设置 */
//        HAL_GPIO_Init(GTIM_TIMX_PWM_CHY_GPIO_PORT, &gpio_init_struct);
//    }
//}

// void beep_start(void)
// {
//     HAL_TIM_PWM_Start(&g_tim4_pwm_ch4_handle, GTIM_TIMX_PWM_CHY);                           /* 开启对应PWM通道 */
// }

// void beep_stop(void)
// {
//     HAL_TIM_PWM_Stop(&g_tim4_pwm_ch4_handle, GTIM_TIMX_PWM_CHY);                           /* 关闭对应PWM通道 */
// }


