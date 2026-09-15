#ifndef BOARD_MOTOR_MOTOR_EXECUTOR_H
#define BOARD_MOTOR_MOTOR_EXECUTOR_H

#include <stdbool.h>
#include <stdint.h>
#include "Application/motion_build_policy.h"

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
    MOTOR_ACTION_RELEASE_PULSE,
    MOTOR_ACTION_CONTINUOUS,
    MOTOR_ACTION_BUILD_SEGMENT,
    MOTOR_ACTION_BUILD_PRELOAD
} MotorAction;

typedef enum
{
    MOTOR_COMPLETION_NONE = 0,
    MOTOR_COMPLETION_NORMAL,
    MOTOR_COMPLETION_BACKSTOP,
    MOTOR_COMPLETION_ERROR,
    MOTOR_COMPLETION_LEASE
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
    uint32_t boost_started_ms, boost_deadline_ms, boost_planned_end_ms;
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

/* Continuous mode is available only in the mutually exclusive ForceServo build.
 * Tokens become invalid on STOP/expiry. Deadline is anchored to sample reception. */
bool MotorExecutor_ContinuousExpired(void);
MotorResult MotorExecutor_BeginContinuous(uint32_t *token);
/* Fixed monotonic deadlines; timer compare is min(receive lease, session,
 * active high-output deadline). A new sample cannot extend either budget. */
/* duration_ms==0 is supported only by runtime characterization: no absolute
 * session deadline, while normal_cap, receive lease and assist cutoff still apply. */
bool MotorExecutor_SetContinuousBudget(uint32_t token,uint32_t now_ms,uint32_t duration_ms,int32_t normal_cap);
bool MotorExecutor_ArmContinuousBoost(uint32_t token,uint32_t now_ms,uint32_t duration_ms);
bool MotorExecutor_EndContinuousBoost(uint32_t token);
/* Prevalidated short assist plan; only the existing owner may service it.
 * Rise/completion are bounded by the original independent hard compare. */
bool MotorExecutor_SetContinuousBoostPlan(uint32_t token,uint32_t now_ms,uint32_t rise_ms,
    uint32_t end_ms,int32_t peak,int32_t handoff);
MotorResult MotorExecutor_ServiceContinuousBoost(uint32_t token,uint32_t now_ms,
    int32_t *committed,bool *finished);
int32_t MotorExecutor_ContinuousBoostCommand(uint32_t token,uint32_t now_ms);
/* Reduction only, same owner/sample lease; cannot start output or renew feedback. */
MotorResult MotorExecutor_HandoffContinuousBoost(uint32_t token,uint32_t now_ms,int32_t requested_mv);
MotorResult MotorExecutor_UpdateContinuous(uint32_t token, uint64_t sequence,
    uint32_t received_ms, uint32_t now_ms, uint32_t lease_ms, uint32_t max_age_ms,
    uint32_t deadtime_ms, int32_t requested_mv, int32_t *committed_mv, bool *interlocked);

#if SD700_BUILD_TO_TARGET
#include "Application/force_build.h"
typedef struct {
 uint32_t epoch, reserved_ms, approach_reserved_ms, energized_upper_ms;
 uint32_t rest_remaining_ms, inhibited, request, phase, command, hard_ms;
 uint32_t started_ms, ended_ms, deadline_ms, receive_deadline_ms, end_reason;
 uint32_t base_command, mode, approach_command_ms;
 uint32_t preload_command, preload_started_ms, preload_deadline_ms, preload_credit_ms, preload_spent_ms;
 bool segment_active, post_pending, preload_active;
} MotorBuildSnapshot;
MotorResult MotorExecutor_BeginBuild(uint32_t now_ms,uint32_t *token);
MotorResult MotorExecutor_StartBuildSegment(uint32_t token,const ForceBuildRequest *request,
 uint64_t sequence,uint32_t received_ms,uint32_t now_ms);
MotorResult MotorExecutor_EndBuildSegment(uint32_t token,uint32_t now_ms);
bool MotorExecutor_AcceptBuildPost(uint32_t token,uint32_t request,uint64_t sequence,
 uint32_t received_ms,uint32_t now_ms);
bool MotorExecutor_BuildOwnerValid(uint32_t token);
MotorBuildSnapshot MotorExecutor_GetBuildSnapshot(uint32_t now_ms);
#endif
#endif
