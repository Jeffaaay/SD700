#ifndef APPLICATION_FORCE_BUILD_MACHINE_H
#define APPLICATION_FORCE_BUILD_MACHINE_H
#include "Application/machine.h"
void ForceBuildMachine_Publish(MachineContext *m,uint32_t now);
void ForceBuildMachine_Account(MachineContext *m,uint32_t now);
void ForceBuildMachine_Safety(MachineContext *m,uint32_t now);
void ForceBuildMachine_Pressure(MachineContext *m,uint32_t now);
void ForceBuildMachine_Service(MachineContext *m,uint32_t now);
bool ForceBuildMachine_Begin(MachineContext *m,uint32_t now);
bool ForceBuildMachine_Ready(MachineContext *m,uint32_t now);
#endif
