#ifndef APPLICATION_PRESSURE_CONTROL_H
#define APPLICATION_PRESSURE_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

bool PressureControl_ConvertRawCounts(uint16_t raw_pressure_counts,
                                      int32_t *control_pressure_units);

#endif
