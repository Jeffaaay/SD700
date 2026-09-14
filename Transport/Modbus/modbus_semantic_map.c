#include "Transport/Modbus/modbus_semantic_map.h"

#include <limits.h>
#include <stddef.h>

#include "Application/bench_config.h"
#include "Application/motion_build_policy.h"
#if SD700_FORCE_SERVO_ENABLED
#include "Transport/Modbus/force_servo_protocol.h"
#endif
#include "Board/Motor/motor_executor.h"
#include "Protocol/Modbus/modbus_protocol_constants.h"
#include "Protocol/Modbus/modbus_register_map.h"

static uint16_t ModbusSemantic_SaturateToU16(uint32_t value)
{
    return (value > UINT16_MAX) ? UINT16_MAX : (uint16_t)value;
}

MachineCommandResult ModbusSemantic_ApplyWrite(MachineContext *context,
                                               uint8_t function,
                                               uint16_t address,
                                               uint16_t value,
                                               uint32_t now_ms)
{
    MachineCommand command = {CMD_STOP, 0};
#if SD700_FORCE_SERVO_ENABLED
    if (ForceServoProtocol_WriteAddress(function,address))
        return ForceServoProtocol_Write(context,function,address,value,now_ms);
#endif

    if (context == NULL)
    {
        return COMMAND_INVALID_VALUE;
    }

    if ((function == MODBUS_FUNCTION_WRITE_SINGLE_REGISTER) &&
        (address == MODBUS_HOLDING_TARGET_PRESSURE))
    {
        command.type = CMD_SET_TARGET;
        command.target_pressure_units = (int32_t)value;
        return Machine_HandleCommand(context, &command, now_ms);
    }

    if ((function == MODBUS_FUNCTION_WRITE_SINGLE_COIL) &&
        (address == MODBUS_COIL_AUTO_PRESSURE))
    {
        if (value == MODBUS_SINGLE_COIL_ON)
        {
            command.type = CMD_AUTO_START;
        }
        else if (value == MODBUS_SINGLE_COIL_OFF)
        {
            command.type = CMD_STOP;
        }
        else
        {
            return COMMAND_INVALID_VALUE;
        }
        return Machine_HandleCommand(context, &command, now_ms);
    }

    if ((function == MODBUS_FUNCTION_WRITE_SINGLE_COIL) &&
        ((address == MODBUS_COIL_DIRECT_PRESS_PULSE) ||
         (address == MODBUS_COIL_DIRECT_RELEASE_PULSE)))
    {
        if (value != MODBUS_SINGLE_COIL_ON)
        {
            return COMMAND_INVALID_VALUE;
        }
        command.type = (address == MODBUS_COIL_DIRECT_PRESS_PULSE) ?
            CMD_DIRECT_PRESS_PULSE : CMD_DIRECT_RELEASE_PULSE;
        return Machine_HandleCommand(context, &command, now_ms);
    }

    return COMMAND_UNSUPPORTED;
}

bool ModbusSemantic_ReadHolding(MachineContext *context,
                                uint16_t address,
                                uint16_t *value)
{
#if SD700_FORCE_SERVO_ENABLED
    if (ForceServoProtocol_Read(context,true,address,value)) return true;
#endif
    if ((context == NULL) || (value == NULL) ||
        (address != MODBUS_HOLDING_TARGET_PRESSURE))
    {
        return false;
    }

    *value = context->target_valid ?
        (uint16_t)context->target_pressure_units : 0U;
    return true;
}

bool ModbusSemantic_ReadInput(MachineContext *context,
                              uint16_t address,
                              uint32_t now_ms,
                              uint16_t *value)
{
#if SD700_FORCE_SERVO_ENABLED
    if (ForceServoProtocol_Read(context,false,address,value)) return true;
#endif
    const MotorExecutorSnapshot *motor;
    uint16_t flags = 0U;

    if ((context == NULL) || (value == NULL))
    {
        return false;
    }
    motor = MotorExecutor_GetSnapshot();
    if (motor == NULL)
    {
        return false;
    }

    if ((address >= MODBUS_INPUT_DIAGNOSTIC_FRAME_BYTE_0) &&
        (address <= MODBUS_INPUT_DIAGNOSTIC_FRAME_BYTE_6))
    {
        *value = context->pressure.frame[
            address - MODBUS_INPUT_DIAGNOSTIC_FRAME_BYTE_0];
        return true;
    }

    if ((address >= MODBUS_INPUT_SAMPLE_SEQUENCE_WORD_0) &&
        (address <= MODBUS_INPUT_SAMPLE_SEQUENCE_WORD_3))
    {
        *value = (uint16_t)(context->pressure.sequence >>
            (16U * (MODBUS_INPUT_SAMPLE_SEQUENCE_WORD_3 - address)));
        return true;
    }

    switch (address)
    {
        case MODBUS_INPUT_CONTROL_PRESSURE_UNITS:
            *value = (context->pressure.frame_valid &&
                      context->pressure.control_units_valid &&
                      (context->pressure.control_pressure_units >= 0) &&
                      (context->pressure.control_pressure_units <=
                       (int32_t)UINT16_MAX)) ?
                     (uint16_t)context->pressure.control_pressure_units :
                     MODBUS_LEGACY_SENSOR_DISCONNECTED_VALUE;
            break;

        case MODBUS_INPUT_RAW_PRESSURE_COUNTS:
            *value = context->pressure.frame_valid ?
                     ModbusSemantic_SaturateToU16(
                         context->pressure.raw_pressure_counts) :
                     MODBUS_LEGACY_SENSOR_DISCONNECTED_VALUE;
            break;

        case MODBUS_INPUT_MACHINE_STATE:
            *value = (uint16_t)context->state;
            break;

        case MODBUS_INPUT_MACHINE_FAULT:
            *value = (uint16_t)context->fault;
            break;

        case MODBUS_INPUT_FAULT_DETAIL:
            *value = (uint16_t)context->fault_detail;
            break;

        case MODBUS_INPUT_LAST_COMMAND_RESULT:
            *value = (uint16_t)context->last_command_result;
            break;

        case MODBUS_INPUT_LAST_MOTOR_ACTION:
            *value = (uint16_t)motor->last_action;
            break;

        case MODBUS_INPUT_LAST_MOTOR_COMMAND_MV:
            *value = ModbusSemantic_SaturateToU16(motor->command_mv);
            break;

        case MODBUS_INPUT_PLANNED_TIM2_CCR3:
            *value = motor->planned_tim2_ccr3;
            break;

        case MODBUS_INPUT_PLANNED_TIM3_CCR3:
            *value = motor->planned_tim3_ccr3;
            break;

        case MODBUS_INPUT_STATUS_FLAGS:
            if (Machine_IsPressureFresh(context, now_ms))
            {
                flags |= MODBUS_STATUS_PRESSURE_FRESH_VALID;
            }
            if (context->target_valid)
            {
                flags |= MODBUS_STATUS_TARGET_VALID;
            }
            if (motor->logical_active)
            {
                flags |= MODBUS_STATUS_MOTOR_LOGICAL_ACTIVE;
            }
            if (motor->physical_output_locked)
            {
                flags |= MODBUS_STATUS_PHYSICAL_OUTPUT_LOCKED;
            }
            if (motor->physical_output_disabled)
            {
                flags |= MODBUS_STATUS_PHYSICAL_OUTPUT_DISABLED;
            }
#if SD700_BENCH_RAW_COUNTS_CONTROL
            flags |= MODBUS_STATUS_BENCH_RAW_COUNTS_CONTROL;
#endif
            if (context->config.bench_release_correction_enabled)
            {
                flags |= MODBUS_STATUS_BENCH_RELEASE_CORRECTION;
            }
            *value = flags;
            break;

        case MODBUS_INPUT_LAST_MOTOR_FAILURE_RESULT:
            *value = (uint16_t)motor->last_failure_result;
            break;

        case MODBUS_INPUT_LAST_MOTOR_FAILURE_STAGE:
            *value = (uint16_t)motor->last_failure_stage;
            break;

        case MODBUS_INPUT_SAMPLE_RECEIVED_MS_HIGH:
            *value = (uint16_t)(context->pressure.received_at_ms >> 16U);
            break;
        case MODBUS_INPUT_SAMPLE_RECEIVED_MS_LOW:
            *value = (uint16_t)context->pressure.received_at_ms;
            break;
        case MODBUS_INPUT_REQUEST_DIRECTION:
            *value = context->diagnostic_direction;
            break;
        case MODBUS_INPUT_REQUEST_COMMAND_MV:
            *value = ModbusSemantic_SaturateToU16(context->diagnostic_command_mv);
            break;
        case MODBUS_INPUT_REQUEST_DURATION_MS:
            *value = ModbusSemantic_SaturateToU16(context->diagnostic_duration_ms);
            break;
        case MODBUS_INPUT_REQUEST_SEQUENCE_HIGH:
            *value = (uint16_t)(context->diagnostic_request_sequence >> 16U);
            break;
        case MODBUS_INPUT_REQUEST_SEQUENCE_LOW:
            *value = (uint16_t)context->diagnostic_request_sequence;
            break;
        case MODBUS_INPUT_REQUEST_AT_MS_HIGH:
            *value = (uint16_t)(context->diagnostic_request_at_ms >> 16U);
            break;
        case MODBUS_INPUT_REQUEST_AT_MS_LOW:
            *value = (uint16_t)context->diagnostic_request_at_ms;
            break;
        case MODBUS_INPUT_TARGET_READBACK:
            *value = context->target_valid ? (uint16_t)context->target_pressure_units : 0U;
            break;
        case MODBUS_INPUT_AUTO_TARGET_CAPABILITY:
            *value = SD700_AUTO_TARGET_ENABLED ? MODBUS_AUTO_TARGET_CAPABILITY_V1 : 0U;
            break;
        case MODBUS_INPUT_DEVICE_NOW_MS_HIGH:
            *value = (uint16_t)(now_ms >> 16U);
            break;
        case MODBUS_INPUT_DEVICE_NOW_MS_LOW:
            *value = (uint16_t)now_ms;
            break;

        default:
            return false;
    }
    return true;
}
