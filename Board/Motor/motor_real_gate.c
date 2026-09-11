#include "Board/Motor/motor_hw_real.h"

#include "Board/Motor/motor_real_config.h"

bool MotorHwReal_OutputArmingAllowed(void)
{
    return SD700_REAL_OUTPUT_ARMING_ENABLED != 0;
}
