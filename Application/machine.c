#include "Application/machine.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#include "Application/direct_pulse_config.h"
#include "Application/motion_build_policy.h"
#if SD700_AUTO_TARGET_ENABLED
#include "Application/auto_target_config.h"
#if defined(STM32F411xE)
#include "stm32f4xx.h"
#endif
#endif
#include "Board/Motor/motor_executor.h"
#include "Board/Motor/motor_plan_config.h"

static bool Machine_ConfigIsValid(const MachineConfig *config)
{
    if (config == NULL)
    {
        return false;
    }

#if SD700_AUTO_TARGET_ENABLED
    /* Reject accidentally selecting the Locked dry-run profile at boot. */
    if ((!config->raw_overpressure_enabled) ||
        (config->raw_overpressure_limit_counts > AUTO_TARGET_RAW_ABORT_COUNTS) ||
        (((int64_t)config->maximum_target_pressure_units +
                    config->hold_exit_tolerance_units) >=
         config->raw_overpressure_limit_counts) ||
        (config->maximum_target_pressure_units > AUTO_TARGET_MAX_TARGET_UNITS) ||
        (config->first_approach_command_mv > AUTO_TARGET_FIRST_COMMAND_MV) ||
        (config->first_approach_duration_ms > AUTO_TARGET_FIRST_DURATION_MS) ||
        (config->first_approach_backstop_ms > AUTO_TARGET_FIRST_BACKSTOP_MS) ||
        (config->recontact_command_mv > AUTO_TARGET_RECONTACT_COMMAND_MV) ||
        (config->recontact_duration_ms > AUTO_TARGET_RECONTACT_DURATION_MS) ||
        (config->recontact_backstop_ms > AUTO_TARGET_RECONTACT_BACKSTOP_MS) ||
        (config->pulse_command_mv > AUTO_TARGET_PRESS_MAX_MV) ||
        (config->pulse_duration_ms > AUTO_TARGET_PULSE_DURATION_MS) ||
        (config->pulse_backstop_ms > AUTO_TARGET_PULSE_BACKSTOP_MS) ||
        (config->automatic_cycle_timeout_ms > AUTO_TARGET_CONVERGENCE_TIMEOUT_MS) ||
        (config->pressure_freshness_ms > AUTO_TARGET_PRESSURE_FRESHNESS_MS) ||
        (config->settle_delay_ms < AUTO_TARGET_SETTLE_DELAY_MS) ||
        (config->settle_feedback_timeout_ms > AUTO_TARGET_FEEDBACK_TIMEOUT_MS))
    {
        return false;
    }
#endif

    return (config->maximum_target_pressure_units > 0) &&
           (config->contact_threshold_units >= 0) &&
           (config->contact_threshold_units <
            config->maximum_target_pressure_units) &&
           (config->hold_enter_tolerance_units >= 0) &&
           (config->hold_exit_tolerance_units >
            config->hold_enter_tolerance_units) &&
           (config->maximum_target_pressure_units <=
            (INT32_MAX - config->hold_exit_tolerance_units)) &&
           (config->pressure_freshness_ms > 0U) &&
           (config->settle_delay_ms > 0U) &&
           (config->settle_feedback_timeout_ms > 0U) &&
           (config->first_approach_command_mv > 0U) &&
           (config->first_approach_duration_ms > 0U) &&
           (config->first_approach_backstop_ms >
            config->first_approach_duration_ms) &&
           (config->recontact_command_mv > 0U) &&
           (config->recontact_duration_ms > 0U) &&
           (config->recontact_backstop_ms >
            config->recontact_duration_ms) &&
           (config->pulse_command_mv > 0U) &&
           (config->pulse_duration_ms > 0U) &&
           (config->pulse_backstop_ms > config->pulse_duration_ms) &&
           (config->automatic_cycle_timeout_ms > 0U) &&
           ((!config->raw_overpressure_enabled) ||
            (config->raw_overpressure_limit_counts > 0U));
}

static bool Machine_StateIsAutomatic(MachineState state)
{
    return (state == AUTO_APPROACH) ||
           (state == AUTO_SETTLE) ||
           (state == AUTO_PULSE) ||
           (state == AUTO_HOLD);
}

static bool Machine_StateIsDirectPulse(MachineState state)
{
    return (state == DIRECT_PRESS_PULSE) ||
           (state == DIRECT_RELEASE_PULSE);
}

static bool Machine_StateRequiresPressureSafety(MachineState state)
{
    if (Machine_StateIsAutomatic(state))
    {
        return true;
    }
#if SD700_DIRECT_PRESSURE_REQUIRED
    return Machine_StateIsDirectPulse(state);
#else
    (void)state;
    return false;
#endif
}

static void Machine_ClearTransientMotion(MachineContext *context)
{
    context->motor_request_sequence = 0U;
    context->settle_gate_sequence = 0U;
    context->settle_phase = SETTLE_WAIT_DELAY;
    context->approach_profile = APPROACH_FIRST;
    context->auto_has_contacted = false;
    context->auto_approach_pending = false;
}

#if SD700_AUTO_TARGET_ENABLED
static void Machine_ResetPressFeedback(MachineContext *context)
{
    (void)memset(&context->press_feedback, 0, sizeof(context->press_feedback));
}

static void Machine_RecordPressEnd(MachineContext *context,
                                   AutoApproachEndReason reason, uint32_t now_ms)
{
    volatile AutoPressDiagnostics *d = &g_sd700_approach_diagnostics.press;
    if (!context->press_feedback.in_flight) { return; }
    d->completed_request_sequence = context->press_feedback.request_sequence;
    d->completed_mv = context->press_feedback.command_mv;
    d->completed_at_ms = now_ms;
    d->end_reason = reason;
    d->before_units = context->press_feedback.before_units;
    d->before_sequence = context->press_feedback.before_sequence;
    d->observed_peak_units = context->press_feedback.observed_peak_units;
    d->after_units = 0;
    d->after_sequence = 0U;
    d->after_received_at_ms = 0U;
    d->feedback_valid = false;
    d->observed_off_fall = false;
    d->output_disabled_at_end = MotorExecutor_OutputIsDisabled();
    if (reason == AUTO_APPROACH_END_NORMAL) { ++d->normal_completion_count; }
    context->press_feedback.in_flight = false;
}

static uint32_t Machine_PressBand(const MachineContext *context)
{
    int64_t error = (int64_t)context->target_pressure_units -
                   context->pressure.control_pressure_units;
    if (error <= context->config.hold_exit_tolerance_units) { return 1U; }
    if (error <= 2LL * context->config.hold_exit_tolerance_units) { return 2U; }
    return 3U;
}

static uint32_t Machine_PressBoostLimit(const MachineContext *context)
{
    uint32_t band = Machine_PressBand(context);
    return (band == 3U) ? AUTO_TARGET_PRESS_MICRO_BOOST_MAX_MV :
           ((band == 2U) ? AUTO_TARGET_PRESS_FINE_BOOST_MAX_MV : 0U);
}

static void Machine_SelectPressBand(MachineContext *context)
{
    uint32_t band = Machine_PressBand(context);
    if ((band != context->press_feedback.band) || (band == 1U))
    {
        context->press_feedback.boost_mv = 0U;
        context->press_feedback.low_response_count = 0U;
    }
    context->press_feedback.band = band;
}

static uint32_t Machine_PressBaseMv(const MachineContext *context)
{
    int64_t error = (int64_t)context->target_pressure_units -
                   context->pressure.control_pressure_units;
    int64_t enter = context->config.hold_enter_tolerance_units;
    int64_t fine_top = 2LL * context->config.hold_exit_tolerance_units;
    if (error <= enter) { return 0U; }
    if (error <= fine_top)
    {
        return AUTO_TARGET_PRESS_FINE_MIN_MV + (uint32_t)((error - enter) *
            (AUTO_TARGET_PRESS_FINE_MAX_MV - AUTO_TARGET_PRESS_FINE_MIN_MV) /
            (fine_top - enter));
    }
    error -= fine_top;
    if (error >= AUTO_TARGET_PRESS_MICRO_RAMP_UNITS)
    { return AUTO_TARGET_PRESS_MICRO_MAX_MV; }
    return AUTO_TARGET_PRESS_MICRO_MIN_MV + (uint32_t)(error *
        (AUTO_TARGET_PRESS_MICRO_MAX_MV - AUTO_TARGET_PRESS_MICRO_MIN_MV) /
        AUTO_TARGET_PRESS_MICRO_RAMP_UNITS);
}

static void Machine_ConsumePressFeedback(MachineContext *context)
{
    volatile AutoPressDiagnostics *d = &g_sd700_approach_diagnostics.press;
    int64_t rise;
    if (!context->press_feedback.feedback_pending) { return; }
    /* Called ONLY after existing safety, output-idle, settle delay, receive-time
     * and new-sequence gates. A normal completion is consumed exactly once. */
    context->press_feedback.feedback_pending = false;
    if ((context->diagnostic_request_sequence != context->press_feedback.request_sequence) ||
        (context->pressure.sequence <= context->press_feedback.before_sequence) ||
        ((uint32_t)(context->pressure.received_at_ms - context->press_feedback.completed_at_ms) >= 0x80000000U))
    { Machine_ResetPressFeedback(context); return; }
    d->after_units = context->pressure.control_pressure_units;
    d->after_sequence = context->pressure.sequence;
    d->after_received_at_ms = context->pressure.received_at_ms;
    d->feedback_valid = true;
    d->observed_off_fall =
        ((int64_t)d->observed_peak_units - d->before_units >= AUTO_TARGET_PRESS_EFFECTIVE_RISE_UNITS) &&
        ((int64_t)d->observed_peak_units - d->after_units >= AUTO_TARGET_PRESS_EFFECTIVE_RISE_UNITS);
    rise = (int64_t)d->after_units - d->before_units;
    if ((context->pressure.control_pressure_units < context->config.contact_threshold_units) ||
        (context->pressure.control_pressure_units >= context->target_pressure_units -
                                                    context->config.hold_enter_tolerance_units))
    { Machine_ResetPressFeedback(context); return; }
    /* Crossing a band requires two new responses in that band. */
    if (context->press_feedback.band != Machine_PressBand(context))
    { Machine_SelectPressBand(context); return; }
    Machine_SelectPressBand(context);
    if ((rise >= AUTO_TARGET_PRESS_EFFECTIVE_RISE_UNITS) ||
        (Machine_PressBoostLimit(context) == 0U))
    {
        /* Effective far-band rise clears the low-response streak, retaining
         * the existing boost within this band's cap. Fine/near still retract. */
        uint32_t limit = (context->press_feedback.band == 3U) ?
                         Machine_PressBoostLimit(context) : 0U;
        if (context->press_feedback.boost_mv > limit)
        { context->press_feedback.boost_mv = limit; }
        context->press_feedback.low_response_count = 0U;
    }
    else if (++context->press_feedback.low_response_count >= AUTO_TARGET_PRESS_LOW_RESPONSE_PAIRS)
    {
        uint32_t next = context->press_feedback.boost_mv + AUTO_TARGET_PRESS_BOOST_STEP_MV;
        uint32_t limit = Machine_PressBoostLimit(context);
        context->press_feedback.boost_mv = (next < limit) ? next : limit;
        context->press_feedback.low_response_count = 0U;
    }
}
#endif

static void Machine_ResetForcePi(MachineContext *context)
{
#if SD700_AUTO_TARGET_ENABLED
    Machine_ResetPressFeedback(context);
#endif
    /* Also clear on fault if configuration/state was corrupted. */
    (void)ForcePi_Reset(&context->force_pi_state);
    context->force_pi_state.integral_mv = 0.0f;
    (void)memset(&context->force_pi_snapshot, 0,
                 sizeof(context->force_pi_snapshot));
    context->force_pi_time_valid = false;
    context->force_pi_received_at_ms = 0U;
    context->force_pi_sample_sequence = 0U;
    context->force_pi_inhibit_reason = FORCE_PI_INHIBIT_NONE;
    context->force_pi_blocked_suggestion_mv = 0.0f;
    context->force_pi_requested_signed_mv = 0;
}

#if SD700_AUTO_TARGET_ENABLED
/* Small RAM measurement only. Freeze at first contact or STOP/fault, including
 * OFF/settle intervals; no delayed sample may update a finished measurement. */
static void Machine_UpdateApproachMeasurement(const MachineContext *context,
                                               uint32_t now_ms, bool finish)
{
    volatile AutoApproachDiagnostics *d = &g_sd700_approach_diagnostics;
    if (!d->search_active) { return; }
    d->search_elapsed_ms = (uint32_t)(now_ms - d->search_started_at_ms);
    d->pressure_units = context->pressure.control_pressure_units;
    d->sample_received_at_ms = context->pressure.received_at_ms;
    d->sample_sequence = context->pressure.sequence;
    d->pressure_fresh = Machine_IsPressureFresh(context, now_ms);
    if (finish) { d->search_active = false; }
}

static void Machine_RecordFirstContact(const MachineContext *context,
                                       uint32_t now_ms)
{
    volatile AutoApproachDiagnostics *d = &g_sd700_approach_diagnostics;
    if (d->first_contact_latched) { return; }
    /* Already-contacted START has zero search time, even if its fresh sample
     * predates START. Otherwise measure receive time, not dispatch latency. */
    d->search_elapsed_ms = d->search_active ?
        (uint32_t)(context->pressure.received_at_ms - d->search_started_at_ms) : 0U;
    d->search_active = false;
    d->first_contact_latched = true;
    d->first_contact_pulse_count = d->pulse_count;
    d->first_contact_pressure_units = context->pressure.control_pressure_units;
    d->first_contact_received_at_ms = context->pressure.received_at_ms;
    d->first_contact_decided_at_ms = now_ms;
    d->first_contact_sample_sequence = context->pressure.sequence;
    d->pressure_units = context->pressure.control_pressure_units;
    d->sample_received_at_ms = context->pressure.received_at_ms;
    d->sample_sequence = context->pressure.sequence;
    d->pressure_fresh = Machine_IsPressureFresh(context, now_ms);
}

static void Machine_RecordApproachEnd(const MachineContext *context,
                                      AutoApproachEndReason reason,
                                      uint32_t now_ms)
{
    const MotorExecutorSnapshot *motor = MotorExecutor_GetSnapshot();
    volatile AutoApproachDiagnostics *d = &g_sd700_approach_diagnostics;

    if (d->end_reason != AUTO_APPROACH_END_RUNNING)
    {
        return;
    }
    d->end_reason = reason;
    d->ended_at_ms = now_ms;
    d->state_after = context->state;
    d->pressure_units = context->pressure.control_pressure_units;
    d->sample_received_at_ms = context->pressure.received_at_ms;
    d->sample_sequence = context->pressure.sequence;
    d->pressure_fresh = Machine_IsPressureFresh(context, now_ms);
    d->output_disabled_at_end = (motor != NULL) && motor->physical_output_disabled;
    d->logical_active_at_end = (motor != NULL) && motor->logical_active;
}
#endif

static void Machine_EnterFault(MachineContext *context,
                               MachineFault fault,
                               FaultDetail detail,
                               uint32_t now_ms)
{
#if SD700_AUTO_TARGET_ENABLED
    const MotorExecutorSnapshot *motor = MotorExecutor_GetSnapshot();
    AutoApproachEndReason approach_reason =
        ((motor != NULL) && (motor->last_completion == MOTOR_COMPLETION_BACKSTOP)) ?
        AUTO_APPROACH_END_BACKSTOP :
        ((fault == FAULT_MOTOR_FAULT) ? AUTO_APPROACH_END_EXECUTOR_ERROR :
                                       AUTO_APPROACH_END_SAFETY_FAULT);
    if ((fault == FAULT_MOTION_TIMEOUT) &&
        (detail == FAULT_DETAIL_APPROACH_TIMEOUT))
    {
        g_sd700_approach_diagnostics.approach_timeout_decided_at_ms = now_ms;
        g_sd700_approach_diagnostics.timeout_cycle_started_ms = context->cycle_started_ms;
        g_sd700_approach_diagnostics.approach_timeout_committed = true;
    }
#endif
    if ((fault == FAULT_MOTOR_FAULT) &&
        (detail == FAULT_DETAIL_NONE))
    {
        detail = FAULT_DETAIL_MOTOR_HARDWARE;
    }
    (void)MotorExecutor_Disable();
#if SD700_AUTO_TARGET_ENABLED
    Machine_RecordPressEnd(context, approach_reason, now_ms);
    Machine_UpdateApproachMeasurement(context, now_ms, true);
#endif
    Machine_ClearTransientMotion(context);
    Machine_ResetForcePi(context);
    context->fault = fault;
    context->fault_detail = detail;
    context->state = FAULT;
    context->state_entered_ms = now_ms;
#if SD700_AUTO_TARGET_ENABLED
    Machine_RecordApproachEnd(context, approach_reason, now_ms);
#endif
}

static MachineCommandResult Machine_CaptureMotorRequest(
    MachineContext *context,
    MotorResult result,
    MachineState next_state,
    uint32_t now_ms)
{
    const MotorExecutorSnapshot *motor;

    if (result != MOTOR_RESULT_OK)
    {
        Machine_EnterFault(context,
                           FAULT_MOTOR_FAULT,
                           FAULT_DETAIL_MOTOR_REQUEST_REJECTED,
                           now_ms);
        return COMMAND_EXECUTOR_FAILED;
    }

    motor = MotorExecutor_GetSnapshot();
    if ((motor == NULL) ||
        (!MotorExecutor_ActiveRequestIsValid()))
    {
        Machine_EnterFault(context,
                           FAULT_MOTOR_FAULT,
                           FAULT_DETAIL_MOTOR_HARDWARE,
                           now_ms);
        return COMMAND_EXECUTOR_FAILED;
    }

    context->motor_request_sequence = motor->request_sequence;
    context->diagnostic_request_sequence = motor->request_sequence;
    context->diagnostic_request_at_ms = now_ms;
    context->diagnostic_command_mv = motor->command_mv;
    context->diagnostic_duration_ms = motor->requested_duration_ms;
    context->diagnostic_direction = (uint16_t)motor->direction;
    context->state = next_state;
    context->state_entered_ms = now_ms;
    return COMMAND_ACCEPTED;
}

static MachineCommandResult Machine_StartApproach(
    MachineContext *context,
    MachineApproachProfile profile,
    uint32_t now_ms)
{
    uint32_t command_mv;
    uint32_t duration_ms;
    uint32_t backstop_ms;
#if SD700_AUTO_TARGET_ENABLED
    MachineCommandResult result;
#endif

    if (profile == APPROACH_FIRST)
    {
        command_mv = context->config.first_approach_command_mv;
        duration_ms = context->config.first_approach_duration_ms;
        backstop_ms = context->config.first_approach_backstop_ms;
    }
    else
    {
        command_mv = context->config.recontact_command_mv;
        duration_ms = context->config.recontact_duration_ms;
        backstop_ms = context->config.recontact_backstop_ms;
    }

    context->approach_profile = profile;
#if SD700_AUTO_TARGET_ENABLED
    context->auto_approach_pending = true;
    result = Machine_CaptureMotorRequest(
        context,
        MotorExecutor_StartPulse(MOTOR_DIRECTION_PRESS, command_mv,
                                  duration_ms, backstop_ms, now_ms),
        AUTO_APPROACH, now_ms);
    if (result == COMMAND_ACCEPTED)
    {
        volatile AutoApproachDiagnostics *d = &g_sd700_approach_diagnostics;
        ++d->pulse_count;
        d->request_sequence = context->motor_request_sequence;
        d->requested_mv = command_mv;
        d->requested_duration_ms = duration_ms;
        d->started_at_ms = now_ms;
        d->ended_at_ms = 0U;
        d->end_reason = AUTO_APPROACH_END_RUNNING;
        d->state_before = AUTO_APPROACH;
        d->state_after = AUTO_APPROACH;
        d->pressure_units = context->pressure.control_pressure_units;
        d->sample_received_at_ms = context->pressure.received_at_ms;
        d->sample_sequence = context->pressure.sequence;
        d->pressure_fresh = Machine_IsPressureFresh(context, now_ms);
        d->output_disabled_at_start = MotorExecutor_GetSnapshot()->physical_output_disabled;
        d->output_disabled_at_end = false;
        d->logical_active_at_end = true;
    }
    return result;
#else
    return Machine_CaptureMotorRequest(
        context,
        MotorExecutor_StartRun(MOTOR_DIRECTION_PRESS,
                               command_mv,
                               duration_ms,
                               backstop_ms,
                               now_ms),
        AUTO_APPROACH,
        now_ms);
#endif
}

static MachineCommandResult Machine_StartPulse(MachineContext *context,
                                               MotorDirection direction,
                                               uint32_t command_mv,
                                               uint32_t now_ms)
{
    return Machine_CaptureMotorRequest(
        context,
        MotorExecutor_StartPulse(direction,
                                 command_mv,
                                 context->config.pulse_duration_ms,
                                 context->config.pulse_backstop_ms,
                                 now_ms),
        AUTO_PULSE,
        now_ms);
}

#if SD700_DIRECT_COMMANDS_ENABLED
static MachineCommandResult Machine_StartDirectPulse(
    MachineContext *context,
    MotorDirection direction,
    uint32_t now_ms)
{
    MachineState next_state =
        (direction == MOTOR_DIRECTION_PRESS) ?
        DIRECT_PRESS_PULSE : DIRECT_RELEASE_PULSE;

    return Machine_CaptureMotorRequest(
        context,
        MotorExecutor_StartPulse(direction,
                                 SD700_DIRECT_PULSE_COMMAND_MV,
                                 SD700_DIRECT_PULSE_DURATION_MS,
                                 SD700_DIRECT_PULSE_BACKSTOP_MS,
                                 now_ms),
        next_state,
        now_ms);
}
#endif

static bool Machine_DisableOrFault(MachineContext *context, uint32_t now_ms)
{
    if ((MotorExecutor_Disable() == MOTOR_RESULT_OK) &&
        MotorExecutor_OutputIsDisabled())
    {
        context->motor_request_sequence = 0U;
        return true;
    }

    Machine_EnterFault(context,
                       FAULT_MOTOR_FAULT,
                       FAULT_DETAIL_MOTOR_HARDWARE,
                       now_ms);
    return false;
}

static bool Machine_EnterSettle(MachineContext *context, uint32_t now_ms)
{
    if (!Machine_DisableOrFault(context, now_ms))
    {
        return false;
    }

    context->state = AUTO_SETTLE;
    context->settle_phase = SETTLE_WAIT_DELAY;
    context->settle_gate_sequence = context->pressure.sequence;
    context->settle_feedback_after_ms = now_ms + context->config.settle_delay_ms;
    context->state_entered_ms = now_ms;
    return true;
}

#if SD700_AUTO_TARGET_ENABLED
static bool Machine_HasCurrentApproachContact(const MachineContext *context,
                                             uint32_t now_ms)
{
    /* MCU receive tick, never dispatch/PC time. Sample age must not exceed
     * episode age, excluding a pre-START cached frame across uint32 tick wrap.
     * Initial search has no total deadline. State/freshness/order gates and
     * the ContactDeadline completion-versus-cancel ordering remain in force.
     * Pressure delivery already deduplicates sequences and rejects bad order.
     * Contact evidence need not be settled feedback: it only stops coarse
     * motion; the existing settle/time/sequence gate still controls fine motion. */
    return context->auto_approach_pending &&
           ((context->state == AUTO_APPROACH) || (context->state == AUTO_SETTLE)) &&
           Machine_IsPressureFresh(context, now_ms) &&
           (context->pressure.control_pressure_units >=
            context->config.contact_threshold_units) &&
           ((uint32_t)(now_ms - context->pressure.received_at_ms) <=
            (uint32_t)(now_ms - context->cycle_started_ms));
}

static void Machine_StopApproachOnContact(MachineContext *context, uint32_t now_ms)
{
    /* Disable clears pending executor completions. Service/classify first so
     * a latched BACKSTOP/error cannot be turned into a successful contact.
     * On target the small service-to-cancel region is indivisible with respect
     * to ISR completion publication. Restore the caller's interrupt state.
     * No request, wait, logging I/O or floating-point calculation in this region.
     * Timer programming/stop/backstop remain owned by the existing executor. */
#if defined(STM32F411xE)
    uint32_t saved_primask = __get_PRIMASK();
    __disable_irq();
#endif
    if (MotorExecutor_Service(now_ms) != MOTOR_RESULT_OK)
    {
        Machine_EnterFault(context, FAULT_MOTOR_FAULT,
                           FAULT_DETAIL_MOTOR_HARDWARE, now_ms);
    }
    else
    {
        Machine_HandleMotorService(context, now_ms);
        if (context->state == AUTO_APPROACH)
        {
            if (Machine_EnterSettle(context, now_ms))
            {
                Machine_RecordApproachEnd(context, AUTO_APPROACH_END_CONTACT, now_ms);
            }
        }
        if (context->state == AUTO_SETTLE)
        {
            g_sd700_approach_diagnostics.contact_cycle_started_ms = context->cycle_started_ms;
            if (!context->auto_has_contacted)
            {
                Machine_RecordFirstContact(context, now_ms);
                /* Start the existing convergence budget at MCU contact receive
                 * time. Later recontacts must not extend this cycle. */
                context->cycle_started_ms = context->pressure.received_at_ms;
            }
            context->auto_has_contacted = true;
            context->auto_approach_pending = false;
            g_sd700_approach_diagnostics.contact_sample_sequence = context->pressure.sequence;
            g_sd700_approach_diagnostics.contact_received_at_ms = context->pressure.received_at_ms;
            g_sd700_approach_diagnostics.contact_decided_at_ms = now_ms;
        }
    }
#if defined(STM32F411xE)
    __set_PRIMASK(saved_primask);
#endif
}
#endif

static bool Machine_EnterHold(MachineContext *context, uint32_t now_ms)
{
    if (!Machine_DisableOrFault(context, now_ms))
    {
        return false;
    }

#if SD700_AUTO_TARGET_ENABLED
    Machine_ResetPressFeedback(context);
#endif
    context->state = AUTO_HOLD;
    context->state_entered_ms = now_ms;
    context->cycle_started_ms = 0U;
    return true;
}

static bool Machine_ForcePiMotorIdle(void)
{
    const MotorExecutorSnapshot *motor = MotorExecutor_GetSnapshot();

    return (motor != NULL) && (!motor->logical_active) &&
           MotorExecutor_IsHealthy() && MotorExecutor_OutputIsDisabled();
}

/* Validate in floating point BEFORE any rounding or unsigned conversion. */
static bool Machine_ForcePiMagnitudeValid(float magnitude,
                                         MotorDirection direction)
{
    uint16_t tim2_plan;
    uint16_t tim3_plan;
    uint32_t rounded;

    if ((!isfinite(magnitude)) || (magnitude < 0.0f) ||
        (magnitude > (float)MOTOR_PLAN_SUPPLY_REFERENCE_MV))
    {
        return false;
    }
    if (magnitude < 1.0f)
    {
        return true;
    }
    rounded = (uint32_t)(magnitude + 0.5f);
    return MotorExecutor_PlanCommand(direction, rounded,
                                     &tim2_plan, &tim3_plan) == MOTOR_RESULT_OK;
}

MachineCommandResult Machine_ConfigureForcePi(MachineContext *context,
                                             const ForcePiConfig *config)
{
#if !SD700_AUTOMATIC_COMMANDS_ENABLED
    (void)context;
    (void)config;
    return COMMAND_UNSUPPORTED;
#else
    ForcePiConfig effective;
    ForcePiState next;

    if ((context == NULL) || (config == NULL))
    {
        return COMMAND_INVALID_VALUE;
    }
    if (context->state != IDLE)
    {
        return COMMAND_NOT_ALLOWED;
    }
    if ((!context->config_valid) || (!Machine_ForcePiMotorIdle()))
    {
        return COMMAND_NOT_READY;
    }
    if ((!ForcePi_Initialize(&next, config)) ||
        (config->output_min_mv > 0.0f) ||
        (config->output_max_mv < 0.0f) ||
        (!Machine_ForcePiMagnitudeValid(-config->output_min_mv,
                                        MOTOR_DIRECTION_RELEASE)) ||
        (!Machine_ForcePiMagnitudeValid(config->output_max_mv,
                                        MOTOR_DIRECTION_PRESS)))
    {
        return COMMAND_INVALID_VALUE;
    }
    effective = *config;
#if SD700_AUTO_TARGET_ENABLED
    if ((config->ki_mv_per_unit_s != 0.0f) ||
        (config->integral_min_mv != 0.0f) || (config->integral_max_mv != 0.0f) ||
        (config->output_max_mv > (float)context->config.pulse_command_mv) ||
        (config->output_min_mv < -(float)AUTO_TARGET_RELEASE_MAX_MV))
    {
        return COMMAND_INVALID_VALUE;
    }
#endif
    if (!context->config.bench_release_correction_enabled)
    {
        effective.output_min_mv = 0.0f;
    }
    if (!ForcePi_Initialize(&next, &effective))
    {
        return COMMAND_INVALID_VALUE;
    }
    context->force_pi_config = effective;
    context->force_pi_state = next;
    context->force_pi_enabled = true;
    Machine_ResetForcePi(context);
    return COMMAND_ACCEPTED;
#endif
}

static bool Machine_ObserveForcePi(MachineContext *context, uint32_t now_ms)
{
    ForcePiState preview_state = context->force_pi_state;
    ForcePiSnapshot preview;
    int32_t pressure = context->pressure.control_pressure_units;
    int32_t tolerance = (context->state == AUTO_HOLD) ?
                        context->config.hold_exit_tolerance_units :
                        context->config.hold_enter_tolerance_units;
    bool below = pressure < (context->target_pressure_units - tolerance);
    bool above = pressure > (context->target_pressure_units + tolerance);
    bool correction_ready =
        ((context->state == AUTO_HOLD) ||
         ((context->state == AUTO_SETTLE) &&
          (context->settle_phase == SETTLE_WAIT_SAMPLE) &&
          (context->pressure.sequence > context->settle_gate_sequence))) &&
        (pressure >= context->config.contact_threshold_units) &&
        (below || above);
    bool idle = Machine_ForcePiMotorIdle();
    bool integrate;
    float dt_s = context->force_pi_time_valid ?
        (float)(uint32_t)(context->pressure.received_at_ms -
                         context->force_pi_received_at_ms) / 1000.0f : 0.0f;

    context->force_pi_requested_signed_mv = 0;
    context->force_pi_blocked_suggestion_mv = 0.0f;
    context->force_pi_inhibit_reason = FORCE_PI_INHIBIT_WAITING;
    /* Preview with frozen I prevents residual I from requesting motion toward
     * the wrong side of the tolerance band. Such a sample freezes I as well. */
    if (!ForcePi_Update(&context->force_pi_config, &preview_state,
                         (float)context->target_pressure_units,
                         (float)pressure, dt_s, false, &preview))
    {
        Machine_EnterFault(context, FAULT_INTERNAL_FAULT,
                           FAULT_DETAIL_INTERNAL_STATE, now_ms);
        return false;
    }
    integrate = correction_ready && idle && (dt_s > 0.0f);
    if (correction_ready)
    {
        context->force_pi_inhibit_reason = FORCE_PI_INHIBIT_NONE;
        if (!idle)
        {
            context->force_pi_inhibit_reason = FORCE_PI_INHIBIT_BUSY;
            integrate = false;
        }
        else if ((above && (preview.limited_output_mv > 0.0f)) ||
                 (below && (preview.limited_output_mv < 0.0f)))
        {
            context->force_pi_inhibit_reason = FORCE_PI_INHIBIT_WRONG_DIRECTION;
            context->force_pi_blocked_suggestion_mv = preview.limited_output_mv;
            integrate = false;
        }
        else if (above && (!context->config.bench_release_correction_enabled))
        {
            context->force_pi_inhibit_reason = FORCE_PI_INHIBIT_RELEASE_DISABLED;
            /* Effective clamp is zero. Log the negative unclamped suggestion. */
            context->force_pi_blocked_suggestion_mv = preview.unlimited_output_mv;
            integrate = false;
        }
    }
    if (!ForcePi_Update(&context->force_pi_config, &context->force_pi_state,
                         (float)context->target_pressure_units,
                         (float)pressure, dt_s, integrate,
                         &context->force_pi_snapshot))
    {
        Machine_EnterFault(context, FAULT_INTERNAL_FAULT,
                           FAULT_DETAIL_INTERNAL_STATE, now_ms);
        return false;
    }
    /* Integral limits can also change a candidate's sign (e.g. limits wholly
     * on one side of zero). Check the final calculation, not only the preview,
     * and roll back that increment while retaining the inhibited suggestion. */
    if (correction_ready && idle && integrate &&
        ((above && (context->force_pi_snapshot.limited_output_mv > 0.0f)) ||
         (below && (context->force_pi_snapshot.limited_output_mv < 0.0f))))
    {
        context->force_pi_blocked_suggestion_mv =
            context->force_pi_snapshot.limited_output_mv;
        context->force_pi_inhibit_reason = FORCE_PI_INHIBIT_WRONG_DIRECTION;
        context->force_pi_state = preview_state;
        context->force_pi_snapshot = preview;
    }
    context->force_pi_time_valid = true;
    context->force_pi_received_at_ms = context->pressure.received_at_ms;
    context->force_pi_sample_sequence = context->pressure.sequence;
    return true;
}

static MachineCommandResult Machine_StartCorrection(MachineContext *context,
                                                     MotorDirection expected,
                                                     uint32_t now_ms)
{
    float suggestion;
    float magnitude;
    uint32_t command_mv;
    MachineCommandResult result;

#if SD700_AUTO_TARGET_ENABLED
    if (expected == MOTOR_DIRECTION_RELEASE) { Machine_ResetPressFeedback(context); }
#endif
    if (!context->force_pi_enabled)
    {
        return Machine_StartPulse(context, expected,
                                   context->config.pulse_command_mv, now_ms);
    }
    suggestion = context->force_pi_snapshot.limited_output_mv;
    magnitude = fabsf(suggestion);
    if (!Machine_ForcePiMagnitudeValid(magnitude, expected))
    {
        Machine_EnterFault(context, FAULT_INTERNAL_FAULT,
                           FAULT_DETAIL_INTERNAL_STATE, now_ms);
        return COMMAND_EXECUTOR_FAILED;
    }
    if ((context->force_pi_inhibit_reason == FORCE_PI_INHIBIT_WRONG_DIRECTION) ||
        (context->force_pi_inhibit_reason == FORCE_PI_INHIBIT_RELEASE_DISABLED) ||
        (magnitude < 1.0f))
    {
        if (context->force_pi_inhibit_reason == FORCE_PI_INHIBIT_NONE)
        {
            context->force_pi_inhibit_reason = FORCE_PI_INHIBIT_BELOW_ONE_MV;
            context->force_pi_blocked_suggestion_mv = suggestion;
        }
        /* Retry only after the existing settle delay AND a new frame. The
         * original cycle timeout keeps repeated zero/blocked retries bounded. */
        return Machine_EnterSettle(context, now_ms) ?
               COMMAND_ACCEPTED : COMMAND_EXECUTOR_FAILED;
    }
    command_mv = (uint32_t)(magnitude + 0.5f);
#if SD700_AUTO_TARGET_ENABLED
    if ((expected == MOTOR_DIRECTION_PRESS) && (suggestion > 0.0f))
    {
        Machine_SelectPressBand(context);
        command_mv = Machine_PressBaseMv(context) + context->press_feedback.boost_mv;
        /* Mapped PRESS bypasses the old 1200 mV P preview, but still respects
         * the configured request ceiling and the existing executor planner. */
        if (command_mv > context->config.pulse_command_mv)
        { command_mv = context->config.pulse_command_mv; }
    }
#endif
    result = Machine_StartPulse(context,
                                (suggestion > 0.0f) ? MOTOR_DIRECTION_PRESS :
                                                      MOTOR_DIRECTION_RELEASE,
                                command_mv, now_ms);
    if (result == COMMAND_ACCEPTED)
    {
#if SD700_AUTO_TARGET_ENABLED
        if ((expected == MOTOR_DIRECTION_PRESS) && (suggestion > 0.0f))
        {
            volatile AutoPressDiagnostics *d = &g_sd700_approach_diagnostics.press;
            context->press_feedback.request_sequence = context->motor_request_sequence;
            context->press_feedback.command_mv = command_mv;
            context->press_feedback.before_units = context->pressure.control_pressure_units;
            context->press_feedback.before_sequence = context->pressure.sequence;
            context->press_feedback.observed_peak_units = context->pressure.control_pressure_units;
            context->press_feedback.in_flight = true;
            context->press_feedback.feedback_pending = false;
            ++d->request_count;
            d->request_sequence = context->motor_request_sequence;
            d->requested_mv = command_mv;
            d->base_mv = Machine_PressBaseMv(context);
            d->boost_mv = context->press_feedback.boost_mv;
            d->duration_ms = context->config.pulse_duration_ms;
            d->started_at_ms = now_ms;
        }
#endif
        context->force_pi_requested_signed_mv = (suggestion > 0.0f) ?
            (int32_t)command_mv : -(int32_t)command_mv;
    }
    return result;
}

static void Machine_ClassifySettledSample(MachineContext *context,
                                          uint32_t now_ms)
{
    int32_t lower_enter = context->target_pressure_units -
                          context->config.hold_enter_tolerance_units;
    int32_t upper_enter = context->target_pressure_units +
                          context->config.hold_enter_tolerance_units;
    int32_t pressure = context->pressure.control_pressure_units;

    if (pressure < context->config.contact_threshold_units)
    {
#if SD700_AUTO_TARGET_ENABLED
        Machine_ResetPressFeedback(context);
#endif
        context->last_command_result =
#if SD700_AUTO_TARGET_ENABLED
            Machine_StartApproach(context, context->auto_has_contacted ?
                APPROACH_RECONTACT : APPROACH_FIRST, now_ms);
#else
            Machine_StartApproach(context, APPROACH_RECONTACT, now_ms);
#endif
    }
    else if (pressure < lower_enter)
    {
        context->last_command_result =
            Machine_StartCorrection(context, MOTOR_DIRECTION_PRESS, now_ms);
    }
    else if (pressure <= upper_enter)
    {
        (void)Machine_EnterHold(context, now_ms);
    }
    else if (context->config.bench_release_correction_enabled ||
             context->force_pi_enabled)
    {
        context->last_command_result =
            Machine_StartCorrection(context, MOTOR_DIRECTION_RELEASE, now_ms);
    }
    else
    {
        (void)Machine_EnterHold(context, now_ms);
    }
}

static void Machine_ClassifyHoldSample(MachineContext *context,
                                       uint32_t now_ms)
{
    int32_t lower_exit = context->target_pressure_units -
                         context->config.hold_exit_tolerance_units;
    int32_t upper_exit = context->target_pressure_units +
                         context->config.hold_exit_tolerance_units;
    int32_t pressure = context->pressure.control_pressure_units;

    if (pressure < context->config.contact_threshold_units)
    {
        context->cycle_started_ms = now_ms;
        context->last_command_result =
            Machine_StartApproach(context, APPROACH_RECONTACT, now_ms);
    }
    else if (pressure < lower_exit)
    {
        context->cycle_started_ms = now_ms;
        context->last_command_result =
            Machine_StartCorrection(context, MOTOR_DIRECTION_PRESS, now_ms);
    }
    else if ((pressure > upper_exit) &&
             (context->config.bench_release_correction_enabled ||
              context->force_pi_enabled))
    {
        context->cycle_started_ms = now_ms;
        context->last_command_result =
            Machine_StartCorrection(context, MOTOR_DIRECTION_RELEASE, now_ms);
    }
    else
    {
        (void)Machine_DisableOrFault(context, now_ms);
    }
}

bool Machine_IsPressureFresh(const MachineContext *context, uint32_t now_ms)
{
    return (context != NULL) &&
           context->pressure.frame_valid &&
           context->pressure.control_units_valid &&
           ((uint32_t)(now_ms - context->pressure.received_at_ms) <=
            context->config.pressure_freshness_ms);
}

void Machine_Initialize(MachineContext *context,
                        const MachineConfig *config,
                        uint32_t now_ms)
{
    MotorResult disable_result;

    if (context == NULL)
    {
        return;
    }

    disable_result = MotorExecutor_Disable();
    (void)memset(context, 0, sizeof(*context));
    context->state = BOOT_SAFE;
    context->fault = FAULT_NONE;
    context->fault_detail = FAULT_DETAIL_NONE;
    context->last_command_result = COMMAND_NOT_ALLOWED;
    context->state_entered_ms = now_ms;
    context->config_valid = Machine_ConfigIsValid(config);
    if (config != NULL)
    {
        context->config = *config;
    }

    if (disable_result != MOTOR_RESULT_OK)
    {
        context->state = FAULT;
        context->fault = FAULT_MOTOR_FAULT;
        context->fault_detail = FAULT_DETAIL_MOTOR_HARDWARE;
    }
}

void Machine_CompleteBoot(MachineContext *context,
                          bool boot_checks_passed,
                          uint32_t now_ms)
{
    if ((context == NULL) || (context->state != BOOT_SAFE))
    {
        return;
    }

    if ((!boot_checks_passed) || (!context->config_valid) ||
        (!MotorExecutor_IsHealthy()))
    {
        Machine_EnterFault(context,
                           FAULT_BOOT_FAULT,
                           FAULT_DETAIL_BOOT_CONFIGURATION,
                           now_ms);
        return;
    }

    if (!Machine_DisableOrFault(context, now_ms))
    {
        return;
    }
    context->state = IDLE;
    context->state_entered_ms = now_ms;
    context->fault = FAULT_NONE;
    context->fault_detail = FAULT_DETAIL_NONE;
}

static MachineCommandResult Machine_Stop(MachineContext *context,
                                         uint32_t now_ms)
{
    MachineState stopped_state = context->state;

    if (!Machine_DisableOrFault(context, now_ms))
    {
        return COMMAND_EXECUTOR_FAILED;
    }

#if SD700_AUTO_TARGET_ENABLED
    Machine_RecordPressEnd(context, AUTO_APPROACH_END_STOP, now_ms);
    Machine_UpdateApproachMeasurement(context, now_ms, true);
#endif
    Machine_ClearTransientMotion(context);
    Machine_ResetForcePi(context);
    context->cycle_started_ms = 0U;
    if (stopped_state == FAULT)
    {
        context->state = FAULT;
    }
    else if (stopped_state == BOOT_SAFE)
    {
        context->state = BOOT_SAFE;
    }
    else
    {
        context->state = IDLE;
    }
    context->state_entered_ms = now_ms;
#if SD700_AUTO_TARGET_ENABLED
    Machine_RecordApproachEnd(context, AUTO_APPROACH_END_STOP, now_ms);
#endif
    return COMMAND_ACCEPTED;
}

MachineCommandResult Machine_HandleCommand(MachineContext *context,
                                           const MachineCommand *command,
                                           uint32_t now_ms)
{
    MachineCommandResult result;
    const MotorExecutorSnapshot *motor;

    if ((context == NULL) || (command == NULL))
    {
        return COMMAND_INVALID_VALUE;
    }

    if (command->type == CMD_STOP)
    {
        result = Machine_Stop(context, now_ms);
        context->last_command_result = result;
        return result;
    }

    if (command->type == CMD_SET_TARGET)
    {
        if (context->state != IDLE)
        {
            result = ((context->state == FAULT) ||
                      (context->state == BOOT_SAFE)) ?
                     COMMAND_NOT_ALLOWED : COMMAND_BUSY;
        }
        else if ((command->target_pressure_units <
                  context->config.contact_threshold_units) ||
                 (command->target_pressure_units >
                  context->config.maximum_target_pressure_units))
        {
            result = COMMAND_INVALID_VALUE;
        }
        else
        {
            context->target_pressure_units =
                command->target_pressure_units;
            context->target_valid = true;
            Machine_ResetForcePi(context);
            result = COMMAND_ACCEPTED;
        }
        context->last_command_result = result;
        return result;
    }

    if ((command->type == CMD_DIRECT_PRESS_PULSE) ||
        (command->type == CMD_DIRECT_RELEASE_PULSE))
    {
#if !SD700_DIRECT_COMMANDS_ENABLED
        result = COMMAND_UNSUPPORTED;
#else
        motor = MotorExecutor_GetSnapshot();
        if (context->state != IDLE)
        {
            result = ((context->state == FAULT) ||
                      (context->state == BOOT_SAFE)) ?
                     COMMAND_NOT_ALLOWED : COMMAND_BUSY;
        }
        else if ((!context->config_valid) ||
                 (!MotorExecutor_IsHealthy()) ||
                 (motor == NULL) || motor->logical_active ||
                 (!motor->physical_output_disabled) ||
                 (!MotorExecutor_OutputIsDisabled()))
        {
            result = COMMAND_NOT_READY;
        }
#if SD700_DIRECT_PRESSURE_REQUIRED
        else if (!Machine_IsPressureFresh(context, now_ms))
        {
            result = COMMAND_NOT_READY;
        }
#endif
        else
        {
            result = Machine_StartDirectPulse(
                context,
                (command->type == CMD_DIRECT_PRESS_PULSE) ?
                    MOTOR_DIRECTION_PRESS : MOTOR_DIRECTION_RELEASE,
                now_ms);
        }
#endif
        context->last_command_result = result;
        return result;
    }

    if (command->type != CMD_AUTO_START)
    {
        context->last_command_result = COMMAND_UNSUPPORTED;
        return COMMAND_UNSUPPORTED;
    }

#if !SD700_AUTOMATIC_COMMANDS_ENABLED
    context->last_command_result = COMMAND_UNSUPPORTED;
    return COMMAND_UNSUPPORTED;
#endif

    motor = MotorExecutor_GetSnapshot();
    if (context->state != IDLE)
    {
        result = (context->state == FAULT) ?
                 COMMAND_NOT_ALLOWED : COMMAND_BUSY;
    }
    else if ((!context->target_valid) || (!context->config_valid) ||
             (!Machine_IsPressureFresh(context, now_ms)) ||
             (!MotorExecutor_IsHealthy()) || (motor == NULL) ||
             motor->logical_active)
    {
        result = COMMAND_NOT_READY;
    }
#if SD700_AUTO_TARGET_ENABLED
    else if ((!context->force_pi_enabled) || (!Machine_ForcePiMotorIdle()))
    {
        result = COMMAND_NOT_READY;
    }
    else if (context->pressure.raw_pressure_counts >=
             context->config.raw_overpressure_limit_counts)
    {
        Machine_EnterFault(context, FAULT_OVERPRESSURE, FAULT_DETAIL_NONE, now_ms);
        result = COMMAND_NOT_READY;
    }
#endif
    else
    {
        Machine_ResetForcePi(context);
        context->cycle_started_ms = now_ms;
#if SD700_AUTO_TARGET_ENABLED
        g_sd700_approach_diagnostics = (AutoApproachDiagnostics){0};
        context->auto_has_contacted = context->pressure.control_pressure_units >=
                                      context->config.contact_threshold_units;
        context->auto_approach_pending = !context->auto_has_contacted;
        g_sd700_approach_diagnostics.search_started_at_ms = now_ms;
        g_sd700_approach_diagnostics.search_active = !context->auto_has_contacted;
#endif
        if (context->pressure.control_pressure_units <
            context->config.contact_threshold_units)
        {
            result = Machine_StartApproach(context,
                                           APPROACH_FIRST,
                                           now_ms);
        }
        else
        {
            result = Machine_EnterSettle(context, now_ms) ?
                     COMMAND_ACCEPTED : COMMAND_EXECUTOR_FAILED;
#if SD700_AUTO_TARGET_ENABLED
            if (result == COMMAND_ACCEPTED) { Machine_RecordFirstContact(context, now_ms); }
#endif
        }
    }

    context->last_command_result = result;
    return result;
}

void Machine_HandlePressureSample(MachineContext *context,
                                  const MachinePressureSample *sample,
                                  uint32_t now_ms)
{
    if ((context == NULL) || (sample == NULL))
    {
        return;
    }

    if ((!sample->frame_valid) || (!sample->control_units_valid))
    {
        context->pressure = *sample;
        if (Machine_StateRequiresPressureSafety(context->state))
        {
            Machine_EnterFault(context,
                               FAULT_PRESSURE_SENSOR_FAULT,
                               FAULT_DETAIL_PRESSURE_INVALID,
                               now_ms);
        }
        return;
    }

    if (context->pressure.frame_valid &&
        context->pressure.control_units_valid &&
        (sample->sequence <= context->pressure.sequence))
    {
        if ((sample->sequence < context->pressure.sequence) &&
            Machine_StateRequiresPressureSafety(context->state))
        {
            Machine_EnterFault(context,
                               FAULT_PRESSURE_SENSOR_FAULT,
                               FAULT_DETAIL_PRESSURE_ORDER_LOST,
                               now_ms);
        }
        return;
    }

#if SD700_AUTO_TARGET_ENABLED
    /* Unsigned half-range comparison is well-defined across tick wrap.
     * A newer sequence with an older timestamp cannot refresh control. */
    if (context->pressure.frame_valid && context->pressure.control_units_valid &&
        ((uint32_t)(sample->received_at_ms - context->pressure.received_at_ms) >=
         0x80000000U))
    {
        if (Machine_StateRequiresPressureSafety(context->state))
        {
            Machine_EnterFault(context, FAULT_PRESSURE_SENSOR_FAULT,
                               FAULT_DETAIL_PRESSURE_ORDER_LOST, now_ms);
        }
        return;
    }
#endif
    context->pressure = *sample;
    if (context->config.raw_overpressure_enabled &&
        (sample->raw_pressure_counts >=
         context->config.raw_overpressure_limit_counts))
    {
        Machine_EnterFault(context,
                           FAULT_OVERPRESSURE,
                           FAULT_DETAIL_NONE,
                           now_ms);
        return;
    }

    if (context->force_pi_enabled && Machine_StateIsAutomatic(context->state))
    {
        /* Freshness, cycle and feedback deadlines take precedence over a new
         * request, including callers which have not yet serviced safety. */
        Machine_CheckPressureSafety(context, now_ms);
#if SD700_AUTO_TARGET_ENABLED
        if ((context->state == AUTO_SETTLE) &&
            ((uint32_t)(sample->received_at_ms - context->settle_feedback_after_ms) == 0U ||
             (uint32_t)(sample->received_at_ms - context->settle_feedback_after_ms) >=
              0x80000000U))
        {
            /* Still retained for safety/diagnostics; never a settled decision. */
            return;
        }
#endif
        if ((context->state == FAULT) ||
            (!Machine_ObserveForcePi(context, now_ms)))
        {
            return;
        }
        if (((context->state == AUTO_SETTLE) ||
             (context->state == AUTO_HOLD)) && (!Machine_ForcePiMotorIdle()))
        {
            return;
        }
    }

#if SD700_AUTO_TARGET_ENABLED
    if (context->press_feedback.in_flight && (context->state == AUTO_PULSE) &&
        Machine_IsPressureFresh(context, now_ms) &&
        (sample->control_pressure_units > context->press_feedback.observed_peak_units))
    { context->press_feedback.observed_peak_units = sample->control_pressure_units; }
#endif
    if ((context->state == AUTO_APPROACH) &&
        (sample->control_pressure_units >=
         context->config.contact_threshold_units))
    {
#if SD700_AUTO_TARGET_ENABLED
        if (Machine_HasCurrentApproachContact(context, now_ms))
        {
            Machine_StopApproachOnContact(context, now_ms);
        }
#else
        (void)Machine_EnterSettle(context, now_ms);
#endif
    }
    else if ((context->state == AUTO_SETTLE) &&
             (context->settle_phase == SETTLE_WAIT_SAMPLE) &&
             (sample->sequence > context->settle_gate_sequence))
    {
#if SD700_AUTO_TARGET_ENABLED
        Machine_ConsumePressFeedback(context);
        if (sample->control_pressure_units >= context->config.contact_threshold_units)
        {
            context->auto_has_contacted = true;
            context->auto_approach_pending = false;
        }
#endif
        Machine_ClassifySettledSample(context, now_ms);
    }
    else if (context->state == AUTO_HOLD)
    {
        Machine_ClassifyHoldSample(context, now_ms);
    }
}

void Machine_CheckPressureSafety(MachineContext *context, uint32_t now_ms)
{
    if ((context == NULL) ||
        (!Machine_StateRequiresPressureSafety(context->state)))
    {
        return;
    }

#if SD700_AUTO_TARGET_ENABLED
    Machine_UpdateApproachMeasurement(context, now_ms, false);
#endif

    if (!Machine_IsPressureFresh(context, now_ms))
    {
        Machine_EnterFault(context,
                           FAULT_PRESSURE_SENSOR_FAULT,
                           FAULT_DETAIL_PRESSURE_TIMEOUT,
                           now_ms);
        return;
    }

    if (Machine_StateIsDirectPulse(context->state))
    {
        return;
    }

    if ((context->state != AUTO_HOLD) &&
#if SD700_AUTO_TARGET_ENABLED
        /* Initial search consumes no convergence budget. Recontact after the
         * first latch remains inside the running cycle; it cannot reset it. */
        context->auto_has_contacted &&
#endif
        ((uint32_t)(now_ms - context->cycle_started_ms) >=
         context->config.automatic_cycle_timeout_ms))
    {
        Machine_EnterFault(context,
                           FAULT_MOTION_TIMEOUT,
                           FAULT_DETAIL_CYCLE_TIMEOUT,
                           now_ms);
        return;
    }

    if ((context->state == AUTO_SETTLE) &&
        (context->settle_phase == SETTLE_WAIT_SAMPLE) &&
        ((uint32_t)(now_ms - context->state_entered_ms) >=
         context->config.settle_feedback_timeout_ms))
    {
        Machine_EnterFault(context,
                           FAULT_PRESSURE_SENSOR_FAULT,
                           FAULT_DETAIL_SETTLE_FEEDBACK_TIMEOUT,
                           now_ms);
    }
#if SD700_AUTO_TARGET_ENABLED
    /* Freshness, active convergence and settle-feedback faults above still
     * take precedence. ContactDeadline's contact stop services and classifies
     * pending hardware completions before cancelling the coarse output. */
    if (Machine_HasCurrentApproachContact(context, now_ms))
    {
        Machine_StopApproachOnContact(context, now_ms);
    }
#endif
}

void Machine_HandleMotorService(MachineContext *context, uint32_t now_ms)
{
    const MotorExecutorSnapshot *motor;

    if ((context == NULL) ||
        ((context->state != AUTO_APPROACH) &&
         (context->state != AUTO_PULSE) &&
         (!Machine_StateIsDirectPulse(context->state))))
    {
        return;
    }

    motor = MotorExecutor_GetSnapshot();
    if (motor == NULL)
    {
        Machine_EnterFault(context,
                           FAULT_MOTOR_FAULT,
                           FAULT_DETAIL_MOTOR_HARDWARE,
                           now_ms);
        return;
    }

    if (motor->request_sequence != context->motor_request_sequence)
    {
        if (Machine_StateIsDirectPulse(context->state))
        {
            Machine_EnterFault(context,
                               FAULT_MOTOR_FAULT,
                               FAULT_DETAIL_MOTOR_HARDWARE,
                               now_ms);
        }
        return;
    }

    if (motor->logical_active)
    {
        if (!MotorExecutor_ActiveRequestIsValid())
        {
            Machine_EnterFault(context,
                               FAULT_MOTOR_FAULT,
                               FAULT_DETAIL_MOTOR_HARDWARE,
                               now_ms);
        }
        return;
    }

    if (!MotorExecutor_OutputIsDisabled())
    {
        Machine_EnterFault(context,
                           FAULT_MOTOR_FAULT,
                           FAULT_DETAIL_MOTOR_HARDWARE,
                           now_ms);
        return;
    }

    if (motor->last_completion == MOTOR_COMPLETION_ERROR)
    {
        Machine_EnterFault(context,
                           FAULT_MOTOR_FAULT,
                           FAULT_DETAIL_MOTOR_HARDWARE,
                           now_ms);
    }
    else if (Machine_StateIsDirectPulse(context->state))
    {
        if (motor->last_completion == MOTOR_COMPLETION_NORMAL)
        {
            context->motor_request_sequence = 0U;
            context->state = IDLE;
            context->state_entered_ms = now_ms;
        }
        else if (motor->last_completion == MOTOR_COMPLETION_BACKSTOP)
        {
            Machine_EnterFault(context,
                               FAULT_MOTION_TIMEOUT,
                               FAULT_DETAIL_DIRECT_PULSE_TIMEOUT,
                               now_ms);
        }
        else
        {
            Machine_EnterFault(context,
                               FAULT_MOTOR_FAULT,
                               FAULT_DETAIL_MOTOR_HARDWARE,
                               now_ms);
        }
    }
    else if (context->state == AUTO_APPROACH)
    {
#if SD700_AUTO_TARGET_ENABLED
        if (motor->last_completion == MOTOR_COMPLETION_NORMAL)
        {
            if (Machine_EnterSettle(context, now_ms))
            {
                Machine_RecordApproachEnd(context, AUTO_APPROACH_END_NORMAL, now_ms);
            }
        }
        else if (motor->last_completion == MOTOR_COMPLETION_BACKSTOP)
#else
        if ((motor->last_completion == MOTOR_COMPLETION_NORMAL) ||
            (motor->last_completion == MOTOR_COMPLETION_BACKSTOP))
#endif
        {
            Machine_EnterFault(context,
                               FAULT_MOTION_TIMEOUT,
                               FAULT_DETAIL_APPROACH_TIMEOUT,
                               now_ms);
        }
        else
        {
            Machine_EnterFault(context,
                               FAULT_MOTOR_FAULT,
                               FAULT_DETAIL_MOTOR_HARDWARE,
                               now_ms);
        }
    }
    else if (motor->last_completion == MOTOR_COMPLETION_NORMAL)
    {
#if SD700_AUTO_TARGET_ENABLED
        if (context->press_feedback.in_flight &&
            (context->press_feedback.request_sequence == motor->request_sequence) &&
            (motor->direction == MOTOR_DIRECTION_PRESS))
        {
            Machine_RecordPressEnd(context, AUTO_APPROACH_END_NORMAL, now_ms);
            context->press_feedback.completed_at_ms = now_ms;
            context->press_feedback.feedback_pending = true;
        }
#endif
        (void)Machine_EnterSettle(context, now_ms);
    }
    else if (motor->last_completion == MOTOR_COMPLETION_BACKSTOP)
    {
        Machine_EnterFault(context,
                           FAULT_MOTION_TIMEOUT,
                           FAULT_DETAIL_PULSE_TIMEOUT,
                           now_ms);
    }
    else
    {
        Machine_EnterFault(context,
                           FAULT_MOTOR_FAULT,
                           FAULT_DETAIL_MOTOR_HARDWARE,
                           now_ms);
    }
}

void Machine_ReportFault(MachineContext *context,
                         MachineFault fault,
                         FaultDetail detail,
                         uint32_t now_ms)
{
    if (context == NULL)
    {
        return;
    }

    if (fault == FAULT_NONE)
    {
        fault = FAULT_INTERNAL_FAULT;
        detail = FAULT_DETAIL_INTERNAL_STATE;
    }
    Machine_EnterFault(context, fault, detail, now_ms);
}

void Machine_Tick(MachineContext *context, uint32_t now_ms)
{
    if (context == NULL)
    {
        return;
    }

    switch (context->state)
    {
        case BOOT_SAFE:
        case IDLE:
        case AUTO_APPROACH:
        case AUTO_PULSE:
        case AUTO_HOLD:
        case DIRECT_PRESS_PULSE:
        case DIRECT_RELEASE_PULSE:
        case FAULT:
            break;

        case AUTO_SETTLE:
            if ((context->settle_phase == SETTLE_WAIT_DELAY) &&
                ((uint32_t)(now_ms - context->state_entered_ms) >=
                 context->config.settle_delay_ms))
            {
                context->settle_gate_sequence = context->pressure.sequence;
#if SD700_AUTO_TARGET_ENABLED
                /* Conservative gate uses actual main-loop opening time. */
                context->settle_feedback_after_ms = now_ms;
#endif
                context->settle_phase = SETTLE_WAIT_SAMPLE;
                context->state_entered_ms = now_ms;
            }
            break;

        case JOG_PRESS:
        case JOG_RELEASE:
        case COMPLETE:
        default:
            Machine_EnterFault(context,
                               FAULT_INTERNAL_FAULT,
                               FAULT_DETAIL_INTERNAL_STATE,
                               now_ms);
            break;
    }
}
