#include "Protocol/PressureSensor/pressure_sensor_bcc.h"

uint8_t PressureSensor_CalculateBcc(const uint8_t *data, size_t length)
{
    uint8_t bcc = 0U;
    size_t index;

    if (data == NULL)
    {
        return 0U;
    }

    for (index = 0U; index < length; ++index)
    {
        bcc ^= data[index];
    }

    return bcc;
}
