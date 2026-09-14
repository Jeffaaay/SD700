#ifndef FORCE_SERVO_PROFILE_H
#define FORCE_SERVO_PROFILE_H
/* StaticForceAuthority2. These are operating-envelope facts, not writable tuning
 * parameters. A future calibrated profile requires reviewed source + firmware.
 * Unit 0 retains the explicitly requested short count-domain experiment.
 * Unit 1 means ALL controller force quantities are N (gain command/N, rate
 * N/s, acceleration N/s^2). Raw counts always remain independently checked.
 * Unit2 is the user-confirmed installed sensor identity in N, without a new
 * calibration or a mechanical/thermal qualification (characterization only).
 * Qualification bits: sensor range=1, calibration=2, mechanics=4, current/time=8.
 * No evidence for these four higher-force qualifications exists in this build. */
#if SD700_FORCE_CHARACTERIZATION
#define FS_LEGACY_OPERATING_MAX 3000
#define FS_LEGACY_RAW_TRIP 3000
#define FS_SENSOR_UNIT 2
#define FS_LIMITS_SOURCE 3
#define FS_ASSIST_RISE 1
#define FS_ASSIST_END 2
#define FS_OFF_MS 5000
/* Runtime unit2: zero removes only elapsed run/build/capture deadlines.
 * Receive lease, assist hard cutoff and conditional stall guards stay finite. */
#define FS_EXPERIMENT_BUDGET_MS 0
#define FS_RESPONSE_UNITS 2
#define FS_EXCESSIVE_RISE 25
#else
#define FS_LEGACY_OPERATING_MAX 275
#define FS_LEGACY_RAW_TRIP 325
#define FS_SENSOR_UNIT 0
#define FS_LIMITS_SOURCE 1
#define FS_ASSIST_RISE 0
#define FS_ASSIST_END 0
#define FS_OFF_MS 0
#define FS_EXPERIMENT_BUDGET_MS 5000
#define FS_RESPONSE_UNITS 0
#define FS_EXCESSIVE_RISE 0
#endif
#define FORCE_SERVO_PROFILE_FIELDS(X) \
 X(id,FS_LIVE_PROFILE_ID) X(unit,FS_SENSOR_UNIT) X(qualifications,0) \
 X(raw_min,0) X(raw_trip,FS_LEGACY_RAW_TRIP) X(scale,1) X(offset,0) \
 X(calibration_min,0) X(calibration_max,0) \
 X(operating_max,FS_LEGACY_OPERATING_MAX) X(force_trip,FS_LEGACY_RAW_TRIP) X(hardware_boundary,0) \
 X(contact,20) X(continuous_press,FS_PRESS_OPERATING_CAP) X(release,FS_RELEASE_OPERATING_CAP) \
 X(peak_press,FS_PEAK_PRESS) X(boost_ms,FS_BOOST_MS) X(boost_total_ms,FS_BOOST_TOTAL_MS) X(taper_margin,FS_BOOST_TAPER_MARGIN) \
 X(energized_ms,FS_EXPERIMENT_BUDGET_MS) X(session_ms,FS_EXPERIMENT_BUDGET_MS) X(capture_ms,FS_EXPERIMENT_BUDGET_MS) X(build_ms,FS_EXPERIMENT_BUDGET_MS) \
 X(progress_window_ms,500) X(progress_units,2) X(no_response_ms,5000) \
 X(experiment_enabled,1) X(limits_source,FS_LIMITS_SOURCE) X(assist_rise_ms,FS_ASSIST_RISE) X(assist_end_ms,FS_ASSIST_END) \
 X(off_ms,FS_OFF_MS) X(response_units,FS_RESPONSE_UNITS) X(excessive_rise_units,FS_EXCESSIVE_RISE)
typedef struct {
#define FS_PROFILE_FIELD(n,d) float n;
 FORCE_SERVO_PROFILE_FIELDS(FS_PROFILE_FIELD)
#undef FS_PROFILE_FIELD
} ForceServoProfile;
#define FORCE_SERVO_PROFILE_WORDS (sizeof(ForceServoProfile)/2U)
extern const ForceServoProfile g_force_servo_profile;
/* Disabled unit0 catalog only: zero time fields mean UNREVIEWED, not approval.
 * Source1 = inherited single-session720/100; source2 = SYNTHETIC test envelope. */
#define FORCE_SERVO_CANDIDATES(X) X(20,4800) X(30,7200) X(40,9600)
#define FORCE_SERVO_CANDIDATE_COUNT 3U
extern const ForceServoProfile g_force_servo_candidates[FORCE_SERVO_CANDIDATE_COUNT];
const ForceServoProfile *ForceServo_FindProfile(uint16_t id);
typedef enum {
 FS_PROFILE_OK=0, FS_PROFILE_INVALID, FS_MISSING_SENSOR_RANGE,
 FS_MISSING_CALIBRATION, FS_MISSING_MECHANICAL_LIMIT,
 FS_MISSING_CURRENT_TIME_LIMIT, FS_TARGET_OUTSIDE_OPERATING_RANGE,
 FS_TARGET_OUTSIDE_CALIBRATION, FS_TRAJECTORY_EXCEEDS_BUDGET,
 FS_BOOST_UNQUALIFIED, FS_WRONG_TARGET_UNIT, FS_EXPERIMENT_LIMITS_UNREVIEWED, FS_COOLING_REQUIRED
} ForceServoRejection;
bool ForceServo_ProfileValid(const ForceServoProfile *p);
uint32_t ForceServo_ProfileDigest(const ForceServoProfile *p);
ForceServoRejection ForceServo_TargetAllowed(const ForceServoProfile *p,float target,bool newtons);
bool ForceServo_Measure(const ForceServoProfile *p,uint32_t raw,int32_t control,float *measured);

bool ForceServo_BoostQualified(const ForceServoProfile *p);
#endif
