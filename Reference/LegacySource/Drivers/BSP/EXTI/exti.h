#ifndef __EXTI_H
#define __EXTI_H

#include "./SYSTEM/sys/sys.h"

/******************************************************************************************/
/* KEY1引脚定义 */
#define KEY1_PORT    GPIOC
#define KEY1_PIN     GPIO_PIN_9
#define KEY1_GPIO_CLK_ENABLE()    do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)

/* KEY2引脚定义 */
#define KEY2_PORT    GPIOA
#define KEY2_PIN     GPIO_PIN_8
#define KEY2_GPIO_CLK_ENABLE()    do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)

/* KEY3引脚定义 */
#define KEY3_PORT    GPIOA
#define KEY3_PIN     GPIO_PIN_9
#define KEY3_GPIO_CLK_ENABLE()    do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)

/* KEY4引脚定义 */
#define KEY4_PORT    GPIOA
#define KEY4_PIN     GPIO_PIN_10
#define KEY4_GPIO_CLK_ENABLE()    do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)

#define LONG_PRESS_THRESHOLD_1000MS    1000  /* 长按阈值，单位：毫秒 */
#define SCAN_INTERVAL_MS           10    /* 扫描间隔，单位：毫秒 */

#define KEY1        HAL_GPIO_ReadPin(KEY1_PORT, KEY1_PIN)     /* KEY1状态 */
#define KEY2        HAL_GPIO_ReadPin(KEY2_PORT, KEY2_PIN)     /* KEY2状态 */
#define KEY3        HAL_GPIO_ReadPin(KEY3_PORT, KEY3_PIN)     /* KEY3状态 */
#define KEY4        HAL_GPIO_ReadPin(KEY4_PORT, KEY4_PIN)     /* KEY4状态 */

typedef enum {
    KEY_NONE = 0,
    
    /* KEY1（返回键）短按和长按 */
    KEY_BACK_SHORT,    /* 短按：返回/取消 */
    KEY_BACK_LONG,     /* 长按：切换界面 */
    
    /* KEY2（上键）短按和长按 */
    KEY_UP_SHORT,      /* 短按：增加数值/选择上一项 */
    KEY_UP_LONG,       /* 长按：快速增加/特殊功能 */
    
    /* KEY3（下键）短按和长按 */
    KEY_DOWN_SHORT,    /* 短按：减少数值/选择下一项 */
    KEY_DOWN_LONG,     /* 长按：快速减少/特殊功能 */
    
    /* KEY4（确认键）短按和长按 */
    KEY_SET_SHORT,     /* 短按：确认/进入 */
    KEY_SET_LONG,      /* 长按：保存/设置 */
} Key_State_t;

extern Key_State_t key_state;  /* 当前按键状态 */

void extix_init(void);  /* 外部中断初始化 */
void key_scan(uint8_t mode);

#endif
