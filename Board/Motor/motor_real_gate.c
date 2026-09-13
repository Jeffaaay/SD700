#include "Board/Motor/motor_hw_real.h"

#include "Board/Motor/motor_real_config.h"
#include "Application/motion_build_policy.h"

bool MotorHwReal_OutputArmingAllowed(void)
{
    return (SD700_REAL_OUTPUT_ARMING_ENABLED != 0) &&
           (!SD700_FORCE_SERVO_ENABLED || SD700_FORCE_SERVO_COMMISSIONING);
}
