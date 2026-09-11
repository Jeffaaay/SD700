#include "Protocol/PressureSensor/pressure_sensor_decode_frame.h"

#include "Protocol/PressureSensor/pressure_sensor_bcc.h"

PressureFrameResult PressureSensor_DecodeFrame(
    const uint8_t *frame,
    size_t frame_length,
    PressureFrameData *output)
{
    uint16_t raw_pressure_counts;

    if ((frame == NULL) || (output == NULL))
    {
        return PRESSURE_FRAME_INVALID_ARGUMENT;
    }
    if (frame_length != PRESSURE_SENSOR_FRAME_LENGTH)
    {
        return PRESSURE_FRAME_INVALID_LENGTH;
    }
    if (frame[0] != PRESSURE_SENSOR_FRAME_HEADER)
    {
        return PRESSURE_FRAME_INVALID_HEADER;
    }
    if (frame[PRESSURE_SENSOR_FRAME_BCC_INDEX] !=
        PressureSensor_CalculateBcc(frame, PRESSURE_SENSOR_FRAME_BCC_INDEX))
    {
        return PRESSURE_FRAME_INVALID_BCC;
    }
    if (frame[6] != PRESSURE_SENSOR_FRAME_MARKER)
    {
        return PRESSURE_FRAME_INVALID_MARKER;
    }

    raw_pressure_counts =
        (uint16_t)frame[1] | ((uint16_t)frame[2] << 8U);
    output->raw_pressure_counts = raw_pressure_counts;
    return PRESSURE_FRAME_OK;
}
