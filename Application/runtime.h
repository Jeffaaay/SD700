#ifndef APPLICATION_RUNTIME_H
#define APPLICATION_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#include "Application/machine.h"
#include "Transport/Pressure/pressure_receiver.h"

typedef struct
{
    MachineContext machine;
    uint64_t delivered_pressure_sequence;
} ApplicationRuntime;

void ApplicationRuntime_Initialize(ApplicationRuntime *runtime,
                                   const MachineConfig *config,
                                   uint32_t now_ms);
void ApplicationRuntime_CompleteBoot(ApplicationRuntime *runtime,
                                     bool boot_checks_passed,
                                     uint32_t now_ms);
bool ApplicationRuntime_ServicePressure(
    ApplicationRuntime *runtime,
    const PressureReceiverSnapshot *pressure,
    uint32_t now_ms);
void ApplicationRuntime_ServiceSafety(ApplicationRuntime *runtime,
                                      uint32_t now_ms);
void ApplicationRuntime_Tick(ApplicationRuntime *runtime, uint32_t now_ms);
MachineContext *ApplicationRuntime_GetMachine(ApplicationRuntime *runtime);

#endif
