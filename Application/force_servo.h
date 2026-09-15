#ifndef APPLICATION_FORCE_SERVO_H
#define APPLICATION_FORCE_SERVO_H
#include <stdbool.h>
#include <stdint.h>
#include "Application/motion_build_policy.h"
#include "Application/force_servo_output_profile.h"

#if SD700_FORCE_CHARACTERIZATION
#if SD700_BUILD_TO_TARGET
#define FORCE_SERVO_SCHEMA 0xF10CU
#define FORCE_SERVO_BUILD_ID 0x46530112U
#else
#define FORCE_SERVO_SCHEMA 0xF109U
#define FORCE_SERVO_BUILD_ID 0x4653010BU
#endif
#define FS_CONFIG_PRESS_MIN 0
#define FS_CONFIG_SESSION_MS 0
#define FS_CONFIG_SESSION_MIN 0
#define FS_CONFIG_TRACKING_MS 5000
#else
#define FORCE_SERVO_SCHEMA 0xF107U
#define FORCE_SERVO_BUILD_ID 0x46530109U
#define FS_CONFIG_PRESS_MIN 1
#define FS_CONFIG_SESSION_MS 45000
#define FS_CONFIG_SESSION_MIN 1000
#define FS_CONFIG_TRACKING_MS 10000
#endif
#define FORCE_SERVO_REPRESENTABLE 100000.0f
#define FORCE_SERVO_DEFAULT_TARGET 250U
#define FORCE_SERVO_COMMISSIONING_LEASE_MS 130U
#define FORCE_SERVO_COMMISSIONING_AGE_MS 20U
#define FORCE_SERVO_COMMISSIONING_DEADTIME_MS 2U
#define FORCE_SERVO_START_WAIT_MS 250U
/* The explicit profile uses existing reference bounds. 101 ms observed frame interval +
 * 20 ms delivery age rounds up to 125 on the 5 ms control grid; lease adds 5 ms.
 * No change to the 20 ms age accepted for a newly delivered sample. */
#if SD700_FORCE_SERVO_COMMISSIONING
#define FS_REFERENCE_RATE 200
#define FS_REFERENCE_ACCEL 1000
#define FS_FEEDBACK_GAP 125
#define FS_LEASE 130
#else
#define FS_REFERENCE_RATE 20
#define FS_REFERENCE_ACCEL 40
#define FS_FEEDBACK_GAP 40
#define FS_LEASE 50
#endif
/* Plain locked ForceServo keeps its previous defaults/bounds. */
#define FORCE_SERVO_PARAMETERS(X) \
 X(kp,FS_INITIAL_KP,0,1000) X(ki,0,0,1000) X(kd,0,0,100) \
 X(d_filter_s,0.02f,0.001f,1) X(reference_rate,FS_REFERENCE_RATE,0.1f,200) \
 X(reference_acceleration,FS_REFERENCE_ACCEL,0.1f,1000) X(output_rate,1000,1,100000) \
 X(press_cap,FS_PRESS_OPERATING_CAP,FS_CONFIG_PRESS_MIN,FS_PRESS_PROFILE_CEILING) X(release_cap,FS_RELEASE_OPERATING_CAP,1,FS_RELEASE_PROFILE_CEILING) \
 X(integral_min,-1000,-5000,0) X(integral_max,1000,0,5000) \
 X(tracking_gain,5,0.01f,100) X(measurement_filter_s,0,0,1) \
 X(control_min_ms,5,1,50) X(feedback_gap_ms,FS_FEEDBACK_GAP,2,FS_FEEDBACK_GAP) \
 X(sample_age_ms,20,1,20) X(lease_ms,FS_LEASE,3,FS_LEASE) \
 X(hold_enter,5,0.1f,20) X(hold_exit,10,0.2f,40) \
 X(session_ms,FS_CONFIG_SESSION_MS,FS_CONFIG_SESSION_MIN,60000) X(saturation_ms,5000,100,30000) \
 X(tracking_error,50,1,275) X(tracking_ms,FS_CONFIG_TRACKING_MS,100,30000) \
 X(reverse_deadtime_ms,2,2,100) X(hold_dwell_ms,500,0,30000)

typedef struct {
#define FS_FIELD(n,d,l,h) float n;
 FORCE_SERVO_PARAMETERS(FS_FIELD)
#undef FS_FIELD
} ForceServoConfig;
#define FORCE_SERVO_CONFIG_WORDS (sizeof(ForceServoConfig)/2U)
extern const ForceServoConfig g_force_servo_default_config;
/* Kept in the ELF as a verifiable candidate contract; not a hardware certificate. */
extern const uint32_t g_force_servo_contract[20];
#include "Application/force_servo_profile.h"
float ForceServo_TrajectorySeconds(const ForceServoConfig *c,float measured,float target);
ForceServoRejection ForceServo_PlanAllowed(const ForceServoProfile *p,const ForceServoConfig *c,
    float measured,float target,float *seconds);
bool ForceServo_ProfileConfigValid(const ForceServoProfile *p,const ForceServoConfig *c);

/* Canonical request: IEEE754 float32, high Modbus word first; FNV1a LE bytes.
 * Unit2 is user-confirmed installed sensor N, NOT qualified/calibrated unit1. */
typedef struct { float target_N, assist_percent, continuous_percent; } ForceCharacterizationPlan;
bool ForceServo_CharacterizationPlanValid(const ForceCharacterizationPlan *p);
uint32_t ForceServo_CharacterizationDigest(const ForceCharacterizationPlan *p);
int32_t ForceServo_PercentCommand(float percent);
extern const uint32_t g_force_characterization_contract[16];
enum { FS_RUN_NONE=0, FS_RUN_OPERATOR_STOP=1, FS_RUN_FAULT=2,
 FS_RUN_TARGET_NOT_REACHED_WITHIN_SESSION=3, FS_RUN_SESSION_COMPLETE=4,
 FS_RUN_BOUNDARY_TARGET_REACHED=5, FS_RUN_TARGET_REACHED_OFF=6 };

typedef struct {
 float reference, reference_rate, filtered, derivative, integral;
 float start, target, trajectory_s, elapsed_s, previous_committed;
 bool initialized;
} ForceServo;
typedef struct {
 float dt_s, reference, reference_rate, filtered, error;
 float p, i, d, ff, raw_output, limited_output, next_integral;
 uint32_t limits;
} ForceServoStep;
enum { FS_LIMIT_AMPLITUDE=1, FS_LIMIT_RATE=2, FS_LIMIT_INTERLOCK=4,
       FS_LIMIT_QUANTIZATION=8, FS_LIMIT_BOOST=16 };
bool ForceServo_ConfigValid(const ForceServoConfig *c);
uint32_t ForceServo_ConfigDigest(const ForceServoConfig *c);
bool ForceServo_Init(ForceServo *s, const ForceServoConfig *c, float measured, float target);
bool ForceServo_Prepare(ForceServo *s, const ForceServoConfig *c,
                        float measured, float dt_s, ForceServoStep *step);
bool ForceServo_Commit(ForceServo *s, const ForceServoConfig *c,
                       ForceServoStep *step, int32_t committed_mv);
bool ForceServo_SequenceAfter(uint64_t a, uint64_t b);
#endif
