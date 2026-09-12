#include "Application/runtime.h"

#include <stddef.h>
#include <string.h>

#include "Application/pressure_control.h"
#include "Application/motion_build_policy.h"
#if SD700_AUTO_TARGET_ENABLED
#include "Application/auto_target_config.h"
#endif
#include "Board/Motor/motor_executor.h"

void ApplicationRuntime_Initialize(ApplicationRuntime *runtime,
                                   const MachineConfig *config,
                                   uint32_t now_ms)
{
    if (runtime == NULL)
    {
        return;
    }

    (void)memset(runtime, 0, sizeof(*runtime));
    Machine_Initialize(&runtime->machine, config, now_ms);
}

void ApplicationRuntime_CompleteBoot(ApplicationRuntime *runtime,
                                     bool boot_checks_passed,
                                     uint32_t now_ms)
{
    if (runtime != NULL)
    {
        Machine_CompleteBoot(&runtime->machine,
                             boot_checks_passed,
                             now_ms);
#if SD700_AUTO_TARGET_ENABLED
        if ((runtime->machine.state == IDLE) &&
            (Machine_ConfigureForcePi(&runtime->machine,
                &g_sd700_auto_target_force_pi_config) != COMMAND_ACCEPTED))
        {
            Machine_ReportFault(&runtime->machine, FAULT_BOOT_FAULT,
                                FAULT_DETAIL_BOOT_CONFIGURATION, now_ms);
        }
#endif
    }
}

bool ApplicationRuntime_ServicePressure(
    ApplicationRuntime *runtime,
    const PressureReceiverSnapshot *pressure,
    uint32_t now_ms)
{
    MachinePressureSample sample = {0};

#if SD700_FORCE_SERVO_ENABLED
    if (runtime == NULL || pressure == NULL ||
        (!runtime->machine.servo.have_sample && pressure->sample_sequence == 0U) ||
        (runtime->machine.servo.have_sample &&
         pressure->sample_sequence == runtime->delivered_pressure_sequence)) return false;
#else
    if ((runtime == NULL) || (pressure == NULL) ||
        (pressure->sample_sequence == 0U) ||
        (pressure->sample_sequence <=
         runtime->delivered_pressure_sequence))
    {
        return false;
    }

#endif

#if SD700_FORCE_SERVO_ENABLED
    /* Receiver ISR interval extrema are distinct from intervals delivered by main. */
    runtime->machine.servo.diagnostic.rx_interval_min_ms=pressure->frame_interval_min_ms;
    runtime->machine.servo.diagnostic.rx_interval_max_ms=pressure->frame_interval_max_ms;
#endif
    sample.sequence = pressure->sample_sequence;
    sample.received_at_ms = pressure->received_at_ms;
    sample.raw_pressure_counts = pressure->raw_pressure_counts;
    (void)memcpy(sample.frame,
                 pressure->last_valid_frame,
                 sizeof(sample.frame));
    sample.frame_valid = true;
    sample.control_units_valid = PressureControl_ConvertRawCounts(
        pressure->raw_pressure_counts,
        &sample.control_pressure_units);
    runtime->delivered_pressure_sequence = pressure->sample_sequence;
    Machine_HandlePressureSample(&runtime->machine, &sample, now_ms);
    return true;
}

void ApplicationRuntime_ServiceSafety(ApplicationRuntime *runtime,
                                      uint32_t now_ms)
{
    MotorResult motor_result;

    if (runtime == NULL)
    {
        return;
    }

    Machine_CheckPressureSafety(&runtime->machine, now_ms);
    motor_result = MotorExecutor_Service(now_ms);
    if (motor_result != MOTOR_RESULT_OK)
    {
        (void)MotorExecutor_Disable();
        Machine_ReportFault(&runtime->machine,
                            FAULT_MOTOR_FAULT,
                            FAULT_DETAIL_MOTOR_HARDWARE,
                            now_ms);
        return;
    }
    Machine_HandleMotorService(&runtime->machine, now_ms);
}

void ApplicationRuntime_Tick(ApplicationRuntime *runtime, uint32_t now_ms)
{
    if (runtime != NULL)
    {
        Machine_Tick(&runtime->machine, now_ms);
    }
}

MachineContext *ApplicationRuntime_GetMachine(ApplicationRuntime *runtime)
{
    return (runtime == NULL) ? NULL : &runtime->machine;
}
