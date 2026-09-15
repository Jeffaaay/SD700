/* Compile the complete, byte-preserved working-tree source as the oracle.
 * Only its platform I/O/timer is simulated; no snapshots/README formulas. */
#include "Tests/Host/field182402_oracle.h"
#include "source_reference_shim.h"
#include <math.h>
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
#endif
#include "Tests/Host/Reference/Field182402/pressure_control.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
static uint32_t oracle_now,oracle_end,oracle_duration;
static void *timer_id;
static TimerCallbackFunction_t timer_callback;
static bool timer_active,braking;
static float voltage;
uint32_t RS485_2_last_receive_time;
struct OraclePressure pressure_sys;
int current_interface,pressure_substate,updowm_substate,key_state,is_fast_updown;
uint32_t Up_start_time,Down_start_time;
uint32_t HAL_GetTick(void) { return oracle_now; }
TimerHandle_t xTimerCreate(const char *name,uint32_t period,int reload,void *id,TimerCallbackFunction_t cb)
{ (void)name; (void)period; (void)reload; timer_id=id; timer_callback=cb; return &timer_id; }
int xTimerChangePeriod(TimerHandle_t timer,uint32_t ms,int block)
{ (void)timer; (void)block; oracle_end=oracle_now+ms; oracle_duration=ms; timer_active=true; return pdPASS; }
int xTimerStop(TimerHandle_t timer,int block) { (void)timer;(void)block;timer_active=false;return pdPASS; }
void *pvTimerGetTimerID(TimerHandle_t timer) { (void)timer;return timer_id; }
void vTimerSetTimerID(TimerHandle_t timer,void *id) { (void)timer;timer_id=id; }
void IR2104_Init(void) { voltage=0; braking=false; }
void IR2104_Brake(void) { voltage=0;braking=true; }
void IR2104_Enable(void) { }
void IR2104_Disable(void) { voltage=0;braking=false; }
void IR2104_EmergencyStop(void) { IR2104_Disable(); }
void IR2104_SetMode(IR2104_Mode_t mode) { if (mode==IR2104_MODE_BRAKE) IR2104_Brake(); }
void IR2104_SetVoltage(float v) { voltage=v;braking=false; }
float IR2104_GetTargetVoltage(void) { return voltage; }
void interface_press_init(void) { }
void FieldOracle_Reset(float target,int force,uint32_t now)
{
 oracle_now=now-10; timer_active=false; oracle_duration=0;
 PressureControl_Init(&g_pressure_sys); PressureControl_SetTarget(&g_pressure_sys,target);
 FieldOracle_Feed(force,now);
}
void FieldOracle_Time(uint32_t now)
{
 oracle_now=now;
 if (timer_active && (int32_t)(now-oracle_end)>=0) { timer_active=false;timer_callback(&timer_id); }
}
void FieldOracle_Feed(int force,uint32_t now)
{ FieldOracle_Time(now); pressure_sys.actual_pressure=force;RS485_2_last_receive_time=now;PressureControl_Update(&g_pressure_sys); }
int32_t FieldOracle_Command(void) { return (int32_t)lroundf(voltage*1000); }
uint32_t FieldOracle_Duration(void) { return oracle_duration; }
uint32_t FieldOracle_Level(void) { return s_precision_level; }
uint32_t FieldOracle_Escape(void) { return s_precision_escape_steps; }
uint32_t FieldOracle_Boost(void) { return (uint32_t)lroundf(s_pulse_boost*1000); }
int FieldOracle_Braking(void) { return braking; }
