#include "Board/RS485/pressure_uart6.h"

#include <stddef.h>

#include "Board/RS485/rs485_board_map.h"
#include "stm32f4xx_hal.h"

static UART_HandleTypeDef s_pressure_uart6;
static uint8_t s_pressure_rx_byte;
static PressureReceiver s_pressure_receiver;

typedef struct
{
    uint32_t rx_complete_count;
    uint32_t error_count;
    uint32_t overrun_error_count;
    uint32_t frame_error_count;
    uint32_t noise_error_count;
    uint32_t parity_error_count;
    uint32_t abort_failure_count;
    uint32_t rearm_failure_count;
    uint32_t last_error_code;
    uint32_t last_rx_state;
} PressureUart6Diagnostics;

static PressureUart6Diagnostics s_pressure_uart6_diagnostics;

static bool PressureUart6_ArmReceive(bool reset_receive_state)
{
    if (reset_receive_state &&
        (HAL_UART_AbortReceive(&s_pressure_uart6) != HAL_OK))
    {
        ++s_pressure_uart6_diagnostics.abort_failure_count;
    }

    if (HAL_UART_Receive_IT(&s_pressure_uart6,
                            &s_pressure_rx_byte,
                            1U) != HAL_OK)
    {
        ++s_pressure_uart6_diagnostics.rearm_failure_count;
        return false;
    }
    return true;
}

static void PressureUart6_ConfigurePins(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* Latch receive mode before PC8 becomes an output. */
    HAL_GPIO_WritePin(RS485_BOARD_PORT2_DE_RE_PORT,
                      RS485_BOARD_PORT2_DE_RE_PIN,
                      GPIO_PIN_RESET);
    gpio.Pin = RS485_BOARD_PORT2_DE_RE_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(RS485_BOARD_PORT2_DE_RE_PORT, &gpio);

    gpio.Pin = RS485_BOARD_PORT2_TX_PIN | RS485_BOARD_PORT2_RX_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOC, &gpio);
}

bool PressureUart6_Initialize(void)
{
    s_pressure_uart6_diagnostics = (PressureUart6Diagnostics){0};
    PressureReceiver_Initialize(&s_pressure_receiver);
    PressureUart6_ConfigurePins();
    __HAL_RCC_USART6_CLK_ENABLE();

    s_pressure_uart6.Instance = RS485_BOARD_PORT2_UART;
    s_pressure_uart6.Init.BaudRate = RS485_BOARD_LEGACY_BAUD_RATE;
    s_pressure_uart6.Init.WordLength = UART_WORDLENGTH_8B;
    s_pressure_uart6.Init.StopBits = UART_STOPBITS_1;
    s_pressure_uart6.Init.Parity = UART_PARITY_NONE;
    s_pressure_uart6.Init.Mode = UART_MODE_TX_RX;
    s_pressure_uart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    s_pressure_uart6.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&s_pressure_uart6) != HAL_OK)
    {
        return false;
    }

    HAL_NVIC_SetPriority(USART6_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(USART6_IRQn);
    if (!PressureUart6_ArmReceive(false))
    {
        HAL_NVIC_DisableIRQ(USART6_IRQn);
        return false;
    }

    return true;
}

bool PressureUart6_Reinitialize(void)
{
    HAL_NVIC_DisableIRQ(USART6_IRQn);
    (void)HAL_UART_AbortReceive(&s_pressure_uart6);
    if (HAL_UART_DeInit(&s_pressure_uart6) != HAL_OK)
    {
        return false;
    }
    if (HAL_UART_Init(&s_pressure_uart6) != HAL_OK)
    {
        return false;
    }

    PressureReceiver_Initialize(&s_pressure_receiver);
    HAL_NVIC_EnableIRQ(USART6_IRQn);
    return PressureUart6_ArmReceive(false);
}

void PressureUart6_IrqHandler(void)
{
    HAL_UART_IRQHandler(&s_pressure_uart6);
}

const PressureReceiverSnapshot *PressureUart6_GetSnapshot(void)
{
    return PressureReceiver_GetSnapshot(&s_pressure_receiver);
}

bool PressureUart6_ReadSnapshot(PressureReceiverSnapshot *snapshot)
{
    uint32_t primask;
    const PressureReceiverSnapshot *source;

    if (snapshot == NULL)
    {
        return false;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    source = PressureReceiver_GetSnapshot(&s_pressure_receiver);
    *snapshot = *source;
    if (primask == 0U)
    {
        __enable_irq();
    }
    return true;
}

bool PressureUart6_HandleRxComplete(UART_HandleTypeDef *uart)
{
    if ((uart == NULL) || (uart->Instance != RS485_BOARD_PORT2_UART))
    {
        return false;
    }

    ++s_pressure_uart6_diagnostics.rx_complete_count;
    PressureReceiver_ProcessBytes(&s_pressure_receiver,
                                  &s_pressure_rx_byte,
                                  1U,
                                  HAL_GetTick());
    if (!PressureUart6_ArmReceive(false))
    {
        (void)PressureUart6_ArmReceive(true);
    }
    return true;
}

bool PressureUart6_HandleError(UART_HandleTypeDef *uart)
{
    if ((uart == NULL) || (uart->Instance != RS485_BOARD_PORT2_UART))
    {
        return false;
    }

    ++s_pressure_uart6_diagnostics.error_count;
    s_pressure_uart6_diagnostics.last_error_code = uart->ErrorCode;
    s_pressure_uart6_diagnostics.last_rx_state = uart->RxState;
    if ((uart->ErrorCode & HAL_UART_ERROR_ORE) != 0U)
    {
        ++s_pressure_uart6_diagnostics.overrun_error_count;
    }
    if ((uart->ErrorCode & HAL_UART_ERROR_FE) != 0U)
    {
        ++s_pressure_uart6_diagnostics.frame_error_count;
    }
    if ((uart->ErrorCode & HAL_UART_ERROR_NE) != 0U)
    {
        ++s_pressure_uart6_diagnostics.noise_error_count;
    }
    if ((uart->ErrorCode & HAL_UART_ERROR_PE) != 0U)
    {
        ++s_pressure_uart6_diagnostics.parity_error_count;
    }
    (void)PressureUart6_ArmReceive(true);
    return true;
}
