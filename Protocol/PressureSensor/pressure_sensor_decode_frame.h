#ifndef PROTOCOL_PRESSURE_SENSOR_DECODE_FRAME_H
#define PROTOCOL_PRESSURE_SENSOR_DECODE_FRAME_H

#include "Protocol/PressureSensor/pressure_sensor_protocol.h"

PressureFrameResult PressureSensor_DecodeFrame(
    const uint8_t *frame,
    size_t frame_length,
    PressureFrameData *output);

#endif
