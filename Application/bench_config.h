#ifndef APPLICATION_BENCH_CONFIG_H
#define APPLICATION_BENCH_CONFIG_H

#include "Application/machine.h"

/*
 * BENCH_ONLY_UNVALIDATED
 * NOT_APPROVED_FOR_PHYSICAL_MOTION
 */
#define SD700_PHYSICAL_MOTOR_OUTPUT_LOCKED          1
#define SD700_BENCH_RAW_COUNTS_CONTROL              1
#define SD700_BENCH_RELEASE_CORRECTION_ENABLED      1

#if SD700_PHYSICAL_MOTOR_OUTPUT_LOCKED != 1
#error "Physical motor output unlock is outside this PR"
#endif

#if SD700_BENCH_RAW_COUNTS_CONTROL && \
    (SD700_PHYSICAL_MOTOR_OUTPUT_LOCKED != 1)
#error "Raw-count bench control cannot coexist with physical motor output"
#endif

extern const MachineConfig g_sd700_bench_machine_config;

#endif
