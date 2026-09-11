#ifndef TRANSPORT_MODBUS_MODBUS_RTU_LINK_H
#define TRANSPORT_MODBUS_MODBUS_RTU_LINK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "Transport/Modbus/modbus_rtu_server.h"

typedef void (*ModbusRtuSetDriverEnable)(void *context, bool transmit);
typedef bool (*ModbusRtuStartTransmit)(void *context,
                                       const uint8_t *bytes,
                                       size_t byte_count);

typedef struct
{
    ModbusRtuServer *server;
    ModbusRtuSetDriverEnable set_driver_enable;
    ModbusRtuStartTransmit start_transmit;
    void *io_context;
    bool transmit_active;
} ModbusRtuLink;

void ModbusRtuLink_Initialize(ModbusRtuLink *link,
                              ModbusRtuServer *server,
                              ModbusRtuSetDriverEnable set_driver_enable,
                              ModbusRtuStartTransmit start_transmit,
                              void *io_context);
bool ModbusRtuLink_Service(ModbusRtuLink *link);
void ModbusRtuLink_OnTransmitComplete(ModbusRtuLink *link);
void ModbusRtuLink_OnError(ModbusRtuLink *link);

#endif
