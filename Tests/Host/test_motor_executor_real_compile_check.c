#include <assert.h>
#include <stdio.h>

#include "Board/Motor/motor_executor.h"
#include "Board/Motor/motor_hw_real.h"
#include "Tests/Host/fake_motor_hw_real.h"
#include "Tests/Host/fake_motor_stop_timer.h"

int main(void)
{
    const MotorExecutorSnapshot *snapshot;

    FakeMotorHwReal_Reset();
    FakeMotorStopTimer_Reset();
    assert(MotorExecutor_Initialize() == MOTOR_RESULT_OK);
    assert(MotorExecutor_IsHealthy());

    assert(MotorExecutor_StartRun(MOTOR_DIRECTION_PRESS,
                                  6000U,
                                  50U,
                                  60U,
                                  0U) == MOTOR_RESULT_HARDWARE_ERROR);
    snapshot = MotorExecutor_GetSnapshot();
    assert(snapshot->physical_output_locked);
    assert(snapshot->physical_output_disabled);
    assert(!snapshot->logical_active);
    assert(MotorHwReal_IsDisabled());
    assert(FakeMotorStopTimer_GetState()->arm_count == 0U);
    assert(FakeMotorHwReal_GetState()->apply_count == 0U);

    assert(MotorExecutor_StartPulse(MOTOR_DIRECTION_PRESS,
                                    0U,
                                    10U,
                                    20U,
                                    0U) == MOTOR_RESULT_INVALID);
    assert(MotorHwReal_IsDisabled());
    puts("RealCompileCheck fail-safe host test: PASS");
    return 0;
}
