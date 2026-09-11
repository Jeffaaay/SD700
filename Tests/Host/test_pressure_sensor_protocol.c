#include <assert.h>
#include <stdint.h>

#include "Protocol/PressureSensor/pressure_sensor_bcc.h"
#include "Protocol/PressureSensor/pressure_sensor_decode_frame.h"

static void MakeValidFrame(uint8_t frame[PRESSURE_SENSOR_FRAME_LENGTH])
{
    frame[0] = PRESSURE_SENSOR_FRAME_HEADER;
    frame[1] = 0x34U;
    frame[2] = 0x12U;
    frame[3] = 0xA5U;
    frame[4] = 0x5AU;
    frame[5] = PressureSensor_CalculateBcc(frame, 5U);
    frame[6] = PRESSURE_SENSOR_FRAME_MARKER;
}

int main(void)
{
    uint8_t frame[PRESSURE_SENSOR_FRAME_LENGTH];
    PressureFrameData output = {0xBEEFU};

    MakeValidFrame(frame);
    assert(PressureSensor_DecodeFrame(frame, sizeof(frame), &output) == PRESSURE_FRAME_OK);
    assert(output.raw_pressure_counts == 0x1234U);

    output.raw_pressure_counts = 0xBEEFU;
    assert(PressureSensor_DecodeFrame(frame, 6U, &output) == PRESSURE_FRAME_INVALID_LENGTH);
    assert(output.raw_pressure_counts == 0xBEEFU);

    frame[0] = 0U;
    assert(PressureSensor_DecodeFrame(frame, sizeof(frame), &output) == PRESSURE_FRAME_INVALID_HEADER);
    MakeValidFrame(frame);
    frame[5] ^= 1U;
    assert(PressureSensor_DecodeFrame(frame, sizeof(frame), &output) == PRESSURE_FRAME_INVALID_BCC);
    MakeValidFrame(frame);
    frame[6] = 0U;
    assert(PressureSensor_DecodeFrame(frame, sizeof(frame), &output) == PRESSURE_FRAME_INVALID_MARKER);

    assert(PressureSensor_DecodeFrame(NULL, sizeof(frame), &output) == PRESSURE_FRAME_INVALID_ARGUMENT);
    assert(PressureSensor_DecodeFrame(frame, sizeof(frame), NULL) == PRESSURE_FRAME_INVALID_ARGUMENT);
    assert(PressureSensor_CalculateBcc(NULL, 5U) == 0U);
    return 0;
}
