#ifndef APPLICATION_FORCE_SERVO_OUTPUT_PROFILE_H
#define APPLICATION_FORCE_SERVO_OUTPUT_PROFILE_H
#include "Application/motion_build_policy.h"
/* StaticForceAuthority2: representation is not experimental authorization.
 * New 10% normal /20/30/40% assist profiles have no reviewed time/cooling
 * envelope. The only enabled live profile retains the previous short720/100. */
#if SD700_FORCE_SERVO_COMMISSIONING
#define FS_PRESS_OPERATING_CAP 720
#define FS_INITIAL_KP 10.0f
#define FS_LIVE_PROFILE_ID 4
#define FS_POWERED_TEST_READY 1
#define FS_COMMAND_CEILING 9600
#else
#define FS_PRESS_OPERATING_CAP 100
#define FS_INITIAL_KP 1.0f
#define FS_LIVE_PROFILE_ID 1
#define FS_POWERED_TEST_READY 0
#define FS_COMMAND_CEILING 100
#endif
#define FS_PEAK_PRESS 0
#define FS_BOOST_MS 0
#define FS_BOOST_TOTAL_MS 0
#define FS_BOOST_TAPER_MARGIN 0
#define FS_RELEASE_OPERATING_CAP 100
#ifndef FS_PRESS_PROFILE_CEILING
#define FS_PRESS_PROFILE_CEILING FS_COMMAND_CEILING
#endif
#ifndef FS_RELEASE_PROFILE_CEILING
#define FS_RELEASE_PROFILE_CEILING FS_RELEASE_OPERATING_CAP
#endif
#if FS_PRESS_PROFILE_CEILING > 24000 || FS_RELEASE_PROFILE_CEILING > 24000
#error "Profile ceiling exceeds the PWM command representation"
#endif
#if !SD700_EXPLICIT_REAL_HOST_BUILD && (FS_PRESS_PROFILE_CEILING != FS_COMMAND_CEILING || FS_RELEASE_PROFILE_CEILING != FS_RELEASE_OPERATING_CAP)
#error "Profile override is outside the selected experimental build; host overrides are not firmware approval"
#endif
#if SD700_EXPLICIT_REAL_HOST_BUILD && FS_RELEASE_PROFILE_CEILING > FS_RELEASE_OPERATING_CAP
#define FS_CONTINUOUS_CEILING 2400
#define FS_EXECUTOR_PRESS_CEILING 9600
#define FS_APPROVED_PEAK_MS 1000
#define FS_SYNTHETIC_BOOST 1
#else
#define FS_CONTINUOUS_CEILING FS_PRESS_OPERATING_CAP
#define FS_EXECUTOR_PRESS_CEILING FS_PRESS_OPERATING_CAP
#define FS_APPROVED_PEAK_MS 0
#define FS_SYNTHETIC_BOOST 0
#endif
#endif
