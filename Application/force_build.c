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
bool ForceBuild_Select(float m,float t,bool contacted,uint32_t boost,ForceBuildRequest *r)
{
 if (!r || !isfinite(m) || !isfinite(t) || m<0 || t<1 || t>3000) return false;
 memset(r,0,sizeof(*r));
 float error=t-m; const ForceBuildConfig *c=&g_force_build_config;
 if (error<=0) { r->phase=BUILD_PHASE_TARGET_OFF; return true; }
 /* Low targets and taper always take priority over waiting for contact. */
 if (error<=c->taper_margin_N) {
     r->phase=BUILD_PHASE_TAPER;
     if (error<=c->fine_margin_N) {
         float ratio=fminf(1,fmaxf(0,(error-1)/(c->fine_margin_N-1)));
         r->command=c->fine_min_command+(uint32_t)(ratio*(c->fine_max_command-c->fine_min_command));
         r->hard_ms=c->fine_hard_ms;
     } else {
         float ratio=(error-c->fine_margin_N)/(c->taper_margin_N-c->fine_margin_N);
         r->command=c->micro_min_command+(uint32_t)(ratio*(c->micro_max_command-c->micro_min_command));
         r->hard_ms=c->pulse_hard_max_ms;
     }
 } else if (!contacted && m<c->contact_N) {
     r->phase=BUILD_PHASE_APPROACH; r->command=c->approach_command; r->hard_ms=c->approach_hard_ms;
 } else {
     r->phase=BUILD_PHASE_BUILD;
     if (boost>c->boost_max_command) boost=c->boost_max_command;
     r->command=c->micro_max_command+boost;
     r->hard_ms=c->pulse_base_ms*c->micro_max_command/r->command;
     if (r->hard_ms<c->pulse_hard_min_ms) r->hard_ms=c->pulse_hard_min_ms;
     if (r->hard_ms>c->pulse_hard_max_ms) r->hard_ms=c->pulse_hard_max_ms;
 }
 return true;
}
