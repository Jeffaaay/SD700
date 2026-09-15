#ifndef __PRESSURE_CONTROL_H
#define __PRESSURE_CONTROL_H

#include "./SYSTEM/sys/sys.h"
#include <stdint.h>
#include <stdbool.h>

/* 系统参数定义 */
#define MAX_PRESSURE_N              3000.0f    /* 软件目标压力上限3000N */
#define PRESSURE_ERROR_COARSE_HIGH  50.0f      /* 粗调上限 +50N: PID控制 */
#define PRESSURE_ERROR_COARSE_LOW  -50.0f      /* 粗调下限 -50N: PID控制 */
#define PRESSURE_ERROR_MICRO_HIGH    3.0f      /* 微调上限 +10N: 脉冲点动 */
#define PRESSURE_ERROR_MICRO_LOW    -3.0f      /* 微调下限 -10N: 脉冲点动 */
#define PRESSURE_ERROR_FINE_HIGH    1.0f       /* 精调上限 +1N: 精细脉冲 */
#define PRESSURE_ERROR_FINE_LOW    -1.0f       /* 精调下限 -1N: 精细脉冲 */
#define PRESSURE_ERROR_HOLD_HIGH    0.0f       /* 低于目标1N仍精调；达到目标才保持 */
#define PRESSURE_ERROR_HOLD_LOW    -1.0f       /* 保持下限 (同精调) */

/* 微调脉冲电压范围 (误差比例缩放) */
#define MICRO_PULSE_VOLTAGE_MIN    0.8f        /* 微调最小脉冲电压 (V) - 误差接近MICRO阈值时 */
#define MICRO_PULSE_VOLTAGE_MAX    3.0f        /* 微调最大脉冲电压 (V) - 误差接近COARSE阈值时 */

/* 精调脉冲电压范围 (误差比例缩放) */
#define FINE_PULSE_VOLTAGE_MIN     0.4f        /* 精调最小脉冲电压 (V) - 误差接近FINE阈值时 */
#define FINE_PULSE_VOLTAGE_MAX     1.0f        /* 精调最大脉冲电压 (V) - 误差接近MICRO阈值时 */

/* 脉冲冷却时间 (ms) - 已废弃: 脉冲节奏现改为"等待新反馈帧"驱动(见pressure_control.c)，此值仅用于初始化兼容 */
#define PULSE_COOLDOWN_MS          150

/* 控制模式定义  */
typedef enum {
    PRESSURE_MODE_COARSE = 0,    /* 加压粗调 */
    PRESSURE_MODE_MICRO,         /* 加压微调 */
    PRESSURE_MODE_FINE,          /* 加压精调 */
    PRESSURE_MODE_HOLD,          /* 停机定压 */
    PRESSURE_MODE_STOP,          /* 停止 */
    PRESSURE_MODE_EMERGENCY      /* 紧急停止 */
} PressureControlMode_t;

/* PID控制器结构 */
typedef struct {
    float Kp;                    /* 比例系数: 0.124  */
    float Ki;                    /* 积分系数: 0.00247  */
    float Kd;                    /* 微分系数: 0.0013  */
    float error_sum;             /* 误差积分 */
    float last_error;            /* 上次误差 */
    float output;                /* 输出值 */
    float output_max;            /* 输出最大值 (24V) */
    float output_min;            /* 输出最小值 (-24V) */
} PID_Controller_t;

/* 压力控制状态结构 */
typedef struct {
    /* 控制参数 */
    float target_pressure;        /* 目标压力 (N) - 来自UI设置 */
    float current_pressure;       /* 当前压力 (N) - 来自传感器 */
    float pressure_error;         /* 压力误差 (N) */
    float max_pressure;           /* 最大压力限制 (N) */
    
    /* 控制模式 */
    PressureControlMode_t mode;   /* 当前控制模式 */
    PressureControlMode_t last_mode;  /* 上次控制模式 */
    
    /* 控制参数 */
    PID_Controller_t pid;         /* PID控制器 */
    uint32_t control_interval_ms; /* 控制间隔: 20ms (专利值) */
    uint32_t last_control_time;   /* 上次控制时间 */
    
    /* 脉冲控制参数 (微调和精调使用) */
    uint32_t pulse_start_time;    /* 脉冲开始时间 */
    bool pulse_active;            /* 脉冲是否激活 */
    uint32_t pulse_duration_ms;   /* 脉冲持续时间 (ms) */
    float pulse_voltage;          /* 脉冲电压 (V) */
    uint32_t pulse_end_time;      /* 脉冲结束时间 (用于冷却计时) */
    uint32_t pulse_cooldown_ms;   /* 脉冲冷却时间 (ms) */
    
    /* 控制标志 */
    bool enable;                  /* 使能控制 */
    bool emergency_stop;          /* 紧急停止标志 */
    
    /* 机械参数 (根据专利中的公式) */
    float spring_coefficient;     /* 弹性元件弹力系数 (N/mm) - 专利中的ka */
    float screw_pitch;            /* 螺纹孔螺距 (mm) - 专利中的ht */
    
} PressureControl_t;

/* 压力传感器接口 */
typedef struct {
    float offset;                  /* 零点偏移 */
    float scale;                   /* 比例系数 */
} PressureSensor_t;

/* 压力控制系统主结构 */
typedef struct {
    PressureControl_t control;     /* 压力控制器 */
    PressureSensor_t sensor;       /* 压力传感器 */
    
    /* 状态标志 */
    bool system_ready;             /* 系统就绪 */
    bool pressure_stable;          /* 压力稳定标志 */
    
    /* 统计数据 */
    uint32_t control_cycles;       /* 控制循环次数 */
    float max_error;               /* 最大误差 */
    float avg_error;               /* 平均误差 */
    
} PressureControlSystem_t;

/* 全局系统实例 */
extern PressureControlSystem_t g_pressure_sys;

/* 函数声明 */
void PressureControl_Init(PressureControlSystem_t *sys);
void PressureControl_SetTarget(PressureControlSystem_t *sys, float target_pressure);
void PressureControl_Update(PressureControlSystem_t *sys);
void PressureControl_EmergencyStop(PressureControlSystem_t *sys);
void PressureControl_Reset(PressureControlSystem_t *sys);
void PressureControl_Enable(PressureControlSystem_t *sys);
void PressureControl_Disable(PressureControlSystem_t *sys);
void PressureControl_SetMaxPressure(PressureControlSystem_t *sys, float max_pressure);
void PressureControl_Stop(PressureControlSystem_t *sys);

/* 压力转换函数 (按10:1换算) */
static inline float N_to_kg(float n) { return n / 10.0f; }
static inline float kg_to_N(float kg) { return kg * 10.0f; }

/* 控制模式判断函数 */
bool PressureControl_IsInCoarseRange(float error);
bool PressureControl_IsInMicroRange(float error);
bool PressureControl_IsInFineRange(float error);
bool PressureControl_IsInHoldRange(float error);

/* 获取当前控制状态 */
PressureControlMode_t PressureControl_GetCurrentMode(PressureControlSystem_t *sys);
float PressureControl_GetCurrentPressure(PressureControlSystem_t *sys);
float PressureControl_GetTargetPressure(PressureControlSystem_t *sys);
float PressureControl_GetPressureError(PressureControlSystem_t *sys);
bool PressureControl_IsStable(PressureControlSystem_t *sys);
/*手动控制函数*/
void PressureControl_ManualControl(void);
void PressureControl_OneTouchControl(void);

#endif /* __PRESSURE_CONTROL_H */
