#include "Transport/Modbus/modbus_rtu_server.h"

#include <string.h>

#include "Protocol/Modbus/modbus_crc16.h"
#include "Protocol/Modbus/modbus_protocol_constants.h"
#include "Protocol/Modbus/modbus_register_map.h"
#include "Transport/Modbus/modbus_semantic_map.h"

static uint8_t ModbusRtuServer_MapCommandError(
    MachineCommandResult result);

static uint16_t ModbusRtuServer_ReadU16(const uint8_t *bytes)
{
    return (uint16_t)(((uint16_t)bytes[0] << 8U) | bytes[1]);
}

static void ModbusRtuServer_AppendCrc(uint8_t *frame, size_t data_length)
{
    uint16_t crc = Modbus_Crc16(frame, data_length);

    frame[data_length] = (uint8_t)(crc & 0xFFU);
    frame[data_length + 1U] = (uint8_t)(crc >> 8U);
}

static void ModbusRtuServer_DropFront(ModbusRtuServer *server,
                                      size_t byte_count)
{
    if (byte_count < server->rx_length)
    {
        server->rx_length -= byte_count;
        (void)memmove(server->rx_buffer,
                      &server->rx_buffer[byte_count],
                      server->rx_length);
    }
    else
    {
        server->rx_length = 0U;
    }
}

static bool ModbusRtuServer_FirstFrameCrcIsValid(
    const ModbusRtuServer *server)
{
    uint16_t expected = Modbus_Crc16(server->rx_buffer, 6U);
    uint16_t received = (uint16_t)server->rx_buffer[6] |
                        ((uint16_t)server->rx_buffer[7] << 8U);

    return expected == received;
}

static bool ModbusRtuServer_RequestIsStop(const uint8_t *request)
{
    return (request[1] == MODBUS_FUNCTION_WRITE_SINGLE_COIL) &&
           (ModbusRtuServer_ReadU16(&request[2]) ==
            MODBUS_COIL_AUTO_PRESSURE) &&
           (ModbusRtuServer_ReadU16(&request[4]) ==
            MODBUS_SINGLE_COIL_OFF);
}

static void ModbusRtuServer_TryExtractRequest(ModbusRtuServer *server)
{
    while (server->rx_length >= MODBUS_RTU_REQUEST_LENGTH)
    {
        if (!ModbusRtuServer_FirstFrameCrcIsValid(server))
        {
            if (server->rx_buffer[0] ==
                MODBUS_LEGACY_DEFAULT_STATION_ID)
            {
                ++server->snapshot.invalid_crc_count;
            }
            ++server->snapshot.dropped_resync_byte_count;
            ModbusRtuServer_DropFront(server, 1U);
        }
        else if (server->rx_buffer[0] !=
                 MODBUS_LEGACY_DEFAULT_STATION_ID)
        {
            ++server->snapshot.wrong_station_count;
            server->snapshot.dropped_resync_byte_count +=
                MODBUS_RTU_REQUEST_LENGTH;
            ModbusRtuServer_DropFront(server,
                                      MODBUS_RTU_REQUEST_LENGTH);
        }
        else
        {
            ++server->snapshot.valid_request_count;
            if (ModbusRtuServer_RequestIsStop(server->rx_buffer))
            {
                if (!server->stop_latched)
                {
                    (void)memcpy(server->stop_request,
                                 server->rx_buffer,
                                 MODBUS_RTU_REQUEST_LENGTH);
                    server->stop_latched = true;
                }
                else
                {
                    ++server->snapshot.duplicate_stop_count;
                }
            }
            else if (server->request_count <
                     MODBUS_RTU_REQUEST_QUEUE_CAPACITY)
            {
                (void)memcpy(
                    server->request_queue[server->request_count],
                    server->rx_buffer,
                    MODBUS_RTU_REQUEST_LENGTH);
                ++server->request_count;
            }
            else
            {
                ++server->snapshot.normal_queue_full_drop_count;
            }
            ModbusRtuServer_DropFront(server,
                                      MODBUS_RTU_REQUEST_LENGTH);
        }
    }
}

static void ModbusRtuServer_PromotePriorityResponse(
    ModbusRtuServer *server)
{
    if ((!server->response_pending) && server->priority_response_pending)
    {
        (void)memcpy(server->response,
                     server->priority_response,
                     server->priority_response_length);
        server->response_length = server->priority_response_length;
        server->response_pending = true;
        server->priority_response_length = 0U;
        server->priority_response_pending = false;
    }
}

static void ModbusRtuServer_QueueStopResponse(
    ModbusRtuServer *server,
    const uint8_t *request,
    MachineCommandResult result)
{
    if (server->priority_response_pending)
    {
        ++server->snapshot.dropped_stop_response_count;
        return;
    }

    if (result == COMMAND_ACCEPTED)
    {
        (void)memcpy(server->priority_response,
                     request,
                     MODBUS_RTU_REQUEST_LENGTH);
        server->priority_response_length = MODBUS_RTU_REQUEST_LENGTH;
    }
    else
    {
        server->priority_response[0] = MODBUS_LEGACY_DEFAULT_STATION_ID;
        server->priority_response[1] =
            MODBUS_FUNCTION_WRITE_SINGLE_COIL | 0x80U;
        server->priority_response[2] =
            ModbusRtuServer_MapCommandError(result);
        ModbusRtuServer_AppendCrc(server->priority_response, 3U);
        server->priority_response_length = 5U;
        ++server->snapshot.exception_response_count;
    }
    server->priority_response_pending = true;
    ModbusRtuServer_PromotePriorityResponse(server);
}

static void ModbusRtuServer_BuildException(ModbusRtuServer *server,
                                           uint8_t function,
                                           uint8_t exception)
{
    server->response[0] = MODBUS_LEGACY_DEFAULT_STATION_ID;
    server->response[1] = function | 0x80U;
    server->response[2] = exception;
    ModbusRtuServer_AppendCrc(server->response, 3U);
    server->response_length = 5U;
    server->response_pending = true;
    ++server->snapshot.exception_response_count;
}

static void ModbusRtuServer_BuildWriteEcho(ModbusRtuServer *server)
{
    (void)memcpy(server->response,
                 server->active_request,
                 MODBUS_RTU_REQUEST_LENGTH);
    server->response_length = MODBUS_RTU_REQUEST_LENGTH;
    server->response_pending = true;
}

static uint8_t ModbusRtuServer_MapCommandError(MachineCommandResult result)
{
    if (result == COMMAND_INVALID_VALUE)
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
    }
    if (result == COMMAND_UNSUPPORTED)
    {
        return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }
    return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
}

static void ModbusRtuServer_ProcessWrite(ModbusRtuServer *server,
                                         uint32_t now_ms)
{
    uint8_t function = server->active_request[1];
    uint16_t address = ModbusRtuServer_ReadU16(
        &server->active_request[2]);
    uint16_t value = ModbusRtuServer_ReadU16(
        &server->active_request[4]);
    MachineCommandResult result;

    if (((function == MODBUS_FUNCTION_WRITE_SINGLE_REGISTER) &&
         (address != MODBUS_HOLDING_TARGET_PRESSURE)) ||
        ((function == MODBUS_FUNCTION_WRITE_SINGLE_COIL) &&
         (address != MODBUS_COIL_AUTO_PRESSURE) &&
         (address != MODBUS_COIL_DIRECT_RELEASE_PULSE) &&
         (address != MODBUS_COIL_DIRECT_PRESS_PULSE)))
    {
        ModbusRtuServer_BuildException(
            server,
            function,
            MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
        return;
    }

    result = ModbusSemantic_ApplyWrite(server->machine,
                                       function,
                                       address,
                                       value,
                                       now_ms);
    if (result == COMMAND_ACCEPTED)
    {
        ModbusRtuServer_BuildWriteEcho(server);
    }
    else
    {
        ModbusRtuServer_BuildException(
            server,
            function,
            ModbusRtuServer_MapCommandError(result));
    }
}

static void ModbusRtuServer_ProcessRead(ModbusRtuServer *server,
                                        uint32_t now_ms)
{
    uint8_t function = server->active_request[1];
    uint16_t address = ModbusRtuServer_ReadU16(
        &server->active_request[2]);
    uint16_t quantity = ModbusRtuServer_ReadU16(
        &server->active_request[4]);
    uint16_t value;
    uint16_t index;
    bool found;

    if ((quantity == 0U) ||
        (quantity > MODBUS_RTU_MAX_READ_REGISTERS))
    {
        ModbusRtuServer_BuildException(
            server,
            function,
            MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
        return;
    }

    for (index = 0U; index < quantity; ++index)
    {
        uint32_t candidate = (uint32_t)address + index;

        if (candidate > UINT16_MAX)
        {
            found = false;
        }
        else if (function == MODBUS_FUNCTION_READ_HOLDING_REGISTERS)
        {
            found = ModbusSemantic_ReadHolding(server->machine,
                                               (uint16_t)candidate,
                                               &value);
        }
        else
        {
            found = ModbusSemantic_ReadInput(server->machine,
                                             (uint16_t)candidate,
                                             now_ms,
                                             &value);
        }

        if (!found)
        {
            ModbusRtuServer_BuildException(
                server,
                function,
                MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
            return;
        }
        server->response[3U + ((size_t)index * 2U)] =
            (uint8_t)(value >> 8U);
        server->response[4U + ((size_t)index * 2U)] =
            (uint8_t)(value & 0xFFU);
    }

    server->response[0] = MODBUS_LEGACY_DEFAULT_STATION_ID;
    server->response[1] = function;
    server->response[2] = (uint8_t)(quantity * 2U);
    server->response_length = 3U + ((size_t)quantity * 2U) + 2U;
    ModbusRtuServer_AppendCrc(server->response,
                             server->response_length - 2U);
    server->response_pending = true;
}

void ModbusRtuServer_Initialize(ModbusRtuServer *server,
                                MachineContext *machine)
{
    if (server != NULL)
    {
        (void)memset(server, 0, sizeof(*server));
        server->machine = machine;
    }
}

void ModbusRtuServer_ProcessBytes(ModbusRtuServer *server,
                                  const uint8_t *bytes,
                                  size_t byte_count)
{
    size_t index;

    if ((server == NULL) || ((bytes == NULL) && (byte_count != 0U)))
    {
        return;
    }

    for (index = 0U; index < byte_count; ++index)
    {
        if (server->rx_length == MODBUS_RTU_RX_BUFFER_CAPACITY)
        {
            ++server->snapshot.dropped_resync_byte_count;
            ModbusRtuServer_DropFront(server, 1U);
        }
        server->rx_buffer[server->rx_length] = bytes[index];
        ++server->rx_length;
        ModbusRtuServer_TryExtractRequest(server);
    }
}

bool ModbusRtuServer_HasPendingRequest(ModbusRtuServer *server)
{
    if (server == NULL)
    {
        return false;
    }
    ModbusRtuServer_TryExtractRequest(server);
    return server->request_count != 0U;
}

bool ModbusRtuServer_PendingIsStop(ModbusRtuServer *server)
{
    if (server == NULL)
    {
        return false;
    }
    ModbusRtuServer_TryExtractRequest(server);
    return server->stop_latched;
}

bool ModbusRtuServer_TakeStop(ModbusRtuServer *server,
                              uint32_t now_ms)
{
    uint8_t request[MODBUS_RTU_REQUEST_LENGTH];
    MachineCommandResult result;

    if ((server == NULL) || (server->machine == NULL))
    {
        return false;
    }

    ModbusRtuServer_TryExtractRequest(server);
    if (!server->stop_latched)
    {
        return false;
    }

    (void)memcpy(request,
                 server->stop_request,
                 MODBUS_RTU_REQUEST_LENGTH);
    server->stop_latched = false;

    /* The safety action is deliberately performed before response handling. */
    result = ModbusSemantic_ApplyWrite(
        server->machine,
        request[1],
        ModbusRtuServer_ReadU16(&request[2]),
        ModbusRtuServer_ReadU16(&request[4]),
        now_ms);

    server->snapshot.normal_discarded_by_stop_count +=
        (uint32_t)server->request_count;
    server->request_count = 0U;
    ModbusRtuServer_QueueStopResponse(server, request, result);
    return true;
}

bool ModbusRtuServer_ProcessPending(ModbusRtuServer *server,
                                    uint32_t now_ms)
{
    uint8_t function;
    size_t index;

    if ((server == NULL) || (server->machine == NULL) ||
        ModbusRtuServer_PendingIsStop(server) ||
        server->response_pending ||
        (!ModbusRtuServer_HasPendingRequest(server)))
    {
        return false;
    }

    (void)memcpy(server->active_request,
                 server->request_queue[0],
                 MODBUS_RTU_REQUEST_LENGTH);
    for (index = 1U; index < server->request_count; ++index)
    {
        (void)memcpy(server->request_queue[index - 1U],
                     server->request_queue[index],
                     MODBUS_RTU_REQUEST_LENGTH);
    }
    --server->request_count;

    function = server->active_request[1];
    if ((function == MODBUS_FUNCTION_READ_HOLDING_REGISTERS) ||
        (function == MODBUS_FUNCTION_READ_INPUT_REGISTERS))
    {
        ModbusRtuServer_ProcessRead(server, now_ms);
    }
    else if ((function == MODBUS_FUNCTION_WRITE_SINGLE_COIL) ||
             (function == MODBUS_FUNCTION_WRITE_SINGLE_REGISTER))
    {
        ModbusRtuServer_ProcessWrite(server, now_ms);
    }
    else
    {
        ModbusRtuServer_BuildException(
            server,
            function,
            MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
    }

    ModbusRtuServer_TryExtractRequest(server);
    return true;
}

const uint8_t *ModbusRtuServer_GetResponse(const ModbusRtuServer *server,
                                           size_t *response_length)
{
    if ((server == NULL) || (response_length == NULL) ||
        (!server->response_pending))
    {
        return NULL;
    }

    *response_length = server->response_length;
    return server->response;
}

void ModbusRtuServer_CompleteResponse(ModbusRtuServer *server)
{
    if (server != NULL)
    {
        server->response_pending = false;
        server->response_length = 0U;
        ModbusRtuServer_PromotePriorityResponse(server);
        ModbusRtuServer_TryExtractRequest(server);
    }
}

const ModbusRtuServerSnapshot *ModbusRtuServer_GetSnapshot(
    const ModbusRtuServer *server)
{
    return (server == NULL) ? NULL : &server->snapshot;
}
