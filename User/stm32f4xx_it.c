#include "stm32f4xx_hal.h"
#include "stm32f4xx_it.h"

#include "Board/Motor/motor_hw_force_disable.h"
#if defined(SD700_MOTOR_MODE_REAL_COMPILE_CHECK) || \
    defined(SD700_MOTOR_MODE_SCOPE_TEST) || \
    defined(SD700_MOTOR_MODE_REAL_BENCH)
#include "Board/Motor/motor_stop_timer.h"
#endif
#include "Board/RS485/modbus_uart2.h"
#include "Board/RS485/pressure_uart6.h"

static __NO_RETURN void FaultHalt(void)
{
    for (;;)
    {
        __DSB();
        __WFI();
    }
}

void NMI_Handler(void)
{
    MotorHw_ForceDisableImmediate();
    FaultHalt();
}

void HardFault_Handler(void)
{
    MotorHw_ForceDisableImmediate();
    FaultHalt();
}

void MemManage_Handler(void)
{
    MotorHw_ForceDisableImmediate();
    FaultHalt();
}

void BusFault_Handler(void)
{
    MotorHw_ForceDisableImmediate();
    FaultHalt();
}

void UsageFault_Handler(void)
{
    MotorHw_ForceDisableImmediate();
    FaultHalt();
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

#if defined(SD700_MOTOR_MODE_REAL_COMPILE_CHECK) || \
    defined(SD700_MOTOR_MODE_SCOPE_TEST) || \
    defined(SD700_MOTOR_MODE_REAL_BENCH)
void TIM5_IRQHandler(void)
{
    MotorStopTimer_IrqHandler();
}
#endif

void USART6_IRQHandler(void)
{
    PressureUart6_IrqHandler();
}

void USART2_IRQHandler(void)
{
    ModbusUart2_IrqHandler();
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if (!PressureUart6_HandleRxComplete(uart))
    {
        (void)ModbusUart2_HandleRxComplete(uart);
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{
    (void)ModbusUart2_HandleTxComplete(uart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    if (!PressureUart6_HandleError(uart))
    {
        (void)ModbusUart2_HandleError(uart);
    }
}
