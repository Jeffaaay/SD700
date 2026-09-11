#include "Board/RS485/modbus_uart2.h"

#include <stddef.h>

#include "Board/RS485/rs485_board_map.h"
#include "Transport/Modbus/modbus_rtu_link.h"

static UART_HandleTypeDef s_modbus_uart2;
static uint8_t s_modbus_rx_byte;
static ModbusRtuServer s_modbus_server;
static ModbusRtuLink s_modbus_link;
static ModbusUartIngress s_modbus_ingress;

static uint32_t ModbusUart2_EnterMainTransferCritical(void)
{
    uint32_t irq_was_enabled = NVIC_GetEnableIRQ(USART2_IRQn);

    HAL_NVIC_DisableIRQ(USART2_IRQn);
    __DMB();
    return irq_was_enabled;
}

static void ModbusUart2_LeaveMainTransferCritical(uint32_t irq_was_enabled)
{
    __DMB();
    if (irq_was_enabled != 0U)
    {
        HAL_NVIC_EnableIRQ(USART2_IRQn);
    }
}

static void ModbusUart2_SetDriverEnable(void *context, bool transmit)
{
    (void)context;
    HAL_GPIO_WritePin(RS485_BOARD_PORT1_DE_RE_PORT,
                      RS485_BOARD_PORT1_DE_RE_PIN,
                      transmit ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static bool ModbusUart2_StartTransmit(void *context,
                                      const uint8_t *bytes,
                                      size_t byte_count)
{
    UART_HandleTypeDef *uart = (UART_HandleTypeDef *)context;
    HAL_StatusTypeDef transmit_status;

    if ((uart == NULL) || (bytes == NULL) ||
        (byte_count == 0U) || (byte_count > UINT16_MAX))
    {
        return false;
    }

    /* The RS485 receiver is disabled while DE is asserted. Leaving the
       one-byte RX interrupt armed can raise a UART error and drop DE before
       the response has finished transmitting. */
    if (HAL_UART_AbortReceive(uart) != HAL_OK)
    {
        return false;
    }
    transmit_status = HAL_UART_Transmit_IT(uart,
                                           (uint8_t *)(uintptr_t)bytes,
                                           (uint16_t)byte_count);
    if (transmit_status != HAL_OK)
    {
        ModbusUartIngress_RecordRearmFromIsr(
            &s_modbus_ingress,
            HAL_UART_Receive_IT(&s_modbus_uart2,
                                &s_modbus_rx_byte,
                                1U) == HAL_OK);
        return false;
    }
    return true;
}

static void ModbusUart2_ConfigurePins(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    HAL_GPIO_WritePin(RS485_BOARD_PORT1_DE_RE_PORT,
                      RS485_BOARD_PORT1_DE_RE_PIN,
                      GPIO_PIN_RESET);
    gpio.Pin = RS485_BOARD_PORT1_DE_RE_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(RS485_BOARD_PORT1_DE_RE_PORT, &gpio);

    gpio.Pin = RS485_BOARD_PORT1_TX_PIN | RS485_BOARD_PORT1_RX_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);
}

bool ModbusUart2_Initialize(MachineContext *machine)
{
    if (machine == NULL)
    {
        return false;
    }

    ModbusRtuServer_Initialize(&s_modbus_server, machine);
    ModbusUartIngress_Initialize(&s_modbus_ingress);
    ModbusUart2_ConfigurePins();
    ModbusRtuLink_Initialize(&s_modbus_link,
                             &s_modbus_server,
                             ModbusUart2_SetDriverEnable,
                             ModbusUart2_StartTransmit,
                             &s_modbus_uart2);
    __HAL_RCC_USART2_CLK_ENABLE();

    s_modbus_uart2.Instance = RS485_BOARD_PORT1_UART;
    s_modbus_uart2.Init.BaudRate = RS485_BOARD_LEGACY_BAUD_RATE;
    s_modbus_uart2.Init.WordLength = UART_WORDLENGTH_8B;
    s_modbus_uart2.Init.StopBits = UART_STOPBITS_1;
    s_modbus_uart2.Init.Parity = UART_PARITY_NONE;
    s_modbus_uart2.Init.Mode = UART_MODE_TX_RX;
    s_modbus_uart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    s_modbus_uart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&s_modbus_uart2) != HAL_OK)
    {
        return false;
    }

    HAL_NVIC_SetPriority(USART2_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    if (HAL_UART_Receive_IT(&s_modbus_uart2,
                            &s_modbus_rx_byte,
                            1U) != HAL_OK)
    {
        HAL_NVIC_DisableIRQ(USART2_IRQn);
        return false;
    }
    return true;
}

void ModbusUart2_IrqHandler(void)
{
    HAL_UART_IRQHandler(&s_modbus_uart2);
}

void ModbusUart2_ServiceMain(void)
{
    uint8_t bytes[MODBUS_UART_INGRESS_CAPACITY - 1U];
    size_t byte_count;
    uint32_t events;
    uint32_t irq_was_enabled;

    /* Only the bounded byte/event transfer is protected; parsing is not. */
    irq_was_enabled = ModbusUart2_EnterMainTransferCritical();
    byte_count = ModbusUartIngress_DrainFromMain(
        &s_modbus_ingress,
        bytes,
        sizeof(bytes));
    events = ModbusUartIngress_TakeEventsFromMain(&s_modbus_ingress);
    ModbusUart2_LeaveMainTransferCritical(irq_was_enabled);

    if (byte_count != 0U)
    {
        ModbusRtuServer_ProcessBytes(&s_modbus_server,
                                     bytes,
                                     byte_count);
    }

    if ((events & MODBUS_UART_EVENT_ERROR) != 0U)
    {
        ModbusRtuLink_OnError(&s_modbus_link);
    }
    else if ((events & MODBUS_UART_EVENT_TX_COMPLETE) != 0U)
    {
        ModbusRtuLink_OnTransmitComplete(&s_modbus_link);
    }
}

bool ModbusUart2_HasPendingStop(void)
{
    return ModbusRtuServer_PendingIsStop(&s_modbus_server);
}

bool ModbusUart2_ProcessPendingStop(uint32_t now_ms)
{
    return ModbusRtuServer_TakeStop(&s_modbus_server, now_ms);
}

bool ModbusUart2_ProcessOneNormalRequest(uint32_t now_ms)
{
    if ((!ModbusRtuServer_HasPendingRequest(&s_modbus_server)) ||
        ModbusRtuServer_PendingIsStop(&s_modbus_server))
    {
        return false;
    }
    return ModbusRtuServer_ProcessPending(&s_modbus_server, now_ms);
}

void ModbusUart2_ServiceTransmit(void)
{
    (void)ModbusRtuLink_Service(&s_modbus_link);
}

bool ModbusUart2_HandleRxComplete(UART_HandleTypeDef *uart)
{
    if ((uart == NULL) || (uart->Instance != RS485_BOARD_PORT1_UART))
    {
        return false;
    }

    (void)ModbusUartIngress_PushFromIsr(&s_modbus_ingress,
                                       s_modbus_rx_byte);
    ModbusUartIngress_RecordRearmFromIsr(
        &s_modbus_ingress,
        HAL_UART_Receive_IT(&s_modbus_uart2,
                            &s_modbus_rx_byte,
                            1U) == HAL_OK);
    return true;
}

bool ModbusUart2_HandleTxComplete(UART_HandleTypeDef *uart)
{
    if ((uart == NULL) || (uart->Instance != RS485_BOARD_PORT1_UART))
    {
        return false;
    }

    HAL_GPIO_WritePin(RS485_BOARD_PORT1_DE_RE_PORT,
                      RS485_BOARD_PORT1_DE_RE_PIN,
                      GPIO_PIN_RESET);
    ModbusUartIngress_SignalEventFromIsr(
        &s_modbus_ingress,
        MODBUS_UART_EVENT_TX_COMPLETE);
    ModbusUartIngress_RecordRearmFromIsr(
        &s_modbus_ingress,
        HAL_UART_Receive_IT(&s_modbus_uart2,
                            &s_modbus_rx_byte,
                            1U) == HAL_OK);
    return true;
}

bool ModbusUart2_HandleError(UART_HandleTypeDef *uart)
{
    if ((uart == NULL) || (uart->Instance != RS485_BOARD_PORT1_UART))
    {
        return false;
    }

    HAL_GPIO_WritePin(RS485_BOARD_PORT1_DE_RE_PORT,
                      RS485_BOARD_PORT1_DE_RE_PIN,
                      GPIO_PIN_RESET);
    ModbusUartIngress_SignalEventFromIsr(&s_modbus_ingress,
                                        MODBUS_UART_EVENT_ERROR);
    ModbusUartIngress_RecordRearmFromIsr(
        &s_modbus_ingress,
        HAL_UART_Receive_IT(&s_modbus_uart2,
                            &s_modbus_rx_byte,
                            1U) == HAL_OK);
    return true;
}

const ModbusRtuServerSnapshot *ModbusUart2_GetSnapshot(void)
{
    return ModbusRtuServer_GetSnapshot(&s_modbus_server);
}

bool ModbusUart2_ReadIngressSnapshot(
    ModbusUartIngressSnapshot *snapshot)
{
    uint32_t irq_was_enabled;

    if (snapshot == NULL)
    {
        return false;
    }
    irq_was_enabled = ModbusUart2_EnterMainTransferCritical();
    ModbusUartIngress_ReadSnapshotFromMain(&s_modbus_ingress, snapshot);
    ModbusUart2_LeaveMainTransferCritical(irq_was_enabled);
    return true;
}
