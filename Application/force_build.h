#ifndef APPLICATION_FORCE_BUILD_H
#define APPLICATION_FORCE_BUILD_H
#include <stdint.h>
#include <stdbool.h>
#include "Application/motion_build_policy.h"
/* Field182402 working-tree source port: Docs/BuildToTarget3_SourcePort1/HANDOFF.md.
 * Command uses the existing24 V mapping. These are NOT measured volts/current,
 * motor ratings or a certified cooling model. Only Target is runtime writable. */
#define FORCE_BUILD_FIELDS(X) \
 X(version,5) X(approach_command,5000) X(contact_N,10) \
 X(approach_hard_ms,8000) X(approach_total_ms,8000) \
 X(micro_min_command,800) X(micro_max_command,3000) \
 X(boost_step_command,500) X(boost_max_command,9000) X(build_ceiling,12000) \
 X(pulse_base_ms,9) X(pulse_hard_max_ms,15) X(pulse_hard_min_ms,3) \
 X(taper_margin_N,50) X(fine_margin_N,3) X(fine_min_command,2000) X(fine_max_command,12000) X(fine_boost_max_command,0) \
 X(off_settle_ms,30) X(total_on_ms,0) X(full_rest_ms,108000) \
 X(no_response_ms,45000) X(progress_N,2) X(low_response_N,1) X(low_response_count,2) X(excessive_rise_N,25) \
 X(pulse_min_ms,2) X(hard_guard_ms,1) X(coarse_check_ms,200) X(coarse_step_command,0) \
 X(coarse_max_command,0) X(approach_ceiling,5000) X(approach_command_ms_budget,40000000) \
 X(preload_initial_command,0) X(preload_min_command,0) X(preload_max_command,0) \
 X(preload_step_command,0) X(preload_drop_N,0) \
 X(far_press_ceiling,10000) X(far_normal_max_ms,12) X(precision_250_ceiling,7000) X(precision_250_max_ms,8) \
 X(final_settle_ms,400) X(brake_enabled,1) X(source_control_ms,10)
typedef struct {
#define BUILD_FIELD(n,v) uint32_t n;
 FORCE_BUILD_FIELDS(BUILD_FIELD)
#undef BUILD_FIELD
} ForceBuildConfig;
#define FORCE_BUILD_CONFIG_WORDS (sizeof(ForceBuildConfig)/2U)
extern const ForceBuildConfig g_force_build_config;
uint32_t ForceBuild_ConfigDigest(void);
enum { BUILD_PHASE_IDLE=0, BUILD_PHASE_APPROACH=1, BUILD_PHASE_BUILD=2,
 BUILD_PHASE_TAPER=3, BUILD_PHASE_TARGET_OFF=4 };
enum { BUILD_END_NONE=0, BUILD_END_NORMAL=1, BUILD_END_REDUCED=2,
 BUILD_END_STOP=3, BUILD_END_DEADLINE=4, BUILD_END_HARDWARE=5 };
enum { BUILD_MODE_COARSE=1, BUILD_MODE_MICRO=2, BUILD_MODE_FINE=3 };
typedef struct { uint32_t phase,command,hard_ms,base_command,mode,preload_command; uint32_t normal_ms,settle_ms,target,precision; } ForceBuildRequest;
/* Persistent until a full uninterrupted OFF interval qualifies a new epoch.
 * STOP/START and pulse OFF never clear cumulative progress/exposure budgets.
 * Source adaptation resets only on explicit START; BRAKE has no forward preload. */
typedef struct {
 uint32_t epoch, phase, no_response_ms, accounted_ms, boost, low_count;
 uint32_t coarse_boost, coarse_check_ms, preload_command;
 uint64_t post_sequence;
 float progress_anchor, pulse_before, coarse_reference;
 bool progress_initialized, post_pending, contact_latched, monitoring;
} ForceBuildState;
#endif
