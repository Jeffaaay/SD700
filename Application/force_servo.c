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
const uint32_t g_force_servo_contract[8] = {
 FORCE_SERVO_SCHEMA, FORCE_SERVO_BUILD_ID, SD700_FORCE_SERVO_COMMISSIONING, FORCE_SERVO_MAX_TARGET,
 FORCE_SERVO_RAW_ABORT, FORCE_SERVO_BUILD_MS,
 FORCE_SERVO_COMMISSIONING_OUTPUT_MV, FORCE_SERVO_COMMISSIONING_OUTPUT_MV
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
 c->saturation_ms<=c->session_ms && c->tracking_ms<=c->session_ms &&
 floorf(c->control_min_ms)==c->control_min_ms && floorf(c->feedback_gap_ms)==c->feedback_gap_ms &&
 floorf(c->sample_age_ms)==c->sample_age_ms && floorf(c->lease_ms)==c->lease_ms &&
 floorf(c->session_ms)==c->session_ms && floorf(c->saturation_ms)==c->saturation_ms &&
 floorf(c->tracking_ms)==c->tracking_ms && floorf(c->reverse_deadtime_ms)==c->reverse_deadtime_ms;
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
 if (!s || !ForceServo_ConfigValid(c) || !isfinite(m) || m<0 || m>325 ||
     !isfinite(target) || target<=0 || target>275) return false;
 memset(s,0,sizeof(*s)); s->start=m; s->filtered=m; s->reference=m; s->target=target;
 /* Cubic smoothstep: max velocity=1.5*distance/T, max acceleration=6*distance/T^2. */
 float distance=fabsf(target-m);
 s->trajectory_s=fmaxf(1.5f*distance/c->reference_rate,
                       sqrtf(6.0f*distance/c->reference_acceleration));
 s->initialized=true; return true;
}
bool ForceServo_Prepare(ForceServo *s,const ForceServoConfig *c,float m,float dt,ForceServoStep *o)
{
 if (!s || !o || !s->initialized || !ForceServo_ConfigValid(c) || !isfinite(m) ||
     m<0 || m>325 || !isfinite(dt) || dt<c->control_min_ms*0.001f ||
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
 o->limited_output=clamp(amplitude,s->previous_committed-c->output_rate*dt,
                                   s->previous_committed+c->output_rate*dt);
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
 float next=c->ki==0 ? 0 : s->integral+o->dt_s*
     (c->ki*o->error+c->tracking_gain*((float)committed-o->raw_output));
 if (!isfinite(next)) return false;
 s->integral=clamp(next,c->integral_min,c->integral_max);
 o->next_integral=s->integral; s->previous_committed=(float)committed; return true;
}
