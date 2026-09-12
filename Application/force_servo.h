#ifndef APPLICATION_FORCE_SERVO_H
#define APPLICATION_FORCE_SERVO_H
#include <stdbool.h>
#include <stdint.h>

#define FORCE_SERVO_SCHEMA 0xF101U
#define FORCE_SERVO_BUILD_ID 0x46530101U
#define FORCE_SERVO_MAX_TARGET 275U
#define FORCE_SERVO_RAW_ABORT 325U
#define FORCE_SERVO_CONTACT 20U
#define FORCE_SERVO_BUILD_MS 30000U
/* Numerical seeds only. ALL physical output is compile locked in this release. */
#define FORCE_SERVO_PARAMETERS(X) \
 X(kp,1.0f,0,1000) X(ki,0,0,1000) X(kd,0,0,100) \
 X(d_filter_s,0.02f,0.001f,1) X(reference_rate,20,0.1f,200) \
 X(reference_acceleration,40,0.1f,1000) X(output_rate,1000,1,100000) \
 X(press_cap,250,1,4999) X(release_cap,100,1,799) \
 X(integral_min,-1000,-5000,0) X(integral_max,1000,0,5000) \
 X(tracking_gain,5,0.01f,100) X(measurement_filter_s,0,0,1) \
 X(control_min_ms,5,1,50) X(feedback_gap_ms,40,2,100) \
 X(sample_age_ms,20,1,99) X(lease_ms,50,3,100) \
 X(hold_enter,5,0.1f,20) X(hold_exit,10,0.2f,40) \
 X(session_ms,45000,1000,60000) X(saturation_ms,5000,100,30000) \
 X(tracking_error,50,1,275) X(tracking_ms,10000,100,30000) \
 X(reverse_deadtime_ms,2,1,100)

typedef struct {
#define FS_FIELD(n,d,l,h) float n;
 FORCE_SERVO_PARAMETERS(FS_FIELD)
#undef FS_FIELD
} ForceServoConfig;
#define FORCE_SERVO_CONFIG_WORDS (sizeof(ForceServoConfig)/2U)
extern const ForceServoConfig g_force_servo_default_config;
/* Kept in the ELF as a verifiable candidate contract; not a hardware certificate. */
extern const uint32_t g_force_servo_contract[8];
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
       FS_LIMIT_QUANTIZATION=8 };
bool ForceServo_ConfigValid(const ForceServoConfig *c);
uint32_t ForceServo_ConfigDigest(const ForceServoConfig *c);
bool ForceServo_Init(ForceServo *s, const ForceServoConfig *c, float measured, float target);
bool ForceServo_Prepare(ForceServo *s, const ForceServoConfig *c,
                        float measured, float dt_s, ForceServoStep *step);
bool ForceServo_Commit(ForceServo *s, const ForceServoConfig *c,
                       ForceServoStep *step, int32_t committed_mv);
bool ForceServo_SequenceAfter(uint64_t a, uint64_t b);
#endif
