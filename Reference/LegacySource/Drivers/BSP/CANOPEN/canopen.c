/**
 ****************************************************************************************************
 * @file        can.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.1
 * @date        2023-06-06
 * @brief       CAN 驱动代码
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 探索者 F407开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 * 修改说明
 * V1.0 20211025
 * 第一次发布
 * V1.1 20230606
 * 1, 优化can_send_msg函数, 新增发送超时处理机制
 ****************************************************************************************************
 */

#include "./BSP/CANOPEN/canopen.h"
#include "./BSP/CANOPEN/canopen_conf.h"
#include "./BSP/LED/led.h"
#include "./BSP/LCD/lcd.h"
#include "./SYSTEM/delay/delay.h"
#include "./SYSTEM/usart/usart.h"
#include "./BSP/LCD/lcd.h"

CAN_HandleTypeDef   g_canx_handler;     /* CANx句柄 */
CAN_TxHeaderTypeDef g_canx_txheader;    /* 发送参数句柄 */
CAN_RxHeaderTypeDef g_canx_rxheader;    /* 接收参数句柄 */

/**
 * @brief       CAN初始化
 * @param       tsjw    : 重新同步跳跃时间单元.范围: 1~3;
 * @param       tbs2    : 时间段2的时间单元.范围: 1~8;
 * @param       tbs1    : 时间段1的时间单元.范围: 1~16;
 * @param       brp     : 波特率分频器.范围: 1~1024;
 *   @note      以上4个参数, 在函数内部会减1, 所以, 任何一个参数都不能等于0
 *              CAN挂在APB1上面, 其输入时钟频率为 Fpclk1 = PCLK1 = 42Mhz
 *              tq     = brp * tpclk1;
 *              波特率 = Fpclk1 / ((tbs1 + tbs2 + 1) * brp);
 *              我们设置 can_init(1, 6, 7, 6, 1), 则CAN波特率为:
 *              42M / ((6 + 7 + 1) * 6) = 500Kbps
 *
 * @param       mode    : CAN_MODE_NORMAL,  普通模式;
                          CAN_MODE_LOOPBACK,回环模式;
 * @retval      0,  初始化成功; 其他, 初始化失败;
 */
uint8_t can_init(uint32_t tsjw, uint32_t tbs2, uint32_t tbs1, uint16_t brp, uint32_t mode)
{
    g_canx_handler.Instance = CAN1;
    g_canx_handler.Init.Prescaler = brp;                /* 分频系数(Fdiv)为brp+1 */
	
	  //正常模式：参与总线竞争；回环模式：自发自检
    g_canx_handler.Init.Mode = mode;                    /* 模式设置 */
	
	  // sjw，用于矫正积累误差导致的收发不同步问题
    g_canx_handler.Init.SyncJumpWidth = tsjw;           /* 重新同步跳跃宽度(Tsjw)为tsjw+1个时间单位 CAN_SJW_1TQ~CAN_SJW_4TQ */
	  // 采集时刻在tbs1和tbs2中间
    g_canx_handler.Init.TimeSeg1 = tbs1;                /* tbs1范围CAN_BS1_1TQ~CAN_BS1_16TQ */
    g_canx_handler.Init.TimeSeg2 = tbs2;                /* tbs2范围CAN_BS2_1TQ~CAN_BS2_8TQ */
	  
    g_canx_handler.Init.TimeTriggeredMode = DISABLE;    /* 非时间触发通信模式 */
	
	  // 错误太多导致总线关闭后，软件是否自动进行管理
    g_canx_handler.Init.AutoBusOff = DISABLE;           /* 软件自动离线管理 */
	
	  // 睡眠模式下是否自动唤醒
    g_canx_handler.Init.AutoWakeUp = DISABLE;           /* 睡眠模式通过软件唤醒(清除CAN->MCR的SLEEP位) */
	
	  // 报文出错后自动重传
    g_canx_handler.Init.AutoRetransmission = ENABLE;    /* 禁止报文自动传送 */
	
	  // FIFO队列满时是否锁住数据不让覆盖
    g_canx_handler.Init.ReceiveFifoLocked = DISABLE;    /* 报文不锁定,新的覆盖旧的 */
		
		// 是否开启报文优先级仲裁决定发送顺序，不开启则使用邮件邮箱编号
    g_canx_handler.Init.TransmitFifoPriority = DISABLE; /* 优先级由报文标识符决定 */
    if (HAL_CAN_Init(&g_canx_handler) != HAL_OK)
    {
        return 1;
    }

		
		// FIFO0接收是否产生中断
#if CAN_RX0_INT_ENABLE

    /* 使用中断接收 */
    __HAL_CAN_ENABLE_IT(&g_canx_handler, CAN_IT_RX_FIFO0_MSG_PENDING); /* FIFO0消息挂号中断允许 */
    HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);                                 /* 使能CAN中断 */
    HAL_NVIC_SetPriority(CAN1_RX0_IRQn, 1, 0);                         /* 抢占优先级1，子优先级0 */
#endif

		
    CAN_FilterTypeDef sFilterConfig;

    /* 配置CAN过滤器 */
		// 过滤器编号
    sFilterConfig.FilterBank = 0;                             /* 过滤器0 */
		
		// 掩码模式：1表示该位要匹配，0表示不关系；列表模式：精确匹配
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
		
		// 32位模式，处理一个32位ID或2个16位ID
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
		
		// 期望接收的ID值
    sFilterConfig.FilterIdHigh = 0x0000;                      /* 32位ID */
    sFilterConfig.FilterIdLow = 0x0000;
		
		// 需要匹配哪几位
    sFilterConfig.FilterMaskIdHigh = 0x0000;                  /* 32位MASK */
    sFilterConfig.FilterMaskIdLow = 0x0000;
		
		// 送到FIFO0
    sFilterConfig.FilterFIFOAssignment = CAN_FILTER_FIFO0;    /* 过滤器0关联到FIFO0 */
		
		//使能
    sFilterConfig.FilterActivation = CAN_FILTER_ENABLE;       /* 激活滤波器0 */
    sFilterConfig.SlaveStartFilterBank = 14;

    /* 过滤器配置 */
    if (HAL_CAN_ConfigFilter(&g_canx_handler, &sFilterConfig) != HAL_OK)
    {
        return 2;
    }

    /* 启动CAN外围设备 */
    if (HAL_CAN_Start(&g_canx_handler) != HAL_OK)
    {
        return 3;
    }
    
    HAL_CAN_StateTypeDef can_state = HAL_CAN_GetState(&g_canx_handler);
    //printf("CAN State after Start: %d\r\n", can_state); 
    // 应该打印 5 (HAL_CAN_STATE_LISTENING)

    // 检查硬件是否真的退出了初始化模式
    // if (HAL_IS_BIT_SET(CAN1->MSR, CAN_MSR_INAK)) {
    //     printf("错误：CAN 仍然在初始化模式 (INAK=1)！\r\n");
    // } else {
    //     printf("正确：CAN 已进入正常工作模式。\r\n");
    // }


    return 0;
}

/**
 * @brief       CAN底层驱动，引脚配置，时钟配置，中断配置
                此函数会被HAL_CAN_Init()调用
 * @param       hcan:CAN句柄
 * @retval      无
 */
void HAL_CAN_MspInit(CAN_HandleTypeDef *hcan) //底层io口初始化
{
    if (CAN1 == hcan->Instance)
    {
        CAN_RX_GPIO_CLK_ENABLE();       /* CAN_RX脚时钟使能 */
        CAN_TX_GPIO_CLK_ENABLE();       /* CAN_TX脚时钟使能 */
        __HAL_RCC_CAN1_CLK_ENABLE();    /* 使能CAN1时钟 */

        GPIO_InitTypeDef gpio_init_struct;

        gpio_init_struct.Pin = CAN_TX_GPIO_PIN;
        gpio_init_struct.Mode = GPIO_MODE_AF_PP;
        gpio_init_struct.Pull = GPIO_PULLUP;
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio_init_struct.Alternate = GPIO_AF9_CAN1;
        HAL_GPIO_Init(CAN_TX_GPIO_PORT, &gpio_init_struct); /* CAN_TX脚 模式设置 */

        gpio_init_struct.Pin = CAN_RX_GPIO_PIN;
        HAL_GPIO_Init(CAN_RX_GPIO_PORT, &gpio_init_struct); /* CAN_RX脚 必须设置成输入模式 */
    }
}

#if CAN_RX0_INT_ENABLE /* 使能RX0中断 */

/**
 * @brief       CAN RX0 中断服务函数
 *   @note      处理CAN FIFO0的接收中断
 * @param       无
 * @retval      无
 */
void CAN1_RX0_IRQHandler(void)
{
    uint8_t rxbuf[8];
    uint32_t id;
    can_receive_msg(id, rxbuf);

		
    printf("id:%d\r\n", g_canx_rxheader.StdId); // ID
    printf("ide:%d\r\n", g_canx_rxheader.IDE);	// 标准/扩展 
    printf("rtr:%d\r\n", g_canx_rxheader.RTR);	// 数据帧/遥控帧
    printf("len:%d\r\n", g_canx_rxheader.DLC);	// 数据位字节数

    printf("rxbuf[0]:%d\r\n", rxbuf[0]);
    printf("rxbuf[1]:%d\r\n", rxbuf[1]);
    printf("rxbuf[2]:%d\r\n", rxbuf[2]);
    printf("rxbuf[3]:%d\r\n", rxbuf[3]);
    printf("rxbuf[4]:%d\r\n", rxbuf[4]);
    printf("rxbuf[5]:%d\r\n", rxbuf[5]);
    printf("rxbuf[6]:%d\r\n", rxbuf[6]);
    printf("rxbuf[7]:%d\r\n", rxbuf[7]);
}

#endif

/**
 * @brief       CAN 发送一组数据
 *   @note      发送格式固定为: 标准ID, 数据帧
 * @param       id      : 标准ID(11位)
 * @param       msg     : 数据指针
 * @param       len     : 数据长度
 * @retval      发送状态 0, 成功; 1, 失败;
 */
uint8_t can_send_msg(uint32_t id, uint8_t *msg, uint8_t len)
{
    uint16_t t = 0;
    uint32_t TxMailbox = CAN_TX_MAILBOX0;	//使用邮箱0
    
    if(id > 0x7FF) return 1;

    g_canx_txheader.StdId = id;         /* 标准标识符 */
    g_canx_txheader.ExtId = 0;         /* 扩展标识符不使用 */
    g_canx_txheader.IDE = CAN_ID_STD;   /* 使用标准帧 */
    g_canx_txheader.RTR = CAN_RTR_DATA; /* 数据帧 */
    g_canx_txheader.DLC = len;

		// 添加消息至邮箱
    if (HAL_CAN_AddTxMessage(&g_canx_handler, &g_canx_txheader, msg, &TxMailbox) != HAL_OK) /* 发送消息 */
    {
        return 1;
    }
    
		// 3个邮箱，所以等待计数值小于等于3
    while (HAL_CAN_GetTxMailboxesFreeLevel(&g_canx_handler) != 3)   /* 等待发送完成,所有邮箱为空 */
    {
        t++;
//        uint32_t tsr = CAN1->TSR;
//        uint32_t esr = CAN1->ESR;
        // 检查邮箱0的发送完成标志
//        if (tsr & (1 << 1)) { // TXOK0
//            ;
//        } else {
//            ;
//        }
        
        if (t > 0xFFF)
        {
            uint32_t error = HAL_CAN_GetError(&g_canx_handler);
            
          // 检查错误状态
//            if (esr & (1 << 16)) { // EWGF
//                printf("Error warning\n");
//            }
//            if (esr & (1 << 17)) { // EPVF
//                printf("Error passive\n");
//            }
//            if (esr & (1 << 18)) { // BOFF
//                printf("Bus off\n");
//            }
            
            HAL_CAN_AbortTxRequest(&g_canx_handler, TxMailbox);     /* 超时，直接中止邮箱的发送请求 */
            return 1;
        }
    }
    
    return 0;
}

// uint8_t can_send_msg(uint32_t id, uint8_t *msg, uint8_t len)
// {
//     uint32_t TxMailbox;
//     char buf[20];
//     static uint32_t send_counter = 0;
//     static uint32_t fail_counter = 0;
    
//     /* 显示发送计数 - 第6行 (约90像素) */
//     send_counter++;
//     sprintf(buf, "TX:%lu F:%lu", send_counter, fail_counter);
//     lcd_show_string(50, 70, 80, 16, 12, buf, GHOST_WHITE, MIDNIGHT_BLUE);
    
//     /* 读取TSR寄存器 */
//     uint32_t tsr = CAN1->TSR;
//     uint32_t tme = (tsr >> 24) & 0x07;  // 空闲邮箱数
    
//     /* 显示TSR - 第7行 (约105像素) */
//     sprintf(buf, "TSR:%08lX", tsr);
//     lcd_show_string(0, 85, 100, 16, 12, buf, YELLOW, MIDNIGHT_BLUE);
    
    
//     /* 调用HAL库发送函数 */
//     if (HAL_CAN_AddTxMessage(&g_canx_handler, &g_canx_txheader, msg, &TxMailbox) != HAL_OK)
//     {
//         fail_counter++;
//         lcd_show_string(0, 95,80, 16, 12, "Add Fail", RED, MIDNIGHT_BLUE);
//         return 1;
//     }
    
//     /* 等待发送完成 */
//     uint16_t timeout = 0;
//     while (HAL_CAN_GetTxMailboxesFreeLevel(&g_canx_handler) != 3)
//     {
//         timeout++;
//         if (timeout > 0xFFF)
//         {
//             fail_counter++;
//             lcd_show_string(0, 100, 80, 16, 12, "Timeout!", RED, MIDNIGHT_BLUE);
//             HAL_CAN_AbortTxRequest(&g_canx_handler, TxMailbox);
//             return 1;
//         }
//         delay_us(10);
//     }
//     uint32_t esr = CAN1->ESR;
//     uint8_t lec = (esr >> 4) & 0x07;
//     if(lec == 3) {  // ACK错误
//         lcd_show_string(0, 100, 120, 16, 12, "ACK ERR!", RED, MIDNIGHT_BLUE);
//     }
//     lcd_show_string(0, 120, 80, 16, 12, "OK      ", FOREST_GREEN, MIDNIGHT_BLUE);
//     return 0;
// }


/**
 * @brief       CAN 接收数据查询
 *   @note      接收数据格式固定为: 标准ID, 数据帧
 * @param       id      : 要查询的 标准ID(11位)
 * @param       buf     : 数据缓存区
 * @retval      接收结果
 *   @arg       0   , 无数据被接收到;
 *   @arg       其他, 接收的数据长度
 */
uint8_t can_receive_msg(uint16_t *id, uint8_t *buf)
{
	  // 检查FIFO0中是否有数据
    if (HAL_CAN_GetRxFifoFillLevel(&g_canx_handler, CAN_RX_FIFO0) == 0)     /* 没有接收到数据 */
    {
        return 0; // 无数据
    }

    if (HAL_CAN_GetRxMessage(&g_canx_handler, CAN_RX_FIFO0, &g_canx_rxheader, buf) != HAL_OK)  /* 读取数据 */
    {
        return 0;  
    }

	// 报文不符合格式要求
    if (g_canx_rxheader.IDE != CAN_ID_STD || g_canx_rxheader.RTR != CAN_RTR_DATA)       /* 接收到的ID不对 / 不是标准帧 / 不是数据帧 */
    {
        return 0;    
    }

		// 返回接收到的数据长度
    *id = g_canx_rxheader.StdId;
    return g_canx_rxheader.DLC;

}

// void can_debug_lcd_small(void)
// {
//     char buf[20];
//     static uint32_t last_update = 0;
    
//     if(HAL_GetTick() - last_update < 300) return;
//     last_update = HAL_GetTick();
    
//     /* 清空调试区域 */
//     lcd_fill(0, 5, 159, 127, MIDNIGHT_BLUE);
    
//     /* 第1行：状态 */
//     HAL_CAN_StateTypeDef state = HAL_CAN_GetState(&g_canx_handler);
//     uint32_t error = HAL_CAN_GetError(&g_canx_handler);
//     sprintf(buf, "S:%d E:%ld", state, error);
//     lcd_show_string(0, 5, 80, 16, 12, buf, 
//                     (state == 2) ? FOREST_GREEN : YELLOW, MIDNIGHT_BLUE);
    
//     /* 第2行：ESR */
//     uint32_t esr = CAN1->ESR;
//     sprintf(buf, "ESR:%08lX", esr);
//     lcd_show_string(0, 20, 100, 16, 12, buf, GHOST_WHITE, MIDNIGHT_BLUE);
    
//     /* 第3行：LEC错误码 */
//     uint8_t lec = (esr >> 4) & 0x07;
//     sprintf(buf, "LEC:%d", lec);
//     lcd_show_string(0, 35, 50, 16, 12, buf, 
//                     (lec == 5) ? RED : GHOST_WHITE, MIDNIGHT_BLUE);
    
//     /* 第4行：引脚电平 */
//     uint8_t tx = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_12);
//     uint8_t rx = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_11);
//     sprintf(buf, "TX:%s RX:%s", tx ? "H" : "L", rx ? "H" : "L");
//     lcd_show_string(0, 50, 100, 16, 12, buf, GHOST_WHITE, MIDNIGHT_BLUE);
    
//     /* 第5行：PA12配置 - ? 关键！ */
//     uint32_t moder = (GPIOA->MODER >> 24) & 0x03;
//     uint32_t afr = (GPIOA->AFR[1] >> 16) & 0x0F;
//     sprintf(buf, "M:%ld A:%ld", moder, afr);
//     lcd_show_string(0, 65, 80, 16, 12, buf, YELLOW, MIDNIGHT_BLUE);
    
//     /* 第6行：MSR寄存器 */
//     uint32_t msr = CAN1->MSR;
//     sprintf(buf, "MSR:%08lX", msr);
//     lcd_show_string(0, 80, 100, 16, 12, buf, CYAN, MIDNIGHT_BLUE);
    
//     /* 第7行：CAN模式 */
//     if(msr & (1 << 1)) {
//         lcd_show_string(0, 95, 100, 16, 12, "INIT MODE!", RED, MIDNIGHT_BLUE);
//     } else {
//         lcd_show_string(0, 95, 100, 16, 12, "NORMAL MODE", FOREST_GREEN, MIDNIGHT_BLUE);
//     }
    
//     /* 第8行：TSR */
//     uint32_t tsr = CAN1->TSR;
//     sprintf(buf, "TSR:%08lX", tsr);
//     lcd_show_string(0, 110, 120, 16, 12, buf, GHOST_WHITE, MIDNIGHT_BLUE);
// }