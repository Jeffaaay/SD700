#include "Application/bench_config.h"

/*
 * BENCH_ONLY_UNVALIDATED
 * NOT_APPROVED_FOR_PHYSICAL_MOTION
 *
 * All values below are dry-run integration values in raw sensor-count control
 * units.  They are not calibrated pressure or approved motion parameters.
 */
const MachineConfig g_sd700_bench_machine_config = {
    60000,
    100,
    5,
    10,
    200U, /* pressure_freshness_ms; measured frame gap reaches 101 ms */
    10U,
    200U, /* settle_feedback_timeout_ms */
    6000U,
    250U,
    300U,
    3000U,
    100U,
    150U,
    2000U,
    20U,
    50U,
    2000U,
    (SD700_BENCH_RELEASE_CORRECTION_ENABLED != 0),
    false,
    0U
};
