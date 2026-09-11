#include "./BSP/LCD/lcd.h"
#include "./BSP/LCD/lcd_font.h"
#include "./BSP/LCD/lcd_spi.h"
#include "./SYSTEM/delay/delay.h"
#include <stdio.h>
#include <math.h>

/* 定义LCD显存 */
#define LCD_BUF_SIZE (LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t) / 10)
/* 定义LCD显存和缓冲区 */
#define FILL_BUF_SIZE 512
#define CHAR_BUF_SIZE (LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t) / 10)
// 背景色
uint32_t g_back_color = DARK_SLATE_GRAY;

// 字符绘制缓冲区
static uint8_t char_buffer[CHAR_BUF_SIZE] __attribute__((aligned(4)));


/**
 * @brief       往LCD写命令
 * @param       无
 * @retval      无
 */
static void lcd_write_cmd(uint8_t cmd)
{
    LCD_CS(0);
    LCD_WR(0);
    lcd_spi_send(&cmd, sizeof(cmd));
    LCD_CS(1);
}

/**
 * @brief       往LCD写数据
 * @param       无
 * @retval      无
 */
static void lcd_write_dat(uint8_t dat)
{
    LCD_CS(0);
    LCD_WR(1);
    lcd_spi_send(&dat, sizeof(dat));
    LCD_CS(1);
}


/**
 * @brief 在同一个CS周期内发送命令及其数据
 * @param cmd: 命令字节
 * @param data: 数据数组指针（可以为NULL）
 * @param data_len: 数据长度（可以为0）
 * @retval 无
 */
void lcd_write_command_with_data(uint8_t cmd, uint8_t *data, uint16_t data_len)
{
    /* CS拉低，开始传输 */
    LCD_CS(0);
    
    /* 发送命令字节 */
    LCD_WR(0);  // DC=0 表示命令

    lcd_spi_send(&cmd, sizeof(cmd));
    
    /* 如果有数据，发送数据 */
    if (data != NULL && data_len > 0) {
        LCD_WR(1);  // DC=1 表示数据

        lcd_spi_send(data, data_len);
    }
    
    /* CS拉高，结束传输 */
    LCD_CS(1);
}

/**
 * @brief       往LCD写16bit数据
 * @param       无
 * @retval      无
 */
static void lcd_write_dat_16b(uint16_t dat)
{
    uint8_t buf[2];
    
    buf[0] = (uint8_t)(dat >> 8) & 0xFF;
    buf[1] = (uint8_t)dat & 0xFF;

    LCD_CS(0);
    LCD_WR(1);

    lcd_spi_send(buf, sizeof(buf));
    LCD_CS(1);
}



/**
 * @brief       平方函数，x^y
 * @param       x: 底数
 *              y: 指数
 * @retval      x^y
 */
static uint32_t lcd_pow(uint8_t x, uint8_t y)
{
    uint8_t loop;
    uint32_t res = 1;
    
    for (loop=0; loop<y; loop++)
    {
        res *= x;
    }
    
    return res;
}

/**
 * @brief       硬件初始化
 * @param       无
 * @retval      无
 */
static void lcd_hw_init(void)
{
    GPIO_InitTypeDef gpio_init_struct = {0};
    
    /* 使能时钟 */
    LCD_PWR_GPIO_CLK_ENABLE();
    LCD_CS_GPIO_CLK_ENABLE();
    LCD_WR_GPIO_CLK_ENABLE();
    LCD_RST_GPIO_CLK_ENABLE();
    
     /* 初始化PWR引脚 */
     gpio_init_struct.Pin    = LCD_PWR_GPIO_PIN;
     gpio_init_struct.Mode   = GPIO_MODE_OUTPUT_PP;
     gpio_init_struct.Pull   = GPIO_PULLUP;
     gpio_init_struct.Speed  = GPIO_SPEED_FREQ_HIGH;
     HAL_GPIO_Init(LCD_PWR_GPIO_PORT, &gpio_init_struct);


     /* 初始化CS引脚 */
     gpio_init_struct.Pin    = LCD_CS_GPIO_PIN;
     gpio_init_struct.Mode   = GPIO_MODE_OUTPUT_PP;
     gpio_init_struct.Pull   = GPIO_PULLUP;
     gpio_init_struct.Speed  = GPIO_SPEED_FREQ_HIGH;
     HAL_GPIO_Init(LCD_CS_GPIO_PORT, &gpio_init_struct);

     /* 初始化WR引脚 */
     gpio_init_struct.Pin    = LCD_WR_GPIO_PIN;
     gpio_init_struct.Mode   = GPIO_MODE_OUTPUT_PP;
     gpio_init_struct.Pull   = GPIO_PULLUP;
     gpio_init_struct.Speed  = GPIO_SPEED_FREQ_HIGH;
     HAL_GPIO_Init(LCD_WR_GPIO_PORT, &gpio_init_struct);

     /* 初始化RST引脚 */
     gpio_init_struct.Pin    = LCD_RST_GPIO_PIN;
     gpio_init_struct.Mode   = GPIO_MODE_OUTPUT_PP;
     gpio_init_struct.Pull   = GPIO_PULLUP;
     gpio_init_struct.Speed  = GPIO_SPEED_FREQ_HIGH;
     HAL_GPIO_Init(LCD_RST_GPIO_PORT, &gpio_init_struct);
    
    LCD_CS(1);

}

/**
 * @brief       硬件复位
 * @param       无
 * @retval      无
 */
static void lcd_hw_reset(void)
{
    LCD_RST(0);
    delay_ms(50);
    LCD_RST(1);
    delay_ms(150);
}

/**
 * @brief ST7735专用初始化序列（针对128x160 1.77寸屏）
 * @note 这个序列经过测试，适用于大多数ST7735屏幕
 */
// static void lcd_reg_init(void)
// {
    
//     /* 1. 软件复位 */
//     lcd_write_cmd(0x01); // SWRESET - Software reset
//     delay_ms(150);       // 等待复位完成
    
//     /* 2. 退出睡眠模式 */
//     lcd_write_cmd(0x11); // SLPOUT - Sleep out
//     delay_ms(150);       // 等待退出睡眠
    
//     /* 3. 帧率控制 - 正常模式 */
//     lcd_write_cmd(0xB1); // FRMCTR1
//     lcd_write_dat(0x01); // Division ratio = fosc
//     lcd_write_dat(0x2C); // 帧率 = 60Hz
//     lcd_write_dat(0x2D); // 帧率 = 60Hz
    
//     /* 4. 帧率控制 - 空闲模式 */
//     lcd_write_cmd(0xB2); // FRMCTR2
//     lcd_write_dat(0x01); // Division ratio = fosc
//     lcd_write_dat(0x2C); // 帧率 = 60Hz
//     lcd_write_dat(0x2D); // 帧率 = 60Hz
    
//     /* 5. 帧率控制 - 部分模式 */
//     lcd_write_cmd(0xB3); // FRMCTR3
//     lcd_write_dat(0x01); // Division ratio = fosc
//     lcd_write_dat(0x2C); // 帧率 = 60Hz
//     lcd_write_dat(0x2D); // 帧率 = 60Hz
//     lcd_write_dat(0x01); // Division ratio = fosc
//     lcd_write_dat(0x2C); // 帧率 = 60Hz
//     lcd_write_dat(0x2D); // 帧率 = 60Hz
    
//     /* 6. 显示反转控制 */
//     lcd_write_cmd(0xB4); // INVCTR - Display inversion control
//     lcd_write_dat(0x07); // Line inversion
    
//     /* 7. 电源控制1 */
//     lcd_write_cmd(0xC0); // PWCTR1
//     lcd_write_dat(0xA2); // GVDD = 4.8V
//     lcd_write_dat(0x02); // 1.0 + (GVDD/GVDD) factor
//     lcd_write_dat(0x84); // VGH = 14.7V, VGL = -7.35V
    
//     /* 8. 电源控制2 */
//     lcd_write_cmd(0xC1); // PWCTR2
//     lcd_write_dat(0xC5); // VGH = 14.7V, VGL = -7.35V
    
//     /* 9. 电源控制3 */
//     lcd_write_cmd(0xC2); // PWCTR3
//     lcd_write_dat(0x0A); // Op-amp current small
//     lcd_write_dat(0x00); // Boost frequency
    
//     /* 10. 电源控制4 */
//     lcd_write_cmd(0xC3); // PWCTR4
//     lcd_write_dat(0x8A); // VCOMH = 4.45V
//     lcd_write_dat(0x2A); // VCOML = -1.5V
    
//     /* 11. 电源控制5 */
//     lcd_write_cmd(0xC4); // PWCTR5
//     lcd_write_dat(0x8A); // VCOMH = 4.45V
//     lcd_write_dat(0xEE); // VCOML = -1.5V
    
//     /* 12. VCOM控制 */
//     lcd_write_cmd(0xC5); // VMCTR1 - VCOM control
//     lcd_write_dat(0x0E); // VCOM = -0.675V
    
//     /* 13. 内存数据访问控制 - 关键！ */
//     lcd_write_cmd(0x36); // MADCTL - Memory data access control
//     // 参数说明：
//     // 0x00: 正常方向 (RGB)
//     // 0x08: 正常方向 (BGR)
//     // 0xC0: 旋转180度 (RGB) - 常用
//     // 0xC8: 旋转180度 (BGR)
//     lcd_write_dat(0x68); // 旋转180度，RGB顺序
    
//     /* 14. 像素格式 */
//     lcd_write_cmd(0x3A); // COLMOD - Interface pixel format
//     lcd_write_dat(0x05); // 16位RGB565
    
//     /* 17. 伽马校正 - 正极性 */
//     lcd_write_cmd(0xE0); // GMCTRP1 - Positive gamma correction
//     lcd_write_dat(0x0F); // 参数1
//     lcd_write_dat(0x1A); // 参数2
//     lcd_write_dat(0x0F); // 参数3
//     lcd_write_dat(0x18); // 参数4
//     lcd_write_dat(0x2F); // 参数5
//     lcd_write_dat(0x28); // 参数6
//     lcd_write_dat(0x20); // 参数7
//     lcd_write_dat(0x22); // 参数8
//     lcd_write_dat(0x1F); // 参数9
//     lcd_write_dat(0x1B); // 参数10
//     lcd_write_dat(0x23); // 参数11
//     lcd_write_dat(0x37); // 参数12
//     lcd_write_dat(0x00); // 参数13
//     lcd_write_dat(0x07); // 参数14
//     lcd_write_dat(0x02); // 参数15
//     lcd_write_dat(0x10); // 参数16
    
//     /* 18. 伽马校正 - 负极性 */
//     lcd_write_cmd(0xE1); // GMCTRN1 - Negative gamma correction
//     lcd_write_dat(0x0F); // 参数1
//     lcd_write_dat(0x1B); // 参数2
//     lcd_write_dat(0x0F); // 参数3
//     lcd_write_dat(0x17); // 参数4
//     lcd_write_dat(0x33); // 参数5
//     lcd_write_dat(0x2C); // 参数6
//     lcd_write_dat(0x29); // 参数7
//     lcd_write_dat(0x2E); // 参数8
//     lcd_write_dat(0x30); // 参数9
//     lcd_write_dat(0x30); // 参数10
//     lcd_write_dat(0x39); // 参数11
//     lcd_write_dat(0x3F); // 参数12
//     lcd_write_dat(0x00); // 参数13
//     lcd_write_dat(0x07); // 参数14
//     lcd_write_dat(0x03); // 参数15
//     lcd_write_dat(0x10); // 参数16
    
//     /* 19. 显示功能控制 */
//     lcd_write_cmd(0xB6); // DISCTRL - Display function control
//     lcd_write_dat(0x0A); // 参数1
//     lcd_write_dat(0xA2); // 参数2 (0xA2=正常，0xB2=部分)
//     lcd_write_dat(0x27); // 参数3
//     lcd_write_dat(0x00); // 参数4
    
//     /* 20. 显示开 */
//     lcd_write_cmd(0x29); // DISPON - Display on
//     delay_ms(100);       // 等待显示稳定
    
//     /* 21. 关闭空闲模式 */
//     lcd_write_cmd(0x38); // IDLEMOFF - Idle mode off
    
//     /* 22. 正常显示模式 */
//     lcd_write_cmd(0x13); // NORON - Normal display mode on
//     delay_ms(10);
// }

// static void lcd_reg_init(void)
// {
//         /* 1. 软件复位 */
//     lcd_write_command_with_data(0x01, NULL, 0);
//     delay_ms(150);
    
//     /* 2. 退出睡眠 */
//     lcd_write_command_with_data(0x11, NULL, 0);
//     delay_ms(120);
    
//     /* 3. 帧率控制 */
//     uint8_t frame_rate[] = {0x01, 0x2C, 0x2D};
//     lcd_write_command_with_data(0xB1, frame_rate, 3);  // 正常模式
//     lcd_write_command_with_data(0xB2, frame_rate, 3);  // 空闲模式
//     uint8_t frame_rate_partial[] = {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D};
//     lcd_write_command_with_data(0xB3, frame_rate_partial, 6);  // 部分模式
    
//     /* 4. 显示反转控制 */
//     uint8_t inversion[] = {0x07};
//     lcd_write_command_with_data(0xB4, inversion, 1);
    
//     /* 5. 电源控制 */
//     uint8_t power1[] = {0xA2, 0x02, 0x84};
//     lcd_write_command_with_data(0xC0, power1, 3);
    
//     uint8_t power2[] = {0xC5};
//     lcd_write_command_with_data(0xC1, power2, 1);
    
//     uint8_t power3[] = {0x0A, 0x00};
//     lcd_write_command_with_data(0xC2, power3, 2);
    
//     uint8_t power4[] = {0x8A, 0x2A};
//     lcd_write_command_with_data(0xC3, power4, 2);
    
//     uint8_t power5[] = {0x8A, 0xEE};
//     lcd_write_command_with_data(0xC4, power5, 2);
    
//     /* 6. VCOM控制 */
//     uint8_t vcom[] = {0x0E};
//     lcd_write_command_with_data(0xC5, vcom, 1);
    
//     /* 7. 内存访问控制 - 尝试不同值 */
//     // 重要：需要尝试不同的值，常见的有：
//     // 0x00: 正常方向 (RGB)
//     // 0x08: 正常方向 (BGR)
//     // 0xC0: 旋转180度 (RGB)
//     // 0xC8: 旋转180度 (BGR)
//     // 0xA0: XY交换 + X镜像
//     uint8_t madctl[] = {0xC0};  // 先尝试旋转180度
//     lcd_write_command_with_data(0x36, madctl, 1);
    
//     /* 8. 像素格式 */
//     uint8_t colmod[] = {0x05};  // 16位RGB565 (ST7735)
//     lcd_write_command_with_data(0x3A, colmod, 1);
    
//     // /* 9. 设置显示区域（重要！） */
//     // uint8_t column_addr[] = {0x00, 0x00, 0x00, 0x7F};  // 0-127列
//     // lcd_write_command_with_data(0x2A, column_addr, 4);
    
//     // uint8_t row_addr[] = {0x00, 0x00, 0x00, 0x9F};     // 0-159行
//     // lcd_write_command_with_data(0x2B, row_addr, 4);
    
//     /* 10. 伽马校正 */
//     uint8_t gamma_pos[] = {
//         0x0F, 0x1A, 0x0F, 0x18, 0x2F, 0x28,
//         0x20, 0x22, 0x1F, 0x1B, 0x23, 0x37,
//         0x00, 0x07, 0x02, 0x10
//     };
//     lcd_write_command_with_data(0xE0, gamma_pos, 16);
    
//     uint8_t gamma_neg[] = {
//         0x0F, 0x1B, 0x0F, 0x17, 0x33, 0x2C,
//         0x29, 0x2E, 0x30, 0x30, 0x39, 0x3F,
//         0x00, 0x07, 0x03, 0x10
//     };
//     lcd_write_command_with_data(0xE1, gamma_neg, 16);
    
//     /* 11. 显示功能控制 */
//     uint8_t display_ctrl[] = {0x0A, 0xA2, 0x27, 0x00};
//     lcd_write_command_with_data(0xB6, display_ctrl, 4);
    
//     /* 12. 显示开 */
//     lcd_write_command_with_data(0x29, NULL, 0);
//     delay_ms(100);
// }


/**
 * @brief       寄存器初始化
 * @param       无
 * @retval      无
 */
// static void lcd_reg_init(void)
// {
//     // 软件复位
//     lcd_write_cmd(0x01);
//     delay_ms(120);
//     /* Sleep Out */
//     lcd_write_cmd(0x11);
//     delay_ms(120);
    
//     /* Memory Data Access Control - 关键！使用0xC0 */
//     lcd_write_cmd(0x36);
//     lcd_write_dat(0x68);  // 尝试不同的值：0xC0, 0xC8, 0xA0, 0x08
    
//     /* Interface Pixel Format */
//     lcd_write_cmd(0x3A);
//     lcd_write_dat(0x05);  // 16-bit RGB565
    
//     /* Frame Rate Control - 使用参考代码的参数 */
//     lcd_write_cmd(0xB1);
//     lcd_write_dat(0x01);
//     lcd_write_dat(0x2C);
//     lcd_write_dat(0x2D);
    
//     lcd_write_cmd(0xB2);
//     lcd_write_dat(0x01);
//     lcd_write_dat(0x2C);
//     lcd_write_dat(0x2D);
    
//     lcd_write_cmd(0xB3);
//     lcd_write_dat(0x01);
//     lcd_write_dat(0x2C);
//     lcd_write_dat(0x2D);
//     lcd_write_dat(0x01);
//     lcd_write_dat(0x2C);
//     lcd_write_dat(0x2D);
    
//     /* Display Inversion Control */
//     lcd_write_cmd(0xB4);
//     lcd_write_dat(0x07);
    
//     /* Power Control */
//     lcd_write_cmd(0xC0);
//     lcd_write_dat(0xA2);
//     lcd_write_dat(0x02);
//     lcd_write_dat(0x84);

    
//     lcd_write_cmd(0xC1);
//     lcd_write_dat(0xC5);
    
//     lcd_write_cmd(0xC2);
//     lcd_write_dat(0x0A);
//     lcd_write_dat(0x00);
    
//     lcd_write_cmd(0xC3);
//     lcd_write_dat(0x8A);
//     lcd_write_dat(0x2A);
    
//     lcd_write_cmd(0xC4);
//     lcd_write_dat(0x8A);
//     lcd_write_dat(0xEE);
    
//     lcd_write_cmd(0xC5);
//     lcd_write_dat(0x0E);
    
//     /* Enable Test Command - 参考代码有 */
//     lcd_write_cmd(0xF0);
//     lcd_write_dat(0x01);
    
//     /* Gamma Correction */
//     lcd_write_cmd(0xE0);
//     lcd_write_dat(0x0F);
//     lcd_write_dat(0x1A);
//     lcd_write_dat(0x0F);
//     lcd_write_dat(0x18);
//     lcd_write_dat(0x2F);
//     lcd_write_dat(0x28);
//     lcd_write_dat(0x20);
//     lcd_write_dat(0x22);
//     lcd_write_dat(0x1F);
//     lcd_write_dat(0x1B);
//     lcd_write_dat(0x23);
//     lcd_write_dat(0x37);
//     lcd_write_dat(0x00);
//     lcd_write_dat(0x07);
//     lcd_write_dat(0x02);
//     lcd_write_dat(0x10);
    
//     lcd_write_cmd(0xE1);
//     lcd_write_dat(0x0F);
//     lcd_write_dat(0x1B);
//     lcd_write_dat(0x0F);
//     lcd_write_dat(0x17);
//     lcd_write_dat(0x33);
//     lcd_write_dat(0x2C);
//     lcd_write_dat(0x29);
//     lcd_write_dat(0x2E);
//     lcd_write_dat(0x30);
//     lcd_write_dat(0x30);
//     lcd_write_dat(0x39);
//     lcd_write_dat(0x3F);
//     lcd_write_dat(0x00);
//     lcd_write_dat(0x07);
//     lcd_write_dat(0x03);
//     lcd_write_dat(0x10);
    
//     /* Disable RAM Power Save Mode */
//     lcd_write_cmd(0xF6);
//     lcd_write_dat(0x00);
    
//     /* Display On */
//     lcd_write_cmd(0x29);
//     delay_ms(120);
// }


static void lcd_reg_init(void)
{
    delay_ms(150);  // 复位后等待
    
    /* Sleep Out */
    lcd_write_cmd(0x11);
    delay_ms(150);
    
    /* Enable Test Command (启用测试命令) */
    lcd_write_cmd(0xF0);
    lcd_write_dat(0x11);
    
   
    lcd_write_cmd(0xD6);
    lcd_write_dat(0xCB);
    
    /* Frame Rate Control - Normal Mode */
    lcd_write_cmd(0xB1);
    lcd_write_dat(0x01);  // RTNA设置
    lcd_write_dat(0x04);  // 前廊 (原代码是0x04不是0x2C)
    lcd_write_dat(0x01);  // 后廊 (原代码是0x01不是0x2D)
    
    /* Frame Rate Control - Idle Mode */
    lcd_write_cmd(0xB2);
    lcd_write_dat(0x05);
    lcd_write_dat(0x3C);
    lcd_write_dat(0x3C);
    
    /* Frame Rate Control - Partial Mode */
    lcd_write_cmd(0xB3);
    lcd_write_dat(0x05);
    lcd_write_dat(0x3C);
    lcd_write_dat(0x3C);
    lcd_write_dat(0x05);
    lcd_write_dat(0x3C);
    lcd_write_dat(0x3C);
    
    /* Display Inversion Control (显示反转控制) */
    lcd_write_cmd(0xB4);
    lcd_write_dat(0x03);  // Dot inversion
    lcd_write_dat(0x02);
    
    /* Power Control 1 */
    lcd_write_cmd(0xC0);
    lcd_write_dat(0x64);  // 改善灰底crosstalk
    lcd_write_dat(0x04);
    lcd_write_dat(0x84);
    
    /* Power Control 2 */
    lcd_write_cmd(0xC1);
    lcd_write_dat(0xC4);
    
    /* Power Control 3 - Normal Mode */
    lcd_write_cmd(0xC2);
    lcd_write_dat(0x0D);
    lcd_write_dat(0x00);
    
    /* Power Control 4 - Idle Mode */
    lcd_write_cmd(0xC3);
    lcd_write_dat(0x8D);
    lcd_write_dat(0x2A);
    
    /* Power Control 5 - Partial Mode */
    lcd_write_cmd(0xC4);
    lcd_write_dat(0x8D);
    lcd_write_dat(0xEE);
    
    /* VCOM Control */
    lcd_write_cmd(0xC5);
    lcd_write_dat(0x01);  // 原代码是0x01，你的代码是0x0E
    
    /* Positive Gamma Correction */
    lcd_write_cmd(0xE0);
    lcd_write_dat(0x08);
    lcd_write_dat(0x19);
    lcd_write_dat(0x1B);
    lcd_write_dat(0x1E);
    lcd_write_dat(0x3F);
    lcd_write_dat(0x38);
    lcd_write_dat(0x30);
    lcd_write_dat(0x32);
    lcd_write_dat(0x2E);
    lcd_write_dat(0x29);
    lcd_write_dat(0x30);
    lcd_write_dat(0x38);
    lcd_write_dat(0x00);
    lcd_write_dat(0x09);
    lcd_write_dat(0x00);
    lcd_write_dat(0x10);
    
    /* Negative Gamma Correction */
    lcd_write_cmd(0xE1);
    lcd_write_dat(0x07);
    lcd_write_dat(0x16);
    lcd_write_dat(0x13);
    lcd_write_dat(0x14);
    lcd_write_dat(0x36);
    lcd_write_dat(0x2F);
    lcd_write_dat(0x29);
    lcd_write_dat(0x2A);
    lcd_write_dat(0x2A);
    lcd_write_dat(0x28);
    lcd_write_dat(0x2F);
    lcd_write_dat(0x3D);
    lcd_write_dat(0x00);
    lcd_write_dat(0x04);
    lcd_write_dat(0x02);
    lcd_write_dat(0x10);
    
    
    /* Tear Effect Line ON */
    lcd_write_cmd(0x35);
    lcd_write_dat(0x00);
    
    /* Interface Pixel Format */
    lcd_write_cmd(0x3A);
    lcd_write_dat(0x05);  // 16-bit RGB565
    
    /* Memory Data Access Control */
    lcd_write_cmd(0x36);
    lcd_write_dat(0xA0);  // 旋转方向
    
    /* Display Inversion ON */
    lcd_write_cmd(0x21);
    
    /* Display ON */
    lcd_write_cmd(0x29);
    
    /* Memory Write (准备写入数据) */
    lcd_write_cmd(0x2C);
    
    delay_ms(150);
}

/**
 * @brief       设置行列地址
 * @param       xs: 列起始地址
 *              ys: 行起始地址
 *              xe: 列结束地址
 *              ye: 行结束地址
 * @retval      无
 */
void lcd_set_address(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye)
{
    // xs += LCD_COLUMN_OFFSET;
    // xe += LCD_COLUMN_OFFSET;
    
    // ys += LCD_LINE_OFFSET;
    // ye += LCD_LINE_OFFSET;

    // lcd_write_cmd(0x2A);
    // lcd_write_dat((uint8_t)(xs >> 8) & 0xFF);
    // lcd_write_dat((uint8_t)xs & 0xFF);
    // lcd_write_dat((uint8_t)(xe >> 8) & 0xFF);
    // lcd_write_dat((uint8_t)xe & 0xFF);
    // lcd_write_cmd(0x2B);
    // lcd_write_dat((uint8_t)(ys >> 8) & 0xFF);
    // lcd_write_dat((uint8_t)ys & 0xFF);
    // lcd_write_dat((uint8_t)(ye >> 8) & 0xFF);
    // lcd_write_dat((uint8_t)ye & 0xFF);
    // lcd_write_cmd(0x2C);

    uint8_t params[4]; // 参数缓冲区
    
    /* 1. 设置列地址 (命令 0x2A) */
    params[0] = (xs >> 8) & 0xFF; // 起始列高8位
    params[1] = xs & 0xFF;        // 起始列低8位
    params[2] = (xe >> 8) & 0xFF; // 结束列高8位
    params[3] = xe & 0xFF;        // 结束列低8位
    lcd_write_command_with_data(0x2A, params, 4); // 关键：一次发送命令和4个参数

    /* 2. 设置行地址 (命令 0x2B) */
    params[0] = (ys >> 8) & 0xFF; // 起始行高8位
    params[1] = ys & 0xFF;        // 起始行低8位
    params[2] = (ye >> 8) & 0xFF; // 结束行高8位
    params[3] = ye & 0xFF;        // 结束行低8位
    lcd_write_command_with_data(0x2B, params, 4); // 关键：一次发送命令和4个参数

    /* 3. 发送内存写命令 (命令 0x2C) */
    lcd_write_command_with_data(0x2C, NULL, 0); // 该命令无参数
}

/**
 * @brief       初始化
 * @param       无
 * @retval      无
 */
void lcd_init(void)
{
    lcd_display_off();  // 确保背光关闭
    lcd_hw_init();
    lcd_spi_init();

    lcd_hw_reset();
    
    lcd_reg_init();
    lcd_set_address(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
    lcd_clear(g_back_color);  // 先清屏
    delay_ms(100);             // 等待显示稳定

    lcd_display_on();         // 最后开背光 

}



/**
 * @brief       开启LCD背光
 * @param       无
 * @retval      无
 */
void lcd_display_on(void)
{
    LCD_PWR(1);
}

/**
 * @brief       关闭LCD背光
 * @param       无
 * @retval      无
 */
void lcd_display_off(void)
{
    LCD_PWR(0);
}



/* 批量发送像素数据函数 */
static void lcd_send_pixels_bulk(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t *pixels)
{
    uint32_t total_pixels = width * height;
    
    // 设置显示区域
    lcd_set_address(x, y, x + width - 1, y + height - 1);
    LCD_WR(1);

    LCD_CS(0);
    // 批量发送数据
    lcd_spi_send((uint8_t*)pixels, total_pixels * 2);
    LCD_CS(1);
}




///**
// * @brief       LCD区域填充
// * @param       xs   : 区域起始X坐标
// *              ys   : 区域起始Y坐标
// *              xe   : 区域终止X坐标
// *              ye   : 区域终止Y坐标
// *              color: 区域填充颜色
// * @retval      无
// */
//void lcd_fill(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color)
//{
//    static uint8_t g_lcd_buf[LCD_BUF_SIZE] = {0};
//    uint32_t area_size;
//    uint32_t area_remain = 0;
//    uint16_t buf_index;
//    
//    area_size = (xe - xs + 1) * (ye - ys + 1) * sizeof(uint16_t);
//    if (area_size > LCD_BUF_SIZE)
//    {
//        area_remain = area_size - LCD_BUF_SIZE;
//        area_size = LCD_BUF_SIZE;
//    }
//    
//    lcd_set_address(xs, ys, xe, ye);
//    LCD_WR(1);
//    while (1)
//    {
//        for (buf_index=0; buf_index<area_size / sizeof(uint16_t); buf_index++)
//        {
//            g_lcd_buf[buf_index * sizeof(uint16_t)] = (uint8_t)(color >> 8) & 0xFF;
//            g_lcd_buf[buf_index * sizeof(uint16_t) + 1] = (uint8_t)color & 0xFF;
//        }
//        
//        lcd_spi_send(g_lcd_buf, area_size);
//        
//        if (area_remain == 0)
//        {
//            break;
//        }
//        
//        if (area_remain > LCD_BUF_SIZE)
//        {
//            area_remain = area_remain - LCD_BUF_SIZE;
//        }
//        else
//        {
//            area_size = area_remain;
//            area_remain = 0;
//        }
//    }
//}

///**
// * @brief       LCD区域填充
// * @param       xs   : 区域起始X坐标
// *              ys   : 区域起始Y坐标
// *              xe   : 区域终止X坐标
// *              ye   : 区域终止Y坐标
// *              color: 区域填充颜色
// * @retval      无
// */
//void lcd_fill(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color)
//{
//    uint32_t total_pixels = (xe - xs + 1) * (ye - ys + 1);
//    uint8_t color_high = (uint8_t)(color >> 8);
//    uint8_t color_low = (uint8_t)color;
//    
//    lcd_set_address(xs, ys, xe, ye);
//    LCD_WR(1);
//    
//    // 使用极小的缓冲区（仅2字节）
//    uint8_t color_buf[2] = {color_high, color_low};
//    
//    // 分块发送数据
//    for (uint32_t i = 0; i < total_pixels; i++) {
//        lcd_spi_send(color_buf, 2);
//    }
//}


#define FILL_BUF_SIZE 512 // 512字节缓冲区

void lcd_fill(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color)
{
    uint32_t total_pixels = (xe - xs + 1) * (ye - ys + 1);
    uint8_t color_high = (uint8_t)(color >> 8);
    uint8_t color_low = (uint8_t)color;
    
    // 设置显示区域
    lcd_set_address(xs, ys, xe, ye);
    LCD_WR(1);

    // 创建缓冲区（512字节，可存储256像素）
    static uint8_t fill_buf[FILL_BUF_SIZE];
    
    // 填充缓冲区
    for (int i = 0; i < FILL_BUF_SIZE; i += 2) {
        fill_buf[i] = color_high;
        fill_buf[i + 1] = color_low;
    }
    
    // 分块发送
    uint32_t pixels_sent = 0;
    LCD_CS(0);
    while (pixels_sent < total_pixels) {
        uint32_t pixels_to_send = (total_pixels - pixels_sent) > (FILL_BUF_SIZE / 2) ? 
                                 (FILL_BUF_SIZE / 2) : (total_pixels - pixels_sent);
        
        // 发送数据
        lcd_spi_send(fill_buf, pixels_to_send * 2);
        
        pixels_sent += pixels_to_send;
    }
    LCD_CS(1);
}

/**
 * @brief       LCD清屏
 * @param       color: 清屏颜色
 * @retval      无
 */
void lcd_clear(uint16_t color)
{
    lcd_fill(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, color);
}


/**
 * @brief       画水平线
 * @param       x,y   : 起点坐标
 * @param       len   : 线长度
 * @param       color : 矩形的颜色
 * @retval      无
 */
void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint16_t color)
{
    if ((len == 0) || (x > LCD_WIDTH) || (y > LCD_HEIGHT))
    {
        return;
    }

    lcd_fill(x, y, x + len - 1, y, color);
}



/**
 * @brief       填充实心圆
 * @param       x,y  : 圆中心坐标
 * @param       r    : 半径
 * @param       color: 圆的颜色
 * @retval      无
 */
void lcd_fill_circle(uint16_t x, uint16_t y, uint16_t r, uint16_t color)
{
    uint32_t i;
    uint32_t imax = ((uint32_t)r * 707) / 1000 + 1;
    uint32_t sqmax = (uint32_t)r * (uint32_t)r + (uint32_t)r / 2;
    uint32_t xr = r;

    lcd_draw_hline(x - r, y, 2 * r, color);

    for (i = 1; i <= imax; i++)
    {
        if ((i * i + xr * xr) > sqmax)
        {
            /* draw lines from outside */
            if (xr > imax)
            {
                lcd_draw_hline (x - i + 1, y + xr, 2 * (i - 1), color);
                lcd_draw_hline (x - i + 1, y - xr, 2 * (i - 1), color);
            }

            xr--;
        }

        /* draw lines from inside (center) */
        lcd_draw_hline(x - xr, y + i, 2 * xr, color);
        lcd_draw_hline(x - xr, y - i, 2 * xr, color);
    }
}



/**
 * @brief       LCD画点
 * @param       x    : 待画点的X坐标
 *              y    : 待画点的Y坐标
 *              color: 待画点的颜色
 * @retval      无
 */
void lcd_draw_point(uint16_t x, uint16_t y, uint16_t color)
{
    lcd_set_address(x, y, x, y);
    lcd_write_dat_16b(color);
}


/**
 * @brief       LCD画线段
 * @param       x1   : 待画线段端点1的X坐标
 *              y1   : 待画线段端点1的Y坐标
 *              x2   : 待画线段端点2的X坐标
 *              y2   : 待画线段端点2的Y坐标
 *              color: 待画线段的颜色
 * @retval      无
 */
void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    uint16_t x_delta;
    uint16_t y_delta;
    int16_t x_sign;
    int16_t y_sign;
    int16_t error;
    int16_t error2;
    
    x_delta = (x1 < x2) ? (x2 - x1) : (x1 - x2);
    y_delta = (y1 < y2) ? (y2 - y1) : (y1 - y2);
    x_sign = (x1 < x2) ? 1 : -1;
    y_sign = (y1 < y2) ? 1 : -1;
    error = x_delta - y_delta;
    
    lcd_draw_point(x2, y2, color);
    
    while ((x1 != x2) || (y1 != y2))
    {
        lcd_draw_point(x1, y1, color);
        
        error2 = error << 1;
        if (error2 > -y_delta)
        {
            error -= y_delta;
            x1 += x_sign;
        }
        
        if (error2 < x_delta)
        {
            error += x_delta;
            y1 += y_sign;
        }
    }
}

/**
 * @brief       LCD画矩形框
 * @param       x1   : 待画矩形框端点1的X坐标
 *              y1   : 待画矩形框端点1的Y坐标
 *              x2   : 待画矩形框端点2的X坐标
 *              y2   : 待画矩形框端点2的Y坐标
 *              color: 待画矩形框的颜色
 * @retval      无
 */
void lcd_draw_rect(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    lcd_draw_line(x1, y1, x2, y1, color);
    lcd_draw_line(x1, y2, x2, y2, color);
    lcd_draw_line(x1, y1, x1, y2, color);
    lcd_draw_line(x2, y1, x2, y2, color);
}

/**
 * @brief       LCD画圆形框
 * @param       x    : 待画圆形框原点的X坐标
 *              y    : 待画圆形框原点的Y坐标
 *              r    : 待画圆形框的半径
 *              color: 待画圆形框的颜色
 * @retval      无
 */
void lcd_draw_circle(uint16_t x, uint16_t y, uint16_t r, uint16_t color)
{
    int32_t x_t;
    int32_t y_t;
    int32_t error;
    int32_t error2;
    
    x_t = -r;
    y_t = 0;
    error = 2 - 2 * r;
    
    do {
        lcd_draw_point(x - x_t, y + y_t, color);
        lcd_draw_point(x + x_t, y + y_t, color);
        lcd_draw_point(x + x_t, y - y_t, color);
        lcd_draw_point(x - x_t, y - y_t, color);
        
        error2 = error;
        if (error2 <= y_t)
        {
            y_t++;
            error = error + (y_t * 2 + 1);
            if ((-x_t == y_t) && (error2 <= x_t))
            {
                error2 = 0;
            }
        }
        
        if (error2 > x_t)
        {
            x_t++;
            error = error + (x_t * 2 + 1);
        }
    } while (x_t <= 0);
}



/* 优化的字符显示函数 - 使用缓冲区批量发送 */
void lcd_show_char(uint16_t x, uint16_t y, char chr, uint8_t size, uint8_t mode, uint16_t color, uint16_t p_back_color)
{
    uint8_t temp, t1, t;
    uint8_t csize = 0;
    uint8_t *pfont = 0;
    uint16_t char_width = size / 2;
    uint16_t char_height = size;
        
    csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2);
    chr = chr - ' ';
  
    switch (size) {
        case 12: pfont = (uint8_t *)asc2_1206[chr]; break;
        case 16: pfont = (uint8_t *)asc2_1608[chr]; break;
        case 24: pfont = (uint8_t *)asc2_2412[chr]; break;
        case 32: pfont = (uint8_t *)asc2_3216[chr]; break;  
        case 40: pfont = (uint8_t *)asc2_4020[chr]; break;  
        default: return;
    }
    
    // 清空字符缓冲区为背景色
    for (uint32_t i = 0; i < char_width * char_height; i++) {
        char_buffer[i * 2] = (uint8_t)(p_back_color >> 8);
        char_buffer[i * 2 + 1] = (uint8_t)p_back_color;
    }
    
    // 在缓冲区中绘制字符
    uint16_t current_x = 0;
    uint16_t current_y = 0;
    
    for (t = 0; t < csize; t++) {
        temp = pfont[t];
        
        for (t1 = 0; t1 < 8; t1++) {
            if (temp & 0x80) {
                // 计算在缓冲区中的位置
                uint32_t pixel_index = current_y * char_width + current_x;
                if (pixel_index < char_width * char_height) {
                    char_buffer[pixel_index * 2] = (uint8_t)(color >> 8);
                    char_buffer[pixel_index  * 2 + 1] = (uint8_t)color;
                }
            }
            
            temp <<= 1;
            current_y++;
            
            // 检查是否到达字符底部
            if (current_y >= char_height) {
                current_y = 0;
                current_x++;
                break;
            }
        }
    }
    
    // 批量发送整个字符
    lcd_send_pixels_bulk(x, y, char_width, char_height, (uint16_t*)char_buffer);
}


/**
 * @brief       显示字符串
 * @param       x,y         : 起始坐标
 * @param       width,height: 区域大小
 * @param       size        : 选择字体 12/16/24/32
 * @param       p           : 字符串首地址
 * @param       color       : 字符串的颜色;
 * @retval      无
 */
void lcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, char *p, uint16_t color, uint16_t p_back_color)
{
    uint8_t x0 = x;
    
    width += x;
    height += y;

    while ((*p <= '~') && (*p >= ' '))   /* 判断是不是非法字符! */
    {
        if (x >= width)
        {
            x = x0;
            y += size;
        }

        if (y >= height)
        {
            break;      /* 退出 */
        }

        lcd_show_char(x, y, *p, size, 0, color ,p_back_color);
        x += size / 2;
        //x += size;
        p++;
    }
}

/**
 * @brief       扩展显示len个数字(高位是0也显示)
 * @param       x,y : 起始坐标
 * @param       num : 数值(0 ~ 2^32)
 * @param       len : 显示数字的位数
 * @param       size: 选择字体 12/16/24/32
 * @param       mode: 显示模式
 *              [7]:0,不填充;1,填充0.
 *              [6:1]:保留
 *              [0]:0,非叠加显示;1,叠加显示.
 * @param       color : 数字的颜色;
 * @retval      无
 */
void lcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint16_t color, uint16_t p_back_color)
{
    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++)       /* 按总显示位数循环 */
    {
        temp = (num / lcd_pow(10, len - t - 1)) % 10;    /* 获取对应位的数字 */

        if (enshow == 0 && t < (len - 1))   /* 没有使能显示,且还有位要显示 */
        {
            if (temp == 0)
            {
                if (mode & 0x80)    /* 高位需要填充0 */
                {
                    lcd_show_char(x + (size / 2) * t, y, '0', size, mode & 0x01, color , p_back_color);    /* 用0占位 */
                }
                else
                {
                    lcd_show_char(x + (size / 2) * t, y, ' ', size, mode & 0x01, color , p_back_color);    /* 用空格占位 */
                }

                continue;
            }
            else
            {
                enshow = 1;         /* 使能显示 */
            }

        }

        lcd_show_char(x + (size / 2) * t, y, temp + '0', size, mode & 0x01, color ,p_back_color);
    }
}

/**
 * @brief       显示len个数字
 * @param       x,y : 起始坐标
 * @param       num : 数值(0 ~ 2^32)
 * @param       len : 显示数字的位数
 * @param       size: 选择字体 12/16/24/32
 * @param       color : 数字的颜色;
 * @retval      无
 */
void lcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint16_t color, uint16_t p_back_color)
{
    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++)   /* 按总显示位数循环 */
    {
        temp = (num / lcd_pow(10, len - t - 1)) % 10;   /* 获取对应位的数字 */

        if (enshow == 0 && t < (len - 1))               /* 没有使能显示,且还有位要显示 */
        {
            if (temp == 0)
            {
                lcd_show_char(x + (size / 2) * t, y, ' ', size, 0, color ,p_back_color);  /* 显示空格,占位 */
                continue;       /* 继续下个一位 */
            }
            else
            {
                enshow = 1;     /* 使能显示 */
            }
        }

        lcd_show_char(x + (size / 2) * t, y, temp + '0', size, 0, color ,p_back_color);   /* 显示字符 */
    }
}


/**
 * @brief       LCD图片
 * @note        图片取模方式: 水平扫描、RGB565、高位在前
 * @param       x     : 待显示图片的X坐标
 *              y     : 待显示图片的Y坐标
 *              width : 待显示图片的宽度
 *              height: 待显示图片的高度
 *              pic   : 待显示图片数组首地址
 * @retval      无
 */
void lcd_show_pic(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t *pic)
{
    if ((x + width > LCD_WIDTH) || (y + height > LCD_HEIGHT))
    {
        return;
    }
    
    lcd_set_address(x, y, x + width - 1, y + height - 1);
    LCD_CS(0);
    LCD_WR(1);


    lcd_spi_send(pic, width * height * sizeof(uint16_t));
    LCD_CS(1);
}



/* 圆角矩形 */
void lcd_draw_rounded_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t radius, uint16_t color) 
{
    // 绘制四个圆角
    lcd_fill_circle(x + radius, y + radius, radius, color);//左上圆角
    lcd_fill_circle(x + w - radius, y + radius, radius, color);//右上圆角
    lcd_fill_circle(x + radius, y + h - radius, radius, color);//左下圆角
    lcd_fill_circle(x + w - radius, y + h - radius, radius, color);//右下圆角
    
    // 填充矩形区域
    lcd_fill(x + radius, y, x + w - radius, y + h, color); // 中间矩形，填充从(x + radius, y)到(x + w - radius, y + h)的区域
    lcd_fill(x, y + radius, x + w, y + h - radius, color); // 左右矩形，填充从(x, y + radius)到(x + w, y + h - radius)的区域
}



/* 中文显示 */
void lcd_showChinese(uint16_t x, uint16_t y, uint8_t num, uint8_t size, uint16_t color, uint16_t back_color)
{
    uint16_t temp, t1, t;
    uint16_t csize = (size / 8 + ((size % 8) ? 1 : 0)) * size;
    uint16_t char_width = size;
    uint16_t char_height = size;

    // 清空缓冲区为背景色
    for (int i = 0; i < char_width * char_height; i++) {
        char_buffer[i * 2] = (uint8_t)(back_color >> 8);
        char_buffer[i * 2 + 1] = (uint8_t)back_color;
    }
    
    // 在缓冲区中绘制中文字符 - 修正扫描顺序
    uint16_t x_pos = 0, y_pos = 0;
    
    for (t = 0; t < csize; t++) {
        // 获取字模数据
        switch(size/8) {
            case 1: temp = Hzk1206[num][t]; break;
            case 2: temp = Hzk1608[num][t]; break; 
            case 3: temp = Hzk2412[num][t]; break; 
            default: return;
        }
        
        // 处理当前字节的8个位（一列中的8个像素）
        for (t1 = 0; t1 < 8; t1++) {
            // 计算在缓冲区中的位置 - 关键修正！
            // 按列扫描：先Y方向，后X方向
            uint16_t pixel_index = y_pos * char_width + x_pos;
            
            if (pixel_index < char_width * char_height) {
                if (temp & 0x80) {
                    // 前景色
                    char_buffer[pixel_index * 2] = (uint8_t)(color >> 8);
                    char_buffer[pixel_index  * 2 + 1] = (uint8_t)color;
                }
                // 否则保持背景色（已在清空时设置）
            }
            
            temp <<= 1;
            y_pos++; // 移动到下一行（同一列）
            
            // 检查是否到达列底部
            if (y_pos >= char_height) {
                y_pos = 0;
                x_pos++; // 移动到下一列
                break;   // 当前列处理完成
            }
        }
    }
    
    // 批量发送
    lcd_send_pixels_bulk(x, y, char_width, char_height, (uint16_t*)char_buffer);
}



void lcd_draw_data_curve(uint16_t x0, uint16_t y0, uint16_t width, uint16_t height, 
                         float *data, uint16_t data_len, 
                         float min_val, float max_val, 
                         uint16_t color, uint16_t bg_color, uint8_t init_cnt)
{
    static uint16_t prev_points[100] = {0}; // 存储上一次绘制的Y坐标
    static uint8_t first_run = 1;
    uint16_t i;
    float x_step, y_scale;
    /* 添加标签 */
    
    // 1. 初始化阶段：只绘制一次坐标系
    if(first_run || init_cnt == 0) {
        first_run = 0;
        
        /* 绘制坐标系背景 */
        lcd_fill(x0, y0 - height, x0 + width, y0, bg_color);
        
        /* 绘制坐标轴 */
        lcd_draw_line(x0 - 2, y0+2, x0 + width + 8, y0+2, WHITE); // X轴
        lcd_draw_line(x0 - 2, y0+2, x0 - 2, y0+2 - height -13, WHITE); // Y轴
        
        /* 绘制刻度 */
        // X轴刻度
//        for (i = 0; i <= 99; i++) {
//            uint16_t x = x0 + 3 + i * width / 100;
//            lcd_draw_line(x, y0, x, y0 + 2, WHITE);
//        }
        
        // Y轴刻度
        for (i = 1; i <= 10; i++) {
            uint16_t y = y0 - i * height / 10;
            lcd_draw_line(x0, y, x0 - 2, y, WHITE);
        }
        //最大值限制
        lcd_draw_line(x0 , y0  - height, x0 + width + 5, y0 - height, RED);
        lcd_show_string(x0 + width - 40, y0 - height - 15, 54, 12, 12, "MAX_LIMIT", RED, bg_color);
        
//        // X轴标签
//        lcd_show_string(x0, y0 + 10, 12, 12, 12, "0", WHITE, bg_color);
        lcd_show_string(x0 + width , y0 +5, 12, 12, 12, "T", WHITE, bg_color);
        
        // Y轴标签
        lcd_show_xnum(x0 - 28, y0 - height -5, 1500, 4, 12, 0x80, WHITE, bg_color);
        lcd_show_xnum(x0 - 22, y0 - height/2 -5, 750, 3, 12, 0x80, WHITE, bg_color);
        lcd_show_xnum(x0 - 10, y0 -5, 0, 1, 12, 0x80, WHITE, bg_color);
        lcd_show_string(x0 - 10, y0+2 - height -20, 12, 1, 12, "N", WHITE, bg_color);
        
        // 初始化prev_points数组
        for (i = 0; i < data_len; i++) {
            prev_points[i] = 0;
        }
    }
    
    // 2. 计算缩放比例
    x_step = (data_len > 1) ? (float)width / (data_len - 1) : 0;
    y_scale = (float)height / max_val;
    
    // 3. 计算新曲线的所有点坐标
    uint16_t new_points[100] = {0};
    for (i = 0; i < data_len; i++) {
        if(data[i]>=max_val) data[i]=max_val - 1;//限制曲线的最大值
        
        uint16_t x = x0 + (uint16_t)(i * x_step);
        uint16_t y = y0 - (uint16_t)(data[i] * y_scale);
        new_points[i] = y;
    }
    
    // 4. 绘制新曲线（同时擦除旧曲线）
    for (i = 1; i < data_len; i++) {
        uint16_t prev_x = x0 + (uint16_t)((i-1) * x_step);
        uint16_t curr_x = x0 + (uint16_t)(i * x_step);
        
        // 擦除旧线段（如果存在）
        if (prev_points[i] != 0 && prev_points[i-1] != 0) {
            lcd_draw_line(prev_x, prev_points[i-1], curr_x, prev_points[i], bg_color);
        }
        
        // 绘制新线段
        if (new_points[i] != 0 && new_points[i-1] != 0) {
            lcd_draw_line(prev_x, new_points[i-1], curr_x, new_points[i], color);
        }
    }
    
    // 5. 更新存储的点
    for (i = 0; i < data_len; i++) {
        prev_points[i] = new_points[i];
    }
}

