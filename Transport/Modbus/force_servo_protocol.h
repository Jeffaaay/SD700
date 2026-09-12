#ifndef FORCE_SERVO_PROTOCOL_H
#define FORCE_SERVO_PROTOCOL_H
#include "Application/machine.h"
#define FS_REG_BEGIN 0x0100U
#define FS_REG_COMMIT 0x0101U
#define FS_REG_SNAPSHOT 0x0102U
#define FS_REG_CONFIG 0x0110U
#define FS_INPUT_INFO 0x0100U
#define FS_INPUT_SNAPSHOT 0x0200U
#define FS_COIL_START 0x0010U
#define FS_COIL_RESET 0x0011U
bool ForceServoProtocol_WriteAddress(uint8_t function,uint16_t address);
MachineCommandResult ForceServoProtocol_Write(MachineContext *m,uint8_t function,
    uint16_t address,uint16_t value,uint32_t now_ms);
bool ForceServoProtocol_Read(const MachineContext *m,bool holding,uint16_t address,uint16_t *value);
#endif
