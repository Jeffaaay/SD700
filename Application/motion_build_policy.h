#ifndef APPLICATION_MOTION_BUILD_POLICY_H
#define APPLICATION_MOTION_BUILD_POLICY_H

#ifndef SD700_BUILD_TO_TARGET
#define SD700_BUILD_TO_TARGET 0
#endif
#if SD700_BUILD_TO_TARGET != 0 && SD700_BUILD_TO_TARGET != 1
#error "BuildToTarget selection must be 0 or 1"
#endif
#if SD700_BUILD_TO_TARGET && (!defined(SD700_FORCE_CHARACTERIZATION) || SD700_FORCE_CHARACTERIZATION != 1)
#error "BuildToTarget requires the atomic characterization base"
#endif
#ifndef SD700_FORCE_CHARACTERIZATION
#define SD700_FORCE_CHARACTERIZATION 0
#endif
#if SD700_FORCE_CHARACTERIZATION != 0 && SD700_FORCE_CHARACTERIZATION != 1
#error "Characterization selection must be 0 or 1"
#endif
#if SD700_FORCE_CHARACTERIZATION
#if !defined(SD700_FORCE_SERVO_COMMISSIONING) || SD700_FORCE_SERVO_COMMISSIONING != 1 || !defined(SD700_REAL_OUTPUT_ARMING_ENABLED) || SD700_REAL_OUTPUT_ARMING_ENABLED != 1
#error "Characterization requires commissioning and the existing output arming guard"
#endif
#endif
#ifndef SD700_FORCE_SERVO_ENABLED
#define SD700_FORCE_SERVO_ENABLED 0
#endif
#ifndef SD700_FORCE_SERVO_COMMISSIONING
#define SD700_FORCE_SERVO_COMMISSIONING 0
#endif
#if (SD700_FORCE_SERVO_COMMISSIONING != 0) && (SD700_FORCE_SERVO_COMMISSIONING != 1)
#error "ForceServo commissioning must be 0 or 1"
#endif
#if SD700_FORCE_SERVO_COMMISSIONING
#if !SD700_FORCE_SERVO_ENABLED || !defined(SD700_REAL_BENCH_ACKNOWLEDGED) || (SD700_REAL_BENCH_ACKNOWLEDGED != 1)
#error "CommissioningUnlock1 requires ForceServo and explicit RealBench acknowledgement"
#endif
#endif
#if (SD700_FORCE_SERVO_ENABLED != 0) && (SD700_FORCE_SERVO_ENABLED != 1)
#error "ForceServo enable must be 0 or 1"
#endif
#if SD700_FORCE_SERVO_ENABLED
#if defined(SD700_AUTO_TARGET_ENABLED) && SD700_AUTO_TARGET_ENABLED
#error "ForceServo and AutoTarget are mutually exclusive output owners"
#endif
#if !defined(SD700_MOTOR_MODE_REAL_BENCH) || !SD700_MOTOR_MODE_REAL_BENCH
#error "ForceServo requires the RealBench backend"
#endif
#endif
/* Default ForceServo builds remain locked. CommissioningUnlock1 explicitly
 * permits only the bounded, contact-established continuous commissioning path. */


/* Permission for operator AUTO_START; never a power-on start. */
#ifndef SD700_AUTO_TARGET_ENABLED
#define SD700_AUTO_TARGET_ENABLED 0
#endif
#if (SD700_AUTO_TARGET_ENABLED != 0) && (SD700_AUTO_TARGET_ENABLED != 1)
#error "SD700_AUTO_TARGET_ENABLED must be 0 or 1"
#endif
#if SD700_AUTO_TARGET_ENABLED
#if !defined(SD700_MOTOR_MODE_REAL_BENCH)
#error "AutoTarget requires RealBench"
#endif
#if !defined(SD700_REAL_BENCH_ACKNOWLEDGED) || (SD700_REAL_BENCH_ACKNOWLEDGED != 1)
#error "AutoTarget requires explicit RealBench acknowledgement, including host tests"
#endif
#if !defined(SD700_REAL_OUTPUT_ARMING_ENABLED) || (SD700_REAL_OUTPUT_ARMING_ENABLED != 1)
#error "AutoTarget requires explicit real-output arming"
#endif
#endif

#ifndef SD700_SCOPE_TEST_ACKNOWLEDGED
#define SD700_SCOPE_TEST_ACKNOWLEDGED 0
#endif

#ifndef SD700_REAL_BENCH_ACKNOWLEDGED
#define SD700_REAL_BENCH_ACKNOWLEDGED 0
#endif

#if (SD700_SCOPE_TEST_ACKNOWLEDGED != 0) && \
    (SD700_SCOPE_TEST_ACKNOWLEDGED != 1)
#error "SD700_SCOPE_TEST_ACKNOWLEDGED must be 0 or 1"
#endif

#if (SD700_REAL_BENCH_ACKNOWLEDGED != 0) && \
    (SD700_REAL_BENCH_ACKNOWLEDGED != 1)
#error "SD700_REAL_BENCH_ACKNOWLEDGED must be 0 or 1"
#endif

#if defined(SD700_MOTOR_REAL_HOST_TEST) && \
    (SD700_MOTOR_REAL_HOST_TEST != 0) && \
    (SD700_MOTOR_REAL_HOST_TEST != 1)
#error "SD700_MOTOR_REAL_HOST_TEST must be 0 or 1"
#endif

#if defined(SD700_MOTOR_REAL_HOST_TEST) && \
    (SD700_MOTOR_REAL_HOST_TEST == 1)
#define SD700_EXPLICIT_REAL_HOST_BUILD 1
#else
#define SD700_EXPLICIT_REAL_HOST_BUILD 0
#endif

#if (SD700_EXPLICIT_REAL_HOST_BUILD == 1) && \
    defined(STM32F411xE)
#error "SD700_MOTOR_REAL_HOST_TEST is forbidden on STM32F411xE"
#endif

#if defined(SD700_MOTOR_MODE_REAL_COMPILE_CHECK) && \
    (SD700_MOTOR_MODE_REAL_COMPILE_CHECK != 1)
#error "SD700_MOTOR_MODE_REAL_COMPILE_CHECK must equal 1 when defined"
#endif

#if defined(SD700_MOTOR_MODE_SCOPE_TEST) && \
    (SD700_MOTOR_MODE_SCOPE_TEST != 1)
#error "SD700_MOTOR_MODE_SCOPE_TEST must equal 1 when defined"
#endif

#if defined(SD700_MOTOR_MODE_REAL_BENCH) && \
    (SD700_MOTOR_MODE_REAL_BENCH != 1)
#error "SD700_MOTOR_MODE_REAL_BENCH must equal 1 when defined"
#endif

#if (defined(SD700_MOTOR_MODE_REAL_COMPILE_CHECK) + \
     defined(SD700_MOTOR_MODE_SCOPE_TEST) + \
     defined(SD700_MOTOR_MODE_REAL_BENCH)) > 1
#error "Select at most one SD700 real motor mode"
#endif

#if (SD700_SCOPE_TEST_ACKNOWLEDGED == 1) && \
    !defined(SD700_MOTOR_MODE_SCOPE_TEST)
#error "ScopeTest acknowledgement requires ScopeTest mode"
#endif

#if (SD700_REAL_BENCH_ACKNOWLEDGED == 1) && \
    !defined(SD700_MOTOR_MODE_REAL_BENCH)
#error "RealBench acknowledgement requires RealBench mode"
#endif

#if defined(SD700_MOTOR_MODE_SCOPE_TEST) && \
    (SD700_EXPLICIT_REAL_HOST_BUILD == 0) && \
    (SD700_SCOPE_TEST_ACKNOWLEDGED != 1)
#error "ScopeTest target requires SD700_SCOPE_TEST_ACKNOWLEDGED=1"
#endif

#if defined(SD700_MOTOR_MODE_REAL_BENCH) && \
    (SD700_EXPLICIT_REAL_HOST_BUILD == 0) && \
    (SD700_REAL_BENCH_ACKNOWLEDGED != 1)
#error "RealBench target requires SD700_REAL_BENCH_ACKNOWLEDGED=1"
#endif

#if defined(SD700_MOTOR_MODE_SCOPE_TEST)
#define SD700_DIRECT_COMMANDS_ENABLED       1
#define SD700_DIRECT_PRESSURE_REQUIRED      0
#define SD700_AUTOMATIC_COMMANDS_ENABLED    0
#elif defined(SD700_MOTOR_MODE_REAL_BENCH)
#define SD700_DIRECT_COMMANDS_ENABLED       1
#define SD700_DIRECT_PRESSURE_REQUIRED      1
#define SD700_AUTOMATIC_COMMANDS_ENABLED    SD700_AUTO_TARGET_ENABLED
#elif defined(SD700_MOTOR_MODE_REAL_COMPILE_CHECK)
#define SD700_DIRECT_COMMANDS_ENABLED       0
#define SD700_DIRECT_PRESSURE_REQUIRED      0
#define SD700_AUTOMATIC_COMMANDS_ENABLED    0
#else
#define SD700_DIRECT_COMMANDS_ENABLED       0
#define SD700_DIRECT_PRESSURE_REQUIRED      0
#define SD700_AUTOMATIC_COMMANDS_ENABLED    1
#endif

#endif
