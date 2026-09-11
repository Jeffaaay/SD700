#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Application/machine.h"
#include "Protocol/Modbus/modbus_crc16.h"
#include "Protocol/Modbus/modbus_protocol_constants.h"
#include "Protocol/Modbus/modbus_register_map.h"
#include "Tests/Host/fake_motor_executor.h"
#include "Transport/Modbus/modbus_rtu_server.h"
#include "Transport/Modbus/modbus_semantic_map.h"

static const MachineConfig s_config = {
    .maximum_target_pressure_units = 1000,
    .contact_threshold_units = 100,
    .hold_enter_tolerance_units = 5,
    .hold_exit_tolerance_units = 10,
    .pressure_freshness_ms = 100U,
    .settle_delay_ms = 10U,
    .settle_feedback_timeout_ms = 50U,
    .first_approach_command_mv = 6000U,
    .first_approach_duration_ms = 50U,
    .first_approach_backstop_ms = 60U,
    .recontact_command_mv = 3000U,
    .recontact_duration_ms = 20U,
    .recontact_backstop_ms = 30U,
    .pulse_command_mv = 2000U,
    .pulse_duration_ms = 10U,
    .pulse_backstop_ms = 20U,
    .automatic_cycle_timeout_ms = 500U,
    .bench_release_correction_enabled = false,
    .raw_overpressure_enabled = false,
    .raw_overpressure_limit_counts = 0U
};

static void BuildWriteCoil(uint8_t *request,
                           uint16_t address,
                           uint16_t value)
{
    uint16_t crc;

    request[0] = MODBUS_LEGACY_DEFAULT_STATION_ID;
    request[1] = MODBUS_FUNCTION_WRITE_SINGLE_COIL;
    request[2] = (uint8_t)(address >> 8U);
    request[3] = (uint8_t)address;
    request[4] = (uint8_t)(value >> 8U);
    request[5] = (uint8_t)value;
    crc = Modbus_Crc16(request, 6U);
    request[6] = (uint8_t)crc;
    request[7] = (uint8_t)(crc >> 8U);
}

static void StartFixture(MachineContext *machine,
                         ModbusRtuServer *server,
                         bool feed_pressure)
{
    FakeMotorExecutor_Reset();
    Machine_Initialize(machine, &s_config, 0U);
    Machine_CompleteBoot(machine, true, 1U);
    if (feed_pressure)
    {
        MachinePressureSample sample = {
            .sequence = 1U,
            .received_at_ms = 2U,
            .raw_pressure_counts = 50U,
            .control_pressure_units = 50,
            .frame_valid = true,
            .control_units_valid = true
        };
        Machine_HandlePressureSample(machine, &sample, 2U);
    }
    ModbusRtuServer_Initialize(server, machine);
}

static const uint8_t *ProcessRequest(ModbusRtuServer *server,
                                     const uint8_t *request,
                                     uint32_t now_ms,
                                     size_t *length)
{
    ModbusRtuServer_ProcessBytes(server,
                                 request,
                                 MODBUS_RTU_REQUEST_LENGTH);
    assert(ModbusRtuServer_ProcessPending(server, now_ms));
    return ModbusRtuServer_GetResponse(server, length);
}

static void AssertException(const uint8_t *response,
                            size_t length,
                            uint8_t exception)
{
    assert(response != NULL);
    assert(length == 5U);
    assert(response[1] ==
           (MODBUS_FUNCTION_WRITE_SINGLE_COIL | 0x80U));
    assert(response[2] == exception);
}

static void TestAddressAndValueSemantics(void)
{
    MachineContext machine;
    ModbusRtuServer server;
    uint8_t request[MODBUS_RTU_REQUEST_LENGTH];
    const uint8_t *response;
    size_t length;
    bool feed_pressure = false;

#if defined(SD700_MOTOR_MODE_REAL_BENCH)
    feed_pressure = true;
#endif

    StartFixture(&machine, &server, feed_pressure);
    BuildWriteCoil(request,
                   MODBUS_COIL_DIRECT_PRESS_PULSE,
                   MODBUS_SINGLE_COIL_ON);
    response = ProcessRequest(&server, request, 3U, &length);
#if defined(SD700_MOTOR_MODE_SCOPE_TEST) || \
    defined(SD700_MOTOR_MODE_REAL_BENCH)
    assert(response != NULL);
    assert(length == MODBUS_RTU_REQUEST_LENGTH);
    assert(machine.state == DIRECT_PRESS_PULSE);
    assert(FakeMotorExecutor_GetState()->start_pulse_count == 1U);
    assert(FakeMotorExecutor_GetState()->snapshot.direction ==
           MOTOR_DIRECTION_PRESS);
#else
    AssertException(response,
                    length,
                    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
    assert(FakeMotorExecutor_GetState()->start_pulse_count == 0U);
#endif

    StartFixture(&machine, &server, feed_pressure);
    BuildWriteCoil(request,
                   MODBUS_COIL_DIRECT_RELEASE_PULSE,
                   MODBUS_SINGLE_COIL_ON);
    response = ProcessRequest(&server, request, 3U, &length);
#if defined(SD700_MOTOR_MODE_SCOPE_TEST) || \
    defined(SD700_MOTOR_MODE_REAL_BENCH)
    assert(response != NULL);
    assert(length == MODBUS_RTU_REQUEST_LENGTH);
    assert(machine.state == DIRECT_RELEASE_PULSE);
    assert(FakeMotorExecutor_GetState()->start_pulse_count == 1U);
    assert(FakeMotorExecutor_GetState()->snapshot.direction ==
           MOTOR_DIRECTION_RELEASE);
#else
    AssertException(response,
                    length,
                    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
#endif

    StartFixture(&machine, &server, feed_pressure);
    BuildWriteCoil(request,
                   MODBUS_COIL_DIRECT_PRESS_PULSE,
                   MODBUS_SINGLE_COIL_OFF);
    response = ProcessRequest(&server, request, 3U, &length);
    AssertException(response,
                    length,
                    MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);

    StartFixture(&machine, &server, feed_pressure);
    BuildWriteCoil(request,
                   MODBUS_COIL_JOG_OR_FAST_UP,
                   MODBUS_SINGLE_COIL_ON);
    response = ProcessRequest(&server, request, 3U, &length);
    AssertException(response,
                    length,
                    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    StartFixture(&machine, &server, feed_pressure);
    BuildWriteCoil(request,
                   MODBUS_COIL_JOG_OR_FAST_DOWN,
                   MODBUS_SINGLE_COIL_ON);
    response = ProcessRequest(&server, request, 3U, &length);
    AssertException(response,
                    length,
                    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

#if defined(SD700_MOTOR_MODE_SCOPE_TEST) || \
    defined(SD700_MOTOR_MODE_REAL_BENCH) || \
    defined(SD700_MOTOR_MODE_REAL_COMPILE_CHECK)
    StartFixture(&machine, &server, feed_pressure);
    BuildWriteCoil(request,
                   MODBUS_COIL_AUTO_PRESSURE,
                   MODBUS_SINGLE_COIL_ON);
    response = ProcessRequest(&server, request, 3U, &length);
    AssertException(response,
                    length,
                    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
#endif
}

static void TestMotorFailureDiagnosticInputs(void)
{
    MachineContext machine;
    ModbusRtuServer server;
    uint16_t value;

    StartFixture(&machine, &server, false);
    FakeMotorExecutor_Complete(MOTOR_COMPLETION_ERROR);
    assert(ModbusSemantic_ReadInput(
        &machine,
        MODBUS_INPUT_LAST_MOTOR_FAILURE_RESULT,
        2U,
        &value));
    assert(value == (uint16_t)MOTOR_RESULT_TIMER_ERROR);
    assert(ModbusSemantic_ReadInput(
        &machine,
        MODBUS_INPUT_LAST_MOTOR_FAILURE_STAGE,
        2U,
        &value));
    assert(value ==
           (uint16_t)MOTOR_FAILURE_STAGE_TIMER_IRQ_UNKNOWN_STATUS);
}

#if defined(SD700_MOTOR_MODE_SCOPE_TEST) || \
    defined(SD700_MOTOR_MODE_REAL_BENCH)
static void TestStopDiscardsQueuedDirectRequests(void)
{
    MachineContext machine;
    ModbusRtuServer server;
    uint8_t requests[3U * MODBUS_RTU_REQUEST_LENGTH];
    const uint8_t *response;
    const ModbusRtuServerSnapshot *snapshot;
    size_t length;
    bool feed_pressure = false;

#if defined(SD700_MOTOR_MODE_REAL_BENCH)
    feed_pressure = true;
#endif
    StartFixture(&machine, &server, feed_pressure);
    BuildWriteCoil(&requests[0],
                   MODBUS_COIL_DIRECT_PRESS_PULSE,
                   MODBUS_SINGLE_COIL_ON);
    BuildWriteCoil(&requests[MODBUS_RTU_REQUEST_LENGTH],
                   MODBUS_COIL_DIRECT_RELEASE_PULSE,
                   MODBUS_SINGLE_COIL_ON);
    BuildWriteCoil(&requests[2U * MODBUS_RTU_REQUEST_LENGTH],
                   MODBUS_COIL_AUTO_PRESSURE,
                   MODBUS_SINGLE_COIL_OFF);
    ModbusRtuServer_ProcessBytes(&server,
                                 requests,
                                 sizeof(requests));
    assert(ModbusRtuServer_PendingIsStop(&server));
    assert(ModbusRtuServer_TakeStop(&server, 3U));
    assert(machine.state == IDLE);
    assert(FakeMotorExecutor_GetState()->start_pulse_count == 0U);
    snapshot = ModbusRtuServer_GetSnapshot(&server);
    assert(snapshot->normal_discarded_by_stop_count == 2U);
    response = ModbusRtuServer_GetResponse(&server, &length);
    assert(response != NULL);
    assert(length == MODBUS_RTU_REQUEST_LENGTH);
    assert(response[2] == 0U && response[3] == 1U);
    assert(response[4] == 0U && response[5] == 0U);
}
#endif

int main(void)
{
    TestAddressAndValueSemantics();
    TestMotorFailureDiagnosticInputs();
#if defined(SD700_MOTOR_MODE_SCOPE_TEST) || \
    defined(SD700_MOTOR_MODE_REAL_BENCH)
    TestStopDiscardsQueuedDirectRequests();
#endif
    puts("Modbus direct pulse and STOP priority host tests: PASS");
    return 0;
}
