#ifndef FORCE_SERVO_MACHINE_H
#define FORCE_SERVO_MACHINE_H
#include "Application/force_servo.h"
/* Wire order is explicitly serialized; no C struct ABI on the bus. */
#define FORCE_SERVO_DIAG_U32(X) \
 X(schema) X(build_id) X(revision) X(now_ms) X(control_at_ms) \
 X(sample_hi) X(sample_lo) X(received_ms) X(age_ms) X(control_sequence) \
 X(session) X(config_version) X(config_digest) X(state) X(fault) X(detail) \
 X(raw) X(limits) X(lease_deadline) X(lease_active) X(tim2) X(tim3) \
 X(locked) X(output_off) X(contact_count) X(contact_lost_count) X(contact_at_ms) \
 X(contact_raw) X(contact_lost_raw) X(contact_threshold) X(session_started_ms) \
 X(saturated_ms) X(tracking_ms) X(skipped_samples) X(rx_interval_min_ms) X(rx_interval_max_ms) X(latest_sample_hi) X(latest_sample_lo) X(latest_received_ms) X(latest_raw) X(delivered_interval_min_ms) X(delivered_interval_max_ms) \
 X(start_pending) X(start_requested_ms) X(last_command_result) \
 X(session_peak_raw) X(session_peak_received_ms) \
 X(profile_id) X(profile_digest) X(unit) X(rejection) X(progress_status) X(no_response_ms) \
 X(reference_complete) X(hold_ms) X(energized_elapsed_ms) X(boost_active) X(boost_deadline_ms) X(boost_spent_ms) X(measured_valid) \
 X(boost_started_ms) X(boost_duration_ms) X(boost_elapsed_ms) X(boost_end_ms) X(boost_end_reason) \
 X(boost_peak_command) X(boost_before_received_ms) X(boost_after_received_ms) X(boost_after_valid) \
 X(assist_admission) X(assist_exit) X(assist_requested_peak) X(assist_peak_ccr) \
 X(assist_rise_ms) X(assist_normal_end_ms) X(cooling_until_ms) X(cooling_active) \
 X(assist_response_pending) X(assist_after_result) X(boost_after_sample_hi) X(boost_after_sample_lo) \
 X(run_reason) X(target_reached) X(target_reached_ms) X(plan_version) X(plan_digest)
#define FORCE_SERVO_DIAG_FLOAT(X) \
 X(dt_s) X(filtered) X(target) X(reference) X(reference_rate) X(error) \
 X(p) X(i) X(d) X(ff) X(raw_output) X(control_committed) X(next_integral) X(current_committed) \
 X(control_pressure) X(requested_output) X(post_limit_output) \
 X(measured) X(force_N) X(planned_reference_s) X(progress_delta) X(session_peak_measured) \
 X(boost_handoff_command) X(boost_pressure_before) X(boost_pressure_after) X(assist_pressure_peak) \
 X(assist_response_peak)
typedef struct {
#define FS_U32(n) uint32_t n;
 FORCE_SERVO_DIAG_U32(FS_U32)
#undef FS_U32
#define FS_FLOAT(n) float n;
 FORCE_SERVO_DIAG_FLOAT(FS_FLOAT)
#undef FS_FLOAT
} ForceServoDiagnostic;
#define FORCE_SERVO_DIAG_WORDS (sizeof(ForceServoDiagnostic)/2U)
typedef struct {
 ForceCharacterizationPlan plan;
 uint16_t plan_staging[10], plan_ack[4];
 uint16_t plan_mask, plan_read_mask, plan_ack_mask;
 uint32_t plan_version, plan_digest;
 bool plan_staging_open, plan_available, plan_armed;
 ForceServoConfig config;
 ForceServoProfile profile;
 ForceServo controller;
 ForceServoDiagnostic diagnostic;
 uint16_t frozen[FORCE_SERVO_DIAG_WORDS];
 uint16_t staging[FORCE_SERVO_CONFIG_WORDS];
 uint64_t staging_mask;
 uint32_t staging_version, config_version, config_digest;
 uint32_t token, session, session_started_ms, contact_at_ms, last_control_rx_ms;
 uint64_t last_control_sequence;
 uint64_t assist_end_sequence;
 uint32_t saturation_started_ms, tracking_started_ms, settle_after_ms;
 uint32_t start_requested_ms;
 uint32_t progress_at_ms, progress_good_ms, hold_at_ms;
 float progress_anchor, progress_start;
 int32_t boost_handoff_request;
 uint32_t cooling_until_ms, required_off_ms;
 bool cooling_active, assist_initial_contact;
 bool progress_commanded, boost_used;
 bool start_pending;
 bool active, contacted, ever_held, saturation_active, tracking_active, approach_wait;
 bool have_sample, staging_open;
} ForceServoMachine;
#endif
