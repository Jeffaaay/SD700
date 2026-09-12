#ifndef MOTOR_ATOMIC_H
#define MOTOR_ATOMIC_H
#include <stdint.h>
#if defined(STM32F411xE)
#include "stm32f4xx_hal.h"
static inline uint32_t MotorAtomic_Enter(void)
{ uint32_t p=__get_PRIMASK(); __disable_irq(); __DMB(); return p; }
static inline void MotorAtomic_Leave(uint32_t p) { __DMB(); __set_PRIMASK(p); }
static inline uint32_t MotorAtomic_Now(uint32_t supplied)
{ (void)supplied; return HAL_GetTick(); }
#else
/* Host implementation supplies deterministic critical-section/race injection. */
uint32_t MotorAtomic_Enter(void);
void MotorAtomic_Leave(uint32_t saved);
uint32_t MotorAtomic_Now(uint32_t supplied);
#endif
#endif
