#ifndef APPLICATION_FORCE_BUILD_SOURCE_H
#define APPLICATION_FORCE_BUILD_SOURCE_H
#include "Application/force_build.h"
enum { BUILD_DECISION_WAIT=0, BUILD_DECISION_PULSE, BUILD_DECISION_BRAKE, BUILD_DECISION_APPROACH, BUILD_DECISION_OFF };
typedef struct {
 uint32_t action,mode,phase,normal_ms,settle_ms,precision_level,precision_extra_ms,precision_escape,precision_band;
 uint32_t boost_command,stall_count;
 int32_t command;
 float filtered;
 bool precision_latched,near_wait;
} ForceBuildDecision;
void ForceBuildSource_Reset(float target,uint32_t now);
ForceBuildDecision ForceBuildSource_Step(float measured,uint64_t sequence,uint32_t received,uint32_t now,
 bool pulse_active,uint32_t ended_ms);
#endif
