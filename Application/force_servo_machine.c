#include "Application/machine.h"
#include "Application/auto_target_config.h"
#include "Board/Motor/motor_executor.h"
#include "Board/Motor/motor_atomic.h"
#include <math.h>
#include <string.h>

static void publish(MachineContext *m,uint32_t now)
{
 ForceServoMachine *s=&m->servo;
 ForceServoDiagnostic *d=&s->diagnostic;
 float measured=0;
 bool measured_valid=ForceServo_Measure(&s->profile,m->pressure.raw_pressure_counts,
                                      m->pressure.control_pressure_units,&measured);
 uint32_t profile_digest=ForceServo_ProfileDigest(&s->profile);
 uint32_t key=MotorAtomic_Enter();
 const MotorExecutorSnapshot *e=MotorExecutor_GetSnapshot();
 d->schema=FORCE_SERVO_SCHEMA; d->build_id=g_force_servo_contract[1];
 d->revision++; d->now_ms=now; d->age_ms=now-d->received_ms;
 d->session=s->session; d->config_version=s->config_version; d->config_digest=s->config_digest;
 d->state=m->state; d->fault=m->fault; d->detail=m->fault_detail;
 d->latest_sample_hi=(uint32_t)(m->pressure.sequence>>32);
 d->latest_sample_lo=(uint32_t)m->pressure.sequence;
 d->latest_received_ms=m->pressure.received_at_ms; d->latest_raw=m->pressure.raw_pressure_counts;
 d->target=(float)m->target_pressure_units; d->contact_threshold=(uint32_t)s->profile.contact;
 d->profile_id=(uint32_t)s->profile.id; d->profile_digest=profile_digest;
 d->unit=(uint32_t)s->profile.unit;
 d->measured_valid=measured_valid && s->have_sample;
 if (d->measured_valid) {
     d->measured=measured; d->force_N=s->profile.unit==1 ? measured : 0;
 } else { d->measured=0; d->force_N=0; } /* Invalid numeric sentinel; use measured_valid. */
 if (s->active) d->energized_elapsed_ms=now-s->session_started_ms;
 d->lease_deadline=e->logical_deadline_ms;
 d->lease_active=s->contacted && e->logical_active && MotorExecutor_ActiveRequestIsValid();
 d->locked=e->physical_output_locked; d->output_off=e->physical_output_disabled;
 d->tim2=e->planned_tim2_ccr3; d->tim3=e->planned_tim3_ccr3;
 d->current_committed=e->physical_output_disabled ? 0 :
     (e->direction==MOTOR_DIRECTION_PRESS ? (float)e->command_mv : -(float)e->command_mv);
 d->session_started_ms=s->session_started_ms;
 d->start_pending=s->start_pending; d->start_requested_ms=s->start_requested_ms;
 d->last_command_result=m->last_command_result;
 MotorAtomic_Leave(key);
}
static void fault(MachineContext *m,MachineFault f,FaultDetail detail,uint32_t now)
{
 (void)MotorExecutor_Disable();
 m->servo.start_pending=false;
 m->servo.active=false; m->servo.diagnostic.boost_active=0; m->servo.controller.initialized=false; m->servo.controller.integral=0;
 if (m->state!=FAULT) { m->fault=f; m->fault_detail=detail; }
 m->state=FAULT; publish(m,now);
}
void Machine_ReportFault(MachineContext *m,MachineFault f,FaultDetail detail,uint32_t now)
{ if (m) fault(m,f,detail,now); }
void Machine_Initialize(MachineContext *m,const MachineConfig *cfg,uint32_t now)
{
 if (!m) return;
 memset(m,0,sizeof(*m)); m->state=BOOT_SAFE;
 if (cfg) m->config=*cfg;
 /* The immutable approach and raw protection contract is inherited, not tunable. */
 m->config_valid=cfg && memcmp(cfg,&g_sd700_auto_target_machine_config,sizeof(*cfg))==0;
 m->servo.config=g_force_servo_default_config;
 m->servo.profile=g_force_servo_profile;
 m->servo.config_version=1;
 m->servo.config_digest=ForceServo_ConfigDigest(&m->servo.config);
#if SD700_FORCE_SERVO_COMMISSIONING
 m->target_pressure_units=FORCE_SERVO_DEFAULT_TARGET; m->target_valid=true;
#endif
 (void)MotorExecutor_Disable(); publish(m,now);
}
void Machine_CompleteBoot(MachineContext *m,bool checks,uint32_t now)
{
 if (!m || m->state!=BOOT_SAFE) return;
 if (!checks || !m->config_valid || !ForceServo_ProfileConfigValid(&m->servo.profile,&m->servo.config) ||
     !MotorExecutor_OutputIsDisabled() || !MotorExecutor_IsHealthy())
     fault(m,FAULT_BOOT_FAULT,FAULT_DETAIL_BOOT_CONFIGURATION,now);
 else { m->state=IDLE; publish(m,now); }
}
bool Machine_IsPressureFresh(const MachineContext *m,uint32_t now)
{
 return m && m->servo.have_sample && m->pressure.frame_valid && m->pressure.control_units_valid &&
        now-m->pressure.received_at_ms<=(uint32_t)m->servo.config.sample_age_ms;
}
MachineCommandResult Machine_ConfigureForcePi(MachineContext *m,const ForcePiConfig *c)
{ (void)m; (void)c; return COMMAND_UNSUPPORTED; }

static bool contact(MachineContext *m,uint32_t now)
{
 ForceServoMachine *s=&m->servo;
 if (MotorExecutor_Disable()!=MOTOR_RESULT_OK) return false;
 s->contacted=true; s->contact_at_ms=m->pressure.received_at_ms;
 m->cycle_started_ms=s->contact_at_ms;
 s->diagnostic.contact_at_ms=s->contact_at_ms;
 s->diagnostic.contact_raw=m->pressure.raw_pressure_counts;
 float measured;
 if (!ForceServo_Measure(&s->profile,m->pressure.raw_pressure_counts,m->pressure.control_pressure_units,&measured)) return false;
 s->diagnostic.contact_count=measured>=s->profile.contact ? 1U : 0U;
 if (!ForceServo_Init(&s->controller,&s->config,measured,
                      (float)m->target_pressure_units) ||
     MotorExecutor_BeginContinuous(&s->token)!=MOTOR_RESULT_OK) return false;
 uint32_t budget=(uint32_t)fminf(s->profile.energized_ms,fminf(s->profile.session_ms,s->config.session_ms));
 if (!MotorExecutor_SetContinuousBudget(s->token,now,budget,(int32_t)s->config.press_cap)) return false;
 int32_t committed; bool interlock;
 if (MotorExecutor_UpdateContinuous(s->token,m->pressure.sequence,m->pressure.received_at_ms,now,
        (uint32_t)s->config.lease_ms,(uint32_t)s->config.sample_age_ms,
        (uint32_t)s->config.reverse_deadtime_ms,0,&committed,&interlock)!=MOTOR_RESULT_OK) return false;
 s->last_control_sequence=m->pressure.sequence; s->last_control_rx_ms=m->pressure.received_at_ms;
 s->diagnostic.filtered=s->controller.filtered; s->diagnostic.reference=s->controller.reference;
 m->state=FORCE_BUILD;
 return true;
}
static MachineCommandResult begin_session(MachineContext *m,uint32_t now)
{
 ForceServoMachine *s=&m->servo;
 s->start_pending=false; s->active=true; s->contacted=false; s->ever_held=false;
 s->saturation_active=false; s->tracking_active=false; s->staging_open=false;
 s->approach_wait=false; s->session_started_ms=now;
 s->progress_at_ms=now; s->progress_good_ms=now; s->progress_commanded=false; s->hold_at_ms=0; s->boost_used=false;
 float measured=0; (void)ForceServo_Measure(&s->profile,m->pressure.raw_pressure_counts,m->pressure.control_pressure_units,&measured);
 s->progress_anchor=measured; s->progress_start=measured;
 if (++s->session==0) ++s->session;
 uint32_t spent=s->diagnostic.boost_spent_ms;
 float planned=s->diagnostic.planned_reference_s;
 memset(&s->diagnostic,0,sizeof(s->diagnostic));
 s->diagnostic.boost_spent_ms=spent; s->diagnostic.planned_reference_s=planned;
 s->diagnostic.session_peak_measured=measured;
 s->diagnostic.raw=m->pressure.raw_pressure_counts;
 s->diagnostic.session_peak_raw=m->pressure.raw_pressure_counts;
 s->diagnostic.session_peak_received_ms=m->pressure.received_at_ms;
 s->diagnostic.control_pressure=(float)m->pressure.control_pressure_units;
 s->diagnostic.received_ms=m->pressure.received_at_ms;
 s->diagnostic.sample_hi=(uint32_t)(m->pressure.sequence>>32);
 s->diagnostic.sample_lo=(uint32_t)m->pressure.sequence;
 m->state=FORCE_APPROACH;
#if !SD700_FORCE_SERVO_COMMISSIONING
 if (s->progress_start>=s->profile.contact)
#endif
 {
     /* The explicit profile always starts the continuous owner from the new measurement;
      * it never uses the old approach pulse, even below the contact threshold. */
     if (!contact(m,now)) fault(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_MOTOR_HARDWARE,now);
     s->contact_at_ms=now; m->cycle_started_ms=now; s->diagnostic.contact_at_ms=now;
 }
 return m->state==FAULT ? COMMAND_EXECUTOR_FAILED : COMMAND_ACCEPTED;
}
MachineCommandResult Machine_HandleCommand(MachineContext *m,const MachineCommand *c,uint32_t now)
{
 if (!m || !c) return COMMAND_INVALID_VALUE;
 MachineCommandResult r=COMMAND_UNSUPPORTED;
 ForceServoMachine *s=&m->servo;
 if (c->type==CMD_STOP || c->type==CMD_JOG_STOP) {
     s->start_pending=false;
     s->active=false; s->diagnostic.boost_active=0; s->controller.initialized=false; s->controller.integral=0;
     s->staging_open=false;
     if (MotorExecutor_Disable()!=MOTOR_RESULT_OK) {
         fault(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_MOTOR_HARDWARE,now); r=COMMAND_EXECUTOR_FAILED;
     } else { if (m->state!=FAULT) m->state=IDLE; r=COMMAND_ACCEPTED; }
 } else if (c->type==CMD_FAULT_RESET) {
     if (m->state==FAULT && MotorExecutor_OutputIsDisabled() && MotorExecutor_IsHealthy()) {
         m->state=IDLE; m->fault=FAULT_NONE; m->fault_detail=FAULT_DETAIL_NONE; r=COMMAND_ACCEPTED;
     } else r=COMMAND_NOT_ALLOWED;
 } else if (c->type==CMD_SET_TARGET) {
     if (m->state!=IDLE || s->start_pending || !MotorExecutor_OutputIsDisabled()) r=COMMAND_BUSY;
     else {
         s->diagnostic.rejection=ForceServo_TargetAllowed(&s->profile,(float)c->target_pressure_units,s->profile.unit==1);
         if (s->diagnostic.rejection!=FS_PROFILE_OK) { m->target_valid=false; r=COMMAND_INVALID_VALUE; }
         else { m->target_pressure_units=c->target_pressure_units; m->target_valid=true; r=COMMAND_ACCEPTED; }
     }
 } else if (c->type==CMD_FORCE_START) {
     if (m->state!=IDLE || s->active || s->start_pending) r=COMMAND_BUSY;
     else if (!m->target_valid ||
#if !SD700_FORCE_SERVO_COMMISSIONING
              !Machine_IsPressureFresh(m,now) ||
#endif
              !ForceServo_ProfileConfigValid(&s->profile,&s->config) || !MotorExecutor_IsHealthy() ||
              !MotorExecutor_OutputIsDisabled() || MotorExecutor_GetSnapshot()->physical_output_locked)
         r=COMMAND_NOT_READY;
#if SD700_FORCE_SERVO_COMMISSIONING
     else {
         float measured=0;
         if (!s->have_sample || !ForceServo_Measure(&s->profile,m->pressure.raw_pressure_counts,m->pressure.control_pressure_units,&measured))
             s->diagnostic.rejection=FS_PROFILE_INVALID;
         else s->diagnostic.rejection=ForceServo_PlanAllowed(&s->profile,&s->config,measured,
             (float)m->target_pressure_units,&s->diagnostic.planned_reference_s);
         if (s->diagnostic.rejection!=FS_PROFILE_OK) r=COMMAND_NOT_READY;
         else { /* One request; the next fresh frame rechecks plan before any output. */
             s->start_pending=true; s->start_requested_ms=now; s->staging_open=false;
             r=COMMAND_ACCEPTED;
         }
     }
#else
     else if (m->pressure.raw_pressure_counts>=s->profile.raw_trip) {
         fault(m,FAULT_OVERPRESSURE,FAULT_DETAIL_NONE,now); r=COMMAND_NOT_READY;
     } else r=begin_session(m,now);
#endif
 }
 m->last_command_result=r; publish(m,now); return r;
}
void Machine_CheckPressureSafety(MachineContext *m,uint32_t now)
{
 if (!m) return;
 ForceServoMachine *s=&m->servo;
 if (s->start_pending) {
     if (!MotorExecutor_OutputIsDisabled() || !MotorExecutor_IsHealthy() ||
         MotorExecutor_GetSnapshot()->logical_active)
         fault(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_MOTOR_HARDWARE,now);
     else if (now-s->start_requested_ms>=FORCE_SERVO_START_WAIT_MS)
         fault(m,FAULT_PRESSURE_SENSOR_FAULT,FAULT_DETAIL_PRESSURE_TIMEOUT,now);
     return;
 }
 if (!s->active) return;
 if (now-s->session_started_ms>=(uint32_t)fminf(s->config.session_ms,fminf(s->profile.session_ms,s->profile.energized_ms)))
     fault(m,FAULT_MOTION_TIMEOUT,FAULT_DETAIL_SESSION_TIMEOUT,now);
 else if (s->contacted && !s->ever_held && now-s->contact_at_ms>=(uint32_t)s->profile.build_ms)
     fault(m,FAULT_MOTION_TIMEOUT,FAULT_DETAIL_CYCLE_TIMEOUT,now);
 else if (now-m->pressure.received_at_ms>(uint32_t)s->config.feedback_gap_ms)
     fault(m,FAULT_PRESSURE_SENSOR_FAULT,FAULT_DETAIL_PRESSURE_TIMEOUT,now);
 else if (s->contacted && (MotorExecutor_ContinuousExpired() || MotorExecutor_GetSnapshot()->last_completion==MOTOR_COMPLETION_LEASE ||
                          !MotorExecutor_ActiveRequestIsValid()))
     fault(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_LEASE_EXPIRED,now);
}
void Machine_HandlePressureSample(MachineContext *m,const MachinePressureSample *p,uint32_t now)
{
 if (!m || !p) return;
 ForceServoMachine *s=&m->servo;
 if (!p->frame_valid || !p->control_units_valid || p->control_pressure_units<0) {
     if (s->active || s->start_pending) fault(m,FAULT_PRESSURE_SENSOR_FAULT,FAULT_DETAIL_PRESSURE_INVALID,now);
     return;
 }
 if (s->have_sample && !ForceServo_SequenceAfter(p->sequence,m->pressure.sequence)) {
     if ((s->active || s->start_pending) && p->sequence!=m->pressure.sequence)
         fault(m,FAULT_PRESSURE_SENSOR_FAULT,FAULT_DETAIL_PRESSURE_ORDER_LOST,now);
     return;
 }
 if (s->have_sample) {
     uint32_t gap=p->received_at_ms-m->pressure.received_at_ms;
     if (gap>=0x80000000U) {
         if (s->active || s->start_pending) fault(m,FAULT_PRESSURE_SENSOR_FAULT,FAULT_DETAIL_PRESSURE_ORDER_LOST,now);
         return;
     }
     if (gap>0 && (s->diagnostic.delivered_interval_min_ms==0 || gap<s->diagnostic.delivered_interval_min_ms))
         s->diagnostic.delivered_interval_min_ms=gap;
     if (gap>s->diagnostic.delivered_interval_max_ms) s->diagnostic.delivered_interval_max_ms=gap;
     uint64_t lost=p->sequence-m->pressure.sequence-1;
     s->diagnostic.skipped_samples+=(uint32_t)(lost>UINT32_MAX ? UINT32_MAX : lost);
 }
 /* Check the old feedback deadline before a newly arrived sample can hide a gap. */
 Machine_CheckPressureSafety(m,now);
 m->pressure=*p; s->have_sample=true;
 /* Valid, ordered, fresh session samples, including samples skipped by the
  * control grid and the raw-abort sample. STOP/fault preserve this record. */
 if (s->active && Machine_IsPressureFresh(m,now) &&
     p->raw_pressure_counts>s->diagnostic.session_peak_raw) {
     s->diagnostic.session_peak_raw=p->raw_pressure_counts;
     s->diagnostic.session_peak_received_ms=p->received_at_ms;
 }
 float measured=0;
 if (p->raw_pressure_counts>=s->profile.raw_trip ||
     (s->profile.unit==1 && p->raw_pressure_counts*s->profile.scale+s->profile.offset>=s->profile.force_trip)) {
     fault(m,FAULT_OVERPRESSURE,FAULT_DETAIL_NONE,now); return;
 }
 if (!ForceServo_Measure(&s->profile,p->raw_pressure_counts,p->control_pressure_units,&measured)) {
     if (s->active || s->start_pending) fault(m,FAULT_PRESSURE_SENSOR_FAULT,FAULT_DETAIL_PRESSURE_INVALID,now);
     return;
 }
 if (s->active && Machine_IsPressureFresh(m,now) && measured>s->diagnostic.session_peak_measured)
     s->diagnostic.session_peak_measured=measured;
 if (s->start_pending) {
     /* Sequence must be new (checked above), received after this START, and
      * delivered within the unchanged 20 ms age budget. Stale frames stay OFF. */
     if (Machine_IsPressureFresh(m,now) && (int32_t)(p->received_at_ms-s->start_requested_ms)>=0) {
         if (m->state!=IDLE || !m->target_valid || m->target_pressure_units<=0 ||
             ForceServo_TargetAllowed(&s->profile,(float)m->target_pressure_units,s->profile.unit==1)!=FS_PROFILE_OK ||
             !ForceServo_ProfileConfigValid(&s->profile,&s->config) || !MotorExecutor_IsHealthy() ||
             !MotorExecutor_OutputIsDisabled() || MotorExecutor_GetSnapshot()->physical_output_locked)
             fault(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_MOTOR_REQUEST_REJECTED,now);
         else {
             s->diagnostic.rejection=ForceServo_PlanAllowed(&s->profile,&s->config,measured,
                 (float)m->target_pressure_units,&s->diagnostic.planned_reference_s);
             if (s->diagnostic.rejection!=FS_PROFILE_OK) fault(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_MOTOR_REQUEST_REJECTED,now);
             else m->last_command_result=begin_session(m,now);
         }
     }
     publish(m,now); return;
 }
 if (!s->active) { publish(m,now); return; }
 if (!Machine_IsPressureFresh(m,now)) {
     fault(m,FAULT_PRESSURE_SENSOR_FAULT,FAULT_DETAIL_PRESSURE_TIMEOUT,now); return;
 }
 if (s->contacted && s->diagnostic.contact_count && measured<s->profile.contact) {
     s->diagnostic.contact_lost_count++; s->diagnostic.contact_lost_raw=p->raw_pressure_counts;
     fault(m,FAULT_PRESSURE_SENSOR_FAULT,FAULT_DETAIL_CONTACT_LOST,now); return;
 }
 if (s->contacted && !s->diagnostic.contact_count && measured>=s->profile.contact) {
     s->diagnostic.contact_count=1; s->diagnostic.contact_raw=p->raw_pressure_counts;
 }
 if (s->contacted && p->received_at_ms-s->last_control_rx_ms<(uint32_t)s->config.control_min_ms)
     return; /* No integration, command update, or lease renewal. Latest sample only. */
 ForceServoDiagnostic *d=&s->diagnostic;
 d->raw=p->raw_pressure_counts; d->sample_hi=(uint32_t)(p->sequence>>32);
 d->control_pressure=(float)p->control_pressure_units;
 d->sample_lo=(uint32_t)p->sequence; d->received_ms=p->received_at_ms;
 if (!s->contacted) {
     if (measured>=s->profile.contact && !contact(m,now))
         fault(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_MOTOR_HARDWARE,now);
     publish(m,now); return;
 }
 ForceServoStep step;
 ForceServoConfig effective=s->config;
 float margin=(float)m->target_pressure_units-measured;
 if (d->boost_active && (margin<=s->profile.taper_margin || s->controller.reference-measured<=s->profile.taper_margin || m->state==FORCE_HOLD)) {
     if (!MotorExecutor_EndContinuousBoost(s->token)) { fault(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_MOTOR_HARDWARE,now); return; }
     d->boost_active=0;
 }
 if (!s->boost_used && !s->ever_held && d->contact_count && margin>s->profile.taper_margin &&
     s->controller.reference-measured>s->profile.taper_margin && ForceServo_BoostQualified(&s->profile) &&
     d->boost_spent_ms+(uint32_t)s->profile.boost_ms<=(uint32_t)s->profile.boost_total_ms &&
     now-s->session_started_ms+(uint32_t)s->profile.boost_ms<
        (uint32_t)fminf(s->config.session_ms,fminf(s->profile.session_ms,s->profile.energized_ms))) {
     /* Reserve full exposure up front. STOP/fault/reset cannot refund it. */
     s->boost_used=true; d->boost_spent_ms+=(uint32_t)s->profile.boost_ms;
     if (!MotorExecutor_ArmContinuousBoost(s->token,now,(uint32_t)s->profile.boost_ms)) {
         fault(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_MOTOR_HARDWARE,now); return;
     }
     d->boost_active=1; d->boost_deadline_ms=now+(uint32_t)s->profile.boost_ms;
 }
 if (d->boost_active) effective.press_cap=s->profile.peak_press;
 float dt=(float)(p->received_at_ms-s->last_control_rx_ms)*0.001f;
 if (!ForceServo_Prepare(&s->controller,&effective,measured,dt,&step)) {
     fault(m,FAULT_INTERNAL_FAULT,FAULT_DETAIL_NUMERIC,now); return;
 }
 int32_t committed; bool interlocked;
 int32_t requested=(int32_t)step.limited_output; /* truncate toward zero */
 if (MotorExecutor_UpdateContinuous(s->token,p->sequence,p->received_at_ms,now,
     (uint32_t)s->config.lease_ms,(uint32_t)s->config.sample_age_ms,
     (uint32_t)s->config.reverse_deadtime_ms,requested,&committed,&interlocked)!=MOTOR_RESULT_OK) {
     fault(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_MOTOR_HARDWARE,now); return;
 }
 if (interlocked) step.limits|=FS_LIMIT_INTERLOCK;
 if ((float)requested!=step.limited_output) step.limits|=FS_LIMIT_QUANTIZATION;
 if (!ForceServo_Commit(&s->controller,&effective,&step,committed)) {
     fault(m,FAULT_INTERNAL_FAULT,FAULT_DETAIL_NUMERIC,now); return;
 }
 s->last_control_rx_ms=p->received_at_ms; s->last_control_sequence=p->sequence;
 d->control_sequence++; d->control_at_ms=now;
 d->dt_s=dt; d->filtered=step.filtered; d->reference=step.reference;
 d->reference_rate=step.reference_rate; d->error=step.error;
 d->p=step.p; d->i=step.i; d->d=step.d; d->ff=step.ff;
 d->raw_output=step.raw_output; d->control_committed=(float)committed;
 d->requested_output=(float)requested;
 d->post_limit_output=step.limited_output;
 d->next_integral=step.next_integral; d->limits=step.limits;
 bool saturated=(step.limits&FS_LIMIT_AMPLITUDE)!=0;
 bool tracking=s->controller.elapsed_s>=s->controller.trajectory_s && fabsf(step.error)>s->config.tracking_error;
 if (saturated && !s->saturation_active) s->saturation_started_ms=now;
 if (tracking && !s->tracking_active) s->tracking_started_ms=now;
 s->saturation_active=saturated; s->tracking_active=tracking;
 d->saturated_ms=saturated ? now-s->saturation_started_ms : 0;
 d->tracking_ms=tracking ? now-s->tracking_started_ms : 0;
 /* Window anchors only advance after a noise-sized NET gain. Alternating
  * noise and sub-threshold creep cannot indefinitely buy another budget. */
 if (committed>0 && margin>s->config.hold_exit) {
     if (!s->progress_commanded) { s->progress_good_ms=now; s->progress_at_ms=now;
         s->progress_anchor=measured; s->progress_start=measured; }
     s->progress_commanded=true;
     if (now-s->progress_at_ms>=(uint32_t)s->profile.progress_window_ms) {
         d->progress_delta=measured-s->progress_anchor;
         if (d->progress_delta>=s->profile.progress_units) { s->progress_good_ms=now; s->progress_anchor=measured; }
         s->progress_at_ms=now;
     }
     d->no_response_ms=now-s->progress_good_ms;
     d->progress_status=d->no_response_ms<(uint32_t)s->profile.progress_window_ms &&
         measured-s->progress_start>=s->profile.progress_units ? (saturated ? 1U : 3U) : 2U;
 } else { s->progress_commanded=false; d->no_response_ms=0; d->progress_status=0; }
 d->reference_complete=s->controller.elapsed_s>=s->controller.trajectory_s;
 if (d->no_response_ms>=(uint32_t)fminf(s->config.saturation_ms,s->profile.no_response_ms))
     fault(m,FAULT_MOTION_TIMEOUT,FAULT_DETAIL_SATURATION_TIMEOUT,now);
 else if (d->reference_complete && d->tracking_ms>=(uint32_t)s->config.tracking_ms &&
          d->progress_status!=1 && d->progress_status!=3)
     fault(m,FAULT_MOTION_TIMEOUT,FAULT_DETAIL_TRACKING_TIMEOUT,now);
 else {
     float error=fabsf((float)m->target_pressure_units-step.filtered);
     if (s->controller.elapsed_s>=s->controller.trajectory_s && error<=s->config.hold_enter) {
         if (m->state!=FORCE_HOLD) s->hold_at_ms=now;
         m->state=FORCE_HOLD; s->ever_held=true; d->hold_ms=now-s->hold_at_ms;
     } else if (m->state!=FORCE_HOLD || error>s->config.hold_exit) { m->state=FORCE_BUILD; d->hold_ms=0; }
 }
 publish(m,now);
}
void Machine_HandleMotorService(MachineContext *m,uint32_t now)
{
 if (!m || !m->servo.active) return;
 const MotorExecutorSnapshot *e=MotorExecutor_GetSnapshot();
 if (m->servo.contacted) {
     if (e->last_completion==MOTOR_COMPLETION_LEASE)
         fault(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_LEASE_EXPIRED,now);
     else if (e->last_completion!=MOTOR_COMPLETION_NONE)
         fault(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_MOTOR_HARDWARE,now);
 } else if (!e->logical_active && m->motor_request_sequence!=0) {
     if (e->request_sequence!=m->motor_request_sequence || e->last_completion!=MOTOR_COMPLETION_NORMAL)
         fault(m,FAULT_MOTION_TIMEOUT,FAULT_DETAIL_APPROACH_TIMEOUT,now);
     else { m->motor_request_sequence=0; m->servo.approach_wait=true;
            m->servo.settle_after_ms=now+AUTO_TARGET_SETTLE_DELAY_MS; }
 }
 publish(m,now);
}
void Machine_Tick(MachineContext *m,uint32_t now)
{
 if (!m) return;
 Machine_CheckPressureSafety(m,now);
 ForceServoMachine *s=&m->servo;
 if (s->active && !s->contacted && !MotorExecutor_GetSnapshot()->logical_active &&
     (!s->approach_wait || ((int32_t)(now-s->settle_after_ms)>=0 &&
                           (int32_t)(m->pressure.received_at_ms-s->settle_after_ms)>=0))) {
     if (MotorExecutor_StartRun(MOTOR_DIRECTION_PRESS,m->config.first_approach_command_mv,
         m->config.first_approach_duration_ms,m->config.first_approach_backstop_ms,now)!=MOTOR_RESULT_OK)
         fault(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_MOTOR_REQUEST_REJECTED,now);
     else m->motor_request_sequence=MotorExecutor_GetSnapshot()->request_sequence;
 }
 publish(m,now);
}
