#include "Application/safe_idle.h"

#include "Board/Motor/motor_hw_force_disable.h"
#include "stm32f4xx.h"

__NO_RETURN void SafeIdle_Run(void)
{
    MotorHw_ForceDisable();

    for (;;)
    {
        __WFI();
    }
}
