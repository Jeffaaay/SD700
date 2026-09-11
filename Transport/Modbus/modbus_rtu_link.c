#include "Transport/Modbus/modbus_rtu_link.h"

#include <stddef.h>
#include <string.h>

void ModbusRtuLink_Initialize(ModbusRtuLink *link,
                              ModbusRtuServer *server,
                              ModbusRtuSetDriverEnable set_driver_enable,
                              ModbusRtuStartTransmit start_transmit,
                              void *io_context)
{
    if (link == NULL)
    {
        return;
    }

    (void)memset(link, 0, sizeof(*link));
    link->server = server;
    link->set_driver_enable = set_driver_enable;
    link->start_transmit = start_transmit;
    link->io_context = io_context;
    if (set_driver_enable != NULL)
    {
        set_driver_enable(io_context, false);
    }
}

bool ModbusRtuLink_Service(ModbusRtuLink *link)
{
    const uint8_t *response;
    size_t response_length;

    if ((link == NULL) || (link->server == NULL) ||
        (link->set_driver_enable == NULL) ||
        (link->start_transmit == NULL) || link->transmit_active)
    {
        return false;
    }

    response = ModbusRtuServer_GetResponse(link->server,
                                           &response_length);
    if (response == NULL)
    {
        return false;
    }

    link->set_driver_enable(link->io_context, true);
    if (!link->start_transmit(link->io_context,
                              response,
                              response_length))
    {
        link->set_driver_enable(link->io_context, false);
        return false;
    }
    link->transmit_active = true;
    return true;
}

void ModbusRtuLink_OnTransmitComplete(ModbusRtuLink *link)
{
    if ((link == NULL) || (link->set_driver_enable == NULL))
    {
        return;
    }

    link->set_driver_enable(link->io_context, false);
    if (!link->transmit_active)
    {
        return;
    }
    link->transmit_active = false;
    ModbusRtuServer_CompleteResponse(link->server);
}

void ModbusRtuLink_OnError(ModbusRtuLink *link)
{
    if ((link == NULL) || (link->set_driver_enable == NULL))
    {
        return;
    }

    link->set_driver_enable(link->io_context, false);
    if (link->transmit_active)
    {
        link->transmit_active = false;
        ModbusRtuServer_CompleteResponse(link->server);
    }
}
