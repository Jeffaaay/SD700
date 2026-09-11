#include "Protocol/Modbus/modbus_crc16.h"

uint16_t Modbus_Crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFFU;
    uint8_t bit;

    if ((data == NULL) || (length == 0U))
    {
        return 0U;
    }

    while (length-- > 0U)
    {
        crc ^= *data++;
        for (bit = 0U; bit < 8U; ++bit)
        {
            if ((crc & 0x0001U) != 0U)
            {
                crc = (uint16_t)((crc >> 1U) ^ 0xA001U);
            }
            else
            {
                crc >>= 1U;
            }
        }
    }

    return crc;
}
