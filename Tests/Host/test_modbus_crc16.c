#include <assert.h>
#include <stdint.h>

#include "Protocol/Modbus/modbus_crc16.h"

int main(void)
{
    static const uint8_t standard_vector[] = "123456789";
    static const uint8_t legacy_request[] = {
        0x01U, 0x06U, 0x00U, 0x00U, 0x05U, 0xDCU
    };

    assert(Modbus_Crc16(standard_vector, 9U) == 0x4B37U);
    assert(Modbus_Crc16(legacy_request, sizeof(legacy_request)) == 0x038BU);
    assert(Modbus_Crc16(standard_vector, 0U) == 0U);
    assert(Modbus_Crc16(NULL, 1U) == 0U);
    assert(Modbus_Crc16(NULL, 0U) == 0U);
    return 0;
}
