#ifndef TESTS_HOST_FAKE_MOTOR_HW_REAL_H
#define TESTS_HOST_FAKE_MOTOR_HW_REAL_H

#include <stdbool.h>
#include <stdint.h>

#include "Board/Motor/motor_diagnostics.h"

typedef struct
{
    uint16_t tim2_ccr3;
    uint16_t tim3_ccr3;
    uint32_t disable_count;
    uint32_t apply_count;
    bool tim2_enabled;
    bool tim3_enabled;
    bool shutdown_enabled;
    bool driver_enabled;
    bool initialized;
    bool fail_next_apply;
    bool fail_next_disable;
    bool hold_output_enabled;
    bool both_legs_active_violation;
    bool last_apply_saw_timer_armed;
    bool last_apply_started_disabled;
    MotorFailureStage last_failure_stage;
} FakeMotorHwRealState;

void FakeMotorHwReal_Reset(void);
void FakeMotorHwReal_FailNextApply(void);
void FakeMotorHwReal_FailNextDisableVerification(void);
void FakeMotorHwReal_ForceOutputDropped(void);
void FakeMotorHwReal_ForceUnexpectedPressOutput(uint16_t duty_counts);
void FakeMotorHwReal_HoldOutputEnabled(bool hold);
void FakeMotorHwReal_OnDisabledQuery(uint32_t query_count, void (*hook)(void));
void FakeMotorHwReal_OnNextDisable(void (*hook)(void));
const FakeMotorHwRealState *FakeMotorHwReal_GetState(void);

#endif
