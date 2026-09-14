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
     * The consuming profile owns units: characterization unit2 declares the
     * user-confirmed installed sensor 1 reported unit = 1 N; no new calibration. */
    *control_pressure_units = (int32_t)raw_pressure_counts;
    return true;
#else
    (void)raw_pressure_counts;
    *control_pressure_units = 0;
    return false;
#endif
}
