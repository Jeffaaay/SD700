

#ifndef __LCD_SPI_H
#define __LCD_SPI_H

#include "./SYSTEM/sys/sys.h"

/* SPI接口定义 */
#define LCD_SPI_INTERFACE                SPI1
#define LCD_SPI_PRESCALER                SPI_BAUDRATEPRESCALER_16
#define LCD_SPI_CLK_ENABLE()             do{ __HAL_RCC_SPI1_CLK_ENABLE(); }while(0)

/* 引脚定义 */
#define LCD_SPI_SCK_GPIO_PORT            GPIOA
#define LCD_SPI_SCK_GPIO_PIN             GPIO_PIN_5
#define LCD_SPI_SCK_GPIO_AF              GPIO_AF5_SPI1
#define LCD_SPI_SCK_GPIO_CLK_ENABLE()    do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)

#define LCD_SPI_SDA_GPIO_PORT            GPIOA
#define LCD_SPI_SDA_GPIO_PIN             GPIO_PIN_7
#define LCD_SPI_SDA_GPIO_AF              GPIO_AF5_SPI1
#define LCD_SPI_SDA_GPIO_CLK_ENABLE()    do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)

/* 操作函数 */
void lcd_spi_init(void);                         /* LCD模块SPI接口初始化 */
void lcd_spi_send(uint8_t *buf, uint16_t len);   /* LCD模块SPI接口发送数据 */

#endif
