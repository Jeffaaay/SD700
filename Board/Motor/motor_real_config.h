#ifndef BOARD_MOTOR_MOTOR_REAL_CONFIG_H
#define BOARD_MOTOR_MOTOR_REAL_CONFIG_H

#include "Application/motion_build_policy.h"

/* Real target output is armed only by an explicit bounded bench mode. */
#ifndef SD700_REAL_OUTPUT_ARMING_ENABLED
#define SD700_REAL_OUTPUT_ARMING_ENABLED 0
#endif

#if (SD700_REAL_OUTPUT_ARMING_ENABLED != 0) && \
    (SD700_REAL_OUTPUT_ARMING_ENABLED != 1)
#error "SD700_REAL_OUTPUT_ARMING_ENABLED must be 0 or 1"
#endif

#if defined(SD700_MOTOR_MODE_REAL_COMPILE_CHECK) && \
    (SD700_REAL_OUTPUT_ARMING_ENABLED != 0)
#error "RealCompileCheck must keep real output unarmed"
#endif

#if defined(SD700_MOTOR_MODE_SCOPE_TEST) && \
    (SD700_REAL_OUTPUT_ARMING_ENABLED != 1)
#error "ScopeTest must explicitly arm real output"
#endif

#if defined(SD700_MOTOR_MODE_REAL_BENCH) && \
    (SD700_REAL_OUTPUT_ARMING_ENABLED != 1)
#error "RealBench must explicitly arm real output"
#endif

#if (SD700_REAL_OUTPUT_ARMING_ENABLED == 1) && \
    (SD700_EXPLICIT_REAL_HOST_BUILD != 1) && \
    !defined(SD700_MOTOR_MODE_SCOPE_TEST) && \
    !defined(SD700_MOTOR_MODE_REAL_BENCH)
#error "Target real output may be armed only by ScopeTest or RealBench"
#endif

#endif
