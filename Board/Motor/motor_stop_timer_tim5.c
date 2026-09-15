#include "Board/Motor/motor_stop_timer.h"

#include <limits.h>
#include <stddef.h>

#include "stm32f4xx_hal.h"
#include "Application/motion_build_policy.h"
#if SD700_FORCE_SERVO_COMMISSIONING
#include "Application/force_servo.h"
#define MOTOR_STOP_MAX_LEASE_MS FORCE_SERVO_COMMISSIONING_LEASE_MS
#else
#define MOTOR_STOP_MAX_LEASE_MS 100U
#endif

#define MOTOR_STOP_TIMER_TICKS_PER_MS 10U
#define MOTOR_STOP_TIMER_TICK_HZ      10000U
#define MOTOR_STOP_TIMER_IRQ_PRIORITY 4U

static MotorStopTimerHandler s_handler;
static uint32_t s_prescaler;
static bool s_initialized;
static bool s_armed;
static bool s_healthy;
static MotorFailureStage s_last_failure_stage;
#if SD700_BUILD_TO_TARGET
static bool s_build_normal_callback, s_build_handoff;
#endif

static uint32_t MotorStopTimer_InputClockHz(void)
{
    uint32_t clock_hz = HAL_RCC_GetPCLK1Freq();

    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1)
    {
        clock_hz *= 2U;
    }
    return clock_hz;
}

void MotorStopTimer_Cancel(void)
{
    TIM5->DIER = 0U;
    TIM5->CR1 &= ~TIM_CR1_CEN;
    TIM5->SR = 0U;
    TIM5->CNT = 0U;
    s_armed = false;
    HAL_NVIC_DisableIRQ(TIM5_IRQn);
    HAL_NVIC_ClearPendingIRQ(TIM5_IRQn);
    __DSB();
}

bool MotorStopTimer_Initialize(MotorStopTimerHandler handler)
{
    uint32_t timer_clock_hz;
    volatile uint32_t clock_enable_readback;

    s_last_failure_stage = MOTOR_FAILURE_STAGE_NONE;
    s_handler = handler;
    s_initialized = false;
    s_armed = false;
    s_healthy = false;
    if (handler == NULL)
    {
        s_last_failure_stage =
            MOTOR_FAILURE_STAGE_TIMER_INIT_NULL_HANDLER;
        return false;
    }

    RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;
    clock_enable_readback = RCC->APB1ENR;
    (void)clock_enable_readback;
    MotorStopTimer_Cancel();

    timer_clock_hz = MotorStopTimer_InputClockHz();
    if ((timer_clock_hz == 0U) ||
        ((timer_clock_hz % MOTOR_STOP_TIMER_TICK_HZ) != 0U) ||
        ((timer_clock_hz / MOTOR_STOP_TIMER_TICK_HZ) == 0U) ||
        ((timer_clock_hz / MOTOR_STOP_TIMER_TICK_HZ) > 65536U))
    {
        s_last_failure_stage =
            MOTOR_FAILURE_STAGE_TIMER_INIT_CLOCK_INVALID;
        return false;
    }

    s_prescaler = (timer_clock_hz / MOTOR_STOP_TIMER_TICK_HZ) - 1U;
    TIM5->CR1 = TIM_CR1_OPM | TIM_CR1_URS;
    TIM5->CR2 = 0U;
    TIM5->SMCR = 0U;
    TIM5->CCMR1 = 0U;
    TIM5->CCER = 0U;
    TIM5->PSC = s_prescaler;
    TIM5->ARR = UINT32_MAX;
    TIM5->CCR1 = 0U;
    TIM5->EGR = TIM_EGR_UG;
    TIM5->SR = 0U;
    HAL_NVIC_SetPriority(TIM5_IRQn,
                         MOTOR_STOP_TIMER_IRQ_PRIORITY,
                         0U);
    s_initialized = true;
    s_healthy = true;
    return true;
}

bool MotorStopTimer_Arm(uint32_t normal_duration_ms,
                        uint32_t backstop_ms)
{
    uint64_t normal_ticks;
    uint64_t backstop_ticks;

    s_last_failure_stage = MOTOR_FAILURE_STAGE_NONE;
    MotorStopTimer_Cancel();
    if ((!s_initialized) || (!s_healthy) ||
        (normal_duration_ms == 0U) ||
        (backstop_ms <= normal_duration_ms))
    {
        s_last_failure_stage =
            MOTOR_FAILURE_STAGE_TIMER_ARM_PRECONDITION;
        return false;
    }

    normal_ticks = (uint64_t)normal_duration_ms *
                   MOTOR_STOP_TIMER_TICKS_PER_MS;
    backstop_ticks = (uint64_t)backstop_ms *
                     MOTOR_STOP_TIMER_TICKS_PER_MS;
    if ((normal_ticks > UINT32_MAX) ||
        (backstop_ticks > UINT32_MAX))
    {
        s_last_failure_stage = MOTOR_FAILURE_STAGE_TIMER_ARM_RANGE;
        return false;
    }

    TIM5->CR1 = TIM_CR1_OPM | TIM_CR1_URS;
    TIM5->PSC = s_prescaler;
    TIM5->ARR = (uint32_t)backstop_ticks - 1U;
    TIM5->CCR1 = (uint32_t)normal_ticks;
    TIM5->CNT = 0U;
    TIM5->EGR = TIM_EGR_UG;
    TIM5->SR = 0U;
    TIM5->DIER = TIM_DIER_CC1IE | TIM_DIER_UIE;
    HAL_NVIC_ClearPendingIRQ(TIM5_IRQn);
    s_armed = true;
    TIM5->CR1 |= TIM_CR1_CEN;
    __DSB();
    if ((TIM5->CR1 & TIM_CR1_CEN) == 0U)
    {
        s_last_failure_stage =
            MOTOR_FAILURE_STAGE_TIMER_ARM_START_VERIFY;
        s_healthy = false;
        MotorStopTimer_Cancel();
        return false;
    }
    return true;
}

bool MotorStopTimer_CommitArm(void)
{
    uint32_t status;

    s_last_failure_stage = MOTOR_FAILURE_STAGE_NONE;
    if ((!s_armed) || (!s_healthy))
    {
        s_last_failure_stage =
            MOTOR_FAILURE_STAGE_TIMER_COMMIT_PRECONDITION;
        return false;
    }

    status = TIM5->SR;
    if ((status & (TIM_SR_UIF | TIM_SR_CC1IF | TIM_SR_CC1OF)) != 0U)
    {
        s_last_failure_stage =
            MOTOR_FAILURE_STAGE_TIMER_COMMIT_PENDING_EVENT;
        MotorStopTimer_IrqHandler();
        s_last_failure_stage =
            MOTOR_FAILURE_STAGE_TIMER_COMMIT_PENDING_EVENT;
        return false;
    }

    HAL_NVIC_EnableIRQ(TIM5_IRQn);
    __DSB();
    return true;
}

bool MotorStopTimer_IsArmed(void)
{
    return s_armed && ((TIM5->CR1 & TIM_CR1_CEN) != 0U);
}

bool MotorStopTimer_IsHealthy(void)
{
    return s_initialized && s_healthy;
}

MotorFailureStage MotorStopTimer_GetLastFailureStage(void)
{
    return s_last_failure_stage;
}

void MotorStopTimer_IrqHandler(void)
{
    MotorStopTimerEvent event;
    uint32_t status = TIM5->SR;

    if (s_handler == NULL)
    {
        s_last_failure_stage =
            MOTOR_FAILURE_STAGE_TIMER_IRQ_MISSING_HANDLER;
        s_healthy = false;
        MotorStopTimer_Cancel();
        return;
    }
    if (!s_armed)
    {
        s_last_failure_stage =
            MOTOR_FAILURE_STAGE_TIMER_IRQ_NOT_ARMED;
        s_healthy = false;
        s_handler(MOTOR_STOP_TIMER_ERROR);
        MotorStopTimer_Cancel();
        return;
    }

    if ((status & TIM_SR_CC1OF) != 0U)
    {
        event = MOTOR_STOP_TIMER_ERROR;
        s_last_failure_stage =
            MOTOR_FAILURE_STAGE_TIMER_IRQ_COMPARE_OVERCAPTURE;
        s_healthy = false;
    }
    else if ((status & TIM_SR_UIF) != 0U)
    {
        event = MOTOR_STOP_TIMER_BACKSTOP;
    }
    else if ((status & TIM_SR_CC1IF) != 0U)
    {
        event = MOTOR_STOP_TIMER_NORMAL;
    }
    else
    {
        event = MOTOR_STOP_TIMER_ERROR;
        s_last_failure_stage =
            MOTOR_FAILURE_STAGE_TIMER_IRQ_UNKNOWN_STATUS;
        s_healthy = false;
    }

#if SD700_BUILD_TO_TARGET
    s_build_handoff=false;
    s_build_normal_callback=event==MOTOR_STOP_TIMER_NORMAL && (TIM5->CR1 & TIM_CR1_OPM);
    if (s_build_normal_callback) {
        /* Acknowledge ONLY the normal compare. A racing UIF/overcapture stays
         * latched and prevents extending the original pulse hard deadline. */
#if SD700_MOTOR_REAL_HOST_TEST
        TIM5->SR &= ~TIM_SR_CC1IF;
#else
        TIM5->SR = ~TIM_SR_CC1IF;
#endif
    }
#endif
    s_handler(event);
#if SD700_BUILD_TO_TARGET
    s_build_normal_callback=false;
    if (s_build_handoff) { s_build_handoff=false; return; }
#endif
    MotorStopTimer_Cancel();
}

/* ForceServo's free-running compare lease. Caller serializes IRQ/STOP/update.
 * Renewal never stops the counter or clears SR/NVIC pending events. */
bool MotorStopTimer_ArmLease(uint32_t remaining_ms)
{
    if (!s_initialized || !s_healthy || s_armed || remaining_ms <= 1U || remaining_ms > MOTOR_STOP_MAX_LEASE_MS)
        return false;
    MotorStopTimer_Cancel(); /* Only legal with verified output OFF, new session. */
    TIM5->CR1 = TIM_CR1_URS;
    TIM5->PSC = s_prescaler;
    TIM5->ARR = UINT32_MAX;
    TIM5->CCR1 = (remaining_ms - 1U) * MOTOR_STOP_TIMER_TICKS_PER_MS;
    TIM5->CNT = 0U;
    TIM5->EGR = TIM_EGR_UG;
    TIM5->SR = 0U;
    TIM5->DIER = TIM_DIER_CC1IE | TIM_DIER_UIE;
    s_armed = true;
    TIM5->CR1 |= TIM_CR1_CEN;
    return MotorStopTimer_CommitArm();
}

bool MotorStopTimer_RenewLease(uint32_t remaining_ms)
{
    uint32_t counter = TIM5->CNT;
    uint32_t old_deadline = TIM5->CCR1;
    if (!MotorStopTimer_IsArmed() || !s_healthy || remaining_ms <= 1U || remaining_ms > MOTOR_STOP_MAX_LEASE_MS)
        return false;
    if ((TIM5->SR & (TIM_SR_UIF | TIM_SR_CC1IF | TIM_SR_CC1OF)) != 0U) {
        MotorStopTimer_IrqHandler();
        return false;
    }
    if ((int32_t)(counter - old_deadline) >= 0) {
        s_handler(MOTOR_STOP_TIMER_NORMAL);
        MotorStopTimer_Cancel();
        return false;
    }
    TIM5->CCR1 = counter + (remaining_ms - 1U) * MOTOR_STOP_TIMER_TICKS_PER_MS;
    __DSB();
    /* A compare event between the read and write remains latched. Never revive it. */
    return MotorStopTimer_CommitArm();
}

#if SD700_BUILD_TO_TARGET
static bool MotorStopTimer_BuildTransitionReady(void)
{
    return MotorStopTimer_IsArmed() && s_healthy &&
        !(TIM5->SR & (TIM_SR_UIF|TIM_SR_CC1IF|TIM_SR_CC1OF));
}
bool MotorStopTimer_BuildHandoff(uint32_t remaining_ms)
{
    /* The executor has already verified the bounded forward compare while the
     * original hard ARR was still armed. No expired high pulse is revived. */
    if (!MotorStopTimer_BuildTransitionReady() || !(TIM5->CR1 & TIM_CR1_OPM) ||
        TIM5->CNT>=TIM5->ARR || remaining_ms<=1 || remaining_ms>MOTOR_STOP_MAX_LEASE_MS) return false;
    uint32_t counter=TIM5->CNT;
    TIM5->CCR1=counter+(remaining_ms-1U)*MOTOR_STOP_TIMER_TICKS_PER_MS;
    TIM5->ARR=UINT32_MAX;
    TIM5->CR1 &= ~TIM_CR1_OPM;
    __DSB();
    if (!MotorStopTimer_BuildTransitionReady() || (int32_t)(TIM5->CNT-TIM5->CCR1)>=0) return false;
    if (s_build_normal_callback) s_build_handoff=true;
    return true;
}
bool MotorStopTimer_BuildPulse(uint32_t normal_ms,uint32_t hard_ms)
{
    if (!MotorStopTimer_BuildTransitionReady() || (TIM5->CR1 & TIM_CR1_OPM) ||
        (int32_t)(TIM5->CNT-TIM5->CCR1)>=0 || !normal_ms || hard_ms<=normal_ms ||
        hard_ms>MOTOR_STOP_MAX_LEASE_MS) return false;
    uint32_t counter=TIM5->CNT;
    TIM5->CCR1=counter+normal_ms*MOTOR_STOP_TIMER_TICKS_PER_MS;
    TIM5->ARR=counter+hard_ms*MOTOR_STOP_TIMER_TICKS_PER_MS-1U;
    TIM5->CR1 |= TIM_CR1_OPM;
    __DSB();
    return MotorStopTimer_BuildTransitionReady() && TIM5->CNT<TIM5->ARR;
}
#endif
