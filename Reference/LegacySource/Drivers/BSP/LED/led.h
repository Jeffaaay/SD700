
#ifndef __LED_H
#define __LED_H

#include "./SYSTEM/sys/sys.h"

/******************************************************************************************/
/* 引脚 定义 */

#define LED1_GPIO_PORT                  GPIOC
#define LED1_GPIO_PIN                   GPIO_PIN_10
#define LED1_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)             /* PC口时钟使能 */

#define LED2_GPIO_PORT                  GPIOC
#define LED2_GPIO_PIN                   GPIO_PIN_11
#define LED2_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)             /* PC口时钟使能 */

#define LED3_GPIO_PORT                  GPIOC
#define LED3_GPIO_PIN                   GPIO_PIN_12
#define LED3_GPIO_CLK_ENABLE()          do{ __HAL_RCC_GPIOC_CLK_ENABLE(); }while(0)             /* PB口时钟使能 */

/******************************************************************************************/

/* LED端口定义 */
#define LED1(x)   do{ x ? \
                      HAL_GPIO_WritePin(LED1_GPIO_PORT, LED1_GPIO_PIN, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(LED1_GPIO_PORT, LED1_GPIO_PIN, GPIO_PIN_RESET); \
                  }while(0)       /* LED0 = RED */

#define LED2(x)   do{ x ? \
                      HAL_GPIO_WritePin(LED2_GPIO_PORT, LED2_GPIO_PIN, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(LED2_GPIO_PORT, LED2_GPIO_PIN, GPIO_PIN_RESET); \
                  }while(0)       /* LED1 = GREEN */

#define LED3(x)   do{ x ? \
                      HAL_GPIO_WritePin(LED3_GPIO_PORT, LED3_GPIO_PIN, GPIO_PIN_SET) : \
                      HAL_GPIO_WritePin(LED3_GPIO_PORT, LED3_GPIO_PIN, GPIO_PIN_RESET); \
                  }while(0)       /* LED1 = GREEN */

                  
/* LED取反定义 */
#define LED1_TOGGLE()    do{ HAL_GPIO_TogglePin(LED1_GPIO_PORT, LED1_GPIO_PIN); }while(0)       /* LED0 = !LED0 */
#define LED2_TOGGLE()    do{ HAL_GPIO_TogglePin(LED2_GPIO_PORT, LED2_GPIO_PIN); }while(0)       /* LED1 = !LED1 */
#define LED3_TOGGLE()    do{ HAL_GPIO_TogglePin(LED3_GPIO_PORT, LED3_GPIO_PIN); }while(0)       /* LED1 = !LED1 */

/******************************************************************************************/
/* 外部接口函数*/
void led_init(void);                                                                            /* 初始化 */

#endif
