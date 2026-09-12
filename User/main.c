#include "stm32f4xx_hal.h"

#include "Application/bench_config.h"
#include "Application/motion_build_policy.h"
#if SD700_AUTO_TARGET_ENABLED || SD700_FORCE_SERVO_ENABLED
#include "Application/auto_target_config.h"
#endif
#include "Application/runtime.h"
#include "Application/safe_boot.h"
#include "Application/safe_idle.h"
#include "Board/Motor/motor_executor.h"
#include "Board/Motor/motor_hw_early_force_disable.h"
#include "Board/RS485/modbus_uart2.h"
#include "Board/RS485/pressure_uart6.h"
#include "SYSTEM/sys/sys.h"

static ApplicationRuntime s_runtime;

int main(void)
{
    PressureReceiverSnapshot pressure_snapshot;
    MachineContext *machine;
    bool stop_processed;
    uint32_t now_ms;

    MotorHw_EarlyForceDisable();
    HAL_Init();
    if (sys_stm32_clock_init(96U, 4U, 2U, 4U) != 0U)
    {
        SafeIdle_Run();
    }
    SafeBoot_Initialize();

    if (MotorExecutor_Initialize() != MOTOR_RESULT_OK)
    {
        SafeIdle_Run();
    }
    ApplicationRuntime_Initialize(&s_runtime,
#if SD700_AUTO_TARGET_ENABLED || SD700_FORCE_SERVO_ENABLED
                                  &g_sd700_auto_target_machine_config,
#else
                                  &g_sd700_bench_machine_config,
#endif
                                  HAL_GetTick());
    machine = ApplicationRuntime_GetMachine(&s_runtime);
    if (!PressureUart6_Initialize())
    {
        SafeIdle_Run();
    }

    /* Discard the reset-release sample and restart USART6 after the external
       RS485 transmitter has been streaming for a full second. */
    HAL_Delay(1000U);
    if ((!PressureUart6_Reinitialize()) ||
        (!ModbusUart2_Initialize(machine)))
    {
        SafeIdle_Run();
    }

    ApplicationRuntime_CompleteBoot(&s_runtime, true, HAL_GetTick());
    if ((machine == NULL) || (machine->state != IDLE))
    {
        SafeIdle_Run();
    }

    for (;;)
    {
        now_ms = HAL_GetTick();

#if SD700_FORCE_SERVO_ENABLED
        /* Drain bounded ingress and apply any queued STOP before new PID work. */
        ModbusUart2_ServiceMain();
        stop_processed = ModbusUart2_ProcessPendingStop(HAL_GetTick());
#endif
        /* 1. Deliver each accepted USART6 pressure sequence at most once. */
        if (PressureUart6_ReadSnapshot(&pressure_snapshot))
        {
#if SD700_AUTO_TARGET_ENABLED || SD700_FORCE_SERVO_ENABLED
            /* The receive ISR can publish across a tick after loop entry.
             * Date the copied snapshot with current time, not an older tick. */
            now_ms = HAL_GetTick();
#endif
            (void)ApplicationRuntime_ServicePressure(&s_runtime,
                                                     &pressure_snapshot,
                                                     now_ms);
        }

        /* 2-4. Pressure safety, selected executor service, motor faults. */
        ApplicationRuntime_ServiceSafety(&s_runtime, now_ms);

        /* 5. Guard after Service has handled pending timer completions. */
        if (MotorExecutor_GuardOutput() != MOTOR_RESULT_OK)
        {
            Machine_ReportFault(machine,
                                FAULT_MOTOR_FAULT,
                                FAULT_DETAIL_MOTOR_HARDWARE,
                                now_ms);
        }

        /* 6. Main alone drains/parses USART2 data and link events. */
        ModbusUart2_ServiceMain();

        /* 7. STOP has priority over every normal Modbus request. */
        stop_processed = ModbusUart2_ProcessPendingStop(now_ms);

        /* 8. At most one non-STOP request is handled per iteration. */
        if (!stop_processed)
        {
            (void)ModbusUart2_ProcessOneNormalRequest(now_ms);
        }

        /* 9-10. Advance state timing, then publish/send diagnostics. */
        ApplicationRuntime_Tick(&s_runtime, now_ms);
        ModbusUart2_ServiceTransmit();
    }
}
