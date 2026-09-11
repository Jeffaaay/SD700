#ifndef TESTS_HOST_FAKE_MOTOR_STOP_TIMER_H
#define TESTS_HOST_FAKE_MOTOR_STOP_TIMER_H

#include <stdbool.h>
#include <stdint.h>

#include "Board/Motor/motor_stop_timer.h"

typedef struct
{
    MotorStopTimerHandler handler;
    uint32_t normal_duration_ms;
    uint32_t backstop_ms;
    uint32_t arm_count;
    uint32_t cancel_count;
    bool initialized;
    bool armed;
    bool healthy;
    bool fail_next_arm;
    bool fail_next_commit;
    bool fail_next_cancel;
    bool hold_armed_on_cancel;
    bool trigger_in_progress;
    bool trigger_on_next_armed_query;
    MotorStopTimerEvent next_armed_query_event;
    bool armed_query_returns_previous_state;
    bool trigger_on_next_healthy_query;
    MotorStopTimerEvent next_healthy_query_event;
    uint32_t expiry_count;
    uint32_t armed_query_count;
    bool expiry_cancel_saw_output_disabled;
    MotorFailureStage last_failure_stage;
} FakeMotorStopTimerState;

void FakeMotorStopTimer_Reset(void);
void FakeMotorStopTimer_FailNextArm(void);
void FakeMotorStopTimer_FailNextCommit(void);
void FakeMotorStopTimer_FailNextCancelVerification(void);
void FakeMotorStopTimer_ForceUnhealthy(void);
void FakeMotorStopTimer_HoldArmedOnCancel(bool hold);
void FakeMotorStopTimer_TriggerNormal(void);
void FakeMotorStopTimer_TriggerBackstop(void);
void FakeMotorStopTimer_TriggerError(void);
void FakeMotorStopTimer_TriggerNormalOnNextArmedQuery(void);
void FakeMotorStopTimer_TriggerOnNextArmedQuery(MotorStopTimerEvent event);
void FakeMotorStopTimer_TriggerOnNextArmedQueryReturningPreviousState(
    MotorStopTimerEvent event);
void FakeMotorStopTimer_TriggerOnNextHealthyQuery(MotorStopTimerEvent event);
const FakeMotorStopTimerState *FakeMotorStopTimer_GetState(void);

#endif
