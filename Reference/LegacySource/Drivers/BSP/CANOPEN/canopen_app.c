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
#include "./BSP/CANOPEN/canopen_app.h"
#include "./BSP/CANOPEN/canopen_conf.h"
#include "./SYSTEM/delay/delay.h"
#include "./SYSTEM/usart/usart.h"
#include "./BSP/LCD/lcd.h"

#include <string.h>


/**
 * @brief       发送NMT命令
 */
void canopen_send_nmt(uint8_t node_id, uint8_t command)
{
    uint8_t tx_data[2] = {command, node_id};
    /* NMT COB-ID固定为0x000 */
    can_send_msg(0x000, tx_data, 2);
}


/**
 * @brief      验证SDO是否正确接收
 * @param      node_id: 节点ID
 * @param      time_out: 超时时间
 * @retval     接收状态
 */
uint8_t sdo_verify(uint8_t node_id, uint8_t *tx_data, uint16_t time_out)
{
    uint32_t sdo_rx_id = 0x580 + node_id;
    uint32_t start_time = HAL_GetTick();
    uint16_t rx_id;
    uint8_t rx_data[8];
    uint8_t rx_len;

    while((HAL_GetTick() - start_time) < time_out)
    {
        if(can_receive_msg(&rx_id, rx_data))
        {
            if(rx_id != sdo_rx_id) continue;

            if(rx_data[0] == 0x60) // 成功响应
            {
                /* 验证索引和子索引完全匹配 */
                if(rx_data[1] == tx_data[1] &&  /* 索引低 */
                   rx_data[2] == tx_data[2] &&  /* 索引高 */
                   rx_data[3] == tx_data[3])    /* 子索引 */
                {
                    return 0;  /* 成功 */
                }
                /* 索引不匹配，继续等待 */
                continue;
            }

            // 错误响应
            else if(rx_data[0] == 0x80) return 2;
        }
    }

    return 1; //超时
}

uint8_t wait_for_bootup(uint8_t node_id, uint32_t timeout)
{
    uint32_t start = HAL_GetTick();
    uint16_t rx_id;
    uint8_t rx_data[8];
    uint32_t boot_id = 0x700 + node_id;
    
    while((HAL_GetTick() - start) < timeout)
    {
        if(can_receive_msg(&rx_id, rx_data))
        {
            // Boot-up: ID=0x700+node, 数据[0]=0x00
            if(rx_id == boot_id && rx_data[0] == 0x00)
            {
                return 0;
            }
        }
        delay_ms(1);
    }
    
    return 1;
}


/**
 * @brief       配置驱动器PDO模式
 * @param       node_id: 节点ID
 * @retval      配置状态
 */
uint8_t configure_drive_pdo(uint8_t node_id)
{
    uint8_t tx_data[8] = {0x00};
    uint32_t sdo_id = 0x600 + node_id;  /* SDO目标ID */
    uint32_t sdo_rx_id = 0x580 + node_id; /*SDO回应ID*/
    uint8_t result;
  /**
   * 功能码与组对象映射关系
   * 对象字典2000h～2006h，分别对应Pn0xx～Pn6xx的参数组，功能码后两位加1即为对应的对象字典子索引
   * 例：对功能码Pn299进行读写操作时，对应的对象字典为2002_9Ah。
   **
    
    */


    /* 1. 设置节点ID (如果需要) */

    // tx_data[0] = CANOPEN_CMD_WRITE_1BYTE;  /* 写1字节 */
    // tx_data[1] = 0x00;  /* 索引低: 2000h:81h */ 
    // tx_data[2] = 0x20;
    // tx_data[3] = 0x81;  /* 子索引 */
    // tx_data[4] = node_id; /* 数据: 节点ID */
    // can_send_msg(sdo_id, tx_data, 8);

    // result = sdo_verify(node_id, tx_data, 200);
    // if(result) return 1;

    // delay_ms(50);
    
    // /* 2. 设置指令源为CanOpen (Pn208=4) */
    // tx_data[1] = 0x02;  /* 2002h:09h */
    // tx_data[2] = 0x20;
    // tx_data[3] = 0x09;
    // tx_data[4] = 0x04;  /* 4=CanOpen */
    // can_send_msg(sdo_id, tx_data, 8);

    // result = sdo_verify(node_id, tx_data, 200);
    // if(result) return 1;

    // delay_ms(50);
    
    /* 3. 禁用RPDO1 (设置最高位为1) */
    tx_data[0] = CANOPEN_CMD_WRITE_4BYTE;  /* 写4字节 */
    tx_data[1] = 0x00;  /* 1400h:01h */
    tx_data[2] = 0x14;
    tx_data[3] = 0x01;  /* 子索引1 */
    /* 数据: 0x80000201 (最高位1=禁用) */
    tx_data[4] = 0x01;
    tx_data[5] = 0x02;
    tx_data[6] = 0x00;
    tx_data[7] = 0x80;  /* 最高位为1 */
    can_send_msg(sdo_id, tx_data, 8);

    result = sdo_verify(node_id, tx_data, 200);
    if(result) 
    {
        //lcd_show_string(0, 60, 120, 12, 12, "forbid rpdo fail!", RED);
        return 1;
    }

    delay_ms(10);
    

    memset(tx_data,0,8);
    /* 4. 设置RPDO1映射参数 */
    /* 4.1 映射对象个数=1 */
    tx_data[0] = CANOPEN_CMD_WRITE_1BYTE;  /* 写1字节 */
    tx_data[1] = 0x00;  /* 1600h:00h */
    tx_data[2] = 0x16;
    tx_data[3] = 0x00;
    tx_data[4] = 0x01;  /* 1个对象 */
    can_send_msg(sdo_id, tx_data, 8);

    result = sdo_verify(node_id, tx_data, 200);
    if(result) 
    {
        //lcd_show_string(0, 72, 120, 12, 12, "number set Fail!", RED);
        return 1;
    }

    delay_ms(10);
    
    /* 4.2 映射对象: 6071h (转矩指令), 16位 */
    tx_data[0] = CANOPEN_CMD_WRITE_4BYTE;  /* 写4字节 */
    tx_data[1] = 0x00;  /* 1600h:01h */
    tx_data[2] = 0x16;
    tx_data[3] = 0x01;
    /* 数据: 0x60710010 (索引6071h, 子索引0, 长度16位) */
    tx_data[4] = 0x10;  /* 长度=0x10 (16位) */
    tx_data[5] = 0x00;  /* 子索引=0 */
    tx_data[6] = 0x71;  /* 索引低 */
    tx_data[7] = 0x60;  /* 索引高 */
    can_send_msg(sdo_id, tx_data, 8);

    result = sdo_verify(node_id, tx_data, 200);
    if(result) 
    {
        //lcd_show_string(0, 85, 120, 12, 12, "torque Config Fail!", RED);
        return 1;
    }

    delay_ms(10);
    
    memset(tx_data,0,8);
    /* 5. 设置传输类型为异步 (0xFF) */
    tx_data[0] = CANOPEN_CMD_WRITE_1BYTE;  /* 写1字节 */
    tx_data[1] = 0x02;  /* 1400h:02h */
    tx_data[2] = 0x14;
    tx_data[3] = 0x02;
    tx_data[4] = 0xFF;  /* 异步传输 */
    can_send_msg(sdo_id, tx_data, 8);

    result = sdo_verify(node_id, tx_data, 200);
    if(result) 
    {
        //lcd_show_string(0, 97, 120, 12, 12, "syn Config Fail!", RED);
        return 1;
    }

    delay_ms(10);
    
    /* 6. 启用RPDO1 (清除最高位) */
    tx_data[0] = CANOPEN_CMD_WRITE_4BYTE;  /* 写4字节 */
    tx_data[1] = 0x00;  /* 1400h:01h */
    tx_data[2] = 0x14;
    tx_data[3] = 0x01;
    /* 数据: 0x00000201 (最高位0=启用) */
    tx_data[4] = 0x01;
    tx_data[5] = 0x02;
    tx_data[6] = 0x00;
    tx_data[7] = 0x00;  /* 最高位为0 */
    can_send_msg(sdo_id, tx_data, 8);

    result = sdo_verify(node_id, tx_data, 200);
    if(result) 
    {
        //lcd_show_string(0, 120, 120, 12, 12, "start pdo Fail!", RED);
        return 1;
    }

    delay_ms(10);
    
    memset(tx_data,0,8);
    
    
    /* 8. 设置最大转矩限制 (6072h=3000) */
    tx_data[0] = CANOPEN_CMD_WRITE_2BYTE;  /* 写2字节 */
    tx_data[1] = 0x72;  /* 6072h:00h */
    tx_data[2] = 0x60;
    tx_data[3] = 0x00;
    tx_data[4] = 0xB8;  /* 3000=0x0BB8 (低字节) */
    tx_data[5] = 0x0B;  /* 高字节 */
    can_send_msg(sdo_id, tx_data, 8);

    result = sdo_verify(node_id, tx_data, 200);
    if(result) 
    {
        //lcd_show_string(0, 60, 120, 12, 12, "max torque Fail!", RED);
        return 1;
    }

    delay_ms(10);

    /* 9. 设置转矩斜坡时间50ms */
    //转矩斜坡时间：达到额定转矩所需的时间
    memset(tx_data, 0, 8);
    tx_data[0] = CANOPEN_CMD_WRITE_4BYTE; 
    tx_data[1] = 0x87; 
    tx_data[2] = 0x60;
    tx_data[3] = 0x00;
    tx_data[4] = 0x32;  /* 50 = 0x32 */
    tx_data[5] = 0x00;
    tx_data[6] = 0x00;
    tx_data[7] = 0x00;
    can_send_msg(sdo_id, tx_data, 8);

    result = sdo_verify(node_id, tx_data, 200);
    if(result)
    {
        // 在设置之前先读取
        uint32_t current_slope;
        result = canopen_sdo_read(node_id, 0x6087, 0x00, &current_slope);
        if(result == 0)
        {
            char buf[30];
            sprintf(buf, "Cur Slope:%ld", current_slope);
            //lcd_show_string(0, 60, 120, 12, 12, buf, CYAN);
        }
    }
    if(result) 
    {
        //lcd_show_string(0, 60, 120, 12, 12, "time config Fail!", RED);
        return 1;
    }

    delay_ms(10);
    
    /* 10. 设置初始转矩值为5% */
    memset(tx_data, 0, 8);
    tx_data[0] = CANOPEN_CMD_WRITE_2BYTE; 
    tx_data[1] = 0x71; 
    tx_data[2] = 0x60;
    tx_data[3] = 0x00;
    tx_data[4] = 0x32;  /* 转矩值5% */
    tx_data[5] = 0x00;
    can_send_msg(sdo_id, tx_data, 8);

    result = sdo_verify(node_id, tx_data, 200);
    if(result) 
    {
        //lcd_show_string(0, 60, 120, 12, 12, "torque init Fail!", RED);
        return 1;
    }

    delay_ms(10);
    
    /* 7. 设置转矩模式 (6060h=4) */
    tx_data[0] = CANOPEN_CMD_WRITE_1BYTE;
    tx_data[1] = 0x60;  /* 6060h:00h */
    tx_data[2] = 0x60;
    tx_data[3] = 0x00;
    tx_data[4] = 0x04;  /* 4=转矩模式 */
    can_send_msg(sdo_id, tx_data, 8);

    result = sdo_verify(node_id, tx_data, 200);
    if(result) 
    {
        //lcd_show_string(0, 150, 120, 12, 12, "torque mode config Fail!", RED);
        return 1;
    }

    delay_ms(10);
    
    //printf("PDO配置完成\r\n");
    return 0;
}

/**
 * @brief       伺服使能序列
 * @param       node_id: 节点ID
 */
uint8_t servo_enable(uint8_t node_id)
{
    uint8_t tx_data[8];
    uint32_t sdo_id = 0x600 + node_id;
    uint8_t result;
    uint32_t status_word;

    //printf("启动伺服使能序列...\r\n");
    result = canopen_sdo_read(node_id, 0x6041, 0x00, &status_word);
    //0x250，伺服无故障
    if((status_word & 0xFFFF) != 0x0250)
    {
        //lcd_show_string(0, 90, 120, 12, 12, "Not 0x250! ", RED);
        return 1;  // 状态错误
    }
    //lcd_show_string(0, 90, 120, 12, 12, "State 0x250 ", GREEN);
    delay_ms(20);

    

    /* 控制字结构 */
    tx_data[0] = CANOPEN_CMD_WRITE_2BYTE;  /* 写2字节 */
    tx_data[1] = 0x40;  /* 6040h:00h */
    tx_data[2] = 0x60;
    tx_data[3] = 0x00;
    
    /* 步骤1: 0x0006 (伺服准备好) */
    tx_data[4] = 0x06;
    tx_data[5] = 0x00;
    can_send_msg(sdo_id, tx_data, 8);


    result = sdo_verify(node_id, tx_data, 200);
    if(result) return 1;

    delay_ms(20);
    
    /* 步骤2: 0x0007 (等待打开伺服使能) */
    tx_data[4] = 0x07;
    tx_data[5] = 0x00;
    can_send_msg(sdo_id, tx_data, 8);

    result = sdo_verify(node_id, tx_data, 200);
    if(result) return 1;

    delay_ms(20);
    
    /* 步骤3: 0x000F (伺服运行) */
    tx_data[4] = 0x0F;
    tx_data[5] = 0x00;
    can_send_msg(sdo_id, tx_data, 8);

    result = sdo_verify(node_id, tx_data, 200);
    if(result) return 1;

    delay_ms(20);
    
    return 0;

    //printf("伺服使能完成\r\n");
}

/**
 * @brief       伺服失能（Servo OFF）- 回到伺服准备好状态
 * @param       node_id: 节点ID
 * @retval      0:成功, 其他:失败
 */
uint8_t servo_disable(uint8_t node_id)
{
    uint8_t tx_data[8];
    uint32_t sdo_id = 0x600 + node_id;
    uint8_t result;
    uint32_t status_word;
    
    //lcd_show_string(0, 105, 120, 12, 12, "Servo OFF...", YELLOW);
    
    /* 1. 先读取当前状态 */
    result = canopen_sdo_read(node_id, 0x6041, 0x00, &status_word);
    if(result != 0)
    {
        //lcd_show_string(0, 105, 120, 12, 12, "Read Fail!  ", RED);
        return result;
    }
    
    /* 2. 如果已经在失能状态，直接返回 */
    if((status_word & 0xFFFF) == 0x0231 || (status_word & 0xFFFF) == 0x0250)
    {
        //lcd_show_string(0, 105, 120, 12, 12, "Already OFF ", GREEN);
        return 0;
    }
    
    /* 3. 发送0x0006 - 回到伺服准备好状态 */
    tx_data[0] = CANOPEN_CMD_WRITE_2BYTE;
    tx_data[1] = 0x40;  /* 6040h低字节 */
    tx_data[2] = 0x60;  /* 6040h高字节 */
    tx_data[3] = 0x00;  /* 子索引 */
    tx_data[4] = 0x00;  /* 0x0000 */ //回到伺服无故障状态
    tx_data[5] = 0x00;
    
    can_send_msg(sdo_id, tx_data, 8);
    result = sdo_verify(node_id, tx_data, 500);
    
    if(result == 0)
    {
        //lcd_show_string(0, 105, 120, 12, 12, "Servo OFF OK", GREEN);
    }
    else
    {
        //lcd_show_string(0, 105, 120, 12, 12, "OFF Fail!   ", RED);
    }
    
    return result;
}

uint8_t read_actual_torque(uint8_t node_id, int16_t *torque)
{
    uint32_t data;
    uint8_t result;
    
    /* 读取6077h - 实际转矩值 */
    result = canopen_sdo_read(node_id, 0x6077, 0x00, &data);
    
    if(result == 0)
    {
        *torque = (int16_t)(data & 0xFFFF);
        
        /* 显示当前扭矩 */
        char buf[30];
        sprintf(buf, "Torque:%d.%d%%", *torque / 10, abs(*torque % 10));
        //lcd_show_string(0, 120, 120, 12, 12, buf, CYAN);
    }
    
    return result;
}


/**
 * @brief       通过PDO发送转矩指令
 * @param       node_id: 节点ID
 * @param       torque_percent: 转矩百分比(-300.0 ~ +300.0)
 * @retval      发送状态
 */
uint8_t send_torque_pdo(uint8_t node_id, float torque_percent)
{
    uint8_t tx_data[8] = {0};
    
    /* 1. 限制范围 */
    if(torque_percent > 300.0f) torque_percent = 300.0f;
    if(torque_percent < -300.0f) torque_percent = -300.0f;
    
    /* 2. 转换为0.1%单位 */
    int16_t torque_value = (int16_t)(torque_percent * 10.0f);
    
    /* 3. PDO数据（小端模式） */
    tx_data[0] = torque_value & 0xFF;        /* 低字节 */
    tx_data[1] = (torque_value >> 8) & 0xFF; /* 高字节 */
    /* 其余字节为0 */
    
    /* 4. PDO CAN ID: RPDO1 = 0x200 + node_id */
    uint32_t pdo_id = 0x200 + node_id;
    
    /* 5. 发送PDO */
    return can_send_msg(pdo_id, tx_data, 8);
}


/**
 * @brief       SDO读取函数
 * @param       node_id: 节点ID
 * @param       index: 对象索引
 * @param       subindex: 子索引
 * @param       data: 返回数据缓冲区（32位，根据data_size使用相应部分）
 * @retval      0:成功, 1:超时, 2:SDO错误, 3:发送失败
 */
uint8_t canopen_sdo_read(uint8_t node_id, uint16_t index, uint8_t subindex, uint32_t *data)
{
    uint8_t tx_data[8] = {0x40, 0,0,0,0,0,0,0};  /* 读命令0x40 */
    uint8_t rx_data[8];
    uint16_t rx_id;
    uint32_t start_time;
    
    /* 设置索引和子索引（小端模式） */
    tx_data[1] = index & 0xFF;        /* 索引低字节 */
    tx_data[2] = (index >> 8) & 0xFF; /* 索引高字节 */
    tx_data[3] = subindex;            /* 子索引 */
    
    /* 发送读请求 */
    if(can_send_msg(0x600 + node_id, tx_data, 8) != 0)
    {
        return 3;  /* 发送失败 */
    }
    
    /* 等待响应 */
    start_time = HAL_GetTick();
    while((HAL_GetTick() - start_time) < 200)
    {
        uint8_t len = can_receive_msg(&rx_id, rx_data);
        if(len > 0 && rx_id == (0x580 + node_id))
        {
            uint8_t cmd = rx_data[0];
            
            /* 验证索引匹配 */
            if(rx_data[1] == tx_data[1] && 
               rx_data[2] == tx_data[2] && 
               rx_data[3] == tx_data[3])
            {
                /* 根据命令字解析数据 */
                switch(cmd)
                {
                    case 0x43:  /* 1字节数据 */
                        *data = rx_data[4];  /* 只使用字节4 */
                        return 0;
                        
                    case 0x4B:  /* 2字节数据 */
                        /* 字节4-5：低字节在前 */
                        *data = (rx_data[5] << 8) | rx_data[4];
                        return 0;

                    case 0x4F:  /* 4字节数据 */
                        /* 字节4-7：小端模式 */
                        *data = (rx_data[7] << 24) | 
                                (rx_data[6] << 16) | 
                                (rx_data[5] << 8) | 
                                rx_data[4];
                        return 0;
                        
                    case 0x80:  /* 错误响应 */
                        return 2;
                        
                    default:
                        //printf("未知SDO响应: 0x%02X\r\n", cmd);
                        return 2;
                }
            }
        }
        delay_ms(1);
    }
    
    return 1;  /* 超时 */
}

