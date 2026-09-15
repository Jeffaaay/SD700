#include "Application/force_build_machine.h"
#include "Board/Motor/motor_executor.h"
#include <math.h>
static void fail(MachineContext *m,MachineFault code,FaultDetail detail,uint32_t now)
{ Machine_ReportFault(m,code,detail,now); }
void ForceBuildMachine_Account(MachineContext *m,uint32_t now)
{
 ForceBuildState *b=&m->servo.build;
 uint32_t dt=now-b->accounted_ms; b->accounted_ms=now;
 if (m->servo.active && !b->monitoring && b->phase!=BUILD_PHASE_APPROACH &&
     b->no_response_ms<g_force_build_config.no_response_ms) {
     uint32_t remaining=g_force_build_config.no_response_ms-b->no_response_ms;
     b->no_response_ms+=dt<remaining ? dt : remaining;
 }
}
static void sync_epoch(MachineContext *m,uint32_t now)
{
 MotorBuildSnapshot e=MotorExecutor_GetBuildSnapshot(now); ForceBuildState *b=&m->servo.build;
 if (b->epoch!=e.epoch) {
     b->epoch=e.epoch; b->no_response_ms=0; b->boost=0; b->low_count=0; b->progress_initialized=false;
     /* Cooling cannot restart target-OFF or clear pending response evidence. */
 }
}
void ForceBuildMachine_Publish(MachineContext *m,uint32_t now)
{
 MotorBuildSnapshot e=MotorExecutor_GetBuildSnapshot(now); ForceBuildState *b=&m->servo.build;
 ForceServoDiagnostic *d=&m->servo.diagnostic;
 d->build_mode=1; d->build_config_digest=ForceBuild_ConfigDigest(); d->build_phase=b->phase;
 d->segment_request=e.request; d->segment_phase=e.phase; d->segment_command=e.command;
 d->segment_hard_ms=e.hard_ms; d->segment_started_ms=e.started_ms; d->segment_deadline_ms=e.deadline_ms;
 d->segment_base_command=e.base_command; d->segment_mode=e.mode;
 d->segment_normal_ms=e.hard_ms ? e.hard_ms-1U : 0U;
 d->coarse_boost_command=b->coarse_boost; d->coarse_check_ms=b->coarse_check_ms;
 d->approach_command_ms=e.approach_command_ms;
 d->interpulse_active=e.preload_active; d->interpulse_command=e.preload_active ? e.preload_command : 0;
 d->interpulse_next_command=b->preload_command; d->interpulse_deadline_ms=e.preload_deadline_ms;
 d->interpulse_spent_ms=e.preload_spent_ms; d->interpulse_credit_ms=e.preload_credit_ms;
 d->segment_end_ms=e.ended_ms; d->segment_end_reason=e.end_reason;
 d->post_pulse_pending=b->post_pending; d->exposure_epoch=e.epoch;
 d->energized_reserved_ms=e.reserved_ms; d->approach_reserved_ms=e.approach_reserved_ms;
 d->energized_upper_ms=e.energized_upper_ms; d->full_rest_remaining_ms=e.rest_remaining_ms;
 d->exposure_inhibited=e.inhibited; d->build_boost_command=b->boost; d->build_low_response_count=b->low_count;
 d->build_no_response_ms=b->no_response_ms; d->no_response_ms=b->no_response_ms;
 d->progress_status=!m->servo.active || b->monitoring ? 0U : b->no_response_ms<500 && d->progress_delta>=2 ? 3U : 2U;
 d->build_progress_anchor=b->progress_anchor;
 d->energized_elapsed_ms=e.energized_upper_ms;
 d->post_limit_output=e.segment_active ? (float)e.command : e.preload_active ? (float)e.preload_command : 0;
 d->requested_equivalent_V=d->post_limit_output*.001f;
 d->mapped_pwm_percent=d->post_limit_output/240.0f;
 d->lease_deadline=e.receive_deadline_ms; d->lease_active=e.segment_active || e.preload_active;
 if (e.inhibited) { d->cooling_active=1; d->cooling_until_ms=now+e.rest_remaining_ms; }
}
bool ForceBuildMachine_Ready(MachineContext *m,uint32_t now)
{
 sync_epoch(m,now);
 return !MotorExecutor_GetBuildSnapshot(now).inhibited && !m->servo.build.post_pending &&
     m->servo.build.no_response_ms<g_force_build_config.no_response_ms;
}
void ForceBuildMachine_Service(MachineContext *m,uint32_t now)
{
 MotorBuildSnapshot e=MotorExecutor_GetBuildSnapshot(now);
 if (m->servo.active && !m->servo.build.monitoring && e.end_reason>=BUILD_END_DEADLINE)
     fail(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_BUILD_CUTOFF,now);
}
void ForceBuildMachine_Safety(MachineContext *m,uint32_t now)
{
 ForceServoMachine *s=&m->servo; ForceBuildState *b=&s->build;
 ForceBuildMachine_Account(m,now); sync_epoch(m,now);
 if (!s->active) return;
 if (now-m->pressure.received_at_ms>(uint32_t)s->config.feedback_gap_ms) {
     fail(m,FAULT_PRESSURE_SENSOR_FAULT,FAULT_DETAIL_PRESSURE_TIMEOUT,now); return;
 }
 if (b->monitoring) {
     if (!MotorExecutor_OutputIsDisabled()) fail(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_MOTOR_HARDWARE,now);
     return;
 }
 MotorBuildSnapshot e=MotorExecutor_GetBuildSnapshot(now);
 if (e.end_reason>=BUILD_END_DEADLINE) {
     fail(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_BUILD_CUTOFF,now); return;
 }
 if (!MotorExecutor_BuildOwnerValid(s->token)) {
     fail(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_LEASE_EXPIRED,now); return;
 }
 if (((e.segment_active || e.preload_active) && (int32_t)(now-e.receive_deadline_ms)>=0) ||
     (e.segment_active && (int32_t)(now-e.deadline_ms)>=0) ||
     (e.preload_active && (int32_t)(now-e.preload_deadline_ms)>=0)) {
     fail(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_BUILD_CUTOFF,now); return;
 }
 if (b->phase!=BUILD_PHASE_APPROACH && b->no_response_ms>=g_force_build_config.no_response_ms)
     fail(m,FAULT_MOTION_TIMEOUT,FAULT_DETAIL_BUILD_NO_RESPONSE,now);
}
static void target_off(MachineContext *m,uint32_t now,float measured)
{
 ForceServoMachine *s=&m->servo; ForceBuildState *b=&s->build;
 ForceBuildMachine_Account(m,now);
 if (MotorExecutor_Disable()!=MOTOR_RESULT_OK) { fail(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_MOTOR_HARDWARE,now); return; }
 s->start_pending=false; s->active=true; b->monitoring=true; b->phase=BUILD_PHASE_TARGET_OFF;
 s->diagnostic.target_reached=1; s->diagnostic.target_reached_ms=m->pressure.received_at_ms;
 s->diagnostic.run_reason=m->target_pressure_units==3000 ? FS_RUN_BOUNDARY_TARGET_REACHED : FS_RUN_TARGET_REACHED_OFF;
 s->diagnostic.requested_equivalent_V=0; s->diagnostic.mapped_pwm_percent=0;
 s->diagnostic.requested_output=0; s->diagnostic.post_limit_output=0; s->diagnostic.control_committed=0;
 s->controller.previous_committed=0; s->controller.integral=0;
 if (measured>s->diagnostic.session_peak_measured) s->diagnostic.session_peak_measured=measured;
 m->state=FORCE_TARGET_REACHED_OFF;
}
bool ForceBuildMachine_Begin(MachineContext *m,uint32_t now)
{
 ForceServoMachine *s=&m->servo; ForceBuildState *b=&s->build;
 sync_epoch(m,now); b->accounted_ms=now; b->monitoring=false;
 float measured=(float)m->pressure.raw_pressure_counts;
 b->boost=0; b->low_count=0; b->coarse_boost=0;
 if (!b->preload_command) b->preload_command=g_force_build_config.preload_initial_command;
 b->coarse_check_ms=now; b->coarse_reference=measured;
 b->contact_latched=measured>=g_force_build_config.contact_N;
 s->diagnostic.contact_count=b->contact_latched ? 1U : 0U;
 /* Reached only for an accepted new START's valid fresh frame. Rebase this
  * session after unloading; do not refund no-response or executor exposure.
  * Only a qualified post-pulse net gain can reset the carried response timer. */
 b->progress_anchor=measured; b->progress_initialized=true;
 if (measured>=(float)m->target_pressure_units) { target_off(m,now,measured); return m->state!=FAULT; }
 if (!ForceServo_Init(&s->controller,&s->config,measured,(float)m->target_pressure_units) ||
     MotorExecutor_BeginBuild(now,&s->token)!=MOTOR_RESULT_OK) return false;
 s->last_control_rx_ms=m->pressure.received_at_ms;
 ForceBuildRequest r;
 if (!ForceBuild_Select(measured,(float)m->target_pressure_units,b->contact_latched,b->boost,b->coarse_boost,&r)) return false;
 b->phase=r.phase; m->state=r.phase==BUILD_PHASE_APPROACH ? FORCE_APPROACH : r.phase==BUILD_PHASE_BUILD ? FORCE_BUILD : FORCE_TAPER;
 ForceBuildMachine_Pressure(m,now); return m->state!=FAULT;
}
static void pid_diagnostic(MachineContext *m,uint32_t now,float measured)
{
 ForceServoMachine *s=&m->servo; ForceServoDiagnostic *d=&s->diagnostic;
 uint32_t interval=m->pressure.received_at_ms-s->last_control_rx_ms;
 if (interval<(uint32_t)s->config.control_min_ms) return;
 ForceServoStep step; ForceServoConfig c=s->config;
 if (!ForceServo_Prepare(&s->controller,&c,measured,interval*.001f,&step)) {
     fail(m,FAULT_INTERNAL_FAULT,FAULT_DETAIL_NUMERIC,now); return;
 }
 /* PID remains a replaceable interface/diagnostic. Actual output comes from
  * the disclosed segment profile and is tracked for actual-output anti-windup. */
 c.press_cap=(float)g_force_build_config.approach_ceiling;
 int32_t actual=MotorExecutor_OutputIsDisabled() ? 0 : (int32_t)MotorExecutor_GetSnapshot()->command_mv;
 if (!ForceServo_Commit(&s->controller,&c,&step,actual)) {
     fail(m,FAULT_INTERNAL_FAULT,FAULT_DETAIL_NUMERIC,now); return;
 }
 s->last_control_rx_ms=m->pressure.received_at_ms; s->last_control_sequence=m->pressure.sequence;
 d->control_sequence++; d->control_at_ms=now; d->received_ms=m->pressure.received_at_ms;
 d->sample_hi=(uint32_t)(m->pressure.sequence>>32); d->sample_lo=(uint32_t)m->pressure.sequence;
 d->dt_s=step.dt_s; d->reference=step.reference; d->reference_rate=step.reference_rate;
 d->raw=m->pressure.raw_pressure_counts; d->control_pressure=(float)m->pressure.control_pressure_units;
 d->age_ms=now-m->pressure.received_at_ms; d->limits=step.limits;
 d->filtered=step.filtered; d->error=step.error; d->p=step.p; d->i=step.i; d->d=step.d;
 d->ff=step.ff; d->raw_output=step.raw_output; d->next_integral=step.next_integral; d->control_committed=(float)actual;
 d->reference_complete=s->controller.elapsed_s>=s->controller.trajectory_s;
 bool tracking=d->reference_complete && fabsf(step.error)>c.tracking_error;
 if (tracking && !s->tracking_active) s->tracking_started_ms=now;
 s->tracking_active=tracking; d->tracking_ms=tracking ? now-s->tracking_started_ms : 0;
 /* No Detail17 trigger. Independent cumulative progress remains mandatory. */
}
void ForceBuildMachine_Pressure(MachineContext *m,uint32_t now)
{
 ForceServoMachine *s=&m->servo; ForceBuildState *b=&s->build; ForceServoDiagnostic *d=&s->diagnostic;
 float measured=(float)m->pressure.raw_pressure_counts;
 bool fresh=Machine_IsPressureFresh(m,now);
 if (!fresh) { if (s->active) fail(m,FAULT_PRESSURE_SENSOR_FAULT,FAULT_DETAIL_PRESSURE_TIMEOUT,now); return; }
 if (m->pressure.raw_pressure_counts>=3000) {
     if (s->active && m->target_pressure_units==3000 && !b->monitoring) target_off(m,now,measured);
     if (m->target_pressure_units!=3000 || !b->monitoring || measured>3000) {
         d->run_reason=FS_RUN_FAULT; fail(m,FAULT_OVERPRESSURE,FAULT_DETAIL_NONE,now); return;
     }
 }
 if (s->active && !b->monitoring && measured>=(float)m->target_pressure_units) {
     target_off(m,now,measured); return;
 }
 if (s->active && measured>d->session_peak_measured) d->session_peak_measured=measured;
 MotorBuildSnapshot e=MotorExecutor_GetBuildSnapshot(now);
 bool post=b->post_pending && !e.segment_active &&
     ForceServo_SequenceAfter(m->pressure.sequence,b->post_sequence) &&
     (int32_t)(m->pressure.received_at_ms-e.ended_ms)>=(int32_t)g_force_build_config.off_settle_ms;
 if (post) {
     /* Only a live, normally completed/reduced, owner-matched segment may
      * change adaptation or progress budgets. Terminal readbacks are evidence. */
     if (s->active && !b->monitoring && !MotorExecutor_AcceptBuildPost(s->token,e.request,
         m->pressure.sequence,m->pressure.received_at_ms,now)) {
         fail(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_MOTOR_REQUEST_REJECTED,now); return;
     }
     float rise=measured-b->pulse_before;
     d->pulse_force_before=b->pulse_before; d->pulse_force_after=measured;
     d->post_pulse_valid=1; d->post_pulse_request=e.request; d->post_pulse_received_ms=m->pressure.received_at_ms;
     d->post_pulse_sample_hi=(uint32_t)(m->pressure.sequence>>32); d->post_pulse_sample_lo=(uint32_t)m->pressure.sequence;
     b->post_pending=false;
     if (!b->monitoring && e.phase!=BUILD_PHASE_APPROACH && rise>=g_force_build_config.excessive_rise_N) {
         fail(m,FAULT_MOTION_TIMEOUT,FAULT_DETAIL_BUILD_RESPONSE,now); return;
     }
     if (s->active && e.phase!=BUILD_PHASE_APPROACH && !b->monitoring) {
         uint32_t maximum=(float)m->target_pressure_units-measured<=g_force_build_config.fine_margin_N ?
             g_force_build_config.fine_boost_max_command : g_force_build_config.boost_max_command;
         ForceBuild_PulseBoost(b,rise,maximum);
     }
     if (s->active && !b->monitoring && measured-b->progress_anchor>=g_force_build_config.progress_N) {
         d->progress_delta=measured-b->progress_anchor; b->progress_anchor=measured; b->no_response_ms=0;
     }
 }
 if (!s->active || b->monitoring) return;
 ForceBuildMachine_Account(m,now);
 if (measured>=g_force_build_config.contact_N && !b->contact_latched) {
     b->contact_latched=true; d->contact_count=1; d->contact_raw=m->pressure.raw_pressure_counts;
     s->contact_at_ms=m->pressure.received_at_ms; d->contact_at_ms=s->contact_at_ms;
 }
 if (!e.segment_active && !b->post_pending && !b->contact_latched &&
     (float)m->target_pressure_units-measured>g_force_build_config.fine_margin_N)
     ForceBuild_CoarseBoost(b,measured,now);
 ForceBuildRequest r;
 if (!ForceBuild_Select(measured,(float)m->target_pressure_units,b->contact_latched,b->boost,b->coarse_boost,&r)) {
     fail(m,FAULT_INTERNAL_FAULT,FAULT_DETAIL_NUMERIC,now); return;
 }
 if (r.mode==BUILD_MODE_FINE && b->boost>g_force_build_config.fine_boost_max_command)
     b->boost=g_force_build_config.fine_boost_max_command;
 b->phase=r.phase; m->state=r.phase==BUILD_PHASE_APPROACH ? FORCE_APPROACH : r.phase==BUILD_PHASE_BUILD ? FORCE_BUILD : FORCE_TAPER;
 d->requested_output=(float)r.command; d->post_limit_output=(float)r.command;
 d->requested_equivalent_V=(float)r.command*.001f;
 d->mapped_pwm_percent=(float)r.command/240.0f;
 if (e.segment_active) {
     /* Contact/taper can only end this segment. That frame is not post-pulse
      * feedback and cannot start a second segment. */
     if (r.phase!=e.phase || r.command<e.command || r.command*r.hard_ms<e.command*e.hard_ms) {
         if (MotorExecutor_EndBuildSegment(s->token,now)!=MOTOR_RESULT_OK) {
             fail(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_BUILD_CUTOFF,now); return;
         }
         b->post_sequence=m->pressure.sequence;
     }
 } else if (!b->post_pending) {
     r.preload_command=r.mode==BUILD_MODE_COARSE ? 0 : b->preload_command;
     if (MotorExecutor_StartBuildSegment(s->token,&r,m->pressure.sequence,m->pressure.received_at_ms,now)!=MOTOR_RESULT_OK) {
         MotorBuildSnapshot rejected=MotorExecutor_GetBuildSnapshot(now);
         fail(m,rejected.inhibited ? FAULT_MOTION_TIMEOUT : FAULT_MOTOR_FAULT,
             rejected.inhibited ? FAULT_DETAIL_BUILD_EXPOSURE : FAULT_DETAIL_MOTOR_REQUEST_REJECTED,now); return;
     }
     e=MotorExecutor_GetBuildSnapshot(now); b->post_pending=true;
     b->post_sequence=m->pressure.sequence; b->pulse_before=measured;
     /* Retain the completed before/after pair and its request across the next
      * segment; PC polling can be slower than the feedback-gated scheduler. */
 }
 pid_diagnostic(m,now,measured);
}
