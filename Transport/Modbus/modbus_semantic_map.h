#ifndef TRANSPORT_MODBUS_MODBUS_SEMANTIC_MAP_H
#define TRANSPORT_MODBUS_MODBUS_SEMANTIC_MAP_H

#include <stdbool.h>
#include <stdint.h>

#include "Application/machine.h"

MachineCommandResult ModbusSemantic_ApplyWrite(MachineContext *context,
                                               uint8_t function,
                                               uint16_t address,
                                               uint16_t value,
                                               uint32_t now_ms);
bool ModbusSemantic_ReadHolding(const MachineContext *context,
                                uint16_t address,
                                uint16_t *value);
bool ModbusSemantic_ReadInput(const MachineContext *context,
                              uint16_t address,
                              uint32_t now_ms,
                              uint16_t *value);

#endif
