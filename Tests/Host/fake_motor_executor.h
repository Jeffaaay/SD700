#ifndef TESTS_HOST_FAKE_MOTOR_EXECUTOR_H
#define TESTS_HOST_FAKE_MOTOR_EXECUTOR_H

#include <stdbool.h>
#include <stdint.h>

#include "Board/Motor/motor_executor.h"

typedef enum
{
    FAKE_MOTOR_CALL_NONE = 0,
    FAKE_MOTOR_CALL_DISABLE,
    FAKE_MOTOR_CALL_START_RUN,
    FAKE_MOTOR_CALL_START_PULSE,
    FAKE_MOTOR_CALL_SERVICE
} FakeMotorCall;

typedef struct
{
    bool output_enabled;
    bool fail_next_start;
    uint32_t disable_count;
    uint32_t start_run_count;
    uint32_t start_pulse_count;
    uint32_t service_count;
    FakeMotorCall first_call;
    MotorExecutorSnapshot snapshot;
} FakeMotorExecutorState;

void FakeMotorExecutor_Reset(void);
void FakeMotorExecutor_FailNextStart(void);
void FakeMotorExecutor_Complete(MotorCompletion completion);
const FakeMotorExecutorState *FakeMotorExecutor_GetState(void);

#endif
