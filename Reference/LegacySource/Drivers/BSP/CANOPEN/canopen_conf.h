/**
 ****************************************************************************************************
 * @file        can.h
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

#ifndef __CANOPEN_CONF_H
#define __CANOPEN_CONF_H

#include "./SYSTEM/sys/sys.h"


/******************************************************************************************/
/* CANopen网络节点ID定义 */

#define CO_MASTER_NODE_ID         0x01    /* 主站节点ID */

/* 从站节点ID范围 */
#define CO_SLAVE_MIN_NODE_ID      0x01
#define CO_SLAVE_MAX_NODE_ID      0x7F

/* 具体设备节点ID分配 */
#define NODE_PRESSURE_SENSOR      0x0A    /* 压力传感器 (ID=10) */
#define NODE_SERVO_DRIVE          0x14    /* 伺服驱动器 (ID=20) */




/******************************************************************************************/ 
 /* 数据类型定义 */

/* CANopen帧结构 */
// typedef struct {
//     uint16_t cob_id;                    /* COB-ID (11位) */
//     uint8_t data[8];                    /* 数据 */
//     uint8_t dlc;                        /* 数据长度 (0-8) */
//     uint32_t timestamp;                 /* 时间戳 (ms) */
// } canopen_frame_t;


/* CanOpen命令字定义 */

#define CANOPEN_CMD_WRITE_1BYTE     0x2F  /* 写入1字节 */
#define CANOPEN_CMD_WRITE_2BYTE     0x2B  /* 写入2字节 */
#define CANOPEN_CMD_WRITE_4BYTE     0x23  /* 写入4字节 */
#define CANOPEN_CMD_READ            0x40  /* 读取 */
#define CANOPEN_CMD_READ_RESP_1B    0x4F  /* 读取响应(1字节) */
#define CANOPEN_CMD_READ_RESP_2B    0x4B  /* 读取响应(2字节) */
#define CANOPEN_CMD_READ_RESP_4B    0x43  /* 读取响应(4字节) */


/* 波特率配置 */
#define CANOPEN_BAUD_20K    0  /* 20Kbps */
#define CANOPEN_BAUD_50K    1  /* 50Kbps */
#define CANOPEN_BAUD_100K   2  /* 100Kbps */
#define CANOPEN_BAUD_125K   3  /* 125Kbps */
#define CANOPEN_BAUD_250K   4  /* 250Kbps 默认值 */
#define CANOPEN_BAUD_500K   5  /* 500Kbps */
#define CANOPEN_BAUD_1M     6  /* 1Mbps */


/* 运行模式 */
#define CANOPEN_MODE_PP  1 /* 轮廓位置模式 */
#define CANOPEN_MODE_PV  3 /* 轮廓速度模式 */
#define CANOPEN_MODE_PT  4 /* 轮廓转矩模式 */
#define CANOPEN_MODE_HM  6 /* 回零模式 */
#define CANOPEN_MODE_IP  7 /* 插补模式 */


/* PDO传输类型 */
#define PDO_TRANS_SYNC_ACYCLIC  0    /* 同步非循环 */
#define PDO_TRANS_SYNC_CYCLIC   1    /* 同步循环(1-240) */
#define PDO_TRANS_ASYNC         0xFF /* 异步事件驱动 */


#endif

