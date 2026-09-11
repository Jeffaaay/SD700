#ifndef TESTS_HOST_FAKE_STM32_HAL_H
#define TESTS_HOST_FAKE_STM32_HAL_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

typedef struct
{
    uint32_t pclk1_hz;
    uint32_t force_disable_count;
    uint32_t fail_disable_on_call;
    TIM_TypeDef *fail_pwm_init_instance;
    TIM_TypeDef *fail_channel_instance;
    TIM_TypeDef *fail_stop_instance;
    TIM_TypeDef *fail_start_instance;
    uint32_t pwm_start_call_count;
    uint32_t tim2_pwm_start_count;
    uint32_t tim3_pwm_start_count;
    uint32_t tim2_last_start_order;
    uint32_t tim3_last_start_order;
    uint32_t sd_enable_call_count;
    uint32_t tim2_ccr3_at_sd_enable;
    uint32_t tim3_ccr3_at_sd_enable;
    bool tim2_cc3e_at_sd_enable;
    bool tim2_cen_at_sd_enable;
    bool tim3_cc3e_at_sd_enable;
    bool tim3_cen_at_sd_enable;
    bool both_zero_carriers_running_at_sd_enable;
    bool both_ccr_nonzero_violation;
    bool break_zero_compare_commit;
    bool break_zero_carrier_verify;
    bool break_driver_enable_verify;
    bool break_active_compare_verify;
    bool output_arming_allowed;
    bool tim5_irq_enabled;
    bool tim5_irq_pending;
} FakeStm32HalState;

void FakeStm32Hal_Reset(void);
FakeStm32HalState *FakeStm32Hal_GetState(void);
void FakeStm32Hal_AssertMotorDisabled(void);

#endif
