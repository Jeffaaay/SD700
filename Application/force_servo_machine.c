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
 uint32_t key=MotorAtomic_Enter();
 const MotorExecutorSnapshot *e=MotorExecutor_GetSnapshot();
 d->schema=FORCE_SERVO_SCHEMA; d->build_id=g_force_servo_contract[1];
 d->revision++; d->now_ms=now; d->age_ms=now-d->received_ms;
 d->session=s->session; d->config_version=s->config_version; d->config_digest=s->config_digest;
 d->state=m->state; d->fault=m->fault; d->detail=m->fault_detail;
 d->latest_sample_hi=(uint32_t)(m->pressure.sequence>>32);
 d->latest_sample_lo=(uint32_t)m->pressure.sequence;
 d->latest_received_ms=m->pressure.received_at_ms; d->latest_raw=m->pressure.raw_pressure_counts;
 d->target=(float)m->target_pressure_units; d->contact_threshold=FORCE_SERVO_CONTACT;
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
 m->servo.active=false; m->servo.controller.initialized=false; m->servo.controller.integral=0;
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
 m->servo.config_version=1;
 m->servo.config_digest=ForceServo_ConfigDigest(&m->servo.config);
#if SD700_FORCE_SERVO_COMMISSIONING
 m->target_pressure_units=FORCE_SERVO_TARGET250; m->target_valid=true;
#endif
 (void)MotorExecutor_Disable(); publish(m,now);
}
void Machine_CompleteBoot(MachineContext *m,bool checks,uint32_t now)
{
 if (!m || m->state!=BOOT_SAFE) return;
 if (!checks || !m->config_valid || !ForceServo_ConfigValid(&m->servo.config) ||
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
 s->diagnostic.contact_count=m->pressure.control_pressure_units>=(int32_t)FORCE_SERVO_CONTACT ? 1U : 0U;
 if (!ForceServo_Init(&s->controller,&s->config,(float)m->pressure.control_pressure_units,
                      (float)m->target_pressure_units) ||
     MotorExecutor_BeginContinuous(&s->token)!=MOTOR_RESULT_OK) return false;
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
 if (++s->session==0) ++s->session;
 memset(&s->diagnostic,0,sizeof(s->diagnostic));
 s->diagnostic.raw=m->pressure.raw_pressure_counts;
 s->diagnostic.control_pressure=(float)m->pressure.control_pressure_units;
 s->diagnostic.received_ms=m->pressure.received_at_ms;
 s->diagnostic.sample_hi=(uint32_t)(m->pressure.sequence>>32);
 s->diagnostic.sample_lo=(uint32_t)m->pressure.sequence;
 m->state=FORCE_APPROACH;
#if !SD700_FORCE_SERVO_COMMISSIONING
 if (m->pressure.control_pressure_units>=(int32_t)FORCE_SERVO_CONTACT)
#endif
 {
     /* Target250 always starts the continuous owner from the new measurement;
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
     s->active=false; s->controller.initialized=false; s->controller.integral=0;
     s->staging_open=false;
     if (MotorExecutor_Disable()!=MOTOR_RESULT_OK) {
         fault(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_MOTOR_HARDWARE,now); r=COMMAND_EXECUTOR_FAILED;
     } else { if (m->state!=FAULT) m->state=IDLE; r=COMMAND_ACCEPTED; }
 } else if (c->type==CMD_FAULT_RESET) {
     if (m->state==FAULT && MotorExecutor_OutputIsDisabled() && MotorExecutor_IsHealthy()) {
         m->state=IDLE; m->fault=FAULT_NONE; m->fault_detail=FAULT_DETAIL_NONE; r=COMMAND_ACCEPTED;
     } else r=COMMAND_NOT_ALLOWED;
 } else if (c->type==CMD_SET_TARGET) {
     if (c->target_pressure_units<=0 || c->target_pressure_units>(int32_t)FORCE_SERVO_MAX_TARGET) r=COMMAND_INVALID_VALUE;
     else if (m->state!=IDLE || s->start_pending || !MotorExecutor_OutputIsDisabled()) r=COMMAND_BUSY;
     else { m->target_pressure_units=c->target_pressure_units; m->target_valid=true; r=COMMAND_ACCEPTED; }
 } else if (c->type==CMD_FORCE_START) {
     if (m->state!=IDLE || s->active || s->start_pending) r=COMMAND_BUSY;
     else if (!m->target_valid ||
#if !SD700_FORCE_SERVO_COMMISSIONING
              !Machine_IsPressureFresh(m,now) ||
#endif
              !ForceServo_ConfigValid(&s->config) || !MotorExecutor_IsHealthy() ||
              !MotorExecutor_OutputIsDisabled() || MotorExecutor_GetSnapshot()->physical_output_locked)
         r=COMMAND_NOT_READY;
#if SD700_FORCE_SERVO_COMMISSIONING
     else {
         /* One operator request, no output/lease until the next fresh frame. */
         s->start_pending=true; s->start_requested_ms=now; s->staging_open=false;
         r=COMMAND_ACCEPTED;
     }
#else
     else if (m->pressure.raw_pressure_counts>=FORCE_SERVO_RAW_ABORT) {
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
 if (now-s->session_started_ms>=(uint32_t)s->config.session_ms)
     fault(m,FAULT_MOTION_TIMEOUT,FAULT_DETAIL_SESSION_TIMEOUT,now);
 else if (s->contacted && !s->ever_held && now-s->contact_at_ms>=FORCE_SERVO_BUILD_MS)
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
 if (p->raw_pressure_counts>=FORCE_SERVO_RAW_ABORT) {
     fault(m,FAULT_OVERPRESSURE,FAULT_DETAIL_NONE,now); return;
 }
 if (s->start_pending) {
     /* Sequence must be new (checked above), received after this START, and
      * delivered within the unchanged 20 ms age budget. Stale frames stay OFF. */
     if (Machine_IsPressureFresh(m,now) && (int32_t)(p->received_at_ms-s->start_requested_ms)>=0) {
         if (m->state!=IDLE || !m->target_valid || m->target_pressure_units<=0 ||
             m->target_pressure_units>(int32_t)FORCE_SERVO_MAX_TARGET ||
             !ForceServo_ConfigValid(&s->config) || !MotorExecutor_IsHealthy() ||
             !MotorExecutor_OutputIsDisabled() || MotorExecutor_GetSnapshot()->physical_output_locked)
             fault(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_MOTOR_REQUEST_REJECTED,now);
         else m->last_command_result=begin_session(m,now);
     }
     publish(m,now); return;
 }
 if (!s->active) { publish(m,now); return; }
 if (!Machine_IsPressureFresh(m,now)) {
     fault(m,FAULT_PRESSURE_SENSOR_FAULT,FAULT_DETAIL_PRESSURE_TIMEOUT,now); return;
 }
 if (s->contacted && s->diagnostic.contact_count && p->control_pressure_units<(int32_t)FORCE_SERVO_CONTACT) {
     s->diagnostic.contact_lost_count++; s->diagnostic.contact_lost_raw=p->raw_pressure_counts;
     fault(m,FAULT_PRESSURE_SENSOR_FAULT,FAULT_DETAIL_CONTACT_LOST,now); return;
 }
 if (s->contacted && !s->diagnostic.contact_count && p->control_pressure_units>=(int32_t)FORCE_SERVO_CONTACT) {
     s->diagnostic.contact_count=1; s->diagnostic.contact_raw=p->raw_pressure_counts;
 }
 if (s->contacted && p->received_at_ms-s->last_control_rx_ms<(uint32_t)s->config.control_min_ms)
     return; /* No integration, command update, or lease renewal. Latest sample only. */
 ForceServoDiagnostic *d=&s->diagnostic;
 d->raw=p->raw_pressure_counts; d->sample_hi=(uint32_t)(p->sequence>>32);
 d->control_pressure=(float)p->control_pressure_units;
 d->sample_lo=(uint32_t)p->sequence; d->received_ms=p->received_at_ms;
 if (!s->contacted) {
     if (p->control_pressure_units>=(int32_t)FORCE_SERVO_CONTACT && !contact(m,now))
         fault(m,FAULT_MOTOR_FAULT,FAULT_DETAIL_MOTOR_HARDWARE,now);
     publish(m,now); return;
 }
 ForceServoStep step;
 float dt=(float)(p->received_at_ms-s->last_control_rx_ms)*0.001f;
 if (!ForceServo_Prepare(&s->controller,&s->config,(float)p->control_pressure_units,dt,&step)) {
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
 if (!ForceServo_Commit(&s->controller,&s->config,&step,committed)) {
     fault(m,FAULT_INTERNAL_FAULT,FAULT_DETAIL_NUMERIC,now); return;
 }
 s->last_control_rx_ms=p->received_at_ms; s->last_control_sequence=p->sequence;
 d->control_sequence++; d->control_at_ms=now;
 d->dt_s=dt; d->filtered=step.filtered; d->reference=step.reference;
 d->reference_rate=step.reference_rate; d->error=step.error;
 d->p=step.p; d->i=step.i; d->d=step.d; d->ff=step.ff;
 d->raw_output=step.raw_output; d->control_committed=(float)committed;
 d->requested_output=(float)requested;
 d->next_integral=step.next_integral; d->limits=step.limits;
 bool saturated=(step.limits&FS_LIMIT_AMPLITUDE)!=0;
 bool tracking=fabsf(step.error)>s->config.tracking_error;
 if (saturated && !s->saturation_active) s->saturation_started_ms=now;
 if (tracking && !s->tracking_active) s->tracking_started_ms=now;
 s->saturation_active=saturated; s->tracking_active=tracking;
 d->saturated_ms=saturated ? now-s->saturation_started_ms : 0;
 d->tracking_ms=tracking ? now-s->tracking_started_ms : 0;
 if (d->saturated_ms>=(uint32_t)s->config.saturation_ms)
     fault(m,FAULT_MOTION_TIMEOUT,FAULT_DETAIL_SATURATION_TIMEOUT,now);
 else if (d->tracking_ms>=(uint32_t)s->config.tracking_ms)
     fault(m,FAULT_MOTION_TIMEOUT,FAULT_DETAIL_TRACKING_TIMEOUT,now);
 else {
     float error=fabsf((float)m->target_pressure_units-step.filtered);
     if (s->controller.elapsed_s>=s->controller.trajectory_s && error<=s->config.hold_enter) {
         m->state=FORCE_HOLD; s->ever_held=true;
     } else if (m->state!=FORCE_HOLD || error>s->config.hold_exit) m->state=FORCE_BUILD;
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
