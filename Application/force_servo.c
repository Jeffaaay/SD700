#include "Application/force_servo.h"
#include "Application/motion_build_policy.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

const ForceServoConfig g_force_servo_default_config = {
#define FS_DEFAULT(n,d,l,h) .n = d,
 FORCE_SERVO_PARAMETERS(FS_DEFAULT)
#undef FS_DEFAULT
};
const ForceServoProfile g_force_servo_profile = {
#define FS_PROFILE_DEFAULT(n,d) .n=d,
 FORCE_SERVO_PROFILE_FIELDS(FS_PROFILE_DEFAULT)
#undef FS_PROFILE_DEFAULT
};
uint32_t ForceServo_ProfileDigest(const ForceServoProfile *p)
{
 uint32_t h=2166136261U,u; unsigned j;
 if (!p) return 0;
#define FS_PROFILE_HASH(n,d) memcpy(&u,&p->n,4); for(j=0;j<4;j++) { h^=(u>>(j*8))&255U; h*=16777619U; }
 FORCE_SERVO_PROFILE_FIELDS(FS_PROFILE_HASH)
#undef FS_PROFILE_HASH
 return h;
}
/* Compiled, explicitly disabled candidates. No guessed exposure/cooling limits. */
const ForceServoProfile g_force_servo_candidates[FORCE_SERVO_CANDIDATE_COUNT]={
#define FS_CANDIDATE(cid,cmd) { .id=cid,.unit=0,.raw_trip=325,.scale=1, \
 .operating_max=275,.force_trip=325,.contact=20,.continuous_press=2400,.release=100, \
 .peak_press=cmd,.progress_window_ms=500,.progress_units=2,.no_response_ms=5000 },
 FORCE_SERVO_CANDIDATES(FS_CANDIDATE)
#undef FS_CANDIDATE
};
const ForceServoProfile *ForceServo_FindProfile(uint16_t id)
{
 if (id==(uint16_t)g_force_servo_profile.id) return &g_force_servo_profile;
 for (unsigned i=0;i<FORCE_SERVO_CANDIDATE_COUNT;i++)
     if (id==(uint16_t)g_force_servo_candidates[i].id) return &g_force_servo_candidates[i];
 return NULL;
}
const uint32_t g_force_characterization_contract[16]={
 SD700_FORCE_CHARACTERIZATION,1,3000,(SD700_BUILD_TO_TARGET ? 0 : 40),(SD700_BUILD_TO_TARGET ? 0 : 10),240,1,2,4,4,5000,FS_EXPERIMENT_BUDGET_MS,30,2,25,2
};
int32_t ForceServo_PercentCommand(float percent) { return (int32_t)floorf(percent*240.0f+0.5f); }
bool ForceServo_CharacterizationPlanValid(const ForceCharacterizationPlan *p)
{
 return p && (!SD700_BUILD_TO_TARGET || (p->assist_percent==0 && p->continuous_percent==0)) && isfinite(p->target_N) && p->target_N>=1 && p->target_N<=3000 &&
     floorf(p->target_N)==p->target_N && isfinite(p->assist_percent) &&
     p->assist_percent>=0 && p->assist_percent<=40 && isfinite(p->continuous_percent) &&
     p->continuous_percent>=0 && p->continuous_percent<=10;
}
uint32_t ForceServo_CharacterizationDigest(const ForceCharacterizationPlan *p)
{
 uint32_t h=2166136261U,u;
 const float values[]={p->target_N,p->assist_percent,p->continuous_percent};
 for (unsigned i=0;i<3;i++) { memcpy(&u,&values[i],4);
     for (unsigned j=0;j<4;j++) { h^=(u>>(j*8))&255U; h*=16777619U; } }
 return h;
}
bool ForceServo_ProfileValid(const ForceServoProfile *p)
{
 if (!p) return false;
#define FS_PROFILE_FINITE(n,d) if (!isfinite(p->n)) return false;
 FORCE_SERVO_PROFILE_FIELDS(FS_PROFILE_FINITE)
#undef FS_PROFILE_FINITE
#if SD700_FORCE_CHARACTERIZATION
 if (p->unit==2) {
     ForceServoProfile fixed=*p;
     fixed.peak_press=g_force_servo_profile.peak_press;
     fixed.continuous_press=g_force_servo_profile.continuous_press;
     return (!SD700_BUILD_TO_TARGET || (p->peak_press==0 && p->continuous_press==0)) &&
         memcmp(&fixed,&g_force_servo_profile,sizeof(fixed))==0 &&
         p->continuous_press>=0 && p->continuous_press<=2400 &&
         floorf(p->continuous_press)==p->continuous_press && p->peak_press>=0 &&
         p->peak_press<=9600 && floorf(p->peak_press)==p->peak_press;
 }
#endif
 if (p->id<1 || p->id>65535 || floorf(p->id)!=p->id ||
     (p->unit!=0 && p->unit!=1) || p->qualifications<0 || p->qualifications>15 ||
     floorf(p->qualifications)!=p->qualifications || p->raw_min<0 ||
     p->raw_trip>65535 || p->raw_min>=p->raw_trip ||
     floorf(p->raw_min)!=p->raw_min || floorf(p->raw_trip)!=p->raw_trip ||
     p->scale<=0 || p->operating_max<=0 || p->operating_max>=p->force_trip ||
     p->force_trip>FORCE_SERVO_REPRESENTABLE || p->contact<0 || p->contact>=p->operating_max ||
     p->continuous_press<1 || p->continuous_press>2400 ||
     p->release<1 || p->release>FS_RELEASE_PROFILE_CEILING ||
     p->peak_press<0 || p->peak_press>9600 || p->boost_ms<0 || p->boost_total_ms<0 || p->taper_margin<0)
     return false;
 if ((p->experiment_enabled!=0 && p->experiment_enabled!=1) ||
     p->limits_source<0 || p->limits_source>2 || floorf(p->limits_source)!=p->limits_source ||
     p->assist_rise_ms<0 || p->assist_end_ms<0 || p->off_ms<0 || p->off_ms>60000 ||
     p->response_units<0 || p->excessive_rise_units<0) return false;
 if (!p->experiment_enabled) {
     return p->limits_source==0 && p->unit==0 && p->scale==1 && p->offset==0 &&
         p->peak_press>p->continuous_press && p->energized_ms==0 && p->session_ms==0 &&
         p->capture_ms==0 && p->build_ms==0 && p->boost_ms==0 && p->boost_total_ms==0 &&
         p->assist_rise_ms==0 && p->assist_end_ms==0 && p->off_ms==0;
 }
 if (p->limits_source==0) return false;
 if (p->peak_press>0 && (p->boost_ms<4 || p->assist_rise_ms<1 ||
     p->assist_rise_ms>=p->assist_end_ms || p->assist_end_ms>p->boost_ms-2 ||
     p->off_ms<1 || p->taper_margin<=0 || p->response_units<=0 ||
     p->excessive_rise_units<=p->response_units)) return false;
 if (p->energized_ms<1 || p->energized_ms>60000 || p->session_ms<p->energized_ms ||
     p->session_ms>60000 || p->capture_ms<1 || p->capture_ms>p->energized_ms ||
     p->build_ms<1 || p->build_ms>p->energized_ms || p->progress_window_ms<1 ||
     p->progress_window_ms>p->no_response_ms || p->progress_units<=0 ||
     p->no_response_ms>p->session_ms || p->boost_ms>p->energized_ms ||
     p->boost_total_ms>60000 || p->boost_ms>p->boost_total_ms) return false;
 const float times[]={p->energized_ms,p->session_ms,p->capture_ms,p->build_ms,
     p->progress_window_ms,p->no_response_ms,p->boost_ms,p->boost_total_ms,
     p->assist_rise_ms,p->assist_end_ms,p->off_ms};
 for (unsigned i=0;i<sizeof(times)/sizeof(times[0]);i++) if (floorf(times[i])!=times[i]) return false;
 if (p->unit==0) return p->scale==1 && p->offset==0;
 /* Numeric margin must lie below BOTH the actuator/mechanical boundary and
  * calibration coverage. The missing qualification bits are reported separately. */
 return p->hardware_boundary>p->force_trip && p->hardware_boundary<=FORCE_SERVO_REPRESENTABLE &&
        p->calibration_min>=0 && p->calibration_min<p->operating_max &&
        p->calibration_max>=p->force_trip && p->calibration_max<=FORCE_SERVO_REPRESENTABLE &&
        p->raw_min*p->scale+p->offset<=p->calibration_min &&
        p->raw_trip*p->scale+p->offset>=p->force_trip;
}
ForceServoRejection ForceServo_TargetAllowed(const ForceServoProfile *p,float t,bool newtons)
{
 if (!p || !isfinite(p->qualifications) || p->qualifications<0 || p->qualifications>15) return FS_PROFILE_INVALID;
#if SD700_FORCE_CHARACTERIZATION
 if (p->unit==2) {
     if (!ForceServo_ProfileValid(p)) return FS_PROFILE_INVALID;
     if (!newtons) return FS_WRONG_TARGET_UNIT;
     return isfinite(t) && t>=1 && t<=3000 && floorf(t)==t ? FS_PROFILE_OK : FS_TARGET_OUTSIDE_OPERATING_RANGE;
 }
#endif
 /* Asking for N on the legacy profile reports the actual missing prerequisite. */
 if (newtons || p->unit==1) {
     unsigned q=(unsigned)p->qualifications;
     if (!(q&1)) return FS_MISSING_SENSOR_RANGE;
     if (!(q&2)) return FS_MISSING_CALIBRATION;
     if (!(q&4)) return FS_MISSING_MECHANICAL_LIMIT;
     if (!(q&8)) return FS_MISSING_CURRENT_TIME_LIMIT;
 }
 if (!ForceServo_ProfileValid(p)) return FS_PROFILE_INVALID;
 if (newtons!=(p->unit==1)) return FS_WRONG_TARGET_UNIT;
 if (!isfinite(t) || t<=0 || t>p->operating_max) return FS_TARGET_OUTSIDE_OPERATING_RANGE;
 if (p->unit==1 && (t<p->calibration_min || t>p->calibration_max)) return FS_TARGET_OUTSIDE_CALIBRATION;
 return FS_PROFILE_OK;
}
bool ForceServo_Measure(const ForceServoProfile *p,uint32_t raw,int32_t control,float *m)
{
#if SD700_FORCE_CHARACTERIZATION
 if (p && p->unit==2) {
     if (!m || !ForceServo_ProfileValid(p) || raw>3000 || control!=(int32_t)raw) return false;
     *m=(float)raw; return true; /* Boundary3000 is consumed by machine OFF path. */
 }
#endif
 if (!m || !ForceServo_ProfileValid(p) || raw<p->raw_min || raw>=p->raw_trip) return false;
 *m=p->unit==1 ? raw*p->scale+p->offset : (float)control;
 return isfinite(*m) && *m>=0 && *m<p->force_trip &&
     (p->unit!=1 || (*m>=p->calibration_min && *m<=p->calibration_max));
}
bool ForceServo_BoostQualified(const ForceServoProfile *p)
{
#if SD700_FORCE_CHARACTERIZATION
 if (p && p->unit==2) return ForceServo_ProfileValid(p) && p->peak_press>0;
#endif
 return ForceServo_ProfileValid(p) && p->experiment_enabled && (p->unit==0 || p->qualifications==15) &&
     p->peak_press>p->continuous_press && p->peak_press<=FS_PRESS_PROFILE_CEILING &&
     p->boost_ms>0 && p->boost_total_ms>=p->boost_ms && p->taper_margin>0;
}
float ForceServo_TrajectorySeconds(const ForceServoConfig *c,float m,float t)
{
 float distance=fabsf(t-m);
 return fmaxf(1.5f*distance/c->reference_rate,sqrtf(6.0f*distance/c->reference_acceleration));
}
bool ForceServo_ProfileConfigValid(const ForceServoProfile *p,const ForceServoConfig *c)
{
#if SD700_FORCE_CHARACTERIZATION
 if (p && c && p->unit==2) {
     ForceServoConfig fixed=*c; fixed.press_cap=g_force_servo_default_config.press_cap;
     return ForceServo_ProfileValid(p) && ForceServo_ConfigValid(c) &&
         memcmp(&fixed,&g_force_servo_default_config,sizeof(fixed))==0 && c->press_cap==p->continuous_press;
 }
#endif
 return ForceServo_ProfileValid(p) && ForceServo_ConfigValid(c) &&
     c->press_cap<=p->continuous_press && c->release_cap<=p->release;
}
ForceServoRejection ForceServo_PlanAllowed(const ForceServoProfile *p,const ForceServoConfig *c,
    float m,float t,float *seconds)
{
 if (p && !p->experiment_enabled) return FS_EXPERIMENT_LIMITS_UNREVIEWED;
 if (!seconds || !ForceServo_ProfileConfigValid(p,c) || !isfinite(m) || m<0 ||
     (m>=p->force_trip && !(SD700_BUILD_TO_TARGET && m==3000 && t==3000)))
     return FS_PROFILE_INVALID;
 ForceServoRejection r=ForceServo_TargetAllowed(p,t,p->unit!=0);
 if (r!=FS_PROFILE_OK) return r;
 *seconds=ForceServo_TrajectorySeconds(c,m,t);
#if SD700_FORCE_CHARACTERIZATION
 if (p->unit==2) return FS_PROFILE_OK; /* Operator-ended run; target attainment is not guaranteed. */
#endif
 float budget=fminf(p->energized_ms,fminf(p->session_ms,c->session_ms));
 if (*seconds*1000+c->hold_dwell_ms>budget || *seconds*1000>p->build_ms)
     return FS_TRAJECTORY_EXCEEDS_BUDGET;
 return FS_PROFILE_OK;
}
const uint32_t g_force_servo_contract[20] = {
 FORCE_SERVO_SCHEMA, FORCE_SERVO_BUILD_ID, SD700_FORCE_SERVO_COMMISSIONING, FS_LEGACY_OPERATING_MAX,
 FS_LEGACY_RAW_TRIP, FS_EXPERIMENT_BUDGET_MS,
 FS_PRESS_PROFILE_CEILING, FS_RELEASE_PROFILE_CEILING,
 FORCE_SERVO_DEFAULT_TARGET, FORCE_SERVO_START_WAIT_MS, FS_FEEDBACK_GAP, FS_LEASE,
 FS_PRESS_OPERATING_CAP, FS_RELEASE_OPERATING_CAP,
 SD700_FORCE_SERVO_COMMISSIONING, /* immediate magnitude reduction; ramp increases */
 FS_POWERED_TEST_READY, FS_EXECUTOR_PRESS_CEILING, FS_CONTINUOUS_CEILING,
 FS_APPROVED_PEAK_MS, FORCE_SERVO_CANDIDATE_COUNT
};
static float clamp(float x, float lo, float hi) { return fminf(hi,fmaxf(lo,x)); }
bool ForceServo_SequenceAfter(uint64_t a, uint64_t b)
{ uint64_t d=a-b; return d!=0 && d<(UINT64_C(1)<<63); }
bool ForceServo_ConfigValid(const ForceServoConfig *c)
{
 if (!c) return false;
#define FS_VALID(n,d,l,h) if (!isfinite(c->n) || c->n<(l) || c->n>(h)) return false;
 FORCE_SERVO_PARAMETERS(FS_VALID)
#undef FS_VALID
 return c->integral_min<c->integral_max && c->hold_enter<c->hold_exit &&
 c->control_min_ms<c->feedback_gap_ms && c->feedback_gap_ms<c->lease_ms &&
 c->sample_age_ms<c->lease_ms && c->tracking_gain*c->feedback_gap_ms*0.001f<=1.0f &&
 ((SD700_FORCE_CHARACTERIZATION && c->session_ms==0) ||
  (c->saturation_ms<=c->session_ms && c->tracking_ms<=c->session_ms)) &&
 floorf(c->control_min_ms)==c->control_min_ms && floorf(c->feedback_gap_ms)==c->feedback_gap_ms &&
 floorf(c->sample_age_ms)==c->sample_age_ms && floorf(c->lease_ms)==c->lease_ms &&
 floorf(c->session_ms)==c->session_ms && floorf(c->saturation_ms)==c->saturation_ms &&
 floorf(c->hold_dwell_ms)==c->hold_dwell_ms && floorf(c->tracking_ms)==c->tracking_ms && floorf(c->reverse_deadtime_ms)==c->reverse_deadtime_ms;
}
uint32_t ForceServo_ConfigDigest(const ForceServoConfig *c)
{
 /* Canonical IEEE754 little-endian field bytes, independent of struct padding. */
 uint32_t h=2166136261U, u; unsigned j;
#define FS_HASH(n,d,l,hi) memcpy(&u,&c->n,4); for(j=0;j<4;j++) { h^=(u>>(j*8))&255U; h*=16777619U; }
 FORCE_SERVO_PARAMETERS(FS_HASH)
#undef FS_HASH
 return h;
}
bool ForceServo_Init(ForceServo *s,const ForceServoConfig *c,float m,float target)
{
 if (!s || !ForceServo_ConfigValid(c) || !isfinite(m) || m<0 || m>FORCE_SERVO_REPRESENTABLE ||
     !isfinite(target) || target<=0 || target>FORCE_SERVO_REPRESENTABLE) return false;
 memset(s,0,sizeof(*s)); s->start=m; s->filtered=m; s->reference=m; s->target=target;
 /* Cubic smoothstep: max velocity=1.5*distance/T, max acceleration=6*distance/T^2. */
 s->trajectory_s=ForceServo_TrajectorySeconds(c,m,target);
 s->initialized=true; return true;
}
bool ForceServo_Prepare(ForceServo *s,const ForceServoConfig *c,float m,float dt,ForceServoStep *o)
{
 if (!s || !o || !s->initialized || !ForceServo_ConfigValid(c) || !isfinite(m) ||
     m<0 || m>FORCE_SERVO_REPRESENTABLE || !isfinite(dt) || dt<c->control_min_ms*0.001f ||
     dt>c->feedback_gap_ms*0.001f) return false;
 if (!isfinite(s->reference) || !isfinite(s->filtered) || !isfinite(s->derivative) ||
     !isfinite(s->integral) || !isfinite(s->elapsed_s) || !isfinite(s->trajectory_s) ||
     !isfinite(s->previous_committed) || !isfinite(s->start) || !isfinite(s->target)) return false;
 memset(o,0,sizeof(*o)); o->dt_s=dt;
 s->elapsed_s=fminf(s->elapsed_s+dt,s->trajectory_s);
 float x=s->trajectory_s>0 ? s->elapsed_s/s->trajectory_s : 1;
 s->reference=s->start+(s->target-s->start)*x*x*(3-2*x);
 s->reference_rate=s->trajectory_s>0 ?
     (s->target-s->start)*6*x*(1-x)/s->trajectory_s : 0;
 float previous=s->filtered;
 s->filtered+=dt/(c->measurement_filter_s+dt)*(m-s->filtered);
 s->derivative+=dt/(c->d_filter_s+dt)*((s->filtered-previous)/dt-s->derivative);
 o->reference=s->reference; o->reference_rate=s->reference_rate; o->filtered=s->filtered;
 o->error=s->reference-s->filtered; o->p=c->kp*o->error;
 o->i=c->ki==0 ? 0 : s->integral; o->d=c->kd==0 ? 0 : -c->kd*s->derivative;
 o->ff=0; /* Protected zero: no identified feedforward, no inert writable FF knob. */
 o->raw_output=o->p+o->i+o->d+o->ff;
 float amplitude=clamp(o->raw_output,-c->release_cap,c->press_cap);
 if (amplitude!=o->raw_output) o->limits|=FS_LIMIT_AMPLITUDE;
#if SD700_FORCE_SERVO_COMMISSIONING
 /* Ramp only increasing magnitude. A smaller demand must not retain PRESS
  * (or RELEASE). Opposite demand ramps from zero; executor still enforces OFF
  * and deadtime before changing direction. Safety stops bypass this function. */
 float origin=amplitude*s->previous_committed>0 ? s->previous_committed : 0;
 o->limited_output=amplitude>=0 ? fminf(amplitude,origin+c->output_rate*dt) :
                                fmaxf(amplitude,origin-c->output_rate*dt);
#else
 o->limited_output=clamp(amplitude,s->previous_committed-c->output_rate*dt,
                                   s->previous_committed+c->output_rate*dt);
#endif
 if (o->limited_output!=amplitude) o->limits|=FS_LIMIT_RATE;
 return isfinite(o->raw_output) && isfinite(o->limited_output) && isfinite(s->derivative);
}
bool ForceServo_Commit(ForceServo *s,const ForceServoConfig *c,ForceServoStep *o,int32_t committed)
{
 if (!s || !o || !ForceServo_ConfigValid(c) || !isfinite(o->raw_output) ||
     !isfinite(o->error) || !isfinite(o->dt_s) || o->dt_s<=0 ||
     o->dt_s>c->feedback_gap_ms*0.001f || committed>c->press_cap || committed<-c->release_cap) return false;
 /* Explicit causal order: compute u[k] with I[k], executor limits/commits u,
  * then compute I[k+1]. Failed commits never call this function. */
 float tracking=(float)committed-o->raw_output;
#if SD700_FORCE_SERVO_COMMISSIONING
 /* Integer command quantization alone must not cancel a small persistent I
  * increment. Keep actual-output tracking for saturation, slew and interlock. */
 if (!(o->limits&(FS_LIMIT_AMPLITUDE|FS_LIMIT_RATE|FS_LIMIT_INTERLOCK)) &&
     (float)committed==truncf(o->limited_output)) tracking=0;
#endif
 float next=c->ki==0 ? 0 : s->integral+o->dt_s*
     (c->ki*o->error+c->tracking_gain*tracking);
 if (!isfinite(next)) return false;
 s->integral=clamp(next,c->integral_min,c->integral_max);
 o->next_integral=s->integral; s->previous_committed=(float)committed; return true;
}
