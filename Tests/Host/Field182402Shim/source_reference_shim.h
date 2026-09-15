#ifndef SOURCE_REFERENCE_SHIM_H
#define SOURCE_REFERENCE_SHIM_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef enum { IR2104_MODE_STOP, IR2104_MODE_POSITIVE, IR2104_MODE_NEGATIVE, IR2104_MODE_BRAKE } IR2104_Mode_t;
typedef void *TimerHandle_t;
typedef void (*TimerCallbackFunction_t)(TimerHandle_t);
#define pdMS_TO_TICKS(n) (n)
#define pdFALSE 0
#define pdPASS 1
uint32_t HAL_GetTick(void);
TimerHandle_t xTimerCreate(const char *,uint32_t,int,void *,TimerCallbackFunction_t);
int xTimerChangePeriod(TimerHandle_t,uint32_t,int);
int xTimerStop(TimerHandle_t,int);
void *pvTimerGetTimerID(TimerHandle_t);
void vTimerSetTimerID(TimerHandle_t,void *);
void IR2104_Init(void);
void IR2104_Brake(void);
void IR2104_Enable(void);
void IR2104_Disable(void);
void IR2104_EmergencyStop(void);
void IR2104_SetMode(IR2104_Mode_t);
void IR2104_SetVoltage(float);
float IR2104_GetTargetVoltage(void);
void interface_press_init(void);
extern uint32_t RS485_2_last_receive_time;
extern struct OraclePressure { int actual_pressure; } pressure_sys;
extern int current_interface,pressure_substate,updowm_substate,key_state,is_fast_updown;
extern uint32_t Up_start_time,Down_start_time;
enum { INTERFACE_PRESSURE,INTERFACE_ONE_TOUCH,PRESSURE_AUTO_RUN,PRESSURE_HOLD,
 PRESSURE_JOG_UP,PRESSURE_JOG_DOWN,PRESSURE_FAST_UP,PRESSURE_FAST_DOWN,PRESSURE_IDLE,
 KEY_BACK_LONG,KEY_NONE,KEY_UP_LONG,KEY_DOWN_LONG,FAST_STOP,FAST_NONE,FAST_UP,FAST_DOWN,
 UPDOWN_UP,UPDOWN_DOWN,UPDOWN_IDLE };
#endif
