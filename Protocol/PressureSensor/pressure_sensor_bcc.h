#ifndef PROTOCOL_PRESSURE_SENSOR_BCC_H
#define PROTOCOL_PRESSURE_SENSOR_BCC_H

#include <stddef.h>
#include <stdint.h>

uint8_t PressureSensor_CalculateBcc(const uint8_t *data, size_t length);

#endif
