/* SYNTHETIC_INPUT: deterministic software stimuli, NOT a mechanical model,
 * measured response, pressure calibration, or PID tuning result. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "Application/auto_target_config.h"
#include "Application/bench_config.h"
#include "Application/runtime.h"
#include "Board/Motor/motor_executor.h"
#include "Tests/Host/fake_motor_hw_real.h"
#include "Tests/Host/fake_motor_stop_timer.h"
#include "Protocol/Modbus/modbus_crc16.h"
#include "Protocol/Modbus/modbus_protocol_constants.h"
#include "Protocol/Modbus/modbus_register_map.h"
#include "Transport/Modbus/modbus_rtu_server.h"
#include "Transport/Modbus/modbus_semantic_map.h"

static ApplicationRuntime r;
static ModbusRtuServer server;
static uint64_t seq;

static void Service(uint32_t now)
{
    ApplicationRuntime_ServiceSafety(&r, now);
    assert(MotorExecutor_GuardOutput() == MOTOR_RESULT_OK);
    ApplicationRuntime_Tick(&r, now);
}

static bool FeedAt(uint16_t pressure, uint64_t sequence, uint32_t received, uint32_t now)
{
    PressureReceiverSnapshot sample = {0};
    sample.sample_sequence = sequence;
    sample.received_at_ms = received;
    sample.raw_pressure_counts = pressure;
    return ApplicationRuntime_ServicePressure(&r, &sample, now);
}

static void Feed(uint16_t pressure, uint32_t now)
{
    assert(FeedAt(pressure, ++seq, now, now));
}

static void Request(uint8_t function, uint16_t address, uint16_t value,
                    uint32_t now, bool stop)
{
    uint8_t request[8] = {1U, function, (uint8_t)(address >> 8U), (uint8_t)address,
                         (uint8_t)(value >> 8U), (uint8_t)value, 0U, 0U};
    uint16_t crc = Modbus_Crc16(request, 6U);
    size_t length;
    const uint8_t *response;
    request[6] = (uint8_t)crc;
    request[7] = (uint8_t)(crc >> 8U);
    ModbusRtuServer_ProcessBytes(&server, request, sizeof(request));
    if (stop) { assert(ModbusRtuServer_TakeStop(&server, now)); }
    else { assert(ModbusRtuServer_ProcessPending(&server, now)); }
    response = ModbusRtuServer_GetResponse(&server, &length);
    assert(response != NULL && length == 8U);
    assert(memcmp(request, response, 8U) == 0);
    ModbusRtuServer_CompleteResponse(&server);
}

static void Init(uint16_t pressure, uint32_t now)
{
    seq = 0U;
    FakeMotorHwReal_Reset();
    FakeMotorStopTimer_Reset();
    assert(MotorExecutor_Initialize() == MOTOR_RESULT_OK);
    ApplicationRuntime_Initialize(&r, &g_sd700_auto_target_machine_config, now);
    assert(r.machine.state == BOOT_SAFE && !r.machine.force_pi_enabled);
    ApplicationRuntime_CompleteBoot(&r, true, now);
    assert(r.machine.state == IDLE && r.machine.force_pi_enabled);
    assert(MotorExecutor_OutputIsDisabled());
    assert(FakeMotorHwReal_GetState()->apply_count == 0U);
    ModbusRtuServer_Initialize(&server, &r.machine);
    Feed(pressure, now);
    Request(6U, MODBUS_HOLDING_TARGET_PRESSURE, 250U, now, false);
}

static void Start(uint32_t now)
{
    Request(5U, MODBUS_COIL_AUTO_PRESSURE, 0xFF00U, now, false);
}

static void Stop(uint32_t now)
{
    Request(5U, MODBUS_COIL_AUTO_PRESSURE, 0U, now, true);
    assert(MotorExecutor_OutputIsDisabled());
}

static void Complete(uint32_t now)
{
    FakeMotorStopTimer_TriggerNormal();
    assert(MotorExecutor_OutputIsDisabled());
    Service(now);
    assert(r.machine.state == AUTO_SETTLE);
}

static void Settled(uint16_t pressure, uint32_t now)
{
    Service(now);
    assert(r.machine.settle_phase == SETTLE_WAIT_SAMPLE);
    Feed(pressure, now + 1U);
}

static void AssertPulse(MotorDirection direction, uint32_t mv)
{
    const MotorExecutorSnapshot *m = MotorExecutor_GetSnapshot();
    assert(r.machine.state == AUTO_PULSE && m->logical_active);
    assert(m->direction == direction && m->command_mv == mv);
    assert(m->requested_duration_ms == AUTO_TARGET_PULSE_DURATION_MS);
    assert(FakeMotorStopTimer_GetState()->normal_duration_ms == AUTO_TARGET_PULSE_DURATION_MS);
    assert(FakeMotorHwReal_GetState()->last_apply_saw_timer_armed);
    assert(!FakeMotorHwReal_GetState()->both_legs_active_violation);
    printf("SYNTHETIC_INPUT seq=%llu pressure=%ld direction=%d requested_mv=%lu duration_ms=%lu\n",
        (unsigned long long)seq, (long)r.machine.pressure.control_pressure_units,
        (int)direction, (unsigned long)mv, (unsigned long)m->requested_duration_ms);
}

static void TestFullCycle(void)
{
    uint32_t count;
    Init(0U, 0U); Start(0U);
    assert(r.machine.state == AUTO_APPROACH);
    assert(MotorExecutor_GetSnapshot()->requested_duration_ms == 20U);
    Complete(20U); Settled(0U, 70U);
    assert(r.machine.state == AUTO_APPROACH);
    assert(r.machine.cycle_started_ms == 0U);
    assert(MotorExecutor_GetSnapshot()->command_mv == 10000U);
    Complete(91U); Settled(100U, 141U); AssertPulse(MOTOR_DIRECTION_PRESS, 3000U);
    Complete(152U); Settled(150U, 202U); AssertPulse(MOTOR_DIRECTION_PRESS, 2600U);
    Complete(213U); Settled(200U, 263U); AssertPulse(MOTOR_DIRECTION_PRESS, 1600U);
    Complete(274U); Settled(240U, 324U); AssertPulse(MOTOR_DIRECTION_PRESS, 600U);
    Complete(335U); Settled(250U, 385U);
    assert(r.machine.state == AUTO_HOLD && MotorExecutor_OutputIsDisabled());
    count = FakeMotorHwReal_GetState()->apply_count;
    Feed(260U, 400U); assert(r.machine.state == AUTO_HOLD);
    assert(FakeMotorHwReal_GetState()->apply_count == count);
    Feed(230U, 420U); AssertPulse(MOTOR_DIRECTION_PRESS, 1000U);
    Complete(430U); Settled(250U, 480U);
    Feed(300U, 500U); AssertPulse(MOTOR_DIRECTION_RELEASE, 400U);
    Complete(510U); Settled(250U, 560U);
    Feed(0U, 580U); assert(r.machine.state == AUTO_APPROACH);
    assert(MotorExecutor_GetSnapshot()->command_mv == 10000U);
    assert(MotorExecutor_GetSnapshot()->requested_duration_ms == 10U);
    Stop(581U);
    puts("AUTO_FULL_CHAIN_MULTISEG_P_HOLD_DRIFT_RECONTACT=PASS");
}

static void TestContactStartAndClamp(void)
{
    Init(300U, 0U); Start(0U);
    assert(r.machine.state == AUTO_SETTLE);
    assert(FakeMotorHwReal_GetState()->apply_count == 0U);
    Settled(320U, 50U); AssertPulse(MOTOR_DIRECTION_RELEASE, 560U);
    Stop(52U);
    Init(250U, 0U); Start(0U); Settled(250U, 50U);
    assert(r.machine.state == AUTO_HOLD);
    assert(FakeMotorHwReal_GetState()->apply_count == 0U);
    Stop(52U);
    Init(300U, 0U);
    Request(6U, MODBUS_HOLDING_TARGET_PRESSURE, 100U, 0U, false);
    Start(0U); Settled(300U, 50U); AssertPulse(MOTOR_DIRECTION_RELEASE, 800U);
    Stop(52U);
    puts("AUTO_CONTACTED_START_NO_EXTRA_PRESS_RELEASE_CLAMP=PASS");
}

static void AssertApproach(uint32_t count, uint32_t duration)
{
    const MotorExecutorSnapshot *m = MotorExecutor_GetSnapshot();
    const FakeMotorStopTimerState *timer = FakeMotorStopTimer_GetState();
    assert(r.machine.state == AUTO_APPROACH && m->logical_active);
    assert(m->last_action == MOTOR_ACTION_PRESS_PULSE);
    assert(m->direction == MOTOR_DIRECTION_PRESS && m->command_mv == 10000U);
    assert(m->requested_duration_ms == duration && timer->normal_duration_ms == duration);
    assert(timer->backstop_ms == duration + 30U && timer->armed);
    assert(FakeMotorHwReal_GetState()->driver_enabled);
    assert(FakeMotorHwReal_GetState()->shutdown_enabled);
    assert(FakeMotorHwReal_GetState()->last_apply_saw_timer_armed);
    assert(!FakeMotorHwReal_GetState()->both_legs_active_violation);
    assert(g_sd700_approach_diagnostics.pulse_count == count);
    assert(g_sd700_approach_diagnostics.requested_mv == 10000U);
    assert(g_sd700_approach_diagnostics.requested_duration_ms == duration);
    assert(g_sd700_approach_diagnostics.end_reason == AUTO_APPROACH_END_RUNNING);
    assert(!g_sd700_approach_diagnostics.output_disabled_at_start);
}

static void TestCoarseApproachFeedbackAndContact(void)
{
    uint32_t count;
    Init(0U, 0U); Start(0U); AssertApproach(1U, 20U);
    Feed(19U, 5U); AssertApproach(1U, 20U);
    assert(!FeedAt(20U, seq, 6U, 6U)); /* duplicated poll cannot create contact */
    AssertApproach(1U, 20U);
    Complete(20U);
    assert(g_sd700_approach_diagnostics.end_reason == AUTO_APPROACH_END_NORMAL);
    assert(g_sd700_approach_diagnostics.output_disabled_at_end);
    assert(!g_sd700_approach_diagnostics.logical_active_at_end);
    assert(g_sd700_approach_diagnostics.state_after == AUTO_SETTLE);
    Service(70U);
    assert(FeedAt(0U, ++seq, 10U, 71U)); /* in-pulse frame delivered after settle */
    assert(FeedAt(0U, ++seq, 70U, 72U)); /* exactly at settle boundary */
    Service(100U);
    assert(r.machine.state == AUTO_SETTLE && MotorExecutor_OutputIsDisabled());
    assert(FakeMotorHwReal_GetState()->apply_count == 1U);
    Feed(0U, 101U); AssertApproach(2U, 20U);
    assert(r.machine.cycle_started_ms == 0U);
    Complete(121U); Settled(19U, 171U); AssertApproach(3U, 20U);
    Feed(20U, 173U); /* one ms into pulse: contact must stop BEFORE duration */
    assert(r.machine.state == AUTO_SETTLE && MotorExecutor_OutputIsDisabled());
    assert(!FakeMotorStopTimer_GetState()->armed);
    assert(r.machine.auto_has_contacted && !r.machine.auto_approach_pending);
    assert(g_sd700_approach_diagnostics.end_reason == AUTO_APPROACH_END_CONTACT);
    assert(g_sd700_approach_diagnostics.ended_at_ms == 173U);
    assert(g_sd700_approach_diagnostics.pressure_units == 20);
    assert(g_sd700_approach_diagnostics.pressure_fresh);
    assert(g_sd700_approach_diagnostics.output_disabled_at_end);
    count = FakeMotorHwReal_GetState()->apply_count;
    FakeMotorStopTimer_GetState()->handler(MOTOR_STOP_TIMER_NORMAL); /* cancelled pulse */
    Service(223U);
    assert(FeedAt(100U, ++seq, 173U, 224U));
    assert(r.machine.state == AUTO_SETTLE && FakeMotorHwReal_GetState()->apply_count == count);
    Feed(100U, 225U); AssertPulse(MOTOR_DIRECTION_PRESS, 3000U);
    assert(g_sd700_approach_diagnostics.pulse_count == 3U);
    Stop(226U);
    assert(g_sd700_approach_diagnostics.end_reason == AUTO_APPROACH_END_CONTACT);

    Init(250U, 0U); Start(0U); Settled(250U, 50U);
    Feed(0U, 60U); AssertApproach(1U, 10U);
    Feed(20U, 61U);
    assert(r.machine.state == AUTO_SETTLE && MotorExecutor_OutputIsDisabled());
    assert(g_sd700_approach_diagnostics.end_reason == AUTO_APPROACH_END_CONTACT);
    Stop(62U);
    Init(20U, 0U); Start(0U);
    assert(r.machine.state == AUTO_SETTLE && FakeMotorHwReal_GetState()->apply_count == 0U);
    assert(g_sd700_approach_diagnostics.pulse_count == 0U);
    Stop(1U);
    puts("COARSE_BOUNDED_PULSES_OFF_NEW_FRAME_CONTACT_IMMEDIATE_FINE_HANDOFF=PASS");
}

static void TestCoarseContactCompletionRaces(void)
{
    unsigned int timing;
    MotorStopTimerEvent event;
    for (timing = 0U; timing < 3U; ++timing)
    {
        for (event = MOTOR_STOP_TIMER_NORMAL; event <= MOTOR_STOP_TIMER_ERROR; ++event)
        {
            Init(0U, 0U); Start(0U);
            if (timing == 0U) { FakeMotorStopTimer_GetState()->handler(event); }
            else if (timing == 1U) { FakeMotorStopTimer_TriggerOnNextArmedQuery(event); }
            else { FakeMotorStopTimer_TriggerOnNextHealthyQuery(event); }
            Feed(20U, 1U); /* before regular Runtime safety service */
            Service(1U);
            assert(MotorExecutor_OutputIsDisabled());
            assert(!FakeMotorStopTimer_GetState()->armed);
            assert(FakeMotorHwReal_GetState()->apply_count == 1U);
            if (event == MOTOR_STOP_TIMER_NORMAL)
            {
                assert(r.machine.state == AUTO_SETTLE);
                assert(g_sd700_approach_diagnostics.end_reason == AUTO_APPROACH_END_NORMAL);
            }
            else
            {
                assert(r.machine.state == FAULT);
                assert(r.machine.fault == (event == MOTOR_STOP_TIMER_BACKSTOP ?
                    FAULT_MOTION_TIMEOUT : FAULT_MOTOR_FAULT));
                assert(g_sd700_approach_diagnostics.end_reason == (event == MOTOR_STOP_TIMER_BACKSTOP ?
                    AUTO_APPROACH_END_BACKSTOP : AUTO_APPROACH_END_EXECUTOR_ERROR));
            }
            Stop(2U); Feed(0U, 60U); Service(60U);
            assert(FakeMotorHwReal_GetState()->apply_count == 1U);
        }
    }
    puts("COARSE_CONTACT_PENDING_AND_SERVICE_COMPLETION_RACES=PASS");
}

static void TestCoarseImmediateStopAndFault(void)
{
    unsigned int scenario;
    for (scenario = 0U; scenario < 3U; ++scenario)
    {
        Init(0U, 0U); Start(0U);
        if (scenario == 0U) { Stop(1U); }
        else if (scenario == 1U) { Feed(325U, 1U); }
        else { Machine_ReportFault(&r.machine, FAULT_OVERCURRENT, FAULT_DETAIL_NONE, 1U); }
        assert(MotorExecutor_OutputIsDisabled() && !FakeMotorStopTimer_GetState()->armed);
        assert(g_sd700_approach_diagnostics.end_reason == (scenario == 0U ?
            AUTO_APPROACH_END_STOP : AUTO_APPROACH_END_SAFETY_FAULT));
        assert(g_sd700_approach_diagnostics.ended_at_ms == 1U);
        FakeMotorStopTimer_GetState()->handler(MOTOR_STOP_TIMER_NORMAL);
        Feed(0U, 60U); Service(60U);
        assert(r.machine.state == (scenario == 0U ? IDLE : FAULT));
        assert(FakeMotorHwReal_GetState()->apply_count == 1U);
    }
    puts("COARSE_STOP_FAULT_IMMEDIATE_NO_AUTOMATIC_RESTART=PASS");
}

static void TestFeedbackGates(void)
{
    uint32_t count;
    Init(100U, 0U); Start(0U);
    Service(50U);
    assert(FeedAt(100U, ++seq, 0U, 51U)); /* pre-action cache, delivered late */
    assert(r.machine.state == AUTO_SETTLE);
    assert(FeedAt(100U, ++seq, 49U, 52U)); /* pre-settle cache */
    assert(r.machine.state == AUTO_SETTLE);
    Feed(100U, 53U); AssertPulse(MOTOR_DIRECTION_PRESS, 3000U);
    count = FakeMotorHwReal_GetState()->apply_count;
    assert(!FeedAt(0U, seq, 54U, 54U)); /* repeated poll cannot become a frame */
    Feed(0U, 55U); /* new sample during action, cannot stack */
    assert(FakeMotorHwReal_GetState()->apply_count == count);
    Complete(63U); Service(113U);
    assert(FeedAt(100U, ++seq, 60U, 114U)); /* action-time frame after settle */
    assert(r.machine.state == AUTO_SETTLE);
    assert(FakeMotorHwReal_GetState()->apply_count == count);
    assert(!FeedAt(100U, seq - 1U, 115U, 115U)); /* runtime ignores old seq */
    Feed(200U, 116U); AssertPulse(MOTOR_DIRECTION_PRESS, 1600U);
    assert(FeedAt(200U, ++seq, 115U, 117U)); /* seq newer, timestamp older */
    assert(r.machine.state == FAULT && r.machine.fault_detail == FAULT_DETAIL_PRESSURE_ORDER_LOST);
    assert(MotorExecutor_OutputIsDisabled());
    Init(100U, 0U); Start(0U); Service(50U);
    assert(FeedAt(100U, ++seq, 1U, 202U));
    assert(r.machine.fault_detail == FAULT_DETAIL_PRESSURE_TIMEOUT);
    Init(100U, 0U); Start(0U);
    assert(FeedAt(100U, ++seq, 100U, 1U)); /* future timestamp */
    assert(r.machine.state == FAULT);
    Init(100U, 0U); Start(0U);
    Feed(100U, 10U);
    {
        MachinePressureSample old = r.machine.pressure;
        --old.sequence;
        Machine_HandlePressureSample(&r.machine, &old, 11U);
        assert(r.machine.fault_detail == FAULT_DETAIL_PRESSURE_ORDER_LOST);
    }
    puts("AUTO_SAMPLE_DEDUP_CACHE_STALE_ORDER_FUTURE=PASS");
}

static void TestWrap(void)
{
    uint32_t start = UINT32_MAX - 30U;
    Init(100U, start); Start(start);
    Service(start + 50U);
    assert(FeedAt(100U, ++seq, start + 20U, start + 51U));
    assert(r.machine.state == AUTO_SETTLE);
    Feed(200U, start + 52U); AssertPulse(MOTOR_DIRECTION_PRESS, 1600U);
    Complete(start + 62U); Settled(250U, start + 112U);
    assert(r.machine.state == AUTO_HOLD);
    Stop(start + 114U);
    puts("AUTO_TIMESTAMP_WRAP=PASS");
}

static void TestNoResponseDeadlines(void)
{
    uint32_t now;
    Init(100U, 0U); Start(0U); /* Already-contacted START: this START's 30000 ms. */
    assert(g_sd700_approach_diagnostics.first_contact_latched);
    assert(g_sd700_approach_diagnostics.search_elapsed_ms == 0U);
    assert(g_sd700_approach_diagnostics.first_contact_pulse_count == 0U);
    assert(g_sd700_approach_diagnostics.pressure_units == 100);
    for (now = 1U; now <= 30000U; ++now)
    {
        const MotorExecutorSnapshot *m = MotorExecutor_GetSnapshot();
        if (m->logical_active && (uint32_t)(now - r.machine.state_entered_ms) >= m->requested_duration_ms)
        { FakeMotorStopTimer_TriggerNormal(); }
        if ((now % 100U) == 0U) { Feed(100U, now); }
        Service(now);
        if (now < 30000U) { assert(r.machine.state != FAULT); }
        assert(r.machine.cycle_started_ms == 0U);
        if (now == 8000U || now == 29999U)
        { printf("CONVERGENCE_MEASURE_ALIVE elapsed=%lu SYNTHETIC_INPUT\n", (unsigned long)now); }
    }
    assert(r.machine.state == FAULT && r.machine.fault == FAULT_MOTION_TIMEOUT);
    assert(r.machine.fault_detail == FAULT_DETAIL_CYCLE_TIMEOUT);
    assert(r.machine.cycle_started_ms == 0U && MotorExecutor_OutputIsDisabled());
    assert(FakeMotorHwReal_GetState()->apply_count > 2U);
    {
        uint32_t count = FakeMotorHwReal_GetState()->apply_count;
        FakeMotorStopTimer_GetState()->handler(MOTOR_STOP_TIMER_NORMAL);
        Feed(150U, 30001U); Service(30001U);
        assert(r.machine.state == FAULT && r.machine.fault_detail == FAULT_DETAIL_CYCLE_TIMEOUT);
        assert(MotorExecutor_OutputIsDisabled() && FakeMotorHwReal_GetState()->apply_count == count);
    }
    puts("AUTO_POST_CONTACT_NO_RESPONSE_DEADLINE=PASS");
}

static void TestAbortAndCompletion(void)
{
    unsigned int event;
    unsigned int phase;
    for (phase = 0U; phase < 2U; ++phase)
    {
        for (event = 0U; event < 2U; ++event)
        {
            Init(phase == 0U ? 0U : 100U, 0U); Start(0U);
            if (phase != 0U) { Settled(100U, 50U); }
            if (event == 0U) { FakeMotorStopTimer_TriggerBackstop(); }
            else { FakeMotorStopTimer_TriggerError(); }
            Feed(250U, 55U); /* contact must not erase the pending ISR error */
            Service(55U);
            assert(r.machine.state == FAULT && MotorExecutor_OutputIsDisabled());
            assert(r.machine.fault == (event == 0U ? FAULT_MOTION_TIMEOUT : FAULT_MOTOR_FAULT));
            {
                uint32_t count = FakeMotorHwReal_GetState()->apply_count;
                Feed(0U, 100U); Service(100U);
                assert(FakeMotorHwReal_GetState()->apply_count == count);
            }
        }
    }
    Init(0U, 0U); Start(0U); Feed(325U, 1U);
    assert(r.machine.state == FAULT && r.machine.fault == FAULT_OVERPRESSURE);
    assert(MotorExecutor_OutputIsDisabled());
    Init(0U, 0U); Start(0U); Service(201U);
    assert(r.machine.fault_detail == FAULT_DETAIL_PRESSURE_TIMEOUT);
    Init(100U, 0U); Start(0U); Service(50U);
    r.machine.config.settle_feedback_timeout_ms = 80U;
    Service(130U);
    assert(r.machine.fault_detail == FAULT_DETAIL_SETTLE_FEEDBACK_TIMEOUT);
    Init(100U, 0U); Start(0U); Service(50U);
    FakeMotorHwReal_FailNextApply(); Feed(100U, 51U);
    assert(r.machine.state == FAULT && r.machine.fault == FAULT_MOTOR_FAULT);
    puts("AUTO_ABORT_OVERPRESSURE_SENSOR_SETTLE_BACKSTOP_EXECUTOR=PASS");
}

static void TestLateRecontactBudget(void)
{
    uint32_t now;
    uint32_t count = 0U;
    Init(100U, 0U); Start(0U);
    for (now = 1U; now <= 30000U && r.machine.state != FAULT; ++now)
    {
        const MotorExecutorSnapshot *m = MotorExecutor_GetSnapshot();
        if (m->logical_active && (uint32_t)(now - r.machine.state_entered_ms) >= m->requested_duration_ms)
        {
            FakeMotorStopTimer_TriggerNormal();
        }
        if (now == 2999U) { count = FakeMotorHwReal_GetState()->apply_count; }
        if ((now % 100U) == 0U) { Feed(now < 3000U || now >= 20000U ? 100U : 0U, now); }
        Service(now);
        if (now < 30000U) { assert(r.machine.state != FAULT); }
        assert(r.machine.cycle_started_ms == 0U);
        if (now == 20000U)
        { assert(r.machine.auto_has_contacted && !r.machine.auto_approach_pending); }
        if (r.machine.state == AUTO_APPROACH)
        {
            assert(MotorExecutor_GetSnapshot()->requested_duration_ms == 10U);
            assert(FakeMotorStopTimer_GetState()->backstop_ms == 40U);
        }
    }
    assert(r.machine.state == FAULT && r.machine.fault == FAULT_MOTION_TIMEOUT &&
           r.machine.fault_detail == FAULT_DETAIL_CYCLE_TIMEOUT);
    assert(FakeMotorHwReal_GetState()->apply_count > count);
    assert(MotorExecutor_OutputIsDisabled());
    puts("AUTO_RECONTACT_KEEPS_EXISTING_CONVERGENCE_BUDGET=PASS");
}

static void TestExtendedConvergenceSafetyAndHold(void)
{
    unsigned scenario;
    for (scenario = 0U; scenario < 7U; ++scenario)
    {
        uint32_t start = UINT32_MAX - 1000U; /* extended window crosses tick wrap */
        uint32_t elapsed, now, count;
        Init(30U, start); Start(start);
        for (elapsed = 1U; elapsed <= 9000U; ++elapsed)
        {
            const MotorExecutorSnapshot *m = MotorExecutor_GetSnapshot();
            now = start + elapsed;
            if (m->logical_active && (uint32_t)(now - r.machine.state_entered_ms) >= m->requested_duration_ms)
            { FakeMotorStopTimer_TriggerNormal(); }
            if ((elapsed % 100U) == 0U) { Feed((uint16_t)(30U + 2U * (elapsed / 2000U)), now); }
            Service(now);
            assert(r.machine.state != FAULT && r.machine.cycle_started_ms == start);
        }
        now = start + 9000U;
        AssertPulse(MOTOR_DIRECTION_PRESS, 5000U);
        assert(r.machine.press_feedback.boost_mv == 2000U);
        count = FakeMotorHwReal_GetState()->apply_count;
        if (scenario == 0U) { Stop(now + 1U); }
        else if (scenario == 1U) { Feed(325U, now + 1U); }
        else if (scenario == 2U)
        {
            MachinePressureSample invalid = r.machine.pressure;
            invalid.frame_valid = false;
            Machine_HandlePressureSample(&r.machine, &invalid, now + 1U);
        }
        else if (scenario == 3U) { Service(now + 201U); }
        else if (scenario == 4U) { FakeMotorStopTimer_TriggerBackstop(); Service(now + 40U); }
        else if (scenario == 5U)
        { Machine_ReportFault(&r.machine, FAULT_OVERCURRENT, FAULT_DETAIL_NONE, now + 1U); }
        else
        {
            Complete(now + 10U); Settled(250U, now + 60U);
            for (elapsed = 9200U; elapsed <= 31000U; elapsed += 100U)
            {
                Feed(250U, start + elapsed); Service(start + elapsed);
                assert(r.machine.state == AUTO_HOLD && r.machine.fault == FAULT_NONE);
                assert(MotorExecutor_OutputIsDisabled() && r.machine.cycle_started_ms == 0U);
                assert(FakeMotorHwReal_GetState()->apply_count == count);
            }
            Feed(200U, start + 31001U); AssertPulse(MOTOR_DIRECTION_PRESS, 1600U);
            assert(r.machine.cycle_started_ms == start + 31001U); /* existing HOLD exit semantics */
            Stop(start + 31002U);
            continue;
        }
        assert(r.machine.state == (scenario == 0U ? IDLE : FAULT));
        if (scenario == 1U) { assert(r.machine.fault == FAULT_OVERPRESSURE); }
        if (scenario == 2U) { assert(r.machine.fault_detail == FAULT_DETAIL_PRESSURE_INVALID); }
        if (scenario == 3U) { assert(r.machine.fault_detail == FAULT_DETAIL_PRESSURE_TIMEOUT); }
        if (scenario == 4U) { assert(r.machine.fault_detail == FAULT_DETAIL_PULSE_TIMEOUT); }
        if (scenario == 5U) { assert(r.machine.fault == FAULT_OVERCURRENT); }
        assert(MotorExecutor_OutputIsDisabled() && !FakeMotorStopTimer_GetState()->armed);
        FakeMotorStopTimer_GetState()->handler(MOTOR_STOP_TIMER_NORMAL);
        Feed(40U, now + 300U); Service(now + 300U);
        assert(r.machine.state == (scenario == 0U ? IDLE : FAULT));
        assert(MotorExecutor_OutputIsDisabled() && FakeMotorHwReal_GetState()->apply_count == count);
    }
    puts("CONVERGENCE_MEASURE_EXTENDED_WINDOW_RISE_BOOST_WRAP_EARLY_STOP_HOLD=PASS SYNTHETIC_INPUT");
}

static void TestStopAllStates(void)
{
    unsigned int phase;
    for (phase = 0U; phase < 5U; ++phase)
    {
        uint32_t count;
        Init(phase == 0U ? 0U : 100U, 0U); Start(0U);
        if (phase == 2U) { Service(50U); }
        if (phase == 3U) { Settled(100U, 50U); }
        if (phase == 4U) { Settled(250U, 50U); }
        count = FakeMotorHwReal_GetState()->apply_count;
        /* Completion queued just before STOP must be cancelled by old path. */
        if (phase == 0U || phase == 3U) { FakeMotorStopTimer_TriggerNormal(); }
        Stop(52U);
        assert(r.machine.state == IDLE);
        /* Inject a late callback without pretending a cancelled timer is armed. */
        FakeMotorStopTimer_GetState()->handler(MOTOR_STOP_TIMER_NORMAL);
        Feed(0U, 60U); Service(60U);
        Feed(100U, 120U); Service(120U);
        assert(r.machine.state == IDLE && MotorExecutor_OutputIsDisabled());
        assert(FakeMotorHwReal_GetState()->apply_count == count);
        Start(121U); /* Only a new legal command may start another cycle. */
        assert(r.machine.state == AUTO_SETTLE);
        Stop(122U);
    }
    puts("AUTO_STOP_APPROACH_SETTLE_DELAY_SETTLE_SAMPLE_PULSE_HOLD=PASS");
}

static void TestConfigAndDiagnostics(void)
{
    ForcePiConfig pi;
    uint16_t value;
    uint16_t address;
    Init(100U, 0U);
    pi = g_sd700_auto_target_force_pi_config;
    pi.ki_mv_per_unit_s = 1.0f;
    assert(Machine_ConfigureForcePi(&r.machine, &pi) == COMMAND_INVALID_VALUE);
    r.machine.force_pi_enabled = false;
    {
        MachineCommand command = {CMD_AUTO_START, 0};
        assert(Machine_HandleCommand(&r.machine, &command, 0U) == COMMAND_NOT_READY);
    }
    assert(Machine_ConfigureForcePi(&r.machine, &g_sd700_auto_target_force_pi_config) == COMMAND_ACCEPTED);
    Start(0U); Settled(200U, 50U); Complete(61U);
    for (address = 0U; address < MODBUS_INPUT_TOTAL_REGISTER_COUNT; ++address)
    {
        assert(ModbusSemantic_ReadInput(&r.machine, address, 62U, &value));
    }
    assert(ModbusSemantic_ReadInput(&r.machine, MODBUS_INPUT_REQUEST_COMMAND_MV, 62U, &value));
    assert(value == 1600U); /* persists after output-off */
    assert(ModbusSemantic_ReadInput(&r.machine, MODBUS_INPUT_REQUEST_DURATION_MS, 62U, &value));
    assert(value == 10U);
    assert(ModbusSemantic_ReadInput(&r.machine, MODBUS_INPUT_SAMPLE_SEQUENCE_WORD_3, 62U, &value));
    assert(value == seq);
    assert(ModbusSemantic_ReadInput(&r.machine, MODBUS_INPUT_AUTO_TARGET_CAPABILITY, 62U, &value));
    assert(value == 0xA701U);
    assert(ModbusSemantic_ReadInput(&r.machine, MODBUS_INPUT_TARGET_READBACK, 62U, &value));
    assert(value == 250U);
    Stop(63U);
    r.machine.pressure.sequence = UINT64_C(0x123456789ABCDEF0);
    r.machine.pressure.received_at_ms = 0xFEDCBA98U;
    {
        const uint16_t expected[] = {0x1234U,0x5678U,0x9ABCU,0xDEF0U,0xFEDCU,0xBA98U};
        for (address = 0U; address < 6U; ++address)
        {
            assert(ModbusSemantic_ReadInput(&r.machine, (uint16_t)(0x14U + address), 64U, &value));
            assert(value == expected[address]);
        }
    }
    ApplicationRuntime_Initialize(&r, &g_sd700_bench_machine_config, 0U);
    ApplicationRuntime_CompleteBoot(&r, true, 0U);
    assert(r.machine.state == FAULT && !r.machine.force_pi_enabled);
    puts("AUTO_CONFIG_BOOT_REJECT_DRY_RUN_DIAGNOSTICS=PASS");
}

static void TestFaultAllStates(void)
{
    unsigned int phase;
    unsigned int fault;
    for (fault = 0U; fault < 2U; ++fault)
    {
        for (phase = 0U; phase < 5U; ++phase)
        {
            uint32_t count;
            Init(phase == 0U ? 0U : 100U, 0U); Start(0U);
            if (phase == 2U) { Service(50U); }
            if (phase == 3U) { Settled(100U, 50U); }
            if (phase == 4U) { Settled(250U, 50U); }
            count = FakeMotorHwReal_GetState()->apply_count;
            if (fault == 0U) { Feed(325U, 52U); }
            else { Service(252U); }
            assert(r.machine.state == FAULT && MotorExecutor_OutputIsDisabled());
            assert(r.machine.fault == (fault == 0U ? FAULT_OVERPRESSURE : FAULT_PRESSURE_SENSOR_FAULT));
            FakeMotorStopTimer_GetState()->handler(MOTOR_STOP_TIMER_NORMAL);
            Feed(0U, 260U); Service(260U); Stop(261U);
            assert(r.machine.state == FAULT && MotorExecutor_OutputIsDisabled());
            assert(FakeMotorHwReal_GetState()->apply_count == count);
        }
    }
    puts("AUTO_ALL_STATES_OVERPRESSURE_SENSOR_TIMEOUT_LATE_COMPLETION=PASS");
}

/* All times below are MCU receive/dispatch ticks relative to episode start.
 * No PC poll timestamp or field CSV is used to construct these inputs. */
static void PrepareContactDeadline(uint32_t start, unsigned int phase)
{
    uint32_t elapsed;
    Init(0U, start); Start(start);
    for (elapsed = 1U; elapsed <= 2975U; ++elapsed)
    {
        const MotorExecutorSnapshot *m = MotorExecutor_GetSnapshot();
        uint32_t now = start + elapsed;
        if (m->logical_active && (uint32_t)(now - r.machine.state_entered_ms) >= m->requested_duration_ms)
        {
            FakeMotorStopTimer_TriggerNormal();
        }
        if ((elapsed % 100U) == 0U) { Feed(0U, now); }
        Service(now);
        assert(r.machine.state != FAULT && r.machine.cycle_started_ms == start);
    }
    assert(r.machine.state == AUTO_SETTLE && r.machine.settle_phase == SETTLE_WAIT_SAMPLE);
    if (phase != 1U)
    {
        Feed(0U, start + (phase == 0U ? 2990U : (phase == 2U ? 2975U : 2980U)));
        AssertApproach(31U, 20U);
    }
    if (phase == 2U) { Complete(start + 2995U); }
    assert(FakeMotorHwReal_GetState()->apply_count >= 30U);
}

static int TestContactDeadline(void)
{
    const uint32_t received[] = {2999U,2999U,3000U,2999U,3000U,3001U};
    const uint32_t handled[]  = {2999U,3000U,3000U,3001U,3001U,3001U};
    unsigned int phase, sample, wrap;
    int failures = 0;
    for (wrap = 0U; wrap < 2U; ++wrap)
    {
        uint32_t start = wrap == 0U ? 1000U : UINT32_MAX - 1500U;
        for (phase = 0U; phase < 3U; ++phase)
        {
            for (sample = 0U; sample < 6U; ++sample)
            {
                uint32_t count;
                bool accepted;
                PrepareContactDeadline(start, phase);
                count = FakeMotorHwReal_GetState()->apply_count;
                assert(FeedAt(20U, ++seq, start + received[sample], start + handled[sample]));
                accepted = r.machine.state != FAULT && r.machine.auto_has_contacted &&
                           !r.machine.auto_approach_pending;
                printf("SYNTHETIC_INPUT CONTACT_DEADLINE wrap=%u phase=%u rx=%lu process=%lu state=%d fault=%d detail=%d accepted=%d expected=1\n",
                    wrap, phase, (unsigned long)received[sample], (unsigned long)handled[sample],
                    r.machine.state, r.machine.fault, r.machine.fault_detail, accepted);
                if (!accepted) { ++failures; continue; }
                assert(g_sd700_approach_diagnostics.contact_cycle_started_ms == start);
                assert(g_sd700_approach_diagnostics.contact_sample_sequence == seq);
                assert(g_sd700_approach_diagnostics.contact_received_at_ms == start + received[sample]);
                assert(g_sd700_approach_diagnostics.contact_decided_at_ms == start + handled[sample]);
                assert(!g_sd700_approach_diagnostics.approach_timeout_committed);
                if (phase != 1U)
                {
                    assert(r.machine.state == AUTO_SETTLE && MotorExecutor_OutputIsDisabled());
                    assert(!FakeMotorStopTimer_GetState()->armed);
                    assert(FakeMotorHwReal_GetState()->apply_count == count);
                    /* Old coarse completion and timeout tick cannot undo contact. */
                    FakeMotorStopTimer_GetState()->handler(MOTOR_STOP_TIMER_NORMAL);
                    Service(start + handled[sample]);
                    Service(start + 3060U);
                    Feed(100U, start + 3061U);
                }
                AssertPulse(MOTOR_DIRECTION_PRESS, 3000U);
                assert(r.machine.cycle_started_ms == start + received[sample]);
                assert(g_sd700_approach_diagnostics.search_elapsed_ms == received[sample]);
                assert(g_sd700_approach_diagnostics.first_contact_decided_at_ms == start + handled[sample]);
                Stop(start + (phase == 1U ? handled[sample] + 1U : 3062U));
            }
        }
    }
    printf("CONTACT_DEADLINE_TIMELY_EVIDENCE_FAILURES=%d\n", failures);
    return failures;
}

static void TestContactDeadlineRejections(void)
{
    unsigned int phase, scenario, wrap;
    for (wrap = 0U; wrap < 2U; ++wrap)
    {
        uint32_t start = wrap == 0U ? 1000U : UINT32_MAX - 1500U;
        for (phase = 0U; phase < 3U; ++phase)
        {
            for (scenario = 0U; scenario < 9U; ++scenario)
            {
                uint32_t count, now = start + 3000U;
                bool pressure_fault = scenario >= 5U;
                PrepareContactDeadline(start, phase);
                count = FakeMotorHwReal_GetState()->apply_count;
                switch (scenario)
                {
                    case 0U: Feed(0U, now); break;
                    case 1U: Feed(19U, now); break;
                    case 2U: /* Safety tick first, then fresh contact: no old 3000 ms fault. */
                        Service(now); Feed(20U, now); break;
                    case 3U: /* Fresh receive after the removed deadline is now eligible. */
                        ++now; Feed(20U, now); break;
                    case 4U: /* Duplicate poll does not replace the previous zero frame. */
                        assert(!FeedAt(20U, seq, now, now)); Service(now); break;
                    case 5U: /* Valid contact, but expired at dispatch (201 ms). */
                        now = start + 3200U;
                        assert(FeedAt(20U, ++seq, start + 2999U, now)); break;
                    case 6U: /* Future received tick must not refresh pressure. */
                        assert(FeedAt(20U, ++seq, now + 1U, now)); break;
                    case 7U: /* Invalid frame retains sensor-fault priority. */
                    {
                        MachinePressureSample invalid = r.machine.pressure;
                        invalid.sequence = ++seq; invalid.received_at_ms = now;
                        invalid.raw_pressure_counts = 20U; invalid.control_pressure_units = 20;
                        invalid.frame_valid = false;
                        Machine_HandlePressureSample(&r.machine, &invalid, now);
                        break;
                    }
                    default: /* New sequence with an older tick remains an order fault. */
                        assert(FeedAt(20U, ++seq, r.machine.pressure.received_at_ms - 1U, now));
                        break;
                }
                if (!pressure_fault)
                {
                    assert(r.machine.fault == FAULT_NONE);
                    assert(!g_sd700_approach_diagnostics.approach_timeout_committed);
                    assert(r.machine.auto_has_contacted == (scenario == 2U || scenario == 3U));
                    if (scenario == 4U) { assert(FakeMotorHwReal_GetState()->apply_count == count); }
                    Stop(now + 1U);
                    count = FakeMotorHwReal_GetState()->apply_count;
                }
                else
                {
                    assert(r.machine.state == FAULT && MotorExecutor_OutputIsDisabled());
                    assert(!FakeMotorStopTimer_GetState()->armed);
                    assert(r.machine.fault == FAULT_PRESSURE_SENSOR_FAULT);
                    assert(g_sd700_approach_diagnostics.contact_sample_sequence == 0U);
                }
                /* Late higher pressure and old completion cannot clear a fault. */
                FakeMotorStopTimer_GetState()->handler(MOTOR_STOP_TIMER_NORMAL);
                Feed(250U, now + 2U); Service(now + 2U); Stop(now + 3U);
                assert(r.machine.state == (pressure_fault ? FAULT : IDLE) && MotorExecutor_OutputIsDisabled());
                assert(FakeMotorHwReal_GetState()->apply_count == count);
            }
        }
    }
    /* Fresh and ordered but received before this episode: not contact evidence. */
    Init(0U, 950U); Start(1000U);
    assert(FeedAt(20U, ++seq, 999U, 1001U));
    assert(r.machine.state == AUTO_APPROACH && !r.machine.auto_has_contacted);
    assert(g_sd700_approach_diagnostics.contact_sample_sequence == 0U);
    Stop(1002U);
    puts("CONTACT_MEASURE_LOW_LATE_ELIGIBLE_STALE_INVALID_DUPLICATE_ORDER_PRIOR_EPISODE=PASS");
}

static void TestContactDeadlineEventOrdering(void)
{
    unsigned int scenario;
    for (scenario = 0U; scenario < 8U; ++scenario)
    {
        uint32_t count;
        PrepareContactDeadline(0U, 3U); /* 2980 + 20 ms normal completion */
        count = FakeMotorHwReal_GetState()->apply_count;
        if (scenario == 0U) { FakeMotorStopTimer_TriggerNormal(); }
        if (scenario == 1U) { FakeMotorStopTimer_TriggerBackstop(); }
        if (scenario == 2U) { FakeMotorStopTimer_TriggerError(); }
        if (scenario == 3U) { Stop(3000U); }
        if (scenario == 4U) { Machine_ReportFault(&r.machine, FAULT_OVERCURRENT, FAULT_DETAIL_NONE, 3000U); }
        if (scenario == 5U) { Feed(325U, 3000U); }
        assert(FeedAt(20U, ++seq, 3000U, 3000U));
        if (scenario == 6U) { Stop(3000U); }
        if (scenario == 7U) { Machine_ReportFault(&r.machine, FAULT_OVERCURRENT, FAULT_DETAIL_NONE, 3000U); }
        Service(3000U);
        assert(MotorExecutor_OutputIsDisabled() && !FakeMotorStopTimer_GetState()->armed);
        assert(FakeMotorHwReal_GetState()->apply_count == count);
        if (scenario == 0U)
        {
            assert(r.machine.state == AUTO_SETTLE && r.machine.auto_has_contacted);
            /* An old normal completion and a same-tick safety check do not revert contact. */
            FakeMotorStopTimer_GetState()->handler(MOTOR_STOP_TIMER_NORMAL);
            Service(3000U);
            assert(r.machine.state == AUTO_SETTLE && !r.machine.auto_approach_pending);
            Stop(3001U);
        }
        else
        {
            assert(r.machine.state == ((scenario == 3U || scenario == 6U) ? IDLE : FAULT));
            if (scenario == 1U) { assert(r.machine.fault_detail == FAULT_DETAIL_APPROACH_TIMEOUT); }
            if (scenario == 2U) { assert(r.machine.fault == FAULT_MOTOR_FAULT); }
            if (scenario == 5U) { assert(r.machine.fault == FAULT_OVERPRESSURE); }
        }
        Feed(250U, 3002U); Service(3002U);
        assert(MotorExecutor_OutputIsDisabled() && FakeMotorHwReal_GetState()->apply_count == count);
    }
    puts("CONTACT_DEADLINE_COMPLETION_STOP_HARDWARE_OVERPRESSURE_PRIORITY=PASS");
}

static void TestContactDoesNotRelaxFineDeadlines(void)
{
    uint32_t now;
    PrepareContactDeadline(0U, 1U);
    r.machine.config.settle_feedback_timeout_ms = 30U; /* WAIT_SAMPLE opened 2970 */
    Feed(20U, 3000U);
    assert(r.machine.state == FAULT && r.machine.fault_detail == FAULT_DETAIL_SETTLE_FEEDBACK_TIMEOUT);
    assert(g_sd700_approach_diagnostics.contact_sample_sequence == 0U);

    PrepareContactDeadline(0U, 0U);
    r.machine.config.automatic_cycle_timeout_ms = 3000U;
    Feed(20U, 3000U);
    assert(r.machine.state == AUTO_SETTLE && r.machine.cycle_started_ms == 3000U);
    Stop(3001U);

    PrepareContactDeadline(0U, 0U); Feed(20U, 3000U);
    for (now = 3001U; now <= 33000U; ++now)
    {
        const MotorExecutorSnapshot *m = MotorExecutor_GetSnapshot();
        if (m->logical_active && (uint32_t)(now - r.machine.state_entered_ms) >= m->requested_duration_ms)
        {
            assert(r.machine.state == AUTO_PULSE && m->command_mv >= 3000U && m->command_mv <= 5000U);
            FakeMotorStopTimer_TriggerNormal();
        }
        if ((now % 100U) == 0U) { Feed(100U, now); }
        Service(now);
        assert(r.machine.cycle_started_ms == 3000U);
        if (now < 33000U) { assert(r.machine.state != FAULT); }
    }
    assert(r.machine.state == FAULT && r.machine.fault == FAULT_MOTION_TIMEOUT &&
           r.machine.fault_detail == FAULT_DETAIL_CYCLE_TIMEOUT);
    assert(MotorExecutor_OutputIsDisabled());
    assert(!g_sd700_approach_diagnostics.approach_timeout_committed);

    PrepareContactDeadline(0U, 0U); Feed(20U, 3000U); Service(3050U);
    {
        uint32_t count = FakeMotorHwReal_GetState()->apply_count;
        Feed(0U, 3051U); /* recontact cannot restart the convergence clock */
        assert(r.machine.state == AUTO_APPROACH && r.machine.cycle_started_ms == 3000U);
        assert(MotorExecutor_GetSnapshot()->requested_duration_ms == 10U);
        assert(FakeMotorHwReal_GetState()->apply_count == count + 1U);
        Feed(20U, 3052U);
        assert(r.machine.state == AUTO_SETTLE && r.machine.cycle_started_ms == 3000U);
        Stop(3053U);
    }
    puts("CONTACT_CONVERGENCE_FROM_RECEIVE_SETTLE_FAULT_RECONTACT_NO_RESET=PASS");
}

static void TestContactEvidenceIsNotSettledFeedback(void)
{
    uint32_t count;
    PrepareContactDeadline(0U, 1U); /* Previous pulse 2900..2920; gate opened 2970. */
    count = FakeMotorHwReal_GetState()->apply_count;
    assert(FeedAt(20U, ++seq, 2910U, 3000U)); /* in-action cached contact, still fresh */
    assert(r.machine.state == AUTO_SETTLE && r.machine.auto_has_contacted);
    assert(!r.machine.auto_approach_pending && MotorExecutor_OutputIsDisabled());
    assert(FakeMotorHwReal_GetState()->apply_count == count);
    assert(g_sd700_approach_diagnostics.contact_received_at_ms == 2910U);
    assert(g_sd700_approach_diagnostics.contact_decided_at_ms == 3000U);
    Service(3000U);
    assert(!FeedAt(100U, seq, 3001U, 3001U)); /* duplicate cannot become fine feedback */
    assert(FakeMotorHwReal_GetState()->apply_count == count);
    Feed(100U, 3002U); AssertPulse(MOTOR_DIRECTION_PRESS, 3000U);
    Stop(3003U);
    puts("CONTACT_DEADLINE_CONTACT_EVIDENCE_NOT_SETTLED_FEEDBACK=PASS");
}

static uint32_t PressResponse(uint16_t pressure, uint32_t now)
{
    Complete(now + 10U);
    assert(r.machine.press_feedback.feedback_pending);
    assert(g_sd700_approach_diagnostics.press.end_reason == AUTO_APPROACH_END_NORMAL);
    Settled(pressure, now + 60U);
    return now + 61U;
}

static void TestSegmentedPressAndBoost(void)
{
    const uint16_t pressure[] = {30U, 129U, 130U, 200U, 229U, 230U, 239U, 240U, 244U};
    const uint32_t base[] = {3000U, 3000U, 3000U, 1600U, 1020U, 1000U, 640U, 600U, 440U};
    unsigned i;
    uint32_t now;
    for (i = 0U; i < sizeof(pressure)/sizeof(pressure[0]); ++i)
    {
        Init(pressure[i], 0U); Start(0U); Settled(pressure[i], 50U);
        AssertPulse(MOTOR_DIRECTION_PRESS, base[i]);
        assert(r.machine.force_pi_config.ki_mv_per_unit_s == 0.0f);
        assert(r.machine.force_pi_state.integral_mv == 0.0f);
        Stop(52U);
    }
    Init(30U, 0U); Start(0U); Settled(30U, 50U); now = 51U;
    now = PressResponse(31U, now); AssertPulse(MOTOR_DIRECTION_PRESS, 3000U);
    now = PressResponse(32U, now); AssertPulse(MOTOR_DIRECTION_PRESS, 3300U);
    now = PressResponse(33U, now);
    assert(r.machine.press_feedback.low_response_count == 1U);
    now = PressResponse(35U, now); AssertPulse(MOTOR_DIRECTION_PRESS, 3300U);
    assert(r.machine.press_feedback.boost_mv == 300U); /* exact >=2 retains far boost */
    assert(r.machine.press_feedback.low_response_count == 0U);
    now = PressResponse(36U, now); AssertPulse(MOTOR_DIRECTION_PRESS, 3300U);
    assert(r.machine.press_feedback.low_response_count == 1U);
    now = PressResponse(37U, now); AssertPulse(MOTOR_DIRECTION_PRESS, 3600U);
    assert(r.machine.press_feedback.low_response_count == 0U);
    Stop(now + 1U);
    Init(30U, 0U); Start(0U); Settled(30U, 50U); now = 51U;
    for (i = 1U; i <= 18U; ++i)
    {
        uint32_t boost = (i / 2U) * 300U;
        if (boost > 2000U) { boost = 2000U; }
        now = PressResponse(30U, now);
        AssertPulse(MOTOR_DIRECTION_PRESS, 3000U + boost);
        assert(r.machine.press_feedback.boost_mv == boost);
        assert(g_sd700_approach_diagnostics.press.normal_completion_count == i);
        assert(g_sd700_approach_diagnostics.press.completed_request_sequence ==
               g_sd700_approach_diagnostics.press.request_sequence - 1U);
        assert(g_sd700_approach_diagnostics.press.feedback_valid);
        assert(r.machine.cycle_started_ms == 0U);
    }
    now = PressResponse(33U, now); /* >=2 units, not a legacy N criterion */
    AssertPulse(MOTOR_DIRECTION_PRESS, 5000U);
    assert(r.machine.press_feedback.boost_mv == 2000U && r.machine.press_feedback.low_response_count == 0U);
    now = PressResponse(33U, now); now = PressResponse(33U, now);
    AssertPulse(MOTOR_DIRECTION_PRESS, 5000U);
    now = PressResponse(200U, now); /* same far band, reduced base + retained cap */
    AssertPulse(MOTOR_DIRECTION_PRESS, 3600U);
    now = PressResponse(229U, now); AssertPulse(MOTOR_DIRECTION_PRESS, 3020U);
    now = PressResponse(229U, now);
    assert(r.machine.press_feedback.low_response_count == 1U);
    now = PressResponse(230U, now); /* exact error=20 boundary */
    AssertPulse(MOTOR_DIRECTION_PRESS, 1000U);
    assert(r.machine.press_feedback.boost_mv == 0U && r.machine.press_feedback.low_response_count == 0U);
    now = PressResponse(231U, now); /* first low response in the fine band */
    AssertPulse(MOTOR_DIRECTION_PRESS, 960U);
    assert(r.machine.press_feedback.boost_mv == 0U);
    for (i = 0U; i < 10U; ++i) { now = PressResponse(231U, now); }
    AssertPulse(MOTOR_DIRECTION_PRESS, 1960U); /* fine extra max=1000 */
    now = PressResponse(233U, now); /* same fine band: effective rise still clears */
    AssertPulse(MOTOR_DIRECTION_PRESS, 880U);
    assert(r.machine.press_feedback.boost_mv == 0U && r.machine.press_feedback.low_response_count == 0U);
    for (i = 0U; i < 10U; ++i) { now = PressResponse(233U, now); }
    AssertPulse(MOTOR_DIRECTION_PRESS, 1880U);
    now = PressResponse(240U, now);
    AssertPulse(MOTOR_DIRECTION_PRESS, 600U);
    for (i = 0U; i < 6U; ++i) { now = PressResponse(240U, now); }
    AssertPulse(MOTOR_DIRECTION_PRESS, 600U); /* near extra max=0 */
    assert(r.machine.press_feedback.boost_mv == 0U);
    now = PressResponse(250U, now);
    assert(r.machine.state == AUTO_HOLD && MotorExecutor_OutputIsDisabled());
    assert(!r.machine.press_feedback.feedback_pending && !r.machine.press_feedback.in_flight);
    Feed(200U, now + 1U); AssertPulse(MOTOR_DIRECTION_PRESS, 1600U);
    Stop(now + 2U);
    puts("PRESS_SEGMENTS_TWO_NORMAL_LOW_RESPONSES_CAP_RESET_NEAR_HOLD=PASS SYNTHETIC_INPUT");
    puts("PRESS_BOOST_RETAIN_FAR_EFFECTIVE_RISE_COUNTER_RESET_CAP_FINE_RETRACTION=PASS SYNTHETIC_INPUT");
}

static void TestRetainedBoostFeedbackGates(void)
{
    unsigned scenario;
    for (scenario = 0U; scenario < 7U; ++scenario)
    {
        uint32_t now = 51U, count;
        Init(30U, 0U); Start(0U); Settled(30U, 50U);
        now = PressResponse(30U, now); now = PressResponse(30U, now);
        now = PressResponse(30U, now);
        assert(r.machine.press_feedback.boost_mv == 300U);
        assert(r.machine.press_feedback.low_response_count == 1U);
        count = FakeMotorHwReal_GetState()->apply_count;
        if (scenario == 0U)
        {
            Feed(32U, now + 5U); /* on-pulse rise cannot consume feedback */
            assert(r.machine.press_feedback.in_flight && !r.machine.press_feedback.feedback_pending);
            assert(r.machine.press_feedback.low_response_count == 1U);
        }
        Complete(now + 10U);
        Feed(32U, now + 11U); /* OFF, but before settle delay */
        assert(r.machine.press_feedback.feedback_pending);
        assert(r.machine.press_feedback.low_response_count == 1U);
        Service(now + 60U);
        if (scenario <= 2U)
        {
            if (scenario == 0U) { assert(!FeedAt(32U, seq, now + 61U, now + 61U)); }
            else { assert(FeedAt(32U, ++seq, now + (scenario == 1U ? 12U : 60U), now + 61U)); }
            /* Duplicate, cached pre-gate, or gate-equality rise cannot clear count. */
            FakeMotorStopTimer_GetState()->handler(MOTOR_STOP_TIMER_NORMAL);
            Service(now + 61U);
            assert(r.machine.press_feedback.feedback_pending);
            assert(!g_sd700_approach_diagnostics.press.feedback_valid);
            assert(r.machine.press_feedback.boost_mv == 300U);
            assert(r.machine.press_feedback.low_response_count == 1U);
            assert(FakeMotorHwReal_GetState()->apply_count == count);
            Feed(32U, now + 62U); AssertPulse(MOTOR_DIRECTION_PRESS, 3300U);
            assert(g_sd700_approach_diagnostics.press.feedback_valid);
            assert(r.machine.press_feedback.low_response_count == 0U);
        }
        else if (scenario == 3U)
        {
            ++r.machine.diagnostic_request_sequence; /* fault-injected request mismatch */
            Feed(32U, now + 61U); AssertPulse(MOTOR_DIRECTION_PRESS, 3000U);
            assert(!g_sd700_approach_diagnostics.press.feedback_valid);
            assert(r.machine.press_feedback.boost_mv == 0U);
            assert(r.machine.press_feedback.low_response_count == 0U);
        }
        else
        {
            MachinePressureSample sample = r.machine.pressure;
            sample.sequence = ++seq;
            sample.raw_pressure_counts = 32U;
            sample.control_pressure_units = 32;
            sample.received_at_ms = now + 61U;
            if (scenario == 4U) { sample.frame_valid = false; }
            else if (scenario == 5U) { sample.control_units_valid = false; }
            else { sample.received_at_ms = now + 62U; } /* future sample */
            Machine_HandlePressureSample(&r.machine, &sample, now + 61U);
            assert(r.machine.state == FAULT && MotorExecutor_OutputIsDisabled());
            assert(!g_sd700_approach_diagnostics.press.feedback_valid);
            assert(r.machine.press_feedback.boost_mv == 0U);
            assert(r.machine.press_feedback.low_response_count == 0U);
            assert(FakeMotorHwReal_GetState()->apply_count == count);
        }
        Stop(now + 63U);
    }
    puts("PRESS_BOOST_RETAIN_VALIDITY_SETTLE_DUPLICATE_REQUEST_MATCH_GATES=PASS SYNTHETIC_INPUT");
}

static void TestRetainedBoostContactHoldReleaseReset(void)
{
    const uint16_t after[] = {19U, 250U, 300U};
    unsigned i;
    for (i = 0U; i < sizeof(after)/sizeof(after[0]); ++i)
    {
        uint32_t now = 51U;
        Init(30U, 0U); Start(0U); Settled(30U, 50U);
        now = PressResponse(30U, now); now = PressResponse(30U, now);
        now = PressResponse(32U, now); /* retain after a qualified effective rise */
        now = PressResponse(32U, now);
        assert(r.machine.press_feedback.boost_mv == 300U && r.machine.press_feedback.low_response_count == 1U);
        now = PressResponse(after[i], now);
        assert(r.machine.press_feedback.boost_mv == 0U && r.machine.press_feedback.low_response_count == 0U);
        assert(!r.machine.press_feedback.in_flight && !r.machine.press_feedback.feedback_pending);
        if (i == 0U) { assert(r.machine.state == AUTO_APPROACH); }
        else if (i == 1U) { assert(r.machine.state == AUTO_HOLD && MotorExecutor_OutputIsDisabled()); }
        else { AssertPulse(MOTOR_DIRECTION_RELEASE, 400U); }
        Stop(now + 1U);
    }
    puts("PRESS_BOOST_RETAIN_CONTACT_LOSS_HOLD_RELEASE_RESET=PASS SYNTHETIC_INPUT");
}

static void TestPressFeedbackEligibility(void)
{
    uint32_t now;
    unsigned wrap;
    for (wrap = 0U; wrap < 2U; ++wrap)
    {
        uint32_t start = wrap ? UINT32_MAX - 50U : 0U;
        Init(30U, start); Start(start); Settled(30U, start + 50U);
        Feed(50U, start + 55U); /* observed on-pulse rise */
        assert(r.machine.press_feedback.boost_mv == 0U);
        Complete(start + 61U); Service(start + 111U);
        assert(FeedAt(30U, ++seq, start + 60U, start + 112U)); /* in-action cached */
        assert(FeedAt(30U, ++seq, start + 111U, start + 113U)); /* gate equality */
        assert(r.machine.press_feedback.feedback_pending);
        assert(r.machine.press_feedback.low_response_count == 0U);
        assert(FakeMotorHwReal_GetState()->apply_count == 1U);
        Feed(30U, start + 114U); AssertPulse(MOTOR_DIRECTION_PRESS, 3000U);
        assert(r.machine.press_feedback.low_response_count == 1U);
        assert(g_sd700_approach_diagnostics.press.feedback_valid);
        assert(g_sd700_approach_diagnostics.press.before_units == 30);
        assert(g_sd700_approach_diagnostics.press.observed_peak_units == 50);
        assert(g_sd700_approach_diagnostics.press.after_units == 30);
        assert(g_sd700_approach_diagnostics.press.observed_off_fall);
        Complete(start + 124U); Service(start + 174U);
        assert(!FeedAt(30U, seq, start + 175U, start + 175U));
        Service(start + 180U); /* polls/ticks and duplicate completion add nothing */
        Machine_HandleMotorService(&r.machine, start + 180U);
        assert(r.machine.press_feedback.low_response_count == 1U);
        assert(r.machine.press_feedback.boost_mv == 0U);
        Feed(30U, start + 181U); AssertPulse(MOTOR_DIRECTION_PRESS, 3300U);
        now = PressResponse(300U, start + 181U);
        AssertPulse(MOTOR_DIRECTION_RELEASE, 400U);
        assert(r.machine.press_feedback.boost_mv == 0U && !r.machine.press_feedback.feedback_pending);
        Complete(now + 10U); Settled(30U, now + 60U);
        AssertPulse(MOTOR_DIRECTION_PRESS, 3000U);
        Stop(now + 62U);
    }
    puts("PRESS_NEW_SETTLED_NORMAL_ONLY_WRAP_RELEASE_RESET_OFF_FALL_DIAGNOSTIC=PASS SYNTHETIC_INPUT");
}

static void TestPressBoostStops(void)
{
    unsigned scenario;
    for (scenario = 0U; scenario < 10U; ++scenario)
    {
        uint32_t now = 51U, count;
        Init(30U, 0U); Start(0U); Settled(30U, 50U);
        now = PressResponse(30U, now); now = PressResponse(30U, now);
        now = PressResponse(32U, now); now = PressResponse(32U, now);
        assert(r.machine.press_feedback.boost_mv == 300U);
        assert(r.machine.press_feedback.low_response_count == 1U);
        count = FakeMotorHwReal_GetState()->apply_count;
        if (scenario == 0U) { Stop(now + 1U); }
        else if (scenario == 1U) { Complete(now + 10U); Stop(now + 11U); }
        else if (scenario == 2U) { Complete(now + 10U); Service(now + 60U); Stop(now + 61U); }
        else if (scenario == 3U) { FakeMotorStopTimer_TriggerBackstop(); Service(now + 40U); }
        else if (scenario == 4U) { FakeMotorHwReal_ForceOutputDropped(); Service(now + 1U); }
        else if (scenario == 5U) { Feed(325U, now + 1U); }
        else if (scenario == 6U) { Service(now + 201U); }
        else if (scenario == 7U)
        {
            assert(!FeedAt(30U, seq - 1U, now + 1U, now + 1U));
            assert(r.machine.press_feedback.boost_mv == 300U);
            Service(now + 201U); /* Runtime discarded old receiver snapshot; no freshness renewal */
        }
        else if (scenario == 8U)
        {
            MachinePressureSample sample = r.machine.pressure;
            sample.frame_valid = false;
            Machine_HandlePressureSample(&r.machine, &sample, now + 1U);
        }
        else
        {
            Complete(now + 10U); Service(now + 60U);
            r.machine.config.settle_feedback_timeout_ms = 80U;
            /* Still fresh at exactly settle feedback timeout: no blind boost. */
            assert(FeedAt(30U, ++seq, now + 60U, now + 100U));
            Service(now + 140U);
        }
        printf("PRESS_STOP_CASE=%u state=%d fault=%d detail=%d\n", scenario,
               (int)r.machine.state, (int)r.machine.fault, (int)r.machine.fault_detail);
        assert(r.machine.state == (scenario < 3U ? IDLE : FAULT));
        assert(MotorExecutor_OutputIsDisabled());
        assert(r.machine.press_feedback.boost_mv == 0U && r.machine.press_feedback.low_response_count == 0U);
        assert(!r.machine.press_feedback.in_flight && !r.machine.press_feedback.feedback_pending);
        FakeMotorStopTimer_GetState()->handler(MOTOR_STOP_TIMER_NORMAL);
        Feed(30U, now + 320U); Service(now + 320U);
        assert(FakeMotorHwReal_GetState()->apply_count == count && MotorExecutor_OutputIsDisabled());
        if (scenario < 3U)
        {
            Start(now + 321U); Settled(30U, now + 371U);
            AssertPulse(MOTOR_DIRECTION_PRESS, 3000U);
            assert(g_sd700_approach_diagnostics.press.normal_completion_count == 0U);
            Stop(now + 373U);
        }
    }
    puts("PRESS_STOP_FAULT_STALE_INVALID_ORDER_NO_FEEDBACK_RESET_NO_RESTART=PASS SYNTHETIC_INPUT");
}

static void TestPressExecutorNormalCompletion(void)
{
    uint32_t now = 51U;
    unsigned i;
    Init(30U, 0U); Start(0U); Settled(30U, 50U);
    for (i = 0U; i < 8U; ++i)
    {
        uint32_t request = r.machine.motor_request_sequence;
        const MotorExecutorSnapshot *m = MotorExecutor_GetSnapshot();
        assert(r.machine.state == AUTO_PULSE && m->logical_active);
        assert(m->direction == MOTOR_DIRECTION_PRESS && m->command_mv > 0U);
        assert(m->requested_duration_ms == 10U);
        assert(FakeMotorHwReal_GetState()->driver_enabled);
        assert(FakeMotorHwReal_GetState()->last_apply_saw_timer_armed);
        printf("PRESS_NORMAL_CHAIN request=%lu mv=%lu SYNTHETIC_INPUT pressure=30\n",
               (unsigned long)request, (unsigned long)m->command_mv);
        FakeMotorStopTimer_TriggerNormal();
        assert(MotorExecutor_OutputIsDisabled());
        assert(MotorExecutor_Service(now + 10U) == MOTOR_RESULT_OK);
        assert(m->request_sequence == request && !m->logical_active);
        assert(m->last_completion == MOTOR_COMPLETION_NORMAL);
        Service(now + 10U);
        assert(r.machine.state == AUTO_SETTLE);
        assert(r.machine.fault == FAULT_NONE);
        assert(FakeMotorHwReal_GetState()->apply_count == i + 1U);
        assert(r.machine.cycle_started_ms == 0U);
        if (i < 7U) { Settled(30U, now + 60U); now += 61U; }
    }
    Stop(now + 11U);
    puts("PRESS_NORMAL_COMPLETION_REAL_EXECUTOR=PASS SYNTHETIC_INPUT");
}

/* Initial gap measurement: real executor plus fake TIM5/HW, SYNTHETIC_INPUT. */
static void RunZeroApproach(uint32_t start, uint32_t elapsed_end)
{
    uint32_t elapsed;
    for (elapsed = 1U; elapsed <= elapsed_end; ++elapsed)
    {
        uint32_t now = start + elapsed;
        const MotorExecutorSnapshot *m = MotorExecutor_GetSnapshot();
        if (m->logical_active)
        {
            assert(r.machine.state == AUTO_APPROACH);
            assert(m->command_mv == 10000U && m->requested_duration_ms == 20U);
            assert(FakeMotorStopTimer_GetState()->normal_duration_ms == 20U);
            assert(FakeMotorStopTimer_GetState()->backstop_ms == 50U);
            if ((uint32_t)(now - r.machine.state_entered_ms) >= 20U)
            { FakeMotorStopTimer_TriggerNormal(); }
        }
        if ((elapsed % 100U) == 0U) { Feed(0U, now); }
        Service(now);
        assert(r.machine.fault == FAULT_NONE);
        assert(!r.machine.auto_has_contacted && r.machine.auto_approach_pending);
        assert(r.machine.cycle_started_ms == start);
    }
}

static void TestLongInitialApproach(void)
{
    uint32_t count;
    Init(0U, 1000U); Start(1000U);
    RunZeroApproach(1000U, 35000U); /* initial search exceeds the new 30000 ms budget */
    count = FakeMotorHwReal_GetState()->apply_count;
    assert(count > 100U);
    AssertApproach(count, 20U);
    Feed(20U, 36001U);
    assert(r.machine.state == AUTO_SETTLE && MotorExecutor_OutputIsDisabled());
    assert(r.machine.auto_has_contacted && !r.machine.auto_approach_pending);
    assert(r.machine.cycle_started_ms == 36001U);
    assert(!g_sd700_approach_diagnostics.search_active);
    assert(g_sd700_approach_diagnostics.first_contact_latched);
    assert(g_sd700_approach_diagnostics.search_elapsed_ms == 35001U);
    assert(g_sd700_approach_diagnostics.first_contact_pulse_count == count);
    assert(g_sd700_approach_diagnostics.first_contact_pressure_units == 20);
    assert(g_sd700_approach_diagnostics.first_contact_received_at_ms == 36001U);
    assert(g_sd700_approach_diagnostics.first_contact_decided_at_ms == 36001U);
    assert(g_sd700_approach_diagnostics.first_contact_sample_sequence == seq);
    assert(FakeMotorHwReal_GetState()->apply_count == count);
    Settled(30U, 36051U); AssertPulse(MOTOR_DIRECTION_PRESS, 3000U);
    Stop(36053U);
    assert(g_sd700_approach_diagnostics.search_elapsed_ms == 35001U);
    assert(g_sd700_approach_diagnostics.first_contact_pulse_count == count);
    puts("APPROACH_MEASURE_LONG_ZERO_BOUNDED_CONTACT_PRESSBOOST=PASS SYNTHETIC_INPUT");
}

static void TestLongApproachStopsAndMeasurement(void)
{
    unsigned int scenario;
    for (scenario = 0U; scenario < 10U; ++scenario)
    {
        uint32_t start = UINT32_MAX - 500U;
        uint32_t now = start + 12000U;
        uint32_t count, stopped_elapsed;
        Init(0U, start); Start(start); RunZeroApproach(start, 12000U);
        count = FakeMotorHwReal_GetState()->apply_count;
        AssertApproach(count, 20U);
        assert(g_sd700_approach_diagnostics.search_active);
        assert(g_sd700_approach_diagnostics.search_elapsed_ms == 12000U);
        if (scenario == 0U) { Stop(now + 1U); }
        else if (scenario == 1U)
        { Complete(now + 20U); Stop(now + 25U); } /* output OFF between pulses */
        else if (scenario == 2U)
        { Complete(now + 20U); Service(now + 70U); Stop(now + 80U); }
        else if (scenario == 3U) { Feed(325U, now + 1U); }
        else if (scenario == 4U)
        { Machine_ReportFault(&r.machine, FAULT_OVERCURRENT, FAULT_DETAIL_NONE, now + 1U); }
        else if (scenario == 5U)
        { Complete(now + 20U); Service(now + 201U); }
        else if (scenario == 6U)
        {
            MachinePressureSample invalid = r.machine.pressure;
            invalid.frame_valid = false;
            Machine_HandlePressureSample(&r.machine, &invalid, now + 1U);
        }
        else if (scenario == 7U)
        { FakeMotorStopTimer_TriggerBackstop(); Service(now + 1U); }
        else if (scenario == 8U)
        { FakeMotorStopTimer_TriggerError(); Service(now + 1U); }
        else
        { Complete(now + 20U); Service(now + 70U); Feed(0U, now + 200U); } /* eligible new pulse */
        if (scenario == 9U)
        {
            assert(r.machine.state == AUTO_APPROACH);
            Stop(now + 201U); ++count;
        }
        assert(r.machine.state == ((scenario < 3U || scenario == 9U) ? IDLE : FAULT));
        assert(MotorExecutor_OutputIsDisabled() && !FakeMotorStopTimer_GetState()->armed);
        assert(!g_sd700_approach_diagnostics.search_active);
        assert(!g_sd700_approach_diagnostics.first_contact_latched);
        assert(g_sd700_approach_diagnostics.pulse_count == count);
        stopped_elapsed = g_sd700_approach_diagnostics.search_elapsed_ms;
        assert(stopped_elapsed >= 12001U && stopped_elapsed <= 12201U);
        if (scenario == 5U) { assert(r.machine.fault_detail == FAULT_DETAIL_PRESSURE_TIMEOUT); }
        if (scenario == 6U) { assert(r.machine.fault_detail == FAULT_DETAIL_PRESSURE_INVALID); }
        FakeMotorStopTimer_GetState()->handler(MOTOR_STOP_TIMER_NORMAL);
        Feed(250U, now + 300U); Service(now + 300U);
        assert(MotorExecutor_OutputIsDisabled() && FakeMotorHwReal_GetState()->apply_count == count);
        assert(g_sd700_approach_diagnostics.search_elapsed_ms == stopped_elapsed);
        assert(!g_sd700_approach_diagnostics.first_contact_latched);
    }
    puts("APPROACH_MEASURE_LONG_STOP_FAULT_SENSOR_BACKSTOP_WRAP_NO_RESTART=PASS SYNTHETIC_INPUT");
}

int main(void)
{
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    TestLongInitialApproach(); TestLongApproachStopsAndMeasurement();
    TestPressExecutorNormalCompletion();
    TestSegmentedPressAndBoost(); TestPressFeedbackEligibility(); TestPressBoostStops();
    TestRetainedBoostFeedbackGates(); TestRetainedBoostContactHoldReleaseReset();
    TestFullCycle(); TestContactStartAndClamp(); TestFeedbackGates(); TestWrap();
    TestNoResponseDeadlines(); TestAbortAndCompletion(); TestStopAllStates();
    TestExtendedConvergenceSafetyAndHold();
    TestConfigAndDiagnostics(); TestFaultAllStates(); TestLateRecontactBudget();
    TestCoarseApproachFeedbackAndContact(); TestCoarseContactCompletionRaces();
    TestCoarseImmediateStopAndFault();
    if (TestContactDeadline() != 0) { return 1; }
    TestContactDeadlineRejections(); TestContactDeadlineEventOrdering();
    TestContactDoesNotRelaxFineDeadlines();
    TestContactEvidenceIsNotSettledFeedback();
    puts("AUTO_TARGET_RUNTIME_REAL_EXECUTOR_FAKE_TIM5_HW=PASS SYNTHETIC_INPUT");
    return 0;
}
