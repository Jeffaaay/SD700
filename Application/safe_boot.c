#include "Application/safe_boot.h"

#include "Board/Motor/motor_hw_init_disabled.h"

void SafeBoot_Initialize(void)
{
    MotorHw_InitDisabled();
}
