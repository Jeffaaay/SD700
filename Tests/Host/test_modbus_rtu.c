#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Application/machine.h"
#include "Protocol/Modbus/modbus_crc16.h"
#include "Protocol/Modbus/modbus_protocol_constants.h"
#include "Protocol/Modbus/modbus_register_map.h"
#include "Tests/Host/fake_motor_executor.h"
#include "Transport/Modbus/modbus_rtu_link.h"
#include "Transport/Modbus/modbus_rtu_server.h"

static const MachineConfig s_config = {
    .maximum_target_pressure_units = 1500,
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
    .recontact_duration_ms = 25U,
    .recontact_backstop_ms = 35U,
    .pulse_command_mv = 2000U,
    .pulse_duration_ms = 10U,
    .pulse_backstop_ms = 20U,
    .automatic_cycle_timeout_ms = 500U,
    .bench_release_correction_enabled = true,
    .raw_overpressure_enabled = false,
    .raw_overpressure_limit_counts = 0U
};

typedef struct
{
    bool driver_enabled;
    bool start_succeeds;
    uint32_t driver_change_count;
    uint32_t transmit_count;
    uint8_t transmitted[MODBUS_RTU_RESPONSE_CAPACITY];
    size_t transmitted_length;
} FakeLinkIo;

static void MakeRequest(uint8_t frame[MODBUS_RTU_REQUEST_LENGTH],
                        uint8_t station,
                        uint8_t function,
                        uint16_t address,
                        uint16_t value)
{
    uint16_t crc;

    frame[0] = station;
    frame[1] = function;
    frame[2] = (uint8_t)(address >> 8U);
    frame[3] = (uint8_t)(address & 0xFFU);
    frame[4] = (uint8_t)(value >> 8U);
    frame[5] = (uint8_t)(value & 0xFFU);
    crc = Modbus_Crc16(frame, 6U);
    frame[6] = (uint8_t)(crc & 0xFFU);
    frame[7] = (uint8_t)(crc >> 8U);
}

static void StartFixture(MachineContext *machine, ModbusRtuServer *server)
{
    FakeMotorExecutor_Reset();
    Machine_Initialize(machine, &s_config, 0U);
    Machine_CompleteBoot(machine, true, 1U);
    assert(machine->state == IDLE);
    ModbusRtuServer_Initialize(server, machine);
}

static void FeedPressure(MachineContext *machine,
                         uint32_t pressure,
                         uint64_t sequence,
                         uint32_t now_ms)
{
    MachinePressureSample sample = {
        .sequence = sequence,
        .received_at_ms = now_ms,
        .raw_pressure_counts = pressure,
        .control_pressure_units = (int32_t)pressure,
        .frame_valid = true,
        .control_units_valid = true
    };
    Machine_HandlePressureSample(machine, &sample, now_ms);
}

static size_t ProcessRequest(ModbusRtuServer *server,
                             const uint8_t request[MODBUS_RTU_REQUEST_LENGTH],
                             uint32_t now_ms,
                             uint8_t response[MODBUS_RTU_RESPONSE_CAPACITY])
{
    const uint8_t *source;
    size_t length;
    uint16_t crc;

    ModbusRtuServer_ProcessBytes(server,
                                 request,
                                 MODBUS_RTU_REQUEST_LENGTH);
    if (ModbusRtuServer_PendingIsStop(server))
    {
        assert(ModbusRtuServer_TakeStop(server, now_ms));
    }
    else
    {
        assert(ModbusRtuServer_ProcessPending(server, now_ms));
    }
    source = ModbusRtuServer_GetResponse(server, &length);
    assert(source != NULL);
    assert(length >= 5U);
    crc = Modbus_Crc16(source, length - 2U);
    assert(source[length - 2U] == (uint8_t)(crc & 0xFFU));
    assert(source[length - 1U] == (uint8_t)(crc >> 8U));
    (void)memcpy(response, source, length);
    ModbusRtuServer_CompleteResponse(server);
    return length;
}

static uint16_t ResponseRegister(const uint8_t *response, uint16_t index)
{
    size_t offset = 3U + ((size_t)index * 2U);
    return (uint16_t)(((uint16_t)response[offset] << 8U) |
                      response[offset + 1U]);
}

static void TestWritesAndReads(void)
{
    MachineContext machine;
    ModbusRtuServer server;
    uint8_t request[MODBUS_RTU_REQUEST_LENGTH];
    uint8_t response[MODBUS_RTU_RESPONSE_CAPACITY];
    size_t length;

    StartFixture(&machine, &server);
    MakeRequest(request, 1U, MODBUS_FUNCTION_WRITE_SINGLE_REGISTER,
                MODBUS_HOLDING_TARGET_PRESSURE, 500U);
    assert(ProcessRequest(&server, request, 2U, response) == 8U);
    assert(machine.target_valid);
    assert(machine.target_pressure_units == 500);

    MakeRequest(request, 1U, MODBUS_FUNCTION_READ_HOLDING_REGISTERS,
                MODBUS_HOLDING_TARGET_PRESSURE, 1U);
    length = ProcessRequest(&server, request, 3U, response);
    assert(length == 7U);
    assert(ResponseRegister(response, 0U) == 500U);

    FeedPressure(&machine, 50U, 1U, 4U);
    MakeRequest(request, 1U, MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                MODBUS_COIL_AUTO_PRESSURE, MODBUS_SINGLE_COIL_ON);
    assert(ProcessRequest(&server, request, 5U, response) == 8U);
    assert(machine.state == AUTO_APPROACH);
    assert(MotorExecutor_GetSnapshot()->last_action ==
           MOTOR_ACTION_PRESS_RUN);

    MakeRequest(request, 1U, MODBUS_FUNCTION_READ_INPUT_REGISTERS,
                MODBUS_INPUT_CONTROL_PRESSURE_UNITS,
                MODBUS_INPUT_REGISTER_COUNT);
    length = ProcessRequest(&server, request, 6U, response);
    assert(length == 27U);
    assert(ResponseRegister(response, 0U) == 50U);
    assert(ResponseRegister(response, 1U) == 50U);
    assert(ResponseRegister(response, 2U) == (uint16_t)AUTO_APPROACH);
    assert(ResponseRegister(response, 3U) == (uint16_t)FAULT_NONE);
    assert(ResponseRegister(response, 6U) ==
           (uint16_t)MOTOR_ACTION_PRESS_RUN);
    assert(ResponseRegister(response, 7U) == 6000U);
    assert(ResponseRegister(response, 8U) == 0U);
    assert(ResponseRegister(response, 9U) > 0U);
    assert((ResponseRegister(response, 10U) &
            MODBUS_STATUS_PHYSICAL_OUTPUT_LOCKED) != 0U);
    assert((ResponseRegister(response, 10U) &
            MODBUS_STATUS_PHYSICAL_OUTPUT_DISABLED) != 0U);

    MakeRequest(request, 1U, MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                MODBUS_COIL_AUTO_PRESSURE, MODBUS_SINGLE_COIL_OFF);
    assert(ProcessRequest(&server, request, 7U, response) == 8U);
    assert(machine.state == IDLE);
    assert(!MotorExecutor_GetSnapshot()->logical_active);
}

static void TestRejectionsAndExceptions(void)
{
    MachineContext machine;
    ModbusRtuServer server;
    uint8_t request[MODBUS_RTU_REQUEST_LENGTH];
    uint8_t response[MODBUS_RTU_RESPONSE_CAPACITY];
    size_t response_length;

    StartFixture(&machine, &server);
    MakeRequest(request, 1U, 0x10U, 0U, 1U);
    (void)ProcessRequest(&server, request, 2U, response);
    assert(response[1] == 0x90U);
    assert(response[2] == MODBUS_EXCEPTION_ILLEGAL_FUNCTION);

    MakeRequest(request, 1U, MODBUS_FUNCTION_READ_INPUT_REGISTERS,
                MODBUS_INPUT_TOTAL_REGISTER_COUNT, 1U);
    (void)ProcessRequest(&server, request, 3U, response);
    assert(response[2] == MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);

    MakeRequest(request, 1U, MODBUS_FUNCTION_READ_INPUT_REGISTERS,
                0U, 0U);
    (void)ProcessRequest(&server, request, 4U, response);
    assert(response[2] == MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);

    MakeRequest(request, 1U, MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                MODBUS_COIL_AUTO_PRESSURE, 1U);
    (void)ProcessRequest(&server, request, 5U, response);
    assert(response[2] == MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);

    MakeRequest(request, 1U, MODBUS_FUNCTION_WRITE_SINGLE_REGISTER,
                MODBUS_HOLDING_TARGET_PRESSURE, 500U);
    request[6] ^= 1U;
    ModbusRtuServer_ProcessBytes(&server, request, sizeof(request));
    assert(!ModbusRtuServer_HasPendingRequest(&server));
    assert(ModbusRtuServer_GetResponse(&server,
                                       &response_length) == NULL);
    assert(ModbusRtuServer_GetSnapshot(&server)->invalid_crc_count > 0U);

    MakeRequest(request, 2U, MODBUS_FUNCTION_WRITE_SINGLE_REGISTER,
                MODBUS_HOLDING_TARGET_PRESSURE, 500U);
    ModbusRtuServer_ProcessBytes(&server, request, sizeof(request));
    assert(!ModbusRtuServer_HasPendingRequest(&server));
    assert(ModbusRtuServer_GetSnapshot(&server)->wrong_station_count == 1U);
}

static void TestFragmentMultipleAndResync(void)
{
    MachineContext machine;
    ModbusRtuServer server;
    uint8_t first[MODBUS_RTU_REQUEST_LENGTH];
    uint8_t second[MODBUS_RTU_REQUEST_LENGTH];
    uint8_t combined[MODBUS_RTU_REQUEST_LENGTH * 2U];
    uint8_t garbage[3U + MODBUS_RTU_REQUEST_LENGTH];
    uint8_t response[MODBUS_RTU_RESPONSE_CAPACITY];

    StartFixture(&machine, &server);
    MakeRequest(first, 1U, MODBUS_FUNCTION_WRITE_SINGLE_REGISTER,
                MODBUS_HOLDING_TARGET_PRESSURE, 500U);
    ModbusRtuServer_ProcessBytes(&server, first, 3U);
    assert(!ModbusRtuServer_HasPendingRequest(&server));
    ModbusRtuServer_ProcessBytes(&server, &first[3], 5U);
    assert(ModbusRtuServer_ProcessPending(&server, 2U));
    ModbusRtuServer_CompleteResponse(&server);
    assert(machine.target_pressure_units == 500);

    MakeRequest(first, 1U, MODBUS_FUNCTION_WRITE_SINGLE_REGISTER,
                MODBUS_HOLDING_TARGET_PRESSURE, 600U);
    MakeRequest(second, 1U, MODBUS_FUNCTION_READ_HOLDING_REGISTERS,
                MODBUS_HOLDING_TARGET_PRESSURE, 1U);
    (void)memcpy(combined, first, sizeof(first));
    (void)memcpy(&combined[sizeof(first)], second, sizeof(second));
    ModbusRtuServer_ProcessBytes(&server, combined, sizeof(combined));
    assert(ModbusRtuServer_ProcessPending(&server, 3U));
    ModbusRtuServer_CompleteResponse(&server);
    assert(ModbusRtuServer_ProcessPending(&server, 4U));
    {
        const uint8_t *source;
        size_t length;
        source = ModbusRtuServer_GetResponse(&server, &length);
        assert(source != NULL);
        (void)memcpy(response, source, length);
        assert(ResponseRegister(response, 0U) == 600U);
    }
    ModbusRtuServer_CompleteResponse(&server);

    garbage[0] = 0xAAU;
    garbage[1] = 0x55U;
    garbage[2] = 0x00U;
    MakeRequest(&garbage[3], 1U,
                MODBUS_FUNCTION_READ_HOLDING_REGISTERS,
                MODBUS_HOLDING_TARGET_PRESSURE, 1U);
    ModbusRtuServer_ProcessBytes(&server, garbage, sizeof(garbage));
    assert(ModbusRtuServer_ProcessPending(&server, 5U));
    ModbusRtuServer_CompleteResponse(&server);
    assert(ModbusRtuServer_GetSnapshot(&server)->
           dropped_resync_byte_count >= 3U);
}

static void TestQueueFullStopPriority(void)
{
    MachineContext machine;
    ModbusRtuServer server;
    uint8_t target[MODBUS_RTU_REQUEST_LENGTH];
    uint8_t queued[MODBUS_RTU_REQUEST_LENGTH * 5U];
    uint8_t response[MODBUS_RTU_RESPONSE_CAPACITY];
    size_t index;

    StartFixture(&machine, &server);
    MakeRequest(target, 1U, MODBUS_FUNCTION_WRITE_SINGLE_REGISTER,
                MODBUS_HOLDING_TARGET_PRESSURE, 500U);
    (void)ProcessRequest(&server, target, 2U, response);
    FeedPressure(&machine, 50U, 1U, 3U);

    MakeRequest(&queued[0], 1U, MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                MODBUS_COIL_AUTO_PRESSURE, MODBUS_SINGLE_COIL_ON);
    for (index = 1U; index < MODBUS_RTU_REQUEST_QUEUE_CAPACITY; ++index)
    {
        MakeRequest(&queued[index * MODBUS_RTU_REQUEST_LENGTH],
                    1U,
                    MODBUS_FUNCTION_READ_INPUT_REGISTERS,
                    MODBUS_INPUT_MACHINE_STATE,
                    1U);
    }
    MakeRequest(
        &queued[MODBUS_RTU_REQUEST_QUEUE_CAPACITY *
                MODBUS_RTU_REQUEST_LENGTH],
        1U,
        MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                MODBUS_COIL_AUTO_PRESSURE, MODBUS_SINGLE_COIL_OFF);
    ModbusRtuServer_ProcessBytes(&server, queued, sizeof(queued));

    assert(ModbusRtuServer_PendingIsStop(&server));
    assert(!ModbusRtuServer_ProcessPending(&server, 5U));
    assert(ModbusRtuServer_TakeStop(&server, 5U));
    assert(machine.state == IDLE);
    assert(!MotorExecutor_GetSnapshot()->logical_active);
    assert(FakeMotorExecutor_GetState()->start_run_count == 0U);
    assert(!ModbusRtuServer_HasPendingRequest(&server));
    assert(ModbusRtuServer_GetSnapshot(&server)->
           normal_discarded_by_stop_count ==
           MODBUS_RTU_REQUEST_QUEUE_CAPACITY);
}

static void TestResponsePendingStopPriority(void)
{
    MachineContext machine;
    ModbusRtuServer server;
    uint8_t request[MODBUS_RTU_REQUEST_LENGTH];
    uint8_t response[MODBUS_RTU_RESPONSE_CAPACITY];
    const uint8_t *pending;
    size_t length;
    uint32_t disable_count;

    StartFixture(&machine, &server);
    MakeRequest(request, 1U, MODBUS_FUNCTION_WRITE_SINGLE_REGISTER,
                MODBUS_HOLDING_TARGET_PRESSURE, 500U);
    (void)ProcessRequest(&server, request, 2U, response);
    FeedPressure(&machine, 50U, 1U, 3U);
    MakeRequest(request, 1U, MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                MODBUS_COIL_AUTO_PRESSURE, MODBUS_SINGLE_COIL_ON);
    (void)ProcessRequest(&server, request, 4U, response);
    assert(machine.state == AUTO_APPROACH);

    MakeRequest(request, 1U, MODBUS_FUNCTION_READ_INPUT_REGISTERS,
                MODBUS_INPUT_MACHINE_STATE, 1U);
    ModbusRtuServer_ProcessBytes(&server, request, sizeof(request));
    assert(ModbusRtuServer_ProcessPending(&server, 5U));
    pending = ModbusRtuServer_GetResponse(&server, &length);
    assert(pending != NULL);
    assert(pending[1] == MODBUS_FUNCTION_READ_INPUT_REGISTERS);

    MakeRequest(request, 1U, MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                MODBUS_COIL_AUTO_PRESSURE, MODBUS_SINGLE_COIL_OFF);
    ModbusRtuServer_ProcessBytes(&server, request, sizeof(request));
    disable_count = FakeMotorExecutor_GetState()->disable_count;
    assert(ModbusRtuServer_TakeStop(&server, 6U));
    assert(machine.state == IDLE);
    assert(FakeMotorExecutor_GetState()->disable_count > disable_count);

    pending = ModbusRtuServer_GetResponse(&server, &length);
    assert(pending != NULL);
    assert(pending[1] == MODBUS_FUNCTION_READ_INPUT_REGISTERS);
    ModbusRtuServer_CompleteResponse(&server);
    pending = ModbusRtuServer_GetResponse(&server, &length);
    assert(pending != NULL);
    assert(length == MODBUS_RTU_REQUEST_LENGTH);
    assert(pending[1] == MODBUS_FUNCTION_WRITE_SINGLE_COIL);
    assert((((uint16_t)pending[2] << 8U) | pending[3]) ==
           MODBUS_COIL_AUTO_PRESSURE);
    assert((((uint16_t)pending[4] << 8U) | pending[5]) ==
           MODBUS_SINGLE_COIL_OFF);
}

static void TestMultipleStopsAreIdempotent(void)
{
    MachineContext machine;
    ModbusRtuServer server;
    uint8_t stop[MODBUS_RTU_REQUEST_LENGTH];
    uint8_t stops[MODBUS_RTU_REQUEST_LENGTH * 3U];
    uint32_t disable_count;

    StartFixture(&machine, &server);
    MakeRequest(stop, 1U, MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                MODBUS_COIL_AUTO_PRESSURE, MODBUS_SINGLE_COIL_OFF);
    (void)memcpy(&stops[0], stop, sizeof(stop));
    (void)memcpy(&stops[sizeof(stop)], stop, sizeof(stop));
    (void)memcpy(&stops[sizeof(stop) * 2U], stop, sizeof(stop));
    ModbusRtuServer_ProcessBytes(&server, stops, sizeof(stops));

    disable_count = FakeMotorExecutor_GetState()->disable_count;
    assert(ModbusRtuServer_TakeStop(&server, 2U));
    assert(machine.state == IDLE);
    assert(FakeMotorExecutor_GetState()->disable_count ==
           disable_count + 1U);
    assert(!ModbusRtuServer_TakeStop(&server, 3U));
    assert(!ModbusRtuServer_HasPendingRequest(&server));
    assert(ModbusRtuServer_GetSnapshot(&server)->duplicate_stop_count ==
           2U);

    ModbusRtuServer_CompleteResponse(&server);
    Machine_ReportFault(&machine, FAULT_INTERNAL_FAULT,
                        FAULT_DETAIL_INTERNAL_STATE, 4U);
    ModbusRtuServer_ProcessBytes(&server, stop, sizeof(stop));
    assert(ModbusRtuServer_TakeStop(&server, 5U));
    assert(machine.state == FAULT);
    assert(machine.fault == FAULT_INTERNAL_FAULT);
    assert(!ModbusRtuServer_HasPendingRequest(&server));
}

static void FakeSetDriverEnable(void *context, bool transmit)
{
    FakeLinkIo *io = (FakeLinkIo *)context;
    io->driver_enabled = transmit;
    ++io->driver_change_count;
}

static bool FakeStartTransmit(void *context,
                              const uint8_t *bytes,
                              size_t byte_count)
{
    FakeLinkIo *io = (FakeLinkIo *)context;
    assert(io->driver_enabled);
    ++io->transmit_count;
    io->transmitted_length = byte_count;
    (void)memcpy(io->transmitted, bytes, byte_count);
    return io->start_succeeds;
}

static void TestDriverEnableLifecycle(void)
{
    MachineContext machine;
    ModbusRtuServer server;
    ModbusRtuLink link;
    FakeLinkIo io = {false, true, 0U, 0U, {0U}, 0U};
    uint8_t request[MODBUS_RTU_REQUEST_LENGTH];
    size_t response_length;

    StartFixture(&machine, &server);
    ModbusRtuLink_Initialize(&link,
                             &server,
                             FakeSetDriverEnable,
                             FakeStartTransmit,
                             &io);
    assert(!io.driver_enabled);

    MakeRequest(request, 1U, MODBUS_FUNCTION_WRITE_SINGLE_REGISTER,
                MODBUS_HOLDING_TARGET_PRESSURE, 500U);
    ModbusRtuServer_ProcessBytes(&server, request, sizeof(request));
    assert(ModbusRtuServer_ProcessPending(&server, 2U));
    assert(ModbusRtuLink_Service(&link));
    assert(io.driver_enabled);
    assert(io.transmit_count == 1U);
    assert(io.transmitted_length == 8U);
    ModbusRtuLink_OnTransmitComplete(&link);
    assert(!io.driver_enabled);
    assert(ModbusRtuServer_GetResponse(&server,
                                       &response_length) == NULL);
}

int main(void)
{
    TestWritesAndReads();
    TestRejectionsAndExceptions();
    TestFragmentMultipleAndResync();
    TestQueueFullStopPriority();
    TestResponsePendingStopPriority();
    TestMultipleStopsAreIdempotent();
    TestDriverEnableLifecycle();
    puts("Modbus RTU host tests: PASS");
    return 0;
}
