#include "Application/auto_target_config.h"

volatile AutoApproachDiagnostics g_sd700_approach_diagnostics;

const MachineConfig g_sd700_auto_target_machine_config = {
    .maximum_target_pressure_units = AUTO_TARGET_MAX_TARGET_UNITS,
    .contact_threshold_units = AUTO_TARGET_CONTACT_UNITS,
    .hold_enter_tolerance_units = AUTO_TARGET_HOLD_ENTER_UNITS,
    .hold_exit_tolerance_units = AUTO_TARGET_HOLD_EXIT_UNITS,
    .pressure_freshness_ms = AUTO_TARGET_PRESSURE_FRESHNESS_MS,
    .settle_delay_ms = AUTO_TARGET_SETTLE_DELAY_MS,
    .settle_feedback_timeout_ms = AUTO_TARGET_FEEDBACK_TIMEOUT_MS,
    .first_approach_command_mv = AUTO_TARGET_FIRST_COMMAND_MV,
    .first_approach_duration_ms = AUTO_TARGET_FIRST_DURATION_MS,
    .first_approach_backstop_ms = AUTO_TARGET_FIRST_BACKSTOP_MS,
    .recontact_command_mv = AUTO_TARGET_RECONTACT_COMMAND_MV,
    .recontact_duration_ms = AUTO_TARGET_RECONTACT_DURATION_MS,
    .recontact_backstop_ms = AUTO_TARGET_RECONTACT_BACKSTOP_MS,
    .pulse_command_mv = AUTO_TARGET_PRESS_MAX_MV,
    .pulse_duration_ms = AUTO_TARGET_PULSE_DURATION_MS,
    .pulse_backstop_ms = AUTO_TARGET_PULSE_BACKSTOP_MS,
    .automatic_cycle_timeout_ms = AUTO_TARGET_CONVERGENCE_TIMEOUT_MS,
    .bench_release_correction_enabled = true,
    .raw_overpressure_enabled = true,
    .raw_overpressure_limit_counts = AUTO_TARGET_RAW_ABORT_COUNTS
};

const ForcePiConfig g_sd700_auto_target_force_pi_config = {
    .kp_mv_per_unit = AUTO_TARGET_KP_MV_PER_UNIT,
    .ki_mv_per_unit_s = 0.0f,
    .output_min_mv = -(float)AUTO_TARGET_RELEASE_MAX_MV,
    .output_max_mv = (float)AUTO_TARGET_PI_PRESS_MAX_MV,
    .integral_min_mv = 0.0f,
    .integral_max_mv = 0.0f,
    .maximum_dt_s = 1.0f
};
