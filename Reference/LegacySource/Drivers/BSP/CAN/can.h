
#ifndef __CAN_H
#define __CAN_H

#include "./SYSTEM/sys/sys.h"


/******************************************************************************************/
/* CAN 引脚 定义 */

#define CAN_RX_GPIO_PORT                GPIOA
#define CAN_RX_GPIO_PIN                 GPIO_PIN_11
#define CAN_RX_GPIO_CLK_ENABLE()        do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)   /* PA口时钟使能 */

#define CAN_TX_GPIO_PORT                GPIOA
#define CAN_TX_GPIO_PIN                 GPIO_PIN_12
#define CAN_TX_GPIO_CLK_ENABLE()        do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)   /* PA口时钟使能 */

/******************************************************************************************/

/* CAN接收RX0中断使能 */
#define CAN_RX0_INT_ENABLE      1               /* 0,不使能; 1,使能; */

extern uint8_t CAN_rxbuf[8];


/* 对外接口函数 */
uint8_t can_receive_msg(uint32_t id, uint8_t *buf);             /* CAN接收数据, 查询 */
uint8_t can_send_msg(uint32_t id, uint8_t *msg, uint8_t len);   /* CAN发送数据 */
uint8_t can_init(uint32_t tsjw,uint32_t tbs2,uint32_t tbs1,uint16_t brp,uint32_t mode); /* CAN初始化 */

#endif

















