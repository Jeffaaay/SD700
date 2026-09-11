
#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/LED/led.h"
#include "./BSP/EXTI/exti.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/SHOW/show.h"

#include "freertos_demo.h"

#include "FreeRTOS.h"
#include "semphr.h"

Key_State_t key_state = KEY_NONE;

//外部中断初始化
void extix_init(void)
{
    GPIO_InitTypeDef GPIO_Initure;
    
    // 使能按键时钟
    KEY1_GPIO_CLK_ENABLE();
    KEY2_GPIO_CLK_ENABLE();
    KEY3_GPIO_CLK_ENABLE();
    KEY4_GPIO_CLK_ENABLE();
    
    /*Configure GPIO pins : PA15 */
    GPIO_Initure.Pin=KEY1_PIN;  
    GPIO_Initure.Mode=GPIO_MODE_INPUT;    
    GPIO_Initure.Pull=GPIO_PULLDOWN;
    HAL_GPIO_Init(KEY1_PORT,&GPIO_Initure);

    GPIO_Initure.Pin=KEY2_PIN;
    HAL_GPIO_Init(KEY2_PORT,&GPIO_Initure);

    GPIO_Initure.Pin=KEY3_PIN;
    HAL_GPIO_Init(KEY3_PORT,&GPIO_Initure);

    GPIO_Initure.Pin=KEY4_PIN;
    HAL_GPIO_Init(KEY4_PORT,&GPIO_Initure);

}



/**
 * @brief       按键扫描函数
 * @note        该函数有响应优先级(同时按下多个按键)
 * @param       mode:0 / 1, 具体含义如下:
 *   @arg       0,  不支持连续按(当按键按下不放时, 只有第一次调用会返回键值,
 *                  必须松开以后, 再次按下才会返回其他键值)
 *   @arg       1,  支持连续按(当按键按下不放时, 每次调用该函数都会返回键值)
 * @retval      
 */
void key_scan(uint8_t mode)
{
    static uint32_t last_scan_time = 0;
    uint32_t current_time = HAL_GetTick();
    static uint32_t press_start = 0;  /* 按下开始时间 */
    static uint8_t key_pressed = 0;   /* 有按键被按下 */



    if (current_time - last_scan_time < SCAN_INTERVAL_MS) return; // 每10ms扫描一次
    last_scan_time = current_time;



    static uint8_t key_up = 1;  /* 按键按松开标志 */
    if (mode) key_up = 1;       /* 支持连按 */

    if (key_up && (KEY1 == 1|| KEY2 == 1 || KEY3 == 1 || KEY4 == 1 ))  /* 按键松开标志为1, 且有任意一个按键按下了 */
    {
        delay_ms(20);           /* 去抖动 */
        key_up = 0;

        if(KEY1 == 1)                               /*返回、切换界面*/
        {
            key_pressed = 1;
            press_start = current_time;
        }
        else if(KEY2 == 1)                         /*上升、改大小*/
        {
            key_pressed = 2;
            press_start = current_time;
        }
        else if (KEY3 == 1)                         /*下降、改数位*/
        {
            key_pressed = 3;  /* 标记按键3可能长按 */
            press_start = current_time;
        } 
        else if (KEY4 == 1)                         /*设置键，修改键*/
        {
            key_pressed = 4;  /* 标记按键4可能长按 */
            press_start = current_time;
        }
    }
    else if (!key_up && (KEY1 == 1|| KEY2 == 1 || KEY3 == 1 || KEY4 == 1 ))  /* 有按键按下，且仍处于按下状态 */
    {
        uint32_t press_duration = current_time - press_start;
        switch (key_pressed)
        {
            case 1:
                if (press_duration >= LONG_PRESS_THRESHOLD_1000MS && KEY1 == 1) // 1s
                {
                    if(key_state == KEY_NONE) key_state = KEY_BACK_LONG;
                    key_pressed = 0;  /* 处理完成 */
                    LED1_TOGGLE();
                }
                break;
            
            case 2:
                if (press_duration >= LONG_PRESS_THRESHOLD_1000MS && KEY2 == 1)
                {
                    if(key_state == KEY_NONE) key_state = KEY_UP_LONG;//因为长按要一直抬升，所以上键和下键长按一直保持判断
                    LED1_TOGGLE();
                }
                break;
            case 3:
                if (press_duration >= LONG_PRESS_THRESHOLD_1000MS && KEY3 == 1)
                {
                    if(key_state == KEY_NONE) key_state = KEY_DOWN_LONG;
                    LED1_TOGGLE();
                }
                break;
            
             case 4:
                 if (press_duration >= LONG_PRESS_THRESHOLD_1000MS && KEY4 == 1)
                 {
                     if(key_state == KEY_NONE) key_state = KEY_SET_LONG;
                     key_pressed = 0;  /* 处理完成 */
                     LED1_TOGGLE();
                 }
                 break;
            default:
                break;
        }

    }
    else if (KEY1 == 0 && KEY2 == 0 && KEY3 == 0 && KEY4 == 0)         /* 没有长按，则进入短按逻辑 */
    {
        uint32_t press_duration = current_time - press_start;
        key_up = 1;
        switch (key_pressed)
        {
            case 1:/* KEY1短按 */
                if (press_duration < LONG_PRESS_THRESHOLD_1000MS)
                {
                    if(key_state == KEY_NONE) key_state = KEY_BACK_SHORT;
                    LED3_TOGGLE();
                }
                break;
            
            case 2:/* KEY2短按 */
                if (press_duration < LONG_PRESS_THRESHOLD_1000MS)
                {
                    if(key_state == KEY_NONE) key_state = KEY_UP_SHORT;
                    LED3_TOGGLE();
                }
                break;

            case 3:/* KEY3短按 */
                if (press_duration < LONG_PRESS_THRESHOLD_1000MS)
                {
                    if(key_state == KEY_NONE) key_state = KEY_DOWN_SHORT;
                    LED3_TOGGLE();
                }
                break;

             case 4:/* KEY4短按 */
                 if (press_duration < LONG_PRESS_THRESHOLD_1000MS)
                 {
                     if(key_state == KEY_NONE) key_state = KEY_SET_SHORT;
                     LED3_TOGGLE();
                 }
                 break;

            default:
                break;
        }

        key_pressed = 0;  /* 清除按键按下标志 */

    }
    
}
