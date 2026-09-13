#include <assert.h>
#include <stdio.h>
#include "Application/motion_build_policy.h"
#include "Board/Motor/motor_hw_real.h"

int main(void)
{
    assert(MotorHwReal_OutputArmingAllowed() == (SD700_FORCE_SERVO_COMMISSIONING != 0));
    puts(SD700_FORCE_SERVO_COMMISSIONING ? "COMMISSIONING_GATE=ARMED" : "DEFAULT_FORCE_SERVO_GATE=LOCKED");
    return 0;
}
