

#include "./BSP/LCD/lcd_spi.h"
#include "./BSP/LED/led.h"
#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/delay/delay.h"

static SPI_HandleTypeDef g_spi_handle = {0};

/**
 * @brief       ATK-MD0240模块SPI接口初始化
 * @param       无
 * @retval      无
 */
void lcd_spi_init(void)
{
    GPIO_InitTypeDef gpio_init_struct = {0};
    
    /* 使能时钟 */
    //LCD_SPI_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();
    LCD_SPI_SCK_GPIO_CLK_ENABLE();
    LCD_SPI_SDA_GPIO_CLK_ENABLE();
    
    // 检查时钟是否真的使能了
    if (!__HAL_RCC_SPI1_IS_CLK_ENABLED()) {
        // 这里可以添加LED闪烁代码
        while(1) {
            HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
            delay_ms(100);
        }
    }
    
    /* 初始化SCK引脚 */
    gpio_init_struct.Pin        = LCD_SPI_SCK_GPIO_PIN;
    gpio_init_struct.Mode       = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull       = GPIO_NOPULL;
    gpio_init_struct.Speed      = GPIO_SPEED_FREQ_HIGH;
    gpio_init_struct.Alternate  = LCD_SPI_SCK_GPIO_AF;
    HAL_GPIO_Init(LCD_SPI_SCK_GPIO_PORT, &gpio_init_struct);
    
    /* 初始化SDA引脚 */
    gpio_init_struct.Pin        = LCD_SPI_SDA_GPIO_PIN;
    gpio_init_struct.Mode       = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull       = GPIO_NOPULL;
    gpio_init_struct.Speed      = GPIO_SPEED_FREQ_HIGH;
    gpio_init_struct.Alternate  = LCD_SPI_SDA_GPIO_AF;
    HAL_GPIO_Init(LCD_SPI_SDA_GPIO_PORT, &gpio_init_struct);

//    g_spi_handle.Instance = SPI3;
//    g_spi_handle.Init.Mode = SPI_MODE_MASTER;
//    g_spi_handle.Init.Direction = SPI_DIRECTION_2LINES;
//    g_spi_handle.Init.DataSize = SPI_DATASIZE_8BIT;
//    g_spi_handle.Init.CLKPolarity = SPI_POLARITY_HIGH;
//    g_spi_handle.Init.CLKPhase = SPI_PHASE_2EDGE;
//    g_spi_handle.Init.NSS = SPI_NSS_SOFT;
//    g_spi_handle.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
//    g_spi_handle.Init.FirstBit = SPI_FIRSTBIT_MSB;
//    g_spi_handle.Init.TIMode = SPI_TIMODE_DISABLE;
//    g_spi_handle.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
//    g_spi_handle.Init.CRCPolynomial = 10;
    
     /* 初始化SPI */
    g_spi_handle.Instance               = LCD_SPI_INTERFACE;
    g_spi_handle.Init.Mode              = SPI_MODE_MASTER;
    g_spi_handle.Init.Direction         = SPI_DIRECTION_2LINES;
    g_spi_handle.Init.DataSize          = SPI_DATASIZE_8BIT;
    g_spi_handle.Init.CLKPolarity       = SPI_POLARITY_LOW;
    g_spi_handle.Init.CLKPhase          = SPI_PHASE_1EDGE;
    g_spi_handle.Init.NSS               = SPI_NSS_SOFT;
    g_spi_handle.Init.BaudRatePrescaler = LCD_SPI_PRESCALER;
    g_spi_handle.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    g_spi_handle.Init.TIMode            = SPI_TIMODE_DISABLE;
    g_spi_handle.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    g_spi_handle.Init.CRCPolynomial     = 10;
    HAL_SPI_Init(&g_spi_handle);
    __HAL_SPI_ENABLE(&g_spi_handle);

    

}






/**
 * @brief       ATK-MD0240模块SPI接口发送数据
 * @param       无
 * @retval      无
 */
void lcd_spi_send(uint8_t *buf, uint16_t len)
{
    HAL_SPI_Transmit(&g_spi_handle, buf, len, HAL_MAX_DELAY);
}
