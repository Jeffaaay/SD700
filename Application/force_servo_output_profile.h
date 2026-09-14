#ifndef APPLICATION_FORCE_SERVO_OUTPUT_PROFILE_H
#define APPLICATION_FORCE_SERVO_OUTPUT_PROFILE_H
#include "Application/motion_build_policy.h"

/* StaticForce3000_Boost1: one bounded breakaway experiment, not a 25% rating.
 * Command units (nominal mV), not measured terminal voltage or a continuous
 * motor/board rating. Physical supply setting remains 0.5 A. */
#if SD700_FORCE_SERVO_COMMISSIONING
#define FS_PRESS_OPERATING_CAP 720
#define FS_INITIAL_KP 10.0f
#define FS_LIVE_PROFILE_ID 3
#define FS_PEAK_PRESS 6000
#define FS_BOOST_MS 10
#define FS_BOOST_TOTAL_MS 10
#define FS_BOOST_TAPER_MARGIN 72
#define FS_COMMAND_CEILING 6000
#define FS_POWERED_TEST_READY 1 /* supervised experiment only; NOT production qualification */
#else
#define FS_PRESS_OPERATING_CAP 100
#define FS_INITIAL_KP 1.0f
#define FS_LIVE_PROFILE_ID 1
#define FS_PEAK_PRESS 0
#define FS_BOOST_MS 0
#define FS_BOOST_TOTAL_MS 0
#define FS_BOOST_TAPER_MARGIN 0
#define FS_COMMAND_CEILING FS_PRESS_OPERATING_CAP
#define FS_POWERED_TEST_READY 0
#endif
#define FS_RELEASE_OPERATING_CAP 100
#ifndef FS_PRESS_PROFILE_CEILING
#define FS_PRESS_PROFILE_CEILING FS_COMMAND_CEILING
#endif
#ifndef FS_RELEASE_PROFILE_CEILING
#define FS_RELEASE_PROFILE_CEILING FS_RELEASE_OPERATING_CAP
#endif

#if FS_PRESS_PROFILE_CEILING < FS_PRESS_OPERATING_CAP || FS_RELEASE_PROFILE_CEILING < FS_RELEASE_OPERATING_CAP
#error "Operating cap must fit the direction profile ceiling"
#endif
#if FS_PRESS_PROFILE_CEILING > 24000 || FS_RELEASE_PROFILE_CEILING > 24000
#error "Profile ceiling exceeds the PWM command representation"
#endif
#if !SD700_EXPLICIT_REAL_HOST_BUILD && (FS_PRESS_PROFILE_CEILING != FS_COMMAND_CEILING || FS_RELEASE_PROFILE_CEILING != FS_RELEASE_OPERATING_CAP)
#error "Profile override is outside the selected experimental build; host overrides are not firmware approval"
#endif
/* Larger continuous envelopes exist only in the explicit synthetic host fixture. */
#if SD700_EXPLICIT_REAL_HOST_BUILD && FS_RELEASE_PROFILE_CEILING > FS_RELEASE_OPERATING_CAP
#define FS_CONTINUOUS_CEILING FS_PRESS_PROFILE_CEILING
#define FS_SYNTHETIC_BOOST 1
#else
#define FS_CONTINUOUS_CEILING FS_PRESS_OPERATING_CAP
#define FS_SYNTHETIC_BOOST 0
#endif
#endif
