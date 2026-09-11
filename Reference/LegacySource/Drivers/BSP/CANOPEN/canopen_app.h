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

#ifndef __CANOPEN_APP_H
#define __CANOPEN_APP_H

#include "./SYSTEM/sys/sys.h"

void canopen_send_nmt(uint8_t node_id, uint8_t command);
uint8_t configure_drive_pdo(uint8_t node_id);
uint8_t servo_enable(uint8_t node_id);
uint8_t send_torque_pdo(uint8_t node_id, float torque_percent);
uint8_t canopen_sdo_read(uint8_t node_id, uint16_t index, uint8_t subindex, uint32_t *data);
uint8_t wait_for_bootup(uint8_t node_id, uint32_t timeout);
uint8_t servo_disable(uint8_t node_id);

#endif

