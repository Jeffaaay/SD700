#include "Application/pressure_control.h"

#include <stddef.h>

#include "Application/bench_config.h"

bool PressureControl_ConvertRawCounts(uint16_t raw_pressure_counts,
                                      int32_t *control_pressure_units)
{
    if (control_pressure_units == NULL)
    {
        return false;
    }

#if SD700_BENCH_RAW_COUNTS_CONTROL
    /* Existing sensor-count identity conversion; also used by AutoTarget.
     * No force calibration or Newton conversion is implied. */
    *control_pressure_units = (int32_t)raw_pressure_counts;
    return true;
#else
    (void)raw_pressure_counts;
    *control_pressure_units = 0;
    return false;
#endif
}
