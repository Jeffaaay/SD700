#ifndef APPLICATION_FORCE_SERVO_OUTPUT_PROFILE_H
#define APPLICATION_FORCE_SERVO_OUTPUT_PROFILE_H
#include "Application/motion_build_policy.h"

/* Target250Continuous2: no higher continuous motor/board rating supplied.
 * These are command units (nominal mV), NOT measured terminal voltage.
 * Keep separate direction ceilings and selected operating defaults.
 * Overrides exist for production-path HOST range tests, not field escalation. */
#ifndef FS_PRESS_PROFILE_CEILING
#define FS_PRESS_PROFILE_CEILING 100
#endif
#ifndef FS_RELEASE_PROFILE_CEILING
#define FS_RELEASE_PROFILE_CEILING 100
#endif
#define FS_PRESS_OPERATING_CAP 100
#define FS_RELEASE_OPERATING_CAP 100
#define FS_INITIAL_KP 1.0f
#define FS_POWERED_TEST_READY 0

#if FS_PRESS_PROFILE_CEILING < FS_PRESS_OPERATING_CAP || FS_RELEASE_PROFILE_CEILING < FS_RELEASE_OPERATING_CAP
#error "Operating cap must fit the direction profile ceiling"
#endif
#if FS_PRESS_PROFILE_CEILING > 24000 || FS_RELEASE_PROFILE_CEILING > 24000
#error "Profile ceiling exceeds the PWM command representation"
#endif
#if !SD700_EXPLICIT_REAL_HOST_BUILD && (FS_PRESS_PROFILE_CEILING != 100 || FS_RELEASE_PROFILE_CEILING != 100)
#error "Higher continuous profile requires hardware rating review; host overrides are not firmware approval"
#endif
#endif
