#ifndef BOARD_MOTOR_MOTOR_EXECUTOR_H
#define BOARD_MOTOR_MOTOR_EXECUTOR_H

#include <stdbool.h>
#include <stdint.h>

#include "Board/Motor/motor_diagnostics.h"

typedef enum
{
    MOTOR_DIRECTION_PRESS = 0,
    MOTOR_DIRECTION_RELEASE
} MotorDirection;

typedef enum
{
    MOTOR_RESULT_OK = 0,
    MOTOR_RESULT_INVALID,
    MOTOR_RESULT_BUSY,
    MOTOR_RESULT_TIMER_ERROR,
    MOTOR_RESULT_HARDWARE_ERROR
} MotorResult;

typedef enum
{
    MOTOR_ACTION_DISABLED = 0,
    MOTOR_ACTION_PRESS_RUN,
    MOTOR_ACTION_RELEASE_RUN,
    MOTOR_ACTION_PRESS_PULSE,
    MOTOR_ACTION_RELEASE_PULSE
} MotorAction;

typedef enum
{
    MOTOR_COMPLETION_NONE = 0,
    MOTOR_COMPLETION_NORMAL,
    MOTOR_COMPLETION_BACKSTOP,
    MOTOR_COMPLETION_ERROR
} MotorCompletion;

typedef struct
{
    MotorAction last_action;
    MotorDirection direction;
    MotorCompletion last_completion;
    uint32_t command_mv;
    uint32_t requested_duration_ms;
    uint32_t logical_deadline_ms;
    uint32_t logical_backstop_ms;
    uint32_t request_sequence;
    uint16_t planned_tim2_ccr3;
    uint16_t planned_tim3_ccr3;
    bool logical_active;
    bool physical_output_locked;
    bool physical_output_disabled;
    MotorResult last_failure_result;
    MotorFailureStage last_failure_stage;
} MotorExecutorSnapshot;

MotorResult MotorExecutor_Initialize(void);
MotorResult MotorExecutor_Disable(void);
MotorResult MotorExecutor_StartRun(MotorDirection direction,
                                   uint32_t command_mv,
                                   uint32_t stop_after_ms,
                                   uint32_t hard_stop_after_ms,
                                   uint32_t now_ms);
MotorResult MotorExecutor_StartPulse(MotorDirection direction,
                                     uint32_t command_mv,
                                     uint32_t duration_ms,
                                     uint32_t backstop_ms,
                                     uint32_t now_ms);
MotorResult MotorExecutor_Service(uint32_t now_ms);
MotorResult MotorExecutor_GuardOutput(void);
bool MotorExecutor_IsHealthy(void);
bool MotorExecutor_ActiveRequestIsValid(void);
bool MotorExecutor_OutputIsDisabled(void);
const MotorExecutorSnapshot *MotorExecutor_GetStatus(void);
const MotorExecutorSnapshot *MotorExecutor_GetSnapshot(void);
MotorResult MotorExecutor_PlanCommand(MotorDirection direction,
                                      uint32_t command_mv,
                                      uint16_t *planned_tim2_ccr3,
                                      uint16_t *planned_tim3_ccr3);

#endif
