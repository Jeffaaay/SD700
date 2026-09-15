#ifndef BOARD_MOTOR_MOTOR_STOP_TIMER_H
#define BOARD_MOTOR_MOTOR_STOP_TIMER_H

#include <stdbool.h>
#include <stdint.h>

#include "Board/Motor/motor_diagnostics.h"

typedef enum
{
    MOTOR_STOP_TIMER_NORMAL = 0,
    MOTOR_STOP_TIMER_BACKSTOP,
    MOTOR_STOP_TIMER_ERROR
} MotorStopTimerEvent;

typedef void (*MotorStopTimerHandler)(MotorStopTimerEvent event);

bool MotorStopTimer_Initialize(MotorStopTimerHandler handler);
bool MotorStopTimer_Arm(uint32_t normal_duration_ms,
                        uint32_t backstop_ms);
bool MotorStopTimer_CommitArm(void);
void MotorStopTimer_Cancel(void);
bool MotorStopTimer_IsArmed(void);
bool MotorStopTimer_IsHealthy(void);
MotorFailureStage MotorStopTimer_GetLastFailureStage(void);
void MotorStopTimer_IrqHandler(void);

bool MotorStopTimer_ArmLease(uint32_t remaining_ms);
bool MotorStopTimer_RenewLease(uint32_t remaining_ms);
/* BUILD-only same-direction transitions. Neither resets CNT nor clears a
 * pending cutoff. Handoff is legal only after verified preload PWM. */
bool MotorStopTimer_BuildHandoff(uint32_t remaining_ms);
bool MotorStopTimer_BuildPulse(uint32_t normal_ms,uint32_t hard_ms);
#endif
