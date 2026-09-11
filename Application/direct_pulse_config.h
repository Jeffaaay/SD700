#ifndef APPLICATION_DIRECT_PULSE_CONFIG_H
#define APPLICATION_DIRECT_PULSE_CONFIG_H

#include "Board/Motor/motor_plan_config.h"

/* Unvalidated RealBench single-pulse test; not production pressure holding. */
#define SD700_DIRECT_PULSE_COMMAND_MV      10000U
#define SD700_DIRECT_PULSE_DURATION_MS       100U
#define SD700_DIRECT_PULSE_BACKSTOP_MS       150U

#if SD700_DIRECT_PULSE_COMMAND_MV == 0U
#error "Direct pulse command must be greater than zero"
#endif

#if SD700_DIRECT_PULSE_COMMAND_MV > MOTOR_PLAN_SUPPLY_REFERENCE_MV
#error "Direct pulse command exceeds the motor supply reference"
#endif

#if SD700_DIRECT_PULSE_DURATION_MS == 0U
#error "Direct pulse duration must be greater than zero"
#endif

#if SD700_DIRECT_PULSE_BACKSTOP_MS <= SD700_DIRECT_PULSE_DURATION_MS
#error "Direct pulse backstop must exceed the normal duration"
#endif

#endif
