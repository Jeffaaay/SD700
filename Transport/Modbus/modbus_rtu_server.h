#ifndef TRANSPORT_MODBUS_MODBUS_RTU_SERVER_H
#define TRANSPORT_MODBUS_MODBUS_RTU_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "Application/machine.h"

#define MODBUS_RTU_RX_BUFFER_CAPACITY       32U
#define MODBUS_RTU_RESPONSE_CAPACITY        32U
#define MODBUS_RTU_REQUEST_QUEUE_CAPACITY    4U

typedef struct
{
    uint32_t valid_request_count;
    uint32_t invalid_crc_count;
    uint32_t wrong_station_count;
    uint32_t dropped_resync_byte_count;
    uint32_t exception_response_count;
    uint32_t normal_queue_full_drop_count;
    uint32_t duplicate_stop_count;
    uint32_t normal_discarded_by_stop_count;
    uint32_t dropped_stop_response_count;
} ModbusRtuServerSnapshot;

typedef struct
{
    MachineContext *machine;
    uint8_t rx_buffer[MODBUS_RTU_RX_BUFFER_CAPACITY];
    size_t rx_length;
    uint8_t request_queue[MODBUS_RTU_REQUEST_QUEUE_CAPACITY][8U];
    size_t request_count;
    uint8_t stop_request[8U];
    bool stop_latched;
    uint8_t active_request[8U];
    uint8_t response[MODBUS_RTU_RESPONSE_CAPACITY];
    size_t response_length;
    bool response_pending;
    uint8_t priority_response[MODBUS_RTU_RESPONSE_CAPACITY];
    size_t priority_response_length;
    bool priority_response_pending;
    ModbusRtuServerSnapshot snapshot;
} ModbusRtuServer;

void ModbusRtuServer_Initialize(ModbusRtuServer *server,
                                MachineContext *machine);
void ModbusRtuServer_ProcessBytes(ModbusRtuServer *server,
                                  const uint8_t *bytes,
                                  size_t byte_count);
bool ModbusRtuServer_HasPendingRequest(ModbusRtuServer *server);
bool ModbusRtuServer_PendingIsStop(ModbusRtuServer *server);
bool ModbusRtuServer_TakeStop(ModbusRtuServer *server,
                              uint32_t now_ms);
bool ModbusRtuServer_ProcessPending(ModbusRtuServer *server,
                                    uint32_t now_ms);
const uint8_t *ModbusRtuServer_GetResponse(const ModbusRtuServer *server,
                                           size_t *response_length);
void ModbusRtuServer_CompleteResponse(ModbusRtuServer *server);
const ModbusRtuServerSnapshot *ModbusRtuServer_GetSnapshot(
    const ModbusRtuServer *server);

#endif
