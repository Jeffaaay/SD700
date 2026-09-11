#ifndef BOARD_MOTOR_MOTOR_HW_REAL_H
#define BOARD_MOTOR_MOTOR_HW_REAL_H

#include <stdbool.h>
#include <stdint.h>

#include "Board/Motor/motor_diagnostics.h"

bool MotorHwReal_InitializeDisabled(void);
void MotorHwReal_DisableImmediate(void);
bool MotorHwReal_ApplyPress(uint16_t duty_counts);
bool MotorHwReal_ApplyRelease(uint16_t duty_counts);
bool MotorHwReal_IsDisabled(void);
bool MotorHwReal_OutputArmingAllowed(void);
MotorFailureStage MotorHwReal_GetLastFailureStage(void);

#endif
