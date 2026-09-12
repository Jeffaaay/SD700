#ifndef APPLICATION_AUTO_TARGET_CONFIG_H
#define APPLICATION_AUTO_TARGET_CONFIG_H

#include "Application/machine.h"

/* INITIAL VALUES FOR CONTROLLED BENCH VALIDATION, NOT TUNED/PRODUCTION APPROVED.
 * Sensor control units are the existing raw-count identity conversion, NOT N.
 * Abort threshold is an experiment limit, NOT a known mechanical safe limit. */
#define AUTO_TARGET_MAX_TARGET_UNITS             275
#define AUTO_TARGET_CONTACT_UNITS                 20
#define AUTO_TARGET_HOLD_ENTER_UNITS                5
#define AUTO_TARGET_HOLD_EXIT_UNITS                10
#define AUTO_TARGET_RAW_ABORT_COUNTS             325U
#define AUTO_TARGET_KP_MV_PER_UNIT                8.0f
#define AUTO_TARGET_PI_PRESS_MAX_MV             1200U /* P preview, not mapped PRESS */
#define AUTO_TARGET_PRESS_MAX_MV                5000U /* mapped request ceiling */
#define AUTO_TARGET_RELEASE_MAX_MV               800U
/* PressBoost1: bench candidates in CONTROL UNITS, never legacy calibrated N.
 * Fine: enter tolerance < error <= 2 * exit tolerance, 400..1000 mV.
 * Micro: larger error, 1000..3000 mV over the next 100 units, then capped.
 * 1000 (within the legacy 800..3000 range) keeps the band boundary continuous.
 * Near: error <= exit tolerance has NO boost. Duration never adapts. */
#define AUTO_TARGET_PRESS_FINE_MIN_MV             400U
#define AUTO_TARGET_PRESS_FINE_MAX_MV            1000U
#define AUTO_TARGET_PRESS_MICRO_MIN_MV           1000U
#define AUTO_TARGET_PRESS_MICRO_MAX_MV           3000U
#define AUTO_TARGET_PRESS_MICRO_RAMP_UNITS         100U
#define AUTO_TARGET_PRESS_BOOST_STEP_MV           300U
#define AUTO_TARGET_PRESS_FINE_BOOST_MAX_MV      1000U
#define AUTO_TARGET_PRESS_MICRO_BOOST_MAX_MV     2000U
#define AUTO_TARGET_PRESS_LOW_RESPONSE_PAIRS         2U
#define AUTO_TARGET_PRESS_EFFECTIVE_RISE_UNITS       2
/* Coarse approach only: the field-reported motion reference is 10000 mV /
 * 100 ms. Retain that amplitude but use 1/5 (first) or 1/10 (recontact) of
 * its duration, then OFF + settled new feedback. Short profiles NOT VERIFIED
 * on the mechanism; do not escalate automatically on zero response. */
#define AUTO_TARGET_FIRST_COMMAND_MV           10000U
#define AUTO_TARGET_FIRST_DURATION_MS             20U
#define AUTO_TARGET_FIRST_BACKSTOP_MS             50U
#define AUTO_TARGET_RECONTACT_COMMAND_MV       10000U
#define AUTO_TARGET_RECONTACT_DURATION_MS         10U
#define AUTO_TARGET_RECONTACT_BACKSTOP_MS         40U
#define AUTO_TARGET_PULSE_DURATION_MS             10U
#define AUTO_TARGET_PULSE_BACKSTOP_MS             40U
#define AUTO_TARGET_SETTLE_DELAY_MS               50U
#define AUTO_TARGET_FEEDBACK_TIMEOUT_MS          250U
#define AUTO_TARGET_PRESSURE_FRESHNESS_MS        200U
/* ApproachMeasure1: initial contact search has no total motion timeout. */
#define AUTO_TARGET_CONVERGENCE_TIMEOUT_MS     30000U

extern const MachineConfig g_sd700_auto_target_machine_config;
extern const ForcePiConfig g_sd700_auto_target_force_pi_config;

/* One last-pulse RAM snapshot for a debugger AFTER verified STOP; no protocol
 * extension, event queue, UART logging or feedback into the controller.
 * Count resets only on a new admitted AUTO_START, survives STOP/fault.
 * Output fields are executor readbacks, never proof of mechanical movement. */
typedef enum
{
    AUTO_APPROACH_END_NONE = 0,
    AUTO_APPROACH_END_RUNNING,
    AUTO_APPROACH_END_NORMAL,
    AUTO_APPROACH_END_CONTACT,
    AUTO_APPROACH_END_STOP,
    AUTO_APPROACH_END_BACKSTOP,
    AUTO_APPROACH_END_EXECUTOR_ERROR,
    AUTO_APPROACH_END_SAFETY_FAULT
} AutoApproachEndReason;

typedef struct
{
    uint32_t request_count;
    uint32_t normal_completion_count;
    uint32_t request_sequence;
    uint32_t requested_mv;
    uint32_t base_mv;
    uint32_t boost_mv;
    uint32_t duration_ms;
    uint32_t started_at_ms;
    /* Last completed PRESS retained while the next request is running. */
    uint32_t completed_request_sequence;
    uint32_t completed_mv;
    uint32_t completed_at_ms; /* Machine service time, not exact ISR stop tick */
    AutoApproachEndReason end_reason; /* same NONE/NORMAL/STOP/BACKSTOP/... codes */
    int32_t before_units;
    int32_t observed_peak_units;
    int32_t after_units;
    uint64_t before_sequence;
    uint64_t after_sequence;
    uint32_t after_received_at_ms;
    bool feedback_valid;
    bool observed_off_fall; /* sampled rise then lower settled value, not true peak */
    bool output_disabled_at_end;
} AutoPressDiagnostics;

typedef struct
{
    uint32_t pulse_count;
    uint32_t request_sequence;
    uint32_t requested_mv;
    uint32_t requested_duration_ms;
    uint32_t started_at_ms;
    uint32_t ended_at_ms;
    AutoApproachEndReason end_reason;
    MachineState state_before;
    MachineState state_after;
    int32_t pressure_units;
    uint32_t sample_received_at_ms;
    uint64_t sample_sequence;
    bool pressure_fresh;
    bool output_disabled_at_start;
    bool output_disabled_at_end;
    bool logical_active_at_end;
    /* Device-side boundary evidence, retained through STOP/fault. A nonzero
     * contact sequence identifies a latched contact; zero ticks are valid. */
    uint32_t contact_cycle_started_ms;
    uint64_t contact_sample_sequence;
    uint32_t contact_received_at_ms;
    uint32_t contact_decided_at_ms;
    uint32_t approach_timeout_decided_at_ms;
    uint32_t timeout_cycle_started_ms;
    bool approach_timeout_committed;
    /* ApproachMeasure1: first START -> contact measurement, frozen through
     * STOP/fault; no contact means elapsed time/pulses are a search lower bound.
     * Times are uint32 MCU ms (wrap-safe elapsed, range < one full tick wrap).
     * pulse_count above remains ALL coarse requests, including later recontact. */
    uint32_t search_started_at_ms;
    uint32_t search_elapsed_ms;
    bool search_active;
    bool first_contact_latched;
    uint32_t first_contact_pulse_count;
    int32_t first_contact_pressure_units;
    uint32_t first_contact_received_at_ms;
    uint32_t first_contact_decided_at_ms;
    uint64_t first_contact_sample_sequence;
    AutoPressDiagnostics press;
} AutoApproachDiagnostics;

extern volatile AutoApproachDiagnostics g_sd700_approach_diagnostics;

#endif
