#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "Application/bench_config.h"
#include "Application/runtime.h"
#include "Board/Motor/motor_executor.h"
#include "Protocol/Modbus/modbus_crc16.h"
#include "Protocol/Modbus/modbus_protocol_constants.h"
#include "Protocol/Modbus/modbus_register_map.h"
#include "Protocol/PressureSensor/pressure_sensor_bcc.h"
#include "Tests/Host/fake_motor_hw.h"
#include "Transport/Modbus/modbus_rtu_server.h"
#include "Transport/Pressure/pressure_receiver.h"

static void MakePressureFrame(uint8_t frame[PRESSURE_SENSOR_FRAME_LENGTH],
                              uint16_t pressure)
{
    frame[0] = PRESSURE_SENSOR_FRAME_HEADER;
    frame[1] = (uint8_t)(pressure & 0xFFU);
    frame[2] = (uint8_t)(pressure >> 8U);
    frame[3] = 0U;
    frame[4] = 0U;
    frame[5] = PressureSensor_CalculateBcc(frame, 5U);
    frame[6] = PRESSURE_SENSOR_FRAME_MARKER;
}

static void FeedLivePressure(ApplicationRuntime *runtime,
                             PressureReceiver *receiver,
                             uint16_t pressure,
                             uint32_t now_ms)
{
    uint8_t frame[PRESSURE_SENSOR_FRAME_LENGTH];

    MakePressureFrame(frame, pressure);
    PressureReceiver_ProcessBytes(receiver, frame, sizeof(frame), now_ms);
    assert(ApplicationRuntime_ServicePressure(
        runtime,
        PressureReceiver_GetSnapshot(receiver),
        now_ms));
}

static void MakeModbusWrite(uint8_t frame[MODBUS_RTU_REQUEST_LENGTH],
                            uint8_t function,
                            uint16_t address,
                            uint16_t value)
{
    uint16_t crc;

    frame[0] = MODBUS_LEGACY_DEFAULT_STATION_ID;
    frame[1] = function;
    frame[2] = (uint8_t)(address >> 8U);
    frame[3] = (uint8_t)(address & 0xFFU);
    frame[4] = (uint8_t)(value >> 8U);
    frame[5] = (uint8_t)(value & 0xFFU);
    crc = Modbus_Crc16(frame, 6U);
    frame[6] = (uint8_t)(crc & 0xFFU);
    frame[7] = (uint8_t)(crc >> 8U);
}

static void ApplyModbusWrite(ModbusRtuServer *server,
                             const uint8_t frame[MODBUS_RTU_REQUEST_LENGTH],
                             uint32_t now_ms)
{
    size_t response_length;

    ModbusRtuServer_ProcessBytes(server,
                                 frame,
                                 MODBUS_RTU_REQUEST_LENGTH);
    if (ModbusRtuServer_PendingIsStop(server))
    {
        assert(ModbusRtuServer_TakeStop(server, now_ms));
    }
    else
    {
        assert(ModbusRtuServer_ProcessPending(server, now_ms));
    }
    assert(ModbusRtuServer_GetResponse(server,
                                       &response_length) != NULL);
    assert(response_length == MODBUS_RTU_REQUEST_LENGTH);
    ModbusRtuServer_CompleteResponse(server);
}

static void StartFixture(ApplicationRuntime *runtime,
                         PressureReceiver *receiver,
                         ModbusRtuServer *server)
{
    FakeMotorHw_Reset();
    assert(MotorExecutor_Initialize() == MOTOR_RESULT_OK);
    ApplicationRuntime_Initialize(runtime,
                                  &g_sd700_bench_machine_config,
                                  0U);
    ApplicationRuntime_CompleteBoot(runtime, true, 1U);
    assert(runtime->machine.state == IDLE);
    PressureReceiver_Initialize(receiver);
    ModbusRtuServer_Initialize(server, &runtime->machine);
}

static void AssertPhysicalLock(void)
{
    const MotorExecutorSnapshot *motor = MotorExecutor_GetSnapshot();
    assert(motor->physical_output_locked);
    assert(motor->physical_output_disabled);
    assert(FakeMotorHw_GetDisableCount() > 0U);
}

static void TestCompleteDryRunPath(void)
{
    ApplicationRuntime runtime;
    PressureReceiver receiver;
    ModbusRtuServer server;
    uint8_t request[MODBUS_RTU_REQUEST_LENGTH];

    StartFixture(&runtime, &receiver, &server);
    FeedLivePressure(&runtime, &receiver, 50U, 2U);
    AssertPhysicalLock();

    MakeModbusWrite(request,
                    MODBUS_FUNCTION_WRITE_SINGLE_REGISTER,
                    MODBUS_HOLDING_TARGET_PRESSURE,
                    500U);
    ApplyModbusWrite(&server, request, 3U);
    MakeModbusWrite(request,
                    MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                    MODBUS_COIL_AUTO_PRESSURE,
                    MODBUS_SINGLE_COIL_ON);
    ApplyModbusWrite(&server, request, 4U);
    assert(runtime.machine.state == AUTO_APPROACH);
    assert(MotorExecutor_GetSnapshot()->last_action ==
           MOTOR_ACTION_PRESS_RUN);
    assert(MotorExecutor_GetSnapshot()->logical_active);
    AssertPhysicalLock();

    FeedLivePressure(&runtime, &receiver, 100U, 5U);
    assert(runtime.machine.state == AUTO_SETTLE);
    assert(!MotorExecutor_GetSnapshot()->logical_active);
    ApplicationRuntime_Tick(&runtime, 15U);
    assert(runtime.machine.settle_phase == SETTLE_WAIT_SAMPLE);

    FeedLivePressure(&runtime, &receiver, 400U, 16U);
    assert(runtime.machine.state == AUTO_PULSE);
    assert(MotorExecutor_GetSnapshot()->last_action ==
           MOTOR_ACTION_PRESS_PULSE);
    AssertPhysicalLock();

    MakeModbusWrite(request,
                    MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                    MODBUS_COIL_AUTO_PRESSURE,
                    MODBUS_SINGLE_COIL_OFF);
    ApplyModbusWrite(&server, request, 17U);
    assert(runtime.machine.state == IDLE);
    assert(!MotorExecutor_GetSnapshot()->logical_active);
    AssertPhysicalLock();
}

static void TestStalePressureFault(void)
{
    ApplicationRuntime runtime;
    PressureReceiver receiver;
    ModbusRtuServer server;
    uint8_t request[MODBUS_RTU_REQUEST_LENGTH];

    StartFixture(&runtime, &receiver, &server);
    FeedLivePressure(&runtime, &receiver, 50U, 2U);
    MakeModbusWrite(request,
                    MODBUS_FUNCTION_WRITE_SINGLE_REGISTER,
                    MODBUS_HOLDING_TARGET_PRESSURE,
                    500U);
    ApplyModbusWrite(&server, request, 3U);
    MakeModbusWrite(request,
                    MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                    MODBUS_COIL_AUTO_PRESSURE,
                    MODBUS_SINGLE_COIL_ON);
    ApplyModbusWrite(&server, request, 4U);
    assert(runtime.machine.state == AUTO_APPROACH);

    ApplicationRuntime_ServiceSafety(
        &runtime,
        2U + g_sd700_bench_machine_config.pressure_freshness_ms + 1U);
    assert(runtime.machine.state == FAULT);
    assert(runtime.machine.fault == FAULT_PRESSURE_SENSOR_FAULT);
    assert(runtime.machine.fault_detail == FAULT_DETAIL_PRESSURE_TIMEOUT);
    assert(!MotorExecutor_GetSnapshot()->logical_active);
    AssertPhysicalLock();
}

int main(void)
{
    TestCompleteDryRunPath();
    TestStalePressureFault();
    puts("Runtime integration host tests: PASS");
    return 0;
}
