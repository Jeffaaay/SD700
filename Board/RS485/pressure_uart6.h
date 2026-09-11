#ifndef BOARD_RS485_PRESSURE_UART6_H
#define BOARD_RS485_PRESSURE_UART6_H

#include <stdbool.h>

#include "Transport/Pressure/pressure_receiver.h"
#include "stm32f4xx_hal.h"

bool PressureUart6_Initialize(void);
bool PressureUart6_Reinitialize(void);
void PressureUart6_IrqHandler(void);
const PressureReceiverSnapshot *PressureUart6_GetSnapshot(void);
bool PressureUart6_ReadSnapshot(PressureReceiverSnapshot *snapshot);
bool PressureUart6_HandleRxComplete(UART_HandleTypeDef *uart);
bool PressureUart6_HandleError(UART_HandleTypeDef *uart);

#endif
