#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Application/bench_config.h"
#include "Application/direct_pulse_config.h"
#include "Application/runtime.h"
#include "Board/Motor/motor_executor.h"
#include "Board/Motor/motor_hw_real.h"
#include "Board/Motor/motor_stop_timer.h"
#include "Protocol/Modbus/modbus_crc16.h"
#include "Protocol/Modbus/modbus_protocol_constants.h"
#include "Protocol/Modbus/modbus_register_map.h"
#include "Tests/Host/fake_motor_hw_real.h"
#include "Tests/Host/fake_motor_stop_timer.h"
#include "Transport/Modbus/modbus_rtu_server.h"

#if !defined(SD700_MOTOR_MODE_SCOPE_TEST) && \
    !defined(SD700_MOTOR_MODE_REAL_BENCH)
#error "Full-chain direct-pulse test requires ScopeTest or RealBench mode"
#endif

typedef struct
{
    ApplicationRuntime runtime;
    ModbusRtuServer modbus;
} FullChainFixture;

typedef struct
{
    uint8_t bytes[MODBUS_RTU_RESPONSE_CAPACITY];
    size_t length;
} TestResponse;

static bool DirectPulseRequiresPressure(void)
{
#if defined(SD700_MOTOR_MODE_REAL_BENCH)
    return true;
#else
    return false;
#endif
}

static void BuildRequest(uint8_t *request,
                         uint8_t function,
                         uint16_t address,
                         uint16_t value)
{
    uint16_t crc;

    request[0] = MODBUS_LEGACY_DEFAULT_STATION_ID;
    request[1] = function;
    request[2] = (uint8_t)(address >> 8U);
    request[3] = (uint8_t)address;
    request[4] = (uint8_t)(value >> 8U);
    request[5] = (uint8_t)value;
    crc = Modbus_Crc16(request, 6U);
    request[6] = (uint8_t)crc;
    request[7] = (uint8_t)(crc >> 8U);
}

static TestResponse ProcessNormalRequest(FullChainFixture *fixture,
                                         const uint8_t *request,
                                         uint32_t now_ms)
{
    const uint8_t *response;
    TestResponse copy = {{0}, 0U};

    ModbusRtuServer_ProcessBytes(&fixture->modbus,
                                 request,
                                 MODBUS_RTU_REQUEST_LENGTH);
    assert(!ModbusRtuServer_PendingIsStop(&fixture->modbus));
    assert(ModbusRtuServer_ProcessPending(&fixture->modbus, now_ms));
    response = ModbusRtuServer_GetResponse(&fixture->modbus,
                                           &copy.length);
    assert(response != NULL);
    assert(copy.length <= sizeof(copy.bytes));
    (void)memcpy(copy.bytes, response, copy.length);
    ModbusRtuServer_CompleteResponse(&fixture->modbus);
    return copy;
}

static TestResponse ProcessPriorityStop(FullChainFixture *fixture,
                                        const uint8_t *request,
                                        uint32_t now_ms)
{
    const uint8_t *response;
    TestResponse copy = {{0}, 0U};

    ModbusRtuServer_ProcessBytes(&fixture->modbus,
                                 request,
                                 MODBUS_RTU_REQUEST_LENGTH);
    assert(ModbusRtuServer_PendingIsStop(&fixture->modbus));
    assert(ModbusRtuServer_TakeStop(&fixture->modbus, now_ms));
    response = ModbusRtuServer_GetResponse(&fixture->modbus,
                                           &copy.length);
    assert(response != NULL);
    assert(copy.length <= sizeof(copy.bytes));
    (void)memcpy(copy.bytes, response, copy.length);
    ModbusRtuServer_CompleteResponse(&fixture->modbus);
    return copy;
}

static void AssertWriteEcho(const TestResponse *response,
                            const uint8_t *request)
{
    assert(response->length == MODBUS_RTU_REQUEST_LENGTH);
    assert(memcmp(response->bytes,
                  request,
                  MODBUS_RTU_REQUEST_LENGTH) == 0);
}

static void AssertException(const TestResponse *response,
                            uint8_t function,
                            uint8_t exception)
{
    uint16_t crc;

    assert(response->length == 5U);
    assert(response->bytes[0] == MODBUS_LEGACY_DEFAULT_STATION_ID);
    assert(response->bytes[1] == (uint8_t)(function | 0x80U));
    assert(response->bytes[2] == exception);
    crc = Modbus_Crc16(response->bytes, 3U);
    assert(response->bytes[3] == (uint8_t)crc);
    assert(response->bytes[4] == (uint8_t)(crc >> 8U));
}

static void FeedPressure(FullChainFixture *fixture,
                         uint16_t raw_counts,
                         uint64_t sequence,
                         uint32_t now_ms)
{
    PressureReceiverSnapshot snapshot = {
        .raw_pressure_counts = raw_counts,
        .sample_sequence = sequence,
        .received_at_ms = now_ms
    };

    assert(ApplicationRuntime_ServicePressure(&fixture->runtime,
                                              &snapshot,
                                              now_ms));
}

static void StartFixture(FullChainFixture *fixture,
                         bool provide_fresh_pressure)
{
    FakeMotorHwReal_Reset();
    FakeMotorStopTimer_Reset();
    assert(MotorExecutor_Initialize() == MOTOR_RESULT_OK);
    ApplicationRuntime_Initialize(&fixture->runtime,
                                  &g_sd700_bench_machine_config,
                                  0U);
    ApplicationRuntime_CompleteBoot(&fixture->runtime, true, 1U);
    assert(fixture->runtime.machine.state == IDLE);
    assert(MotorExecutor_OutputIsDisabled());
    ModbusRtuServer_Initialize(&fixture->modbus,
                               &fixture->runtime.machine);
    if (provide_fresh_pressure)
    {
        FeedPressure(fixture, 50U, 1U, 2U);
    }
}

static TestResponse WriteSingle(FullChainFixture *fixture,
                                uint8_t function,
                                uint16_t address,
                                uint16_t value,
                                uint32_t now_ms)
{
    uint8_t request[MODBUS_RTU_REQUEST_LENGTH];

    BuildRequest(request, function, address, value);
    return ProcessNormalRequest(fixture, request, now_ms);
}

static uint16_t ReadInput(FullChainFixture *fixture,
                          uint16_t address,
                          uint32_t now_ms)
{
    uint8_t request[MODBUS_RTU_REQUEST_LENGTH];
    TestResponse response;

    BuildRequest(request,
                 MODBUS_FUNCTION_READ_INPUT_REGISTERS,
                 address,
                 1U);
    response = ProcessNormalRequest(fixture, request, now_ms);
    assert(response.length == 7U);
    assert(response.bytes[0] == MODBUS_LEGACY_DEFAULT_STATION_ID);
    assert(response.bytes[1] == MODBUS_FUNCTION_READ_INPUT_REGISTERS);
    assert(response.bytes[2] == 2U);
    return (uint16_t)(((uint16_t)response.bytes[3] << 8U) |
                      response.bytes[4]);
}

static void StartDirectPulse(FullChainFixture *fixture,
                             uint16_t coil,
                             uint32_t now_ms)
{
    TestResponse response = WriteSingle(
        fixture,
        MODBUS_FUNCTION_WRITE_SINGLE_COIL,
        coil,
        MODBUS_SINGLE_COIL_ON,
        now_ms);

    assert(response.length == MODBUS_RTU_REQUEST_LENGTH);
    assert((fixture->runtime.machine.state == DIRECT_PRESS_PULSE) ||
           (fixture->runtime.machine.state == DIRECT_RELEASE_PULSE));
    assert(MotorExecutor_ActiveRequestIsValid());
    assert(!MotorHwReal_IsDisabled());
}

static void TestDocumentedRtuFrames(void)
{
    static const uint8_t release_expected[8] = {
        0x01U, 0x05U, 0x00U, 0x02U,
        0xFFU, 0x00U, 0x2DU, 0xFAU
    };
    static const uint8_t press_expected[8] = {
        0x01U, 0x05U, 0x00U, 0x03U,
        0xFFU, 0x00U, 0x7CU, 0x3AU
    };
    static const uint8_t stop_expected[8] = {
        0x01U, 0x05U, 0x00U, 0x01U,
        0x00U, 0x00U, 0x9CU, 0x0AU
    };
    uint8_t request[8];

    BuildRequest(request,
                 MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                 MODBUS_COIL_DIRECT_RELEASE_PULSE,
                 MODBUS_SINGLE_COIL_ON);
    assert(memcmp(request, release_expected, sizeof(request)) == 0);
    BuildRequest(request,
                 MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                 MODBUS_COIL_DIRECT_PRESS_PULSE,
                 MODBUS_SINGLE_COIL_ON);
    assert(memcmp(request, press_expected, sizeof(request)) == 0);
    BuildRequest(request,
                 MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                 MODBUS_COIL_AUTO_PRESSURE,
                 MODBUS_SINGLE_COIL_OFF);
    assert(memcmp(request, stop_expected, sizeof(request)) == 0);
}

static void TestDirectionAndFixedPlan(void)
{
    FullChainFixture fixture;
    const FakeMotorHwRealState *hw;
    const FakeMotorStopTimerState *timer;
    const MotorExecutorSnapshot *motor;

    StartFixture(&fixture, DirectPulseRequiresPressure());
    StartDirectPulse(&fixture, MODBUS_COIL_DIRECT_PRESS_PULSE, 3U);
    hw = FakeMotorHwReal_GetState();
    timer = FakeMotorStopTimer_GetState();
    motor = MotorExecutor_GetSnapshot();
    assert(fixture.runtime.machine.state == DIRECT_PRESS_PULSE);
    assert(motor->direction == MOTOR_DIRECTION_PRESS);
    assert(motor->command_mv == SD700_DIRECT_PULSE_COMMAND_MV);
    assert(motor->requested_duration_ms == SD700_DIRECT_PULSE_DURATION_MS);
    assert(timer->normal_duration_ms == SD700_DIRECT_PULSE_DURATION_MS);
    assert(timer->backstop_ms == SD700_DIRECT_PULSE_BACKSTOP_MS);
    assert(hw->tim3_enabled && hw->tim2_enabled);
    assert(hw->shutdown_enabled && hw->driver_enabled);
    assert(hw->tim3_ccr3 == motor->planned_tim3_ccr3);
    assert(hw->tim2_ccr3 == 0U);
    assert(!hw->both_legs_active_violation);

    StartFixture(&fixture, DirectPulseRequiresPressure());
    StartDirectPulse(&fixture, MODBUS_COIL_DIRECT_RELEASE_PULSE, 3U);
    hw = FakeMotorHwReal_GetState();
    timer = FakeMotorStopTimer_GetState();
    motor = MotorExecutor_GetSnapshot();
    assert(fixture.runtime.machine.state == DIRECT_RELEASE_PULSE);
    assert(motor->direction == MOTOR_DIRECTION_RELEASE);
    assert(motor->command_mv == SD700_DIRECT_PULSE_COMMAND_MV);
    assert(motor->requested_duration_ms == SD700_DIRECT_PULSE_DURATION_MS);
    assert(timer->normal_duration_ms == SD700_DIRECT_PULSE_DURATION_MS);
    assert(timer->backstop_ms == SD700_DIRECT_PULSE_BACKSTOP_MS);
    assert(hw->tim2_enabled && hw->tim3_enabled);
    assert(hw->shutdown_enabled && hw->driver_enabled);
    assert(hw->tim2_ccr3 == motor->planned_tim2_ccr3);
    assert(hw->tim3_ccr3 == 0U);
    assert(!hw->both_legs_active_violation);
}

static void TestNormalCompletion(void)
{
    FullChainFixture fixture;
    const MotorExecutorSnapshot *motor;
    const uint32_t now_ms = 3U + SD700_DIRECT_PULSE_DURATION_MS;

    StartFixture(&fixture, DirectPulseRequiresPressure());
    StartDirectPulse(&fixture, MODBUS_COIL_DIRECT_PRESS_PULSE, 3U);
    FakeMotorStopTimer_TriggerNormal();
    motor = MotorExecutor_GetSnapshot();
    assert(MotorHwReal_IsDisabled());
    assert(!MotorStopTimer_IsArmed());
    assert(FakeMotorStopTimer_GetState()->
           expiry_cancel_saw_output_disabled);
    assert(motor->logical_active);
    assert(motor->last_completion == MOTOR_COMPLETION_NONE);

    ApplicationRuntime_ServiceSafety(&fixture.runtime, now_ms);
    motor = MotorExecutor_GetSnapshot();
    assert(!motor->logical_active);
    assert(motor->last_completion == MOTOR_COMPLETION_NORMAL);
    assert(fixture.runtime.machine.state == IDLE);
    assert(ReadInput(&fixture,
                     MODBUS_INPUT_MACHINE_STATE,
                     now_ms) == (uint16_t)IDLE);
}

static void TestCompletionDuringMachineValidation(uint16_t coil)
{
    FullChainFixture fixture;
    const MotorExecutorSnapshot *motor;
    const uint32_t start_ms = 3U;
    const uint32_t now_ms = start_ms + SD700_DIRECT_PULSE_DURATION_MS;
    uint32_t request_sequence;
    uint32_t apply_count;
    uint32_t arm_count;
    uint32_t armed_query_count;

    StartFixture(&fixture, true);
    StartDirectPulse(&fixture, coil, start_ms);
    request_sequence = MotorExecutor_GetSnapshot()->request_sequence;
    apply_count = FakeMotorHwReal_GetState()->apply_count;
    arm_count = FakeMotorStopTimer_GetState()->arm_count;
    assert(now_ms < start_ms + SD700_DIRECT_PULSE_BACKSTOP_MS);
    assert(now_ms < 2U + g_sd700_bench_machine_config.pressure_freshness_ms);
    assert(Machine_IsPressureFresh(&fixture.runtime.machine, now_ms));

    assert(MotorExecutor_Service(now_ms) == MOTOR_RESULT_OK);
    FakeMotorStopTimer_TriggerNormalOnNextArmedQuery();
    Machine_HandleMotorService(&fixture.runtime.machine, now_ms);

    /* Diagnostic remains visible in the before-fix assertion failure log. */
    printf("MACHINE_VALIDATION_RACE coil=%u fault=%u detail=%u\n",
           (unsigned)coil, (unsigned)fixture.runtime.machine.fault,
           (unsigned)fixture.runtime.machine.fault_detail);
    (void)fflush(stdout);
    assert(fixture.runtime.machine.fault == FAULT_NONE);
    assert(fixture.runtime.machine.fault_detail == FAULT_DETAIL_NONE);
    assert(fixture.runtime.machine.state ==
           ((coil == MODBUS_COIL_DIRECT_PRESS_PULSE) ?
            DIRECT_PRESS_PULSE : DIRECT_RELEASE_PULSE));
    assert(MotorHwReal_IsDisabled());
    assert(!MotorStopTimer_IsArmed());
    assert(FakeMotorStopTimer_GetState()->expiry_count == 1U);
    motor = MotorExecutor_GetSnapshot();
    assert(motor->logical_active);
    assert(motor->last_completion == MOTOR_COMPLETION_NONE);
    assert(motor->last_failure_result == MOTOR_RESULT_OK);
    assert(motor->request_sequence == request_sequence);

    ApplicationRuntime_ServiceSafety(&fixture.runtime, now_ms);
    assert(fixture.runtime.machine.state == IDLE);
    assert(fixture.runtime.machine.fault == FAULT_NONE);
    assert(fixture.runtime.machine.fault_detail == FAULT_DETAIL_NONE);
    assert(!motor->logical_active);
    assert(motor->last_completion == MOTOR_COMPLETION_NORMAL);
    assert(fixture.runtime.machine.motor_request_sequence == 0U);
    assert(fixture.runtime.machine.state_entered_ms == now_ms);

    armed_query_count = FakeMotorStopTimer_GetState()->armed_query_count;
    ApplicationRuntime_ServiceSafety(&fixture.runtime, now_ms + 1U);
    /* Service must take its inactive validation path. A duplicated pending
       completion would return through PublishCompletion without this query. */
    assert(FakeMotorStopTimer_GetState()->armed_query_count ==
           armed_query_count + 1U);
    assert(fixture.runtime.machine.state == IDLE);
    assert(fixture.runtime.machine.fault == FAULT_NONE);
    assert(fixture.runtime.machine.state_entered_ms == now_ms);
    assert(fixture.runtime.machine.motor_request_sequence == 0U);
    assert(!motor->logical_active);
    assert(motor->last_completion == MOTOR_COMPLETION_NORMAL);
    assert(motor->request_sequence == request_sequence);
    assert(FakeMotorHwReal_GetState()->apply_count == apply_count);
    assert(FakeMotorStopTimer_GetState()->arm_count == arm_count);
    assert(FakeMotorStopTimer_GetState()->expiry_count == 1U);
    assert(MotorHwReal_IsDisabled());
    assert(!MotorStopTimer_IsArmed());
}

static void TestFaultCompletionDuringMachineValidation(uint16_t coil,
                                                       bool timer_error)
{
    FullChainFixture fixture;
    const uint32_t now_ms = 3U + SD700_DIRECT_PULSE_DURATION_MS;
    const MotorExecutorSnapshot *motor;

    StartFixture(&fixture, true);
    StartDirectPulse(&fixture, coil, 3U);
    assert(Machine_IsPressureFresh(&fixture.runtime.machine, now_ms));
    assert(MotorExecutor_Service(now_ms) == MOTOR_RESULT_OK);
    /* Inject classification independently of real interrupt timing. */
    FakeMotorStopTimer_TriggerOnNextArmedQuery(
        timer_error ? MOTOR_STOP_TIMER_ERROR : MOTOR_STOP_TIMER_BACKSTOP);
    Machine_HandleMotorService(&fixture.runtime.machine, now_ms);
    assert(fixture.runtime.machine.fault == FAULT_NONE);
    motor = MotorExecutor_GetSnapshot();
    assert(motor->logical_active);
    assert(motor->last_completion == MOTOR_COMPLETION_NONE);
    assert(MotorHwReal_IsDisabled());
    assert(!MotorStopTimer_IsArmed());

    ApplicationRuntime_ServiceSafety(&fixture.runtime, now_ms);
    assert(fixture.runtime.machine.state == FAULT);
    assert(fixture.runtime.machine.fault ==
           (timer_error ? FAULT_MOTOR_FAULT : FAULT_MOTION_TIMEOUT));
    assert(fixture.runtime.machine.fault_detail ==
           (timer_error ? FAULT_DETAIL_MOTOR_HARDWARE :
                          FAULT_DETAIL_DIRECT_PULSE_TIMEOUT));
    assert(motor->last_completion ==
           (timer_error ? MOTOR_COMPLETION_ERROR : MOTOR_COMPLETION_BACKSTOP));
    if (timer_error)
    {
        assert(motor->last_failure_result == MOTOR_RESULT_TIMER_ERROR);
        assert(motor->last_failure_stage ==
               MOTOR_FAILURE_STAGE_TIMER_IRQ_COMPARE_OVERCAPTURE);
    }
    assert(MotorHwReal_IsDisabled());
    assert(!MotorStopTimer_IsArmed());
}

static MotorStopTimerEvent s_runtime_injected_event;

static void TriggerRuntimeCompletion(void)
{
    if (s_runtime_injected_event == MOTOR_STOP_TIMER_NORMAL)
    {
        FakeMotorStopTimer_TriggerNormal();
    }
    else if (s_runtime_injected_event == MOTOR_STOP_TIMER_BACKSTOP)
    {
        FakeMotorStopTimer_TriggerBackstop();
    }
    else
    {
        FakeMotorStopTimer_TriggerError();
    }
}

static void TestCompletionDuringRuntimePhase(uint16_t coil,
                                             MotorStopTimerEvent event,
                                             bool guard,
                                             bool physical_query)
{
    FullChainFixture fixture;
    const MotorExecutorSnapshot *motor;
    const uint32_t now_ms = 3U + SD700_DIRECT_PULSE_DURATION_MS;
    const MotorCompletion expected = (event == MOTOR_STOP_TIMER_NORMAL) ?
        MOTOR_COMPLETION_NORMAL : ((event == MOTOR_STOP_TIMER_BACKSTOP) ?
            MOTOR_COMPLETION_BACKSTOP : MOTOR_COMPLETION_ERROR);
    uint32_t armed_queries;

    StartFixture(&fixture, true);
    StartDirectPulse(&fixture, coil, 3U);
    motor = MotorExecutor_GetSnapshot();
    assert(Machine_IsPressureFresh(&fixture.runtime.machine, now_ms));
    assert(now_ms < motor->logical_backstop_ms);
    assert(MotorExecutor_Service(now_ms) == MOTOR_RESULT_OK);
    s_runtime_injected_event = event;
    if (physical_query)
    {
        FakeMotorHwReal_OnDisabledQuery(1U, TriggerRuntimeCompletion);
    }
    else
    {
        if (guard)
        {
            /* Guard must reinforce a pending handoff with a failed cancel. */
            FakeMotorStopTimer_FailNextCancelVerification();
        }
        FakeMotorStopTimer_TriggerOnNextArmedQuery(event);
    }
    if (guard)
    {
        assert(MotorExecutor_GuardOutput() == MOTOR_RESULT_OK);
        assert(motor->logical_active);
        assert(motor->last_completion == MOTOR_COMPLETION_NONE);
        assert(fixture.runtime.machine.fault == FAULT_NONE);
        assert(MotorHwReal_IsDisabled());
        assert(!MotorStopTimer_IsArmed());
    }
    ApplicationRuntime_ServiceSafety(&fixture.runtime, now_ms);
    assert(motor->last_completion == expected);
    assert(!motor->logical_active);
    assert(MotorHwReal_IsDisabled());
    assert(!MotorStopTimer_IsArmed());
    if (event == MOTOR_STOP_TIMER_NORMAL)
    {
        assert(fixture.runtime.machine.state == IDLE);
        assert(fixture.runtime.machine.fault == FAULT_NONE);
        assert(fixture.runtime.machine.fault_detail == FAULT_DETAIL_NONE);
        armed_queries = FakeMotorStopTimer_GetState()->armed_query_count;
        ApplicationRuntime_ServiceSafety(&fixture.runtime, now_ms + 1U);
        assert(FakeMotorStopTimer_GetState()->armed_query_count == armed_queries + 1U);
        assert(fixture.runtime.machine.state_entered_ms == now_ms);
        assert(fixture.runtime.machine.state == IDLE);
        assert(motor->last_completion == MOTOR_COMPLETION_NORMAL);
    }
    else
    {
        assert(fixture.runtime.machine.state == FAULT);
        assert(fixture.runtime.machine.fault ==
               ((event == MOTOR_STOP_TIMER_BACKSTOP) ? FAULT_MOTION_TIMEOUT : FAULT_MOTOR_FAULT));
        assert(fixture.runtime.machine.fault_detail ==
               ((event == MOTOR_STOP_TIMER_BACKSTOP) ? FAULT_DETAIL_DIRECT_PULSE_TIMEOUT : FAULT_DETAIL_MOTOR_HARDWARE));
    }
    assert(motor->request_sequence == 1U);
    assert(FakeMotorHwReal_GetState()->apply_count == 1U);
    assert(FakeMotorStopTimer_GetState()->arm_count == 1U);
    assert(FakeMotorStopTimer_GetState()->expiry_count == 1U);
}

static void TestRuntimeCompletionRaces(void)
{
    static const uint16_t coils[] = {
        MODBUS_COIL_DIRECT_PRESS_PULSE, MODBUS_COIL_DIRECT_RELEASE_PULSE
    };
    static const MotorStopTimerEvent events[] = {
        MOTOR_STOP_TIMER_NORMAL, MOTOR_STOP_TIMER_BACKSTOP, MOTOR_STOP_TIMER_ERROR
    };
    unsigned direction;
    unsigned event;
    for (direction = 0U; direction < 2U; ++direction)
    {
        for (event = 0U; event < 3U; ++event)
        {
            TestCompletionDuringRuntimePhase(coils[direction], events[event], false, false);
            TestCompletionDuringRuntimePhase(coils[direction], events[event], false, true);
            TestCompletionDuringRuntimePhase(coils[direction], events[event], true, false);
            TestCompletionDuringRuntimePhase(coils[direction], events[event], true, true);
        }
    }
    puts("Full-chain Service/Guard races (PRESS/RELEASE, all events): PASS");
}

static void TestPriorityStopDuringMotion(void)
{
    FullChainFixture fixture;
    uint8_t request[MODBUS_RTU_REQUEST_LENGTH];
    TestResponse response;

    StartFixture(&fixture, DirectPulseRequiresPressure());
    StartDirectPulse(&fixture, MODBUS_COIL_DIRECT_RELEASE_PULSE, 3U);
    BuildRequest(request,
                 MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                 MODBUS_COIL_AUTO_PRESSURE,
                 MODBUS_SINGLE_COIL_OFF);
    assert(!MotorHwReal_IsDisabled());
    response = ProcessPriorityStop(&fixture, request, 4U);
    AssertWriteEcho(&response, request);
    assert(MotorHwReal_IsDisabled());
    assert(!MotorStopTimer_IsArmed());
    assert(fixture.runtime.machine.state == IDLE);
}

static void TestBackstopFault(void)
{
    FullChainFixture fixture;
    const uint32_t now_ms = 3U + SD700_DIRECT_PULSE_BACKSTOP_MS;

    StartFixture(&fixture, DirectPulseRequiresPressure());
    StartDirectPulse(&fixture, MODBUS_COIL_DIRECT_PRESS_PULSE, 3U);
    FakeMotorStopTimer_TriggerBackstop();
    assert(MotorHwReal_IsDisabled());
    ApplicationRuntime_ServiceSafety(&fixture.runtime, now_ms);
    assert(fixture.runtime.machine.state == FAULT);
    assert(fixture.runtime.machine.fault == FAULT_MOTION_TIMEOUT);
    assert(fixture.runtime.machine.fault_detail ==
           FAULT_DETAIL_DIRECT_PULSE_TIMEOUT);
    assert(ReadInput(&fixture,
                     MODBUS_INPUT_MACHINE_FAULT,
                     now_ms) == (uint16_t)FAULT_MOTION_TIMEOUT);
    assert(ReadInput(&fixture,
                     MODBUS_INPUT_FAULT_DETAIL,
                     now_ms) ==
           (uint16_t)FAULT_DETAIL_DIRECT_PULSE_TIMEOUT);
}

static void TestTimerErrorDiagnostics(void)
{
    FullChainFixture fixture;

    StartFixture(&fixture, DirectPulseRequiresPressure());
    StartDirectPulse(&fixture, MODBUS_COIL_DIRECT_PRESS_PULSE, 3U);
    FakeMotorStopTimer_TriggerError();
    assert(MotorHwReal_IsDisabled());
    ApplicationRuntime_ServiceSafety(&fixture.runtime, 4U);
    assert(fixture.runtime.machine.state == FAULT);
    assert(fixture.runtime.machine.fault == FAULT_MOTOR_FAULT);
    assert(fixture.runtime.machine.fault_detail != FAULT_DETAIL_NONE);
    assert(ReadInput(&fixture,
                     MODBUS_INPUT_LAST_MOTOR_FAILURE_RESULT,
                     4U) == (uint16_t)MOTOR_RESULT_TIMER_ERROR);
    assert(ReadInput(&fixture,
                     MODBUS_INPUT_LAST_MOTOR_FAILURE_STAGE,
                     4U) ==
           (uint16_t)MOTOR_FAILURE_STAGE_TIMER_IRQ_COMPARE_OVERCAPTURE);
}

static void TestReleaseHardwareFailureDiagnostics(void)
{
    FullChainFixture fixture;
    TestResponse response;

    StartFixture(&fixture, DirectPulseRequiresPressure());
    FakeMotorHwReal_FailNextApply();
    response = WriteSingle(&fixture,
                           MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                           MODBUS_COIL_DIRECT_RELEASE_PULSE,
                           MODBUS_SINGLE_COIL_ON,
                           3U);
    AssertException(&response,
                    MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                    MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE);
    assert(MotorHwReal_IsDisabled());
    assert(fixture.runtime.machine.state == FAULT);
    assert(fixture.runtime.machine.fault == FAULT_MOTOR_FAULT);
    assert(fixture.runtime.machine.fault_detail != FAULT_DETAIL_NONE);
    assert(ReadInput(&fixture,
                     MODBUS_INPUT_LAST_MOTOR_FAILURE_RESULT,
                     3U) == (uint16_t)MOTOR_RESULT_HARDWARE_ERROR);
    assert(ReadInput(&fixture,
                     MODBUS_INPUT_LAST_MOTOR_FAILURE_STAGE,
                     3U) ==
           (uint16_t)MOTOR_FAILURE_STAGE_HW_APPLY_PWM_START);
}

static void TestDirectOffIsInvalidAndNotStop(void)
{
    FullChainFixture fixture;
    TestResponse response;

    StartFixture(&fixture, DirectPulseRequiresPressure());
    StartDirectPulse(&fixture, MODBUS_COIL_DIRECT_PRESS_PULSE, 3U);
    response = WriteSingle(&fixture,
                           MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                           MODBUS_COIL_DIRECT_RELEASE_PULSE,
                           MODBUS_SINGLE_COIL_OFF,
                           4U);
    AssertException(&response,
                    MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                    MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    assert(fixture.runtime.machine.state == DIRECT_PRESS_PULSE);
    assert(!MotorHwReal_IsDisabled());

    response = WriteSingle(&fixture,
                           MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                           MODBUS_COIL_DIRECT_PRESS_PULSE,
                           MODBUS_SINGLE_COIL_OFF,
                           5U);
    AssertException(&response,
                    MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                    MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    assert(fixture.runtime.machine.state == DIRECT_PRESS_PULSE);
    assert(!MotorHwReal_IsDisabled());
    assert(MotorExecutor_Disable() == MOTOR_RESULT_OK);
}

static void TestAutoStartRejectedAfterTarget(void)
{
    FullChainFixture fixture;
    TestResponse response;
    const FakeMotorHwRealState *hw;

    StartFixture(&fixture, DirectPulseRequiresPressure());
    response = WriteSingle(&fixture,
                           MODBUS_FUNCTION_WRITE_SINGLE_REGISTER,
                           MODBUS_HOLDING_TARGET_PRESSURE,
                           500U,
                           3U);
    assert(response.length == MODBUS_RTU_REQUEST_LENGTH);
    assert(fixture.runtime.machine.target_valid);
    response = WriteSingle(&fixture,
                           MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                           MODBUS_COIL_AUTO_PRESSURE,
                           MODBUS_SINGLE_COIL_ON,
                           4U);
    AssertException(&response,
                    MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
    hw = FakeMotorHwReal_GetState();
    assert(hw->apply_count == 0U);
    assert(MotorExecutor_GetSnapshot()->request_sequence == 0U);
    assert(fixture.runtime.machine.state == IDLE);
}

#if defined(SD700_MOTOR_MODE_REAL_BENCH)
static void TestRealBenchPressureAdmissionAndSafety(void)
{
    FullChainFixture fixture;
    TestResponse response;
    MachinePressureSample invalid_sample = {
        .sequence = 2U,
        .received_at_ms = 4U,
        .raw_pressure_counts = 0U,
        .control_pressure_units = 0,
        .frame_valid = false,
        .control_units_valid = false
    };

    StartFixture(&fixture, false);
    response = WriteSingle(&fixture,
                           MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                           MODBUS_COIL_DIRECT_PRESS_PULSE,
                           MODBUS_SINGLE_COIL_ON,
                           3U);
    AssertException(&response,
                    MODBUS_FUNCTION_WRITE_SINGLE_COIL,
                    MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE);
    assert(FakeMotorHwReal_GetState()->apply_count == 0U);
    assert(MotorExecutor_GetSnapshot()->request_sequence == 0U);
    assert(fixture.runtime.machine.state == IDLE);

    StartFixture(&fixture, true);
    StartDirectPulse(&fixture, MODBUS_COIL_DIRECT_PRESS_PULSE, 3U);
    ApplicationRuntime_ServiceSafety(
        &fixture.runtime,
        2U + g_sd700_bench_machine_config.pressure_freshness_ms + 1U);
    assert(fixture.runtime.machine.state == FAULT);
    assert(fixture.runtime.machine.fault ==
           FAULT_PRESSURE_SENSOR_FAULT);
    assert(fixture.runtime.machine.fault_detail ==
           FAULT_DETAIL_PRESSURE_TIMEOUT);
    assert(MotorHwReal_IsDisabled());

    StartFixture(&fixture, true);
    StartDirectPulse(&fixture, MODBUS_COIL_DIRECT_RELEASE_PULSE, 3U);
    Machine_HandlePressureSample(&fixture.runtime.machine,
                                 &invalid_sample,
                                 4U);
    assert(fixture.runtime.machine.state == FAULT);
    assert(fixture.runtime.machine.fault ==
           FAULT_PRESSURE_SENSOR_FAULT);
    assert(fixture.runtime.machine.fault_detail ==
           FAULT_DETAIL_PRESSURE_INVALID);
    assert(MotorHwReal_IsDisabled());
}
#endif

int main(int argc, char **argv)
{
    /* Separate processes prove both directions fail on the original guard. */
    if (argc == 2)
    {
        if (strcmp(argv[1], "race-press") == 0)
        {
            TestCompletionDuringMachineValidation(MODBUS_COIL_DIRECT_PRESS_PULSE);
        }
        else if (strcmp(argv[1], "race-release") == 0)
        {
            TestCompletionDuringMachineValidation(MODBUS_COIL_DIRECT_RELEASE_PULSE);
        }
        else
        {
            return 2;
        }
        puts("Machine active-request completion race: PASS");
        return 0;
    }
    assert(argc == 1);
    TestDocumentedRtuFrames();
    TestDirectionAndFixedPlan();
    TestNormalCompletion();
    TestRuntimeCompletionRaces();
    TestCompletionDuringMachineValidation(MODBUS_COIL_DIRECT_PRESS_PULSE);
    TestCompletionDuringMachineValidation(MODBUS_COIL_DIRECT_RELEASE_PULSE);
    TestFaultCompletionDuringMachineValidation(MODBUS_COIL_DIRECT_PRESS_PULSE, false);
    TestFaultCompletionDuringMachineValidation(MODBUS_COIL_DIRECT_RELEASE_PULSE, false);
    TestFaultCompletionDuringMachineValidation(MODBUS_COIL_DIRECT_PRESS_PULSE, true);
    TestFaultCompletionDuringMachineValidation(MODBUS_COIL_DIRECT_RELEASE_PULSE, true);
    TestPriorityStopDuringMotion();
    TestBackstopFault();
    TestTimerErrorDiagnostics();
    TestReleaseHardwareFailureDiagnostics();
    TestDirectOffIsInvalidAndNotStop();
    TestAutoStartRejectedAfterTarget();
#if defined(SD700_MOTOR_MODE_REAL_BENCH)
    TestRealBenchPressureAdmissionAndSafety();
    puts("Full-chain direct-pulse RealBench host test: PASS");
#else
    puts("Full-chain direct-pulse ScopeTest host test: PASS");
#endif
    return 0;
}
