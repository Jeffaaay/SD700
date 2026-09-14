#ifndef FORCE_SERVO_PROFILE_H
#define FORCE_SERVO_PROFILE_H
/* StaticForce3000_Boost1. These are operating-envelope facts, not writable tuning
 * parameters. A future calibrated profile requires reviewed source + firmware.
 * Unit 0 retains the explicitly requested short count-domain experiment.
 * Unit 1 means ALL controller force quantities are N (gain command/N, rate
 * N/s, acceleration N/s^2). Raw counts always remain independently checked.
 * Qualification bits: sensor range=1, calibration=2, mechanics=4, current/time=8.
 * No evidence for these four higher-force qualifications exists in this build. */
#define FS_LEGACY_OPERATING_MAX 275
#define FS_LEGACY_RAW_TRIP 325
#define FS_EXPERIMENT_BUDGET_MS 5000
#define FORCE_SERVO_PROFILE_FIELDS(X) \
 X(id,FS_LIVE_PROFILE_ID) X(unit,0) X(qualifications,0) \
 X(raw_min,0) X(raw_trip,FS_LEGACY_RAW_TRIP) X(scale,1) X(offset,0) \
 X(calibration_min,0) X(calibration_max,0) \
 X(operating_max,FS_LEGACY_OPERATING_MAX) X(force_trip,FS_LEGACY_RAW_TRIP) X(hardware_boundary,0) \
 X(contact,20) X(continuous_press,FS_PRESS_OPERATING_CAP) X(release,FS_RELEASE_OPERATING_CAP) \
 X(peak_press,FS_PEAK_PRESS) X(boost_ms,FS_BOOST_MS) X(boost_total_ms,FS_BOOST_TOTAL_MS) X(taper_margin,FS_BOOST_TAPER_MARGIN) \
 X(energized_ms,FS_EXPERIMENT_BUDGET_MS) X(session_ms,FS_EXPERIMENT_BUDGET_MS) X(capture_ms,FS_EXPERIMENT_BUDGET_MS) X(build_ms,FS_EXPERIMENT_BUDGET_MS) \
 X(progress_window_ms,500) X(progress_units,2) X(no_response_ms,5000)
typedef struct {
#define FS_PROFILE_FIELD(n,d) float n;
 FORCE_SERVO_PROFILE_FIELDS(FS_PROFILE_FIELD)
#undef FS_PROFILE_FIELD
} ForceServoProfile;
#define FORCE_SERVO_PROFILE_WORDS (sizeof(ForceServoProfile)/2U)
extern const ForceServoProfile g_force_servo_profile;
typedef enum {
 FS_PROFILE_OK=0, FS_PROFILE_INVALID, FS_MISSING_SENSOR_RANGE,
 FS_MISSING_CALIBRATION, FS_MISSING_MECHANICAL_LIMIT,
 FS_MISSING_CURRENT_TIME_LIMIT, FS_TARGET_OUTSIDE_OPERATING_RANGE,
 FS_TARGET_OUTSIDE_CALIBRATION, FS_TRAJECTORY_EXCEEDS_BUDGET,
 FS_BOOST_UNQUALIFIED, FS_WRONG_TARGET_UNIT
} ForceServoRejection;
bool ForceServo_ProfileValid(const ForceServoProfile *p);
uint32_t ForceServo_ProfileDigest(const ForceServoProfile *p);
ForceServoRejection ForceServo_TargetAllowed(const ForceServoProfile *p,float target,bool newtons);
bool ForceServo_Measure(const ForceServoProfile *p,uint32_t raw,int32_t control,float *measured);
bool ForceServo_IsBreakawayProfile(const ForceServoProfile *p);
bool ForceServo_BoostQualified(const ForceServoProfile *p);
#endif
