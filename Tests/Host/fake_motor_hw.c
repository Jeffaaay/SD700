#include "Tests/Host/fake_motor_hw.h"

#include "Board/Motor/motor_hw_force_disable.h"

static uint32_t s_disable_count;

void FakeMotorHw_Reset(void)
{
    s_disable_count = 0U;
}

uint32_t FakeMotorHw_GetDisableCount(void)
{
    return s_disable_count;
}

void MotorHw_ForceDisableImmediate(void)
{
    ++s_disable_count;
}

void MotorHw_ForceDisable(void)
{
    MotorHw_ForceDisableImmediate();
}
