#ifndef BOARD_RS485_MODBUS_UART2_H
#define BOARD_RS485_MODBUS_UART2_H

#include <stdbool.h>
#include <stdint.h>

#include "Application/machine.h"
#include "Transport/Modbus/modbus_rtu_server.h"
#include "Transport/Modbus/modbus_uart_ingress.h"
#include "stm32f4xx_hal.h"

bool ModbusUart2_Initialize(MachineContext *machine);
void ModbusUart2_IrqHandler(void);
void ModbusUart2_ServiceMain(void);
bool ModbusUart2_HasPendingStop(void);
bool ModbusUart2_ProcessPendingStop(uint32_t now_ms);
bool ModbusUart2_ProcessOneNormalRequest(uint32_t now_ms);
void ModbusUart2_ServiceTransmit(void);
bool ModbusUart2_HandleRxComplete(UART_HandleTypeDef *uart);
bool ModbusUart2_HandleTxComplete(UART_HandleTypeDef *uart);
bool ModbusUart2_HandleError(UART_HandleTypeDef *uart);
const ModbusRtuServerSnapshot *ModbusUart2_GetSnapshot(void);
bool ModbusUart2_ReadIngressSnapshot(
    ModbusUartIngressSnapshot *snapshot);

#endif
