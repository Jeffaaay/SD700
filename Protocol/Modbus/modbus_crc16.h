#ifndef PROTOCOL_MODBUS_CRC16_H
#define PROTOCOL_MODBUS_CRC16_H

#include <stddef.h>
#include <stdint.h>

uint16_t Modbus_Crc16(const uint8_t *data, size_t length);

#endif
