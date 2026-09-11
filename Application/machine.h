#ifndef APPLICATION_MACHINE_H
#define APPLICATION_MACHINE_H

#include <stdbool.h>
#include <stdint.h>

#include "Application/force_pi.h"
#include "Protocol/PressureSensor/pressure_sensor_protocol.h"

/* Real automatic motion requires the explicit AutoTarget build policy. */

typedef enum
{
    BOOT_SAFE = 0,
    IDLE,
    JOG_PRESS,
    JOG_RELEASE,
    AUTO_APPROACH,
    AUTO_SETTLE,
    AUTO_PULSE,
    AUTO_HOLD,
    COMPLETE,
    FAULT,
    DIRECT_PRESS_PULSE,
    DIRECT_RELEASE_PULSE
} MachineState;

typedef enum
{
    CMD_AUTO_START = 0,
    CMD_STOP,
    CMD_JOG_PRESS,
    CMD_JOG_RELEASE,
    CMD_JOG_STOP,
    CMD_FAULT_RESET,
    CMD_ACK_COMPLETE,
    CMD_SET_TARGET,
    CMD_DIRECT_PRESS_PULSE,
    CMD_DIRECT_RELEASE_PULSE
} MachineCommandType;

typedef enum
{
    FAULT_NONE = 0,
    FAULT_BOOT_FAULT,
    FAULT_PRESSURE_SENSOR_FAULT,
    FAULT_OVERPRESSURE,
    FAULT_OVERCURRENT,
    FAULT_MOTION_TIMEOUT,
    FAULT_MOTOR_FAULT,
    FAULT_INTERNAL_FAULT
} MachineFault;

typedef enum
{
    FAULT_DETAIL_NONE = 0,
    FAULT_DETAIL_BOOT_CONFIGURATION,
    FAULT_DETAIL_PRESSURE_TIMEOUT,
    FAULT_DETAIL_PRESSURE_INVALID,
    FAULT_DETAIL_PRESSURE_ORDER_LOST,
    FAULT_DETAIL_SETTLE_FEEDBACK_TIMEOUT,
    FAULT_DETAIL_APPROACH_TIMEOUT,
    FAULT_DETAIL_PULSE_TIMEOUT,
    FAULT_DETAIL_CYCLE_TIMEOUT,
    FAULT_DETAIL_MOTOR_REQUEST_REJECTED,
    FAULT_DETAIL_MOTOR_HARDWARE,
    FAULT_DETAIL_INTERNAL_STATE,
    FAULT_DETAIL_DIRECT_PULSE_TIMEOUT
} FaultDetail;

typedef enum
{
    COMMAND_ACCEPTED = 0,
    COMMAND_INVALID_VALUE,
    COMMAND_NOT_ALLOWED,
    COMMAND_BUSY,
    COMMAND_NOT_READY,
    COMMAND_WRONG_OWNER,
    COMMAND_UNSUPPORTED,
    COMMAND_EXECUTOR_FAILED
} MachineCommandResult;

typedef enum
{
    SETTLE_WAIT_DELAY = 0,
    SETTLE_WAIT_SAMPLE
} MachineSettlePhase;

typedef enum
{
    APPROACH_FIRST = 0,
    APPROACH_RECONTACT
} MachineApproachProfile;

typedef struct
{
    MachineCommandType type;
    int32_t target_pressure_units;
} MachineCommand;

typedef struct
{
    int32_t maximum_target_pressure_units;
    int32_t contact_threshold_units;
    int32_t hold_enter_tolerance_units;
    int32_t hold_exit_tolerance_units;
    uint32_t pressure_freshness_ms;
    uint32_t settle_delay_ms;
    uint32_t settle_feedback_timeout_ms;
    uint32_t first_approach_command_mv;
    uint32_t first_approach_duration_ms;
    uint32_t first_approach_backstop_ms;
    uint32_t recontact_command_mv;
    uint32_t recontact_duration_ms;
    uint32_t recontact_backstop_ms;
    uint32_t pulse_command_mv;
    uint32_t pulse_duration_ms;
    uint32_t pulse_backstop_ms;
    uint32_t automatic_cycle_timeout_ms;
    bool bench_release_correction_enabled;
    bool raw_overpressure_enabled;
    uint32_t raw_overpressure_limit_counts;
} MachineConfig;

typedef struct
{
    uint64_t sequence;
    uint32_t received_at_ms;
    uint32_t raw_pressure_counts;
    uint8_t frame[PRESSURE_SENSOR_FRAME_LENGTH];
    int32_t control_pressure_units;
    bool frame_valid;
    bool control_units_valid;
} MachinePressureSample;

typedef enum
{
    FORCE_PI_INHIBIT_NONE = 0,
    FORCE_PI_INHIBIT_WAITING,
    FORCE_PI_INHIBIT_BUSY,
    FORCE_PI_INHIBIT_WRONG_DIRECTION,
    FORCE_PI_INHIBIT_RELEASE_DISABLED,
    FORCE_PI_INHIBIT_BELOW_ONE_MV
} MachineForcePiInhibitReason;

typedef struct
{
    MachineState state;
    MachineFault fault;
    FaultDetail fault_detail;
    MachineCommandResult last_command_result;
    MachineSettlePhase settle_phase;
    MachineApproachProfile approach_profile;
    MachineConfig config;
    MachinePressureSample pressure;
    int32_t target_pressure_units;
    uint64_t settle_gate_sequence;
    uint32_t motor_request_sequence;
    uint32_t state_entered_ms;
    uint32_t cycle_started_ms;
    bool target_valid;
    bool config_valid;
    /* AutoTarget gates are used only in the opt-in build. */
    uint32_t settle_feedback_after_ms;
    bool auto_has_contacted;
    bool auto_approach_pending;
    /* Last accepted request, retained through output-off for PC diagnostics.
     * Request data only: never measured motor terminal voltage. */
    uint32_t diagnostic_request_sequence;
    uint32_t diagnostic_request_at_ms;
    uint32_t diagnostic_command_mv;
    uint32_t diagnostic_duration_ms;
    uint16_t diagnostic_direction;
    /* Optional Locked/Host pulse strategy; MachineConfig layout is unchanged.
     * Snapshot is a calculation, requested_signed_mv is the accepted request
     * from THIS sample (zero means none). Inhibition records the suppressed
     * suggestion separately; these fields do not report physical voltage. */
    ForcePiConfig force_pi_config;
    ForcePiState force_pi_state;
    ForcePiSnapshot force_pi_snapshot;
    bool force_pi_enabled;
    bool force_pi_time_valid;
    uint32_t force_pi_received_at_ms;
    uint64_t force_pi_sample_sequence;
    MachineForcePiInhibitReason force_pi_inhibit_reason;
    float force_pi_blocked_suggestion_mv;
    int32_t force_pi_requested_signed_mv;
#if defined(SD700_AUTO_TARGET_ENABLED) && SD700_AUTO_TARGET_ENABLED
    /* Only normal independent PRESS completion can arm one settled comparison.
     * Cleared on HOLD/direction change/STOP/fault/start; not an integrator. */
    struct {
        uint32_t boost_mv;
        uint32_t low_response_count;
        uint32_t band;
        uint32_t request_sequence;
        uint32_t command_mv;
        uint32_t completed_at_ms;
        uint64_t before_sequence;
        int32_t before_units;
        int32_t observed_peak_units;
        bool in_flight;
        bool feedback_pending;
    } press_feedback;
#endif
} MachineContext;

/* Successful configuration explicitly enables the strategy and resets it.
 * RealCompileCheck/ScopeTest/ordinary RealBench return UNSUPPORTED, even in host
 * tests. Release-disabled machines use an effective output minimum of zero. */
MachineCommandResult Machine_ConfigureForcePi(MachineContext *context,
                                             const ForcePiConfig *config);
void Machine_Initialize(MachineContext *context,
                        const MachineConfig *config,
                        uint32_t now_ms);
void Machine_CompleteBoot(MachineContext *context,
                          bool boot_checks_passed,
                          uint32_t now_ms);
MachineCommandResult Machine_HandleCommand(MachineContext *context,
                                           const MachineCommand *command,
                                           uint32_t now_ms);
void Machine_HandlePressureSample(MachineContext *context,
                                  const MachinePressureSample *sample,
                                  uint32_t now_ms);
void Machine_CheckPressureSafety(MachineContext *context, uint32_t now_ms);
void Machine_HandleMotorService(MachineContext *context, uint32_t now_ms);
void Machine_ReportFault(MachineContext *context,
                         MachineFault fault,
                         FaultDetail detail,
                         uint32_t now_ms);
void Machine_Tick(MachineContext *context, uint32_t now_ms);
bool Machine_IsPressureFresh(const MachineContext *context, uint32_t now_ms);

#endif
