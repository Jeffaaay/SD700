#include "Application/force_build.h"
#include <math.h>
#include <string.h>
const ForceBuildConfig g_force_build_config={
#define BUILD_VALUE(n,v) .n=v,
 FORCE_BUILD_FIELDS(BUILD_VALUE)
#undef BUILD_VALUE
};
uint32_t ForceBuild_ConfigDigest(void)
{
 uint32_t h=2166136261U;
#define BUILD_HASH(n,v) for (unsigned i=0;i<4;i++) { h^=(g_force_build_config.n>>(8*i))&255U; h*=16777619U; }
 FORCE_BUILD_FIELDS(BUILD_HASH)
#undef BUILD_HASH
 return h;
}
/* pressure_control.c:102-129. Movement, including a decrease, resets the
 * actuator compensation. Progress safety separately requires net NEW HIGHS. */
void ForceBuild_PulseBoost(ForceBuildState *b,float movement,uint32_t maximum)
{
 const ForceBuildConfig *c=&g_force_build_config;
 if (fabsf(movement)<c->low_response_N) {
     if (++b->low_count>=c->low_response_count) {
         b->low_count=0; b->boost+=c->boost_step_command;
     }
 } else { b->low_count=0; b->boost=0; }
 /* Enforce the selected FINE ceiling even when inherited from MICRO. */
 if (b->boost>maximum) b->boost=maximum;
}
/* pressure_control.c:131-147. Caller admits only a fresh OFF/post-pulse frame;
 * elapsed time alone never authorizes an output or reuses feedback. */
void ForceBuild_CoarseBoost(ForceBuildState *b,float measured,uint32_t now)
{
 const ForceBuildConfig *c=&g_force_build_config;
 if (now-b->coarse_check_ms<c->coarse_check_ms) return;
 b->coarse_check_ms=now;
 if (fabsf(measured-b->coarse_reference)<c->low_response_N) {
     b->coarse_boost+=c->coarse_step_command;
     if (b->coarse_boost>c->coarse_max_command) b->coarse_boost=c->coarse_max_command;
 } else b->coarse_boost=0;
 b->coarse_reference=measured;
}
bool ForceBuild_Select(float m,float t,bool contacted,uint32_t boost,uint32_t coarse,ForceBuildRequest *r)
{
 if (!r || !isfinite(m) || !isfinite(t) || m<0 || t<1 || t>3000) return false;
 memset(r,0,sizeof(*r));
 float error=t-m; const ForceBuildConfig *c=&g_force_build_config;
 if (error<=0) { r->phase=BUILD_PHASE_TARGET_OFF; return true; }
 /* Old COARSE selection: first approach, error>3, before contact. */
 if (!contacted && m<c->contact_N && error>c->fine_margin_N) {
     if (coarse>c->coarse_max_command) coarse=c->coarse_max_command;
     r->phase=BUILD_PHASE_APPROACH; r->mode=BUILD_MODE_COARSE;
     r->base_command=c->approach_command; r->command=r->base_command+coarse;
     r->hard_ms=c->approach_hard_ms; return true;
 }
 r->phase=error<=c->taper_margin_N ? BUILD_PHASE_TAPER : BUILD_PHASE_BUILD;
 if (error<=c->fine_margin_N) {
     r->mode=BUILD_MODE_FINE;
     float ratio=fminf(1,fmaxf(0,(error-1)/(c->fine_margin_N-1)));
     r->base_command=c->fine_min_command+(uint32_t)(ratio*(c->fine_max_command-c->fine_min_command));
     if (boost>c->fine_boost_max_command) boost=c->fine_boost_max_command;
 } else {
     r->mode=BUILD_MODE_MICRO;
     float ratio=fminf(1,fmaxf(0,(error-c->fine_margin_N)/(c->taper_margin_N-c->fine_margin_N)));
     r->base_command=c->micro_min_command+(uint32_t)(ratio*(c->micro_max_command-c->micro_min_command));
     if (boost>c->boost_max_command) boost=c->boost_max_command;
 }
 r->command=r->base_command+boost;
 /* Old volt-second compensation (floor, minimum2 ms), now actual normal
  * timer duration; independent hard cutoff is one additional ms. */
 uint32_t normal=c->pulse_base_ms*r->base_command/r->command;
 if (normal<c->pulse_min_ms) normal=c->pulse_min_ms;
 r->hard_ms=normal+c->hard_guard_ms;
 return true;
}
