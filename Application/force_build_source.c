/* Direct pre-target port of the supplied working tree, SHA256
 * E6E08DC4E370A853EA9A586D765D97D5B6A1E7D6DDC9760202B947F99FB4D512.
 * Platform bindings emit decisions only. Original source is a host fixture. */
#include "Application/force_build_source.h"
#include <math.h>
#include <string.h>
#define MAX_PRESSURE_N              3000.0f    /* 软件目标压力上限3000N */
#define PRESSURE_ERROR_COARSE_HIGH  50.0f      /* 粗调上限 +50N: PID控制 */
#define PRESSURE_ERROR_COARSE_LOW  -50.0f      /* 粗调下限 -50N: PID控制 */
#define PRESSURE_ERROR_MICRO_HIGH    3.0f      /* 微调上限 +10N: 脉冲点动 */
#define PRESSURE_ERROR_MICRO_LOW    -3.0f      /* 微调下限 -10N: 脉冲点动 */
#define PRESSURE_ERROR_FINE_HIGH    1.0f       /* 精调上限 +1N: 精细脉冲 */
#define PRESSURE_ERROR_FINE_LOW    -1.0f       /* 精调下限 -1N: 精细脉冲 */
#define PRESSURE_ERROR_HOLD_HIGH    0.0f       /* 低于目标1N仍精调；达到目标才保持 */
#define PRESSURE_ERROR_HOLD_LOW    -1.0f       /* 保持下限 (同精调) */
#define MICRO_PULSE_VOLTAGE_MIN    0.8f        /* 微调最小脉冲电压 (V) - 误差接近MICRO阈值时 */
#define MICRO_PULSE_VOLTAGE_MAX    3.0f        /* 微调最大脉冲电压 (V) - 误差接近COARSE阈值时 */
#define FINE_PULSE_VOLTAGE_MIN     0.4f        /* 精调最小脉冲电压 (V) - 误差接近FINE阈值时 */
#define FINE_PULSE_VOLTAGE_MAX     1.0f        /* 精调最大脉冲电压 (V) - 误差接近MICRO阈值时 */
#define PULSE_COOLDOWN_MS          150
#define MAX_V 6.0f
#define MIN_V -6.0f
#define APPROACH_VOLTAGE        5.0f    /* 首次未接触工件时保留5V连续接近 */
#define PRESSURE_FB_TIMEOUT_MS  500     /* 压力反馈超时: 超过此时间无新压力帧则停机保护(需大于传感器上传周期) */
#define PULSE_BOOST_STEP_V      0.3f    /* 脉冲推不动时每次增加的助推电压(V) */
#define LOAD_BOOST_STEP_V       0.5f    /* 离目标较远时更快地寻找有效推动电压 */
#define LOAD_RISE_MIN_N         2.0f    /* 单次压力增量不足2N时逐步加大 */
#define LOAD_RISE_MAX_N         8.0f    /* 单次升压超过8N时收小，防止过冲 */
#define PULSE_BOOST_MAX_V       9.0f    /* MICRO助推上限: 基础最高3V + 助推9V = 12V */
#define FINE_BOOST_MAX_V        3.0f    /* FINE推不动时逐步助推，正向最终最高4V */
#define FINE_APPLIED_MAX_V      4.0f    /* FINE正向短脉冲最终上限 */
#define FINE_REVERSE_MAX_V      2.0f    /* FINE反向卸压保持较低，避免回退过头 */
#define MICRO_LOAD_PULSE_MAX_V 10.0f   /* 推不动时短脉冲逐步助推的上限 */
#define PULSE_DURATION_MS       9U    /* 默认短脉冲；精调和反向仍最多9ms */
#define MICRO_FORWARD_MAX_MS   12U    /* 远离目标且升压不足时正向微调可逐步加到12ms */
#define NEAR_ENTRY_ERROR_N     50.0f  /* 所有目标统一在剩余50N时进入减速区 */
#define NEAR_FORWARD_MAX_V     5.0f
#define NEAR_FORWARD_MS        6U
#define PRECISION_START_MS      2U      /* 小步区从更短脉冲起步，靠反馈逐级找有效推动量 */
#define NEAR_STALL_MAX_LEVEL   4U      /* 连续停滞才逐级解除5V/6ms限制 */
#define NEAR_STALL_V_STEP      0.5f
#define PRECISION_ENTRY_ERROR_N 40.0f /* 所有目标统一在剩余40N时进入小步区 */
#define PRECISION_START_V      2.5f
#define PRECISION_STEP_V       0.25f
#define PRECISION_MAX_LEVEL   32U     /* 超高负载连续停滞后可逐级达到12V，不能直接跳到上限 */
#define PRECISION_MAX_EXTRA_MS 3U     /* 电压仍压不动后每两次停滞再加1ms */
#define PRECISION_FINAL_ERROR_N 10.0f /* 最后10N起始脉宽不超过4ms，停滞才加宽 */
#define PRECISION_MID_ERROR_N   20.0f /* 最后20N先收紧输出 */
#define PRECISION_ESCAPE_ARM_PULSES 6U /* 新档位连续6次没升压才开始解锁 */
#define PRECISION_ESCAPE_MAX_STEPS 26U
#define PRECISION_HIGH_LOAD_N 800.0f /* 大目标保留较高推动量，仍按每次压力反馈升级 */
#define PRECISION_HIGH_FINAL_START_V 6.0f /* 800N以上最后10N直接从较高启动力开始 */
#define PRECISION_VERY_HIGH_LOAD_N 800.0f /* 800N以上均允许逐级增强，解决1000N末段静摩擦停滞 */
#define PRECISION_VERY_HIGH_MAX_V 12.0f
#define PRECISION_VERY_HIGH_MAX_MS 12U
#define PRECISION_HEAVY_BAND_MIN_N 1500.0f
#define PRECISION_HEAVY_BAND_MAX_N 2200.0f
#define PRECISION_HEAVY_EXTRA_MS 2U /* 1500~2200N连续停滞时额外增加脉宽，不增加电压 */
#define PRECISION_HEAVY_FINAL_EXTRA_MS 1U /* 1500~2200N最后10N再增加1ms，改善末段推进 */
#define PRECISION_MID_HIGH_MAX_N 1500.0f
#define PRECISION_MID_HIGH_FINAL_EXTRA_MS 1U /* 800~1500N最后10N增加1ms，缩短末段到压时间 */
#define PRECISION_FINAL_HIGH_MAX_V 12.0f /* 最后10N保留启动力，主要靠短脉宽限制单次增量 */
#define PRECISION_FINAL_HIGH_MAX_MS 5U
#define PRECISION_FINAL_SETTLE_MS 400U /* 最后10N每次脉冲后充分等待压力稳定再判断 */
#define PRECISION_LOW_TARGET_N 200.0f /* 小目标降低起步与正反向脉冲包络 */
#define PRECISION_TARGET_RISE_N 2.0f  /* 希望单次新反馈升压1-2N */
#define PRECISION_PAUSE_RISE_N 4.0f   /* 一次升压超过4N先暂停观察 */
#define NEAR_FULL_BOOST_ERROR_N 20.0f /* 仅剩余20N以上允许升到7V/8ms */
#define NEAR_JUMP_LIMIT_N      20.0f
#define NEAR_SETTLE_WAIT_MS   400U
#define MICRO_REVERSE_BASE_V   3.0f   /* 超过目标3N后的反向卸压起始电压 */
#define MICRO_REVERSE_MAX_V    4.0f
#define MICRO_REVERSE_MS       6U
#define FINE_REVERSE_MS        6U
#define FINE_REVERSE_BASE_V    1.5f
#define FINE_REVERSE_BOOST_V   0.5f
#define REVERSE_BOOST_STEP_V   0.5f
#define PULSE_MIN_DURATION_MS   2       /* 伏秒补偿时脉冲最短持续时间(ms) */
#define PULSE_MIN_COOLDOWN_MS   30      /* 脉冲后最短间隔(ms)，等待机械稳定 */
#define PULSE_FRESH_TIMEOUT_MS  400     /* 等待新压力帧的超时(ms)，超时按停滞处理(需<PRESSURE_FB_TIMEOUT_MS) */
#define PRESSURE_GLITCH_JUMP_N  80.0f   /* 反馈跳变滤波阈值: 单帧跳变超过此值视为干扰帧丢弃(连续3帧以上才接受) */
#define HOLD_STABLE_FRAME_COUNT 8U      /* 连续8个压力反馈帧稳定后才允许自锁 */
#define HOLD_STABLE_DELTA_N    1.0f    /* 相邻反馈帧最大允许变化 */
#define HOLD_STABLE_SPAN_N     0.5f    /* 反馈以整数N上报，10秒内不能跳动1N */
#define HOLD_MIN_SETTLE_MS  10000U     /* 整段连续稳定反馈至少10秒，波动后重新计时 */
#define HOLD_PRELOAD_VOLTAGE    0.30f   /* 自锁前 HOLD 阶段的小电压主动保压 */
#define PID_KP_DEFAULT  0.62f
#define PID_KI_DEFAULT  0.0005f
#define PID_KD_DEFAULT  0.0003f
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

static float    s_pulse_ref_pressure = -1.0f;   /* 上次脉冲启动时的压力 */
static float    s_pulse_boost = 0.0f;           /* 脉冲助推电压 */
static uint32_t s_pulse_fb_snapshot = 0;        /* 脉冲启动时的反馈帧时间戳(用于等待新帧) */
static uint32_t s_low_feedback_stamp = 0;
static uint8_t  s_low_settle_frames = 0;
static uint32_t s_cur_pulse_duration = PULSE_DURATION_MS;
static float    s_last_valid_pressure = -1.0f;  /* 上一个有效压力反馈(跳变滤波用) */
static uint8_t  s_glitch_cnt = 0;               /* 连续异常帧计数 */
static float    s_glitch_candidate = -1.0f;     /* 当前待确认的异常压力值 */
static uint32_t s_glitch_feedback_stamp = 0;    /* 已处理的压力反馈帧时间戳 */
static uint8_t  s_pulse_stall_cnt = 0;          /* 脉冲连续停滞计数(连续2次停滞才助推) */
static uint32_t s_forward_pulse_ms = PULSE_DURATION_MS;
static bool     s_last_pulse_positive = false;
static bool     s_has_contacted = false;
static bool     s_near_forward_active = false;
static uint8_t  s_near_stall_level = 0;
static uint8_t  s_precision_level = 0;
static uint8_t  s_precision_extra_ms = 0;
static uint8_t  s_precision_tightest_band = 0;
static uint8_t  s_precision_no_rise_cnt = 0;
static uint8_t  s_precision_escape_steps = 0;
static bool     s_precision_latched = false;
static uint8_t  s_precision_stall_cnt = 0;
static float    s_precision_ref_pressure = -1.0f;
static uint32_t s_precision_fb_stamp = 0;
static uint32_t s_near_feedback_stamp = 0;
static float    s_near_last_pressure = -1.0f;
static uint32_t s_near_pause_start = 0;
static bool     s_near_wait = false;
static float    s_reverse_ref_pressure = -1.0f;
static float    s_reverse_boost = 0.0f;
static uint8_t  s_reverse_stall_cnt = 0;
static uint32_t s_hold_feedback_stamp = 0;
static uint8_t  s_hold_stable_frames = 0;
static uint32_t s_hold_enter_time = 0;
static float    s_hold_last_pressure = -1.0f;
static float    s_hold_window_min = 0.0f;
static float    s_hold_window_max = 0.0f;

static PressureControlSystem_t source;
static ForceBuildDecision decision;
static uint32_t source_now;
static uint64_t source_sequence;
static bool source_have_sequence;
static struct { float actual_pressure; } pressure_sys;
static uint32_t source_feedback_stamp;
static uint32_t Num_OK, pressure_substate, current_interface;
enum { PRESSURE_AUTO_RUN, INTERFACE_PRESSURE };
typedef enum { SourceOutput_MODE_STOP, SourceOutput_MODE_POSITIVE, SourceOutput_MODE_NEGATIVE } SourceOutput_Mode_t;
static uint32_t SourceTime(void) { return source_now; }
static void SourceOutput_Brake(void) { decision.action=BUILD_DECISION_BRAKE; decision.command=0; }
static void SourceOutput_EmergencyStop(void) { decision.action=BUILD_DECISION_OFF; decision.command=0; }
static void SourceOutput_SetMode(SourceOutput_Mode_t mode) { (void)mode; }
static void SourceOutput_SetVoltage(float v) { decision.command=(int32_t)lroundf(v*1000); }
static void PressureControl_StartPulseTimer(PressureControlSystem_t *sys)
{ (void)sys; decision.action=BUILD_DECISION_PULSE; decision.normal_ms=s_cur_pulse_duration; }
static void PressureControl_EnsureMotorEnabled(void) { }
static void PressureControl_StopMode(PressureControlSystem_t *sys) { (void)sys; SourceOutput_EmergencyStop(); }
static void PressureControl_HoldMode(PressureControlSystem_t *sys) { (void)sys; SourceOutput_EmergencyStop(); }
static void interface_press_init(void) { }
static void PressureControl_ResetAdaptive(void)
{
    s_pulse_ref_pressure = -1.0f;
    s_pulse_boost = 0.0f;
    s_pulse_fb_snapshot = 0;
    s_low_feedback_stamp = 0;
    s_low_settle_frames = 0;
    s_cur_pulse_duration = PULSE_DURATION_MS;
    s_last_valid_pressure = -1.0f;
    s_glitch_cnt = 0;
    s_glitch_candidate = -1.0f;
    s_glitch_feedback_stamp = 0;
    s_pulse_stall_cnt = 0;
    s_forward_pulse_ms = PULSE_DURATION_MS;
    s_last_pulse_positive = false;
    s_has_contacted = false;
    s_near_forward_active = false;
    s_near_stall_level = 0;
    s_precision_level = 0;
    s_precision_extra_ms = 0;
    s_precision_tightest_band = 0;
    s_precision_no_rise_cnt = 0;
    s_precision_escape_steps = 0;
    s_precision_latched = false;
    s_precision_stall_cnt = 0;
    s_precision_ref_pressure = -1.0f;
    s_precision_fb_stamp = 0;
    s_near_feedback_stamp = 0;
    s_near_last_pressure = -1.0f;
    s_near_pause_start = 0;
    s_near_wait = false;
    s_reverse_ref_pressure = -1.0f;
    s_reverse_boost = 0.0f;
    s_reverse_stall_cnt = 0;
    s_hold_feedback_stamp = 0;
    s_hold_stable_frames = 0;
    s_hold_enter_time = 0;
    s_hold_last_pressure = -1.0f;
    s_hold_window_min = 0.0f;
    s_hold_window_max = 0.0f;
}

static float PressureControl_PulseBoost(float current_pressure, float boost_max,
                                        bool fast_forward, bool allow_longer)
{
    /* MICRO 与 FINE 共用助推状态；切入更低档位时必须立即收紧上限。 */
    if (s_pulse_boost > boost_max) {
        s_pulse_boost = boost_max;
    }
    if (!allow_longer) s_forward_pulse_ms = PULSE_DURATION_MS;

    if (s_pulse_ref_pressure >= 0.0f) {
        float rise = s_last_pulse_positive ?
                     (current_pressure - s_pulse_ref_pressure) :
                     (s_pulse_ref_pressure - current_pressure);
        if (fast_forward && s_last_pulse_positive) {
            if (rise < LOAD_RISE_MIN_N) {
                if (++s_pulse_stall_cnt >= 2U) {
                    s_pulse_stall_cnt = 0;
                    s_pulse_boost += LOAD_BOOST_STEP_V;
                    if (s_pulse_boost > boost_max) s_pulse_boost = boost_max;
                    if (allow_longer && s_forward_pulse_ms < MICRO_FORWARD_MAX_MS) {
                        s_forward_pulse_ms++;
                    }
                }
            } else if (rise > LOAD_RISE_MAX_N) {
                s_pulse_stall_cnt = 0;
                s_pulse_boost -= LOAD_BOOST_STEP_V;
                if (s_pulse_boost < 0.0f) s_pulse_boost = 0.0f;
                if (s_forward_pulse_ms > PULSE_DURATION_MS) s_forward_pulse_ms--;
            } else {
                /* 2~8N/帧时维持现有推动量，不退回低电压重新爬坡。 */
                s_pulse_stall_cnt = 0;
            }
        } else if (rise < 1.0f) {
            s_pulse_stall_cnt++;
            if (s_pulse_stall_cnt >= 2) {
                s_pulse_stall_cnt = 0;
                if (s_near_forward_active && s_last_pulse_positive &&
                    s_near_stall_level < NEAR_STALL_MAX_LEVEL) {
                    s_near_stall_level++;
                }
                s_pulse_boost += (s_near_forward_active && s_last_pulse_positive) ?
                                 LOAD_BOOST_STEP_V : PULSE_BOOST_STEP_V;
                if (s_pulse_boost > boost_max) s_pulse_boost = boost_max;
                if (allow_longer && s_forward_pulse_ms < MICRO_FORWARD_MAX_MS) {
                    s_forward_pulse_ms++;
                }
            }
        } else if (s_near_forward_active && s_last_pulse_positive &&
                   rise <= LOAD_RISE_MAX_N) {
            /* 减速区已有小幅进展时保留有效推力，避免每次都退回3V再爬坡。 */
            s_pulse_stall_cnt = 0;
        } else if (s_near_forward_active && s_last_pulse_positive) {
            s_pulse_stall_cnt = 0;
            if (s_near_stall_level > 1U) s_near_stall_level -= 2U;
            else s_near_stall_level = 0U;
            s_pulse_boost -= LOAD_BOOST_STEP_V;
            if (s_pulse_boost < 0.0f) s_pulse_boost = 0.0f;
        } else {
            s_pulse_stall_cnt = 0;
            s_pulse_boost = 0.0f;
            s_forward_pulse_ms = PULSE_DURATION_MS;
        }
    }
    s_pulse_ref_pressure = current_pressure;
    return s_pulse_boost;
}

static void PressureControl_ResetPrecision(void)
{
    s_precision_level = 0U;
    s_precision_extra_ms = 0U;
    s_precision_no_rise_cnt = 0U;
    s_precision_escape_steps = 0U;
    s_precision_stall_cnt = 0U;
    s_precision_ref_pressure = -1.0f;
    s_precision_fb_stamp = 0U;
}

static bool PressureControl_PrecisionForward(PressureControlSystem_t *sys,
                                              float *voltage, uint32_t *duration)
{
    float pressure = sys->control.current_pressure;
    bool low_target = (sys->control.target_pressure <= PRECISION_LOW_TARGET_N);
    bool high_target = (sys->control.target_pressure >= PRECISION_HIGH_LOAD_N);
    bool very_high_target = (sys->control.target_pressure >= PRECISION_VERY_HIGH_LOAD_N);
    bool heavy_band_target = (sys->control.target_pressure >= PRECISION_HEAVY_BAND_MIN_N &&
                              sys->control.target_pressure <= PRECISION_HEAVY_BAND_MAX_N);
    bool mid_high_target = (sys->control.target_pressure >= PRECISION_HIGH_LOAD_N &&
                            sys->control.target_pressure < PRECISION_MID_HIGH_MAX_N);
    float last_rise = -1.0f;
    if (s_precision_ref_pressure >= 0.0f) {
        if (source_feedback_stamp == s_precision_fb_stamp) {
            return false;  /* 没有新反馈不能把同一帧当成停滞而连续加脉冲。 */
        }
        float rise = pressure - s_precision_ref_pressure;
        last_rise = rise;
        if (rise < 1.0f) {
            /* 明显回落比整数N反馈不变更可信：加快找推力，但每次仍只发一个短脉冲。 */
            uint8_t no_rise_step = (rise <= -3.0f) ? 2U : 1U;
            uint8_t previous_no_rise = s_precision_no_rise_cnt;
            if (s_precision_no_rise_cnt <= (uint8_t)(250U - no_rise_step))
                s_precision_no_rise_cnt += no_rise_step;
            else s_precision_no_rise_cnt = 250U;
            if ((s_precision_latched || s_precision_tightest_band > 0U) &&
                s_precision_no_rise_cnt >= PRECISION_ESCAPE_ARM_PULSES &&
                s_precision_no_rise_cnt / 2U > previous_no_rise / 2U &&
                s_precision_escape_steps < PRECISION_ESCAPE_MAX_STEPS) {
                s_precision_escape_steps++;
            }
            s_precision_stall_cnt += no_rise_step;
            if (s_precision_stall_cnt >= 2U) {
                s_precision_stall_cnt = 0U;
                uint8_t level_step = ((very_high_target && rise < 1.0f) ||
                                      (high_target && rise <= -3.0f)) ? 2U : 1U;
                if (s_precision_level < PRECISION_MAX_LEVEL) {
                    uint8_t next_level = s_precision_level + level_step;
                    s_precision_level = (next_level < PRECISION_MAX_LEVEL) ?
                                        next_level : PRECISION_MAX_LEVEL;
                }
                else if (s_precision_extra_ms < PRECISION_MAX_EXTRA_MS)
                    s_precision_extra_ms++;
            }
        } else if (rise <= PRECISION_TARGET_RISE_N) {
            s_precision_stall_cnt = 0U;  /* 1-2N正好，保持当前有效脉冲。 */
            s_precision_no_rise_cnt = 0U;
        } else {
            s_precision_stall_cnt = 0U;
            s_precision_no_rise_cnt = 0U;
            uint8_t decrease = (rise > PRECISION_PAUSE_RISE_N) ? 3U : 2U;
            uint8_t width_decrease = (rise > PRECISION_PAUSE_RISE_N) ? 2U : 1U;
            s_precision_escape_steps = (s_precision_escape_steps > decrease) ?
                                       (uint8_t)(s_precision_escape_steps - decrease) : 0U;
            s_precision_extra_ms = (s_precision_extra_ms > width_decrease) ?
                                   (uint8_t)(s_precision_extra_ms - width_decrease) : 0U;
            s_precision_level = (s_precision_level > decrease) ?
                                (uint8_t)(s_precision_level - decrease) : 0U;
            if (rise > PRECISION_PAUSE_RISE_N ||
                (sys->control.pressure_error <= PRECISION_FINAL_ERROR_N &&
                 rise > PRECISION_TARGET_RISE_N)) {
                s_near_wait = true;
                s_near_pause_start = SourceTime();
                s_precision_ref_pressure = -1.0f;
                SourceOutput_Brake();
                return false;  /* 反馈已明显跳变，先等待机械稳定。 */
            }
        }
    }

    /* 越接近目标越收紧一次脉冲；只记录已到过的最近档位，回落不放宽。 */
    uint8_t band = 0U;
    if (sys->control.pressure_error <= PRESSURE_ERROR_MICRO_HIGH) band = 3U;
    else if (sys->control.pressure_error <= PRECISION_FINAL_ERROR_N) band = 2U;
    else if (sys->control.pressure_error <= PRECISION_MID_ERROR_N) band = 1U;
    if (band > s_precision_tightest_band) {
        s_precision_tightest_band = band;
        s_precision_no_rise_cnt = 0U;
        s_precision_stall_cnt = 0U;
        /* 高负载已有1-2N有效增量时保留找到的推力；跳变或低负载仍收紧。 */
        if (!(high_target && last_rise >= 1.0f &&
              last_rise <= PRECISION_TARGET_RISE_N)) {
            s_precision_escape_steps = 0U;
            s_precision_extra_ms = 0U;
            uint8_t initial_level_cap = (band == 1U) ? 8U :
                                        ((band == 2U) ? 6U : 4U);
            if (s_precision_level > initial_level_cap)
                s_precision_level = initial_level_cap;
        }
    }

    float start_v = low_target ? 2.0f :
                    (high_target && sys->control.pressure_error <= PRECISION_FINAL_ERROR_N ?
                     PRECISION_HIGH_FINAL_START_V :
                     (high_target ? 4.0f : PRECISION_START_V));
    uint32_t start_ms = high_target ? 3U : PRECISION_START_MS;
    *voltage = start_v + PRECISION_STEP_V * (float)s_precision_level;
    uint32_t base_duration = start_ms +
                              (s_precision_level >= 4U ? 1U : 0U) +
                              (s_precision_level >= 8U ? 1U : 0U) +
                              (s_precision_level >= 12U ? 1U : 0U);
    if (!low_target && !high_target) {
        /* 200~800N：电机已启动但位移不足时，逐级把末段脉宽由6ms扩到8ms。 */
        base_duration += (s_precision_level >= 18U ? 1U : 0U) +
                         (s_precision_level >= 24U ? 1U : 0U) +
                         (s_precision_level >= 30U ? 1U : 0U);
    }
    if (very_high_target && sys->control.pressure_error > PRECISION_FINAL_ERROR_N) {
        base_duration += (s_precision_level >= 16U ? 1U : 0U) +
                         (s_precision_level >= 22U ? 1U : 0U) +
                         (s_precision_level >= 28U ? 1U : 0U);
    }
    if (heavy_band_target) {
        /* 只在多次无有效增量后逐级增加，避免1500N正常工况一开始就变粗。 */
        base_duration += (s_precision_level >= 20U ? 1U : 0U) +
                         (s_precision_level >= 26U ? 1U : 0U);
    }
    if (mid_high_target && sys->control.pressure_error <= PRECISION_FINAL_ERROR_N) {
        base_duration += PRECISION_MID_HIGH_FINAL_EXTRA_MS;
    }
    *duration = base_duration + s_precision_extra_ms;
    float voltage_cap = 5.5f;
    uint32_t duration_cap = 5U;
    uint8_t escape = s_precision_escape_steps;
    if (s_precision_tightest_band == 0U) {
        voltage_cap += PRECISION_STEP_V * (float)escape;
        duration_cap += (escape >= 4U ? 1U : 0U) +
                        (escape >= 8U ? 1U : 0U);
    } else if (s_precision_tightest_band == 1U) {
        voltage_cap = 5.0f + PRECISION_STEP_V * (float)escape;
        duration_cap = 4U + (escape >= 4U ? 1U : 0U) +
                       (escape >= 8U ? 1U : 0U) + (escape >= 12U ? 1U : 0U);
    } else if (s_precision_tightest_band == 2U) {
        voltage_cap = 4.5f + PRECISION_STEP_V * (float)escape;
        duration_cap = 3U + (escape >= 4U ? 1U : 0U) +
                       (escape >= 8U ? 1U : 0U) + (escape >= 12U ? 1U : 0U);
    } else if (s_precision_tightest_band >= 3U) {
        voltage_cap = 4.0f + PRECISION_STEP_V * (float)escape;
        duration_cap = 3U + (escape >= 6U ? 1U : 0U) +
                       (escape >= 10U ? 1U : 0U) + (escape >= 12U ? 1U : 0U);
    }
    if (high_target) {
        duration_cap += (escape >= 16U ? 1U : 0U) +
                        (escape >= 20U ? 1U : 0U);
    } else if (!low_target) {
        /* 中负载最后10N确认连续无增量后，允许逐级解锁到8ms。 */
        duration_cap += (escape >= 16U ? 1U : 0U) +
                        (escape >= 22U ? 1U : 0U);
    }
    if (very_high_target) {
        /* 只有连续无增量才逐项解锁，确保800N以上目标末段能获得足够启动力。 */
        duration_cap += (escape >= 20U ? 1U : 0U) +
                        (escape >= 23U ? 1U : 0U) +
                        (escape >= 26U ? 1U : 0U);
        if (s_precision_tightest_band == 2U)
            voltage_cap = 5.5f + PRECISION_STEP_V * (float)escape;
        else if (s_precision_tightest_band >= 3U)
            voltage_cap = 5.0f + PRECISION_STEP_V * (float)escape;
    }
    if (high_target && sys->control.pressure_error <= PRECISION_FINAL_ERROR_N &&
        voltage_cap < PRECISION_HIGH_FINAL_START_V) {
        voltage_cap = PRECISION_HIGH_FINAL_START_V;
    }
    if (heavy_band_target) {
        duration_cap += (escape >= 20U ? 1U : 0U) +
                        (escape >= 26U ? 1U : 0U);
    }
    float voltage_max = low_target ? 5.5f :
                        (very_high_target ? PRECISION_VERY_HIGH_MAX_V :
                         (high_target ? 9.0f : 7.0f));
    uint32_t duration_max = low_target ? 4U :
                            (very_high_target ? PRECISION_VERY_HIGH_MAX_MS : 8U);
    if (heavy_band_target) duration_max += PRECISION_HEAVY_EXTRA_MS;
    if (sys->control.pressure_error <= PRECISION_FINAL_ERROR_N && high_target) {
        /* 最后10N优先控制单次增量：电压负责克服静摩擦，脉宽限制冲量。 */
        if (voltage_max > PRECISION_FINAL_HIGH_MAX_V)
            voltage_max = PRECISION_FINAL_HIGH_MAX_V;
        uint32_t final_duration_max = PRECISION_FINAL_HIGH_MAX_MS +
                                      (heavy_band_target ?
                                       (PRECISION_HEAVY_EXTRA_MS + PRECISION_HEAVY_FINAL_EXTRA_MS) : 0U) +
                                      (mid_high_target ? PRECISION_MID_HIGH_FINAL_EXTRA_MS : 0U);
        if (duration_max > final_duration_max)
            duration_max = final_duration_max;
    }
    if (voltage_cap > voltage_max) voltage_cap = voltage_max;
    if (duration_cap > duration_max) duration_cap = duration_max;
    if (*voltage > voltage_cap) *voltage = voltage_cap;
    if (*duration > duration_cap) *duration = duration_cap;
    s_precision_ref_pressure = pressure;
    s_precision_fb_stamp = source_feedback_stamp;
    return true;
}

static float PressureControl_ReverseBoost(float current_pressure, float boost_max)
{
    if (s_reverse_boost > boost_max) {
        s_reverse_boost = 0.0f;  /* 从微调切入精调时避免继承过强反向电压 */
        s_reverse_stall_cnt = 0;
    }
    if (s_reverse_ref_pressure >= 0.0f) {
        float fall = s_reverse_ref_pressure - current_pressure;
        if (fall < 1.0f) {
            if (++s_reverse_stall_cnt >= 2U) {
                s_reverse_stall_cnt = 0;
                s_reverse_boost += REVERSE_BOOST_STEP_V;
                if (s_reverse_boost > boost_max) s_reverse_boost = boost_max;
            }
        } else if (fall > 3.0f) {
            s_reverse_stall_cnt = 0;
            s_reverse_boost -= REVERSE_BOOST_STEP_V;
            if (s_reverse_boost < 0.0f) s_reverse_boost = 0.0f;
        } else {
            s_reverse_stall_cnt = 0;
        }
    }
    s_reverse_ref_pressure = current_pressure;
    return s_reverse_boost;
}

bool ForceBuildSource_IsInCoarseRange(float error)
{
    return (error > PRESSURE_ERROR_COARSE_HIGH) || (error < PRESSURE_ERROR_COARSE_LOW);
}

bool ForceBuildSource_IsInMicroRange(float error)
{
    return (error <= PRESSURE_ERROR_COARSE_HIGH && error >= PRESSURE_ERROR_COARSE_LOW) &&
           ((error > PRESSURE_ERROR_MICRO_HIGH) || (error < PRESSURE_ERROR_MICRO_LOW));
}

static bool PressureControl_IsInFineRange(float error)
{
    return (error <= PRESSURE_ERROR_MICRO_HIGH && error >= PRESSURE_ERROR_MICRO_LOW) &&
           ((error > PRESSURE_ERROR_HOLD_HIGH) ||
            (error < PRESSURE_ERROR_HOLD_LOW));
}

static bool PressureControl_IsInHoldRange(float error)
{
    return (error <= PRESSURE_ERROR_HOLD_HIGH &&
            error >= PRESSURE_ERROR_HOLD_LOW);
}

static void PressureControl_MicroMode(PressureControlSystem_t *sys)
{
    if (sys == NULL) return;
    
    /* 检查是否在脉冲控制中 */
    if (sys->control.pulse_active) {
        /* 检查脉冲是否结束 (使用伏秒补偿后的实际脉宽) */
        if (SourceTime() - sys->control.pulse_start_time >= s_cur_pulse_duration) {
            /* 脉冲结束，短接制动保持(抵抗负载反拖，防止脉冲间隙压力回落) */
            SourceOutput_Brake();
            sys->control.pulse_active = false;
            sys->control.pulse_end_time = SourceTime();  /* 记录脉冲结束时间 */
        }
        return;
    }

    /* 脉冲节奏: 机械稳定且等到新的压力反馈帧后才允许下次脉冲(以传感器实际速率为节奏，不再固定等待) */
    if (sys->control.pulse_end_time > 0) {
        uint32_t settle_ms = (s_last_pulse_positive && s_precision_tightest_band >= 2U) ?
                             PRECISION_FINAL_SETTLE_MS : PULSE_MIN_COOLDOWN_MS;
        bool settled  = (SourceTime() - sys->control.pulse_end_time >= settle_ms);
        bool fresh    = (source_feedback_stamp != s_pulse_fb_snapshot);
        bool timeout  = (SourceTime() - sys->control.pulse_end_time >= PULSE_FRESH_TIMEOUT_MS);
        if (sys->control.target_pressure <= PRECISION_LOW_TARGET_N) {
            if (source_feedback_stamp != s_low_feedback_stamp) {
                s_low_feedback_stamp = source_feedback_stamp;
                if (s_low_settle_frames < 2U) s_low_settle_frames++;
            }
            if (!(settled && fresh && s_low_settle_frames >= 2U)) return;
        }
        if (!(settled && fresh) && !timeout) {
            return;  /* 反馈尚未刷新，等待 */
        }
    }
    
    /* 根据误差方向决定脉冲方向 */
    float pulse_voltage = 0.0f;
    SourceOutput_Mode_t pulse_mode = SourceOutput_MODE_STOP;
    
    /* 计算误差比例缩放电压: 误差越大脉冲电压越高，误差越小脉冲电压越低 */
    float error_abs = fabsf(sys->control.pressure_error);
    float error_ratio = (error_abs - PRESSURE_ERROR_MICRO_HIGH) / 
                        (PRESSURE_ERROR_COARSE_HIGH - PRESSURE_ERROR_MICRO_HIGH);
    if (error_ratio > 1.0f) error_ratio = 1.0f;
    if (error_ratio < 0.0f) error_ratio = 0.0f;
    float scaled_voltage = MICRO_PULSE_VOLTAGE_MIN + 
                           error_ratio * (MICRO_PULSE_VOLTAGE_MAX - MICRO_PULSE_VOLTAGE_MIN);
    
    if (sys->control.pressure_error > PRESSURE_ERROR_MICRO_HIGH) {
        /* 误差为正，向下脉冲点动 */
        pulse_voltage = scaled_voltage;
        pulse_mode = SourceOutput_MODE_POSITIVE;
    } 
    else if (sys->control.pressure_error < PRESSURE_ERROR_MICRO_LOW) {
        /* 误差为负，向上脉冲点动 */
        pulse_voltage = -scaled_voltage;
        pulse_mode = SourceOutput_MODE_NEGATIVE;
    } 
    else {
        /* 误差在微调阈值以内，进入精调 */
        sys->control.mode = PRESSURE_MODE_FINE;
        return;
    }

    float applied_v = 0.0f;
    uint32_t dur = PULSE_DURATION_MS;
    if (pulse_voltage > 0.0f &&
        (error_abs <= PRECISION_ENTRY_ERROR_N || s_precision_latched)) {
        /* 进入目标前40N后锁定小步方式，压力回落也不切回强脉冲。 */
        s_precision_latched = true;
        if (!PressureControl_PrecisionForward(sys, &applied_v, &dur)) return;
        s_near_forward_active = false;
        s_near_stall_level = 0U;
        s_pulse_ref_pressure = -1.0f;
        s_pulse_boost = 0.0f;
        s_pulse_stall_cnt = 0U;
    } else {
        PressureControl_ResetPrecision();
        /* 正向、反向分别助推，避免正向的小幅升压误判为反向已卸压。 */
        bool near_forward = (pulse_voltage > 0.0f && error_abs <= NEAR_ENTRY_ERROR_N);
        if (near_forward && !s_near_forward_active) {
            s_pulse_ref_pressure = -1.0f;
            s_pulse_stall_cnt = 0;
            s_near_stall_level = 0;
            s_forward_pulse_ms = PULSE_DURATION_MS;
        }
        s_near_forward_active = near_forward;
        bool fast_forward = (pulse_voltage > 0.0f && !near_forward &&
                             error_abs > PRESSURE_ERROR_COARSE_HIGH);
        bool allow_longer = (pulse_voltage > 0.0f && !near_forward);
        float boost = (pulse_voltage > 0.0f) ?
                       PressureControl_PulseBoost(sys->control.current_pressure,
                                                  PULSE_BOOST_MAX_V, fast_forward, allow_longer) :
                       PressureControl_ReverseBoost(sys->control.current_pressure,
                                                    MICRO_REVERSE_MAX_V - MICRO_REVERSE_BASE_V);
        bool low_target = (sys->control.target_pressure <= PRECISION_LOW_TARGET_N);
        float base_v = (pulse_voltage > 0.0f) ? fabsf(pulse_voltage) :
                       (low_target ? 1.5f : MICRO_REVERSE_BASE_V);
        applied_v = base_v + boost;
        if (near_forward) {
            /* 40-50N段压不动才缓慢解锁，随后转入锁定的小步区。 */
            if (error_abs <= NEAR_FULL_BOOST_ERROR_N &&
                s_near_stall_level > 2U) s_near_stall_level = 2U;
            uint8_t level = s_near_stall_level;
            float voltage_limit = low_target ?
                                  (3.0f + NEAR_STALL_V_STEP * (float)level) :
                                  (NEAR_FORWARD_MAX_V + NEAR_STALL_V_STEP * (float)level);
            if (applied_v > voltage_limit) {
                applied_v = voltage_limit;
                float boost_limit = voltage_limit - base_v;
                if (boost_limit < 0.0f) boost_limit = 0.0f;
                if (s_pulse_boost > boost_limit) s_pulse_boost = boost_limit;
            }
            dur = low_target ? (3U + (uint32_t)(level / 2U)) :
                               (NEAR_FORWARD_MS + (uint32_t)(level / 2U));
            if (low_target && dur > 4U) dur = 4U;
        } else if (pulse_voltage > 0.0f) {
            if (applied_v > MICRO_LOAD_PULSE_MAX_V) applied_v = MICRO_LOAD_PULSE_MAX_V;
            dur = s_forward_pulse_ms;
            if (dur > MICRO_FORWARD_MAX_MS) dur = MICRO_FORWARD_MAX_MS;
        } else {
            float reverse_max = low_target ? 2.5f : MICRO_REVERSE_MAX_V;
            if (applied_v > reverse_max) applied_v = reverse_max;
            dur = low_target ? 3U : MICRO_REVERSE_MS;
        }
    }
    s_cur_pulse_duration = dur;
    s_pulse_fb_snapshot = source_feedback_stamp;
    s_low_feedback_stamp = s_pulse_fb_snapshot;
    s_low_settle_frames = 0U;

    sys->control.pulse_start_time = SourceTime();
    sys->control.pulse_active = true;
    s_last_pulse_positive = (pulse_voltage > 0.0f);
    if (pulse_voltage > 0.0f) {
        s_reverse_ref_pressure = -1.0f;
        s_reverse_boost = 0.0f;
        s_reverse_stall_cnt = 0;
    } else {
        s_pulse_ref_pressure = -1.0f;
        s_pulse_boost = 0.0f;
        s_pulse_stall_cnt = 0;
        s_forward_pulse_ms = PULSE_DURATION_MS;
    }
    SourceOutput_SetMode(pulse_mode);
    if (pulse_voltage >= 0.0f) {
        pulse_voltage = applied_v;
    } else {
        pulse_voltage = -applied_v;
    }
    SourceOutput_SetVoltage(pulse_voltage);
    PressureControl_StartPulseTimer(sys);
}

static void PressureControl_FineMode(PressureControlSystem_t *sys)
{
    if (sys == NULL) return;
    s_near_forward_active = false;
    
    /* 检查是否在脉冲控制中 */
    if (sys->control.pulse_active) {
        /* 检查脉冲是否结束 (使用伏秒补偿后的实际脉宽) */
        if (SourceTime() - sys->control.pulse_start_time >= s_cur_pulse_duration) {
            /* 脉冲结束，短接制动保持(抵抗负载反拖，防止脉冲间隙压力回落) */
            SourceOutput_Brake();
            sys->control.pulse_active = false;
            sys->control.pulse_end_time = SourceTime();  /* 记录脉冲结束时间 */
        }
        return;
    }

    /* 脉冲节奏: 机械稳定且等到新的压力反馈帧后才允许下次脉冲(以传感器实际速率为节奏，不再固定等待) */
    if (sys->control.pulse_end_time > 0) {
        uint32_t settle_ms = (s_last_pulse_positive && s_precision_tightest_band >= 2U) ?
                             PRECISION_FINAL_SETTLE_MS : PULSE_MIN_COOLDOWN_MS;
        bool settled  = (SourceTime() - sys->control.pulse_end_time >= settle_ms);
        bool fresh    = (source_feedback_stamp != s_pulse_fb_snapshot);
        bool timeout  = (SourceTime() - sys->control.pulse_end_time >= PULSE_FRESH_TIMEOUT_MS);
        if (sys->control.target_pressure <= PRECISION_LOW_TARGET_N) {
            if (source_feedback_stamp != s_low_feedback_stamp) {
                s_low_feedback_stamp = source_feedback_stamp;
                if (s_low_settle_frames < 2U) s_low_settle_frames++;
            }
            if (!(settled && fresh && s_low_settle_frames >= 2U)) return;
        }
        if (!(settled && fresh) && !timeout) {
            return;  /* 反馈尚未刷新，等待 */
        }
    }
    
    /* 根据误差方向决定脉冲方向 */
    float pulse_voltage = 0.0f;
    SourceOutput_Mode_t pulse_mode = SourceOutput_MODE_STOP;
    
    /* 计算误差比例缩放电压: 误差越大脉冲电压越高，误差越小脉冲电压越低 */
    float error_abs = fabsf(sys->control.pressure_error);
    float error_ratio = (error_abs - PRESSURE_ERROR_FINE_HIGH) / 
                        (PRESSURE_ERROR_MICRO_HIGH - PRESSURE_ERROR_FINE_HIGH);
    if (error_ratio > 1.0f) error_ratio = 1.0f;
    if (error_ratio < 0.0f) error_ratio = 0.0f;
    float scaled_voltage = FINE_PULSE_VOLTAGE_MIN + 
                           error_ratio * (FINE_PULSE_VOLTAGE_MAX - FINE_PULSE_VOLTAGE_MIN);
    
    if (sys->control.pressure_error > PRESSURE_ERROR_HOLD_HIGH) {
        /* 误差为正，向下短脉冲点动 */
        pulse_voltage = scaled_voltage;
        pulse_mode = SourceOutput_MODE_POSITIVE;
    } 
    else if (sys->control.pressure_error < PRESSURE_ERROR_FINE_LOW) {
        /* 误差为负，向上短脉冲点动 */
        pulse_voltage = -scaled_voltage;
        pulse_mode = SourceOutput_MODE_NEGATIVE;
    } 
    else {
        /* 误差在精调阈值以内，进入保持模式 */
        sys->control.mode = PRESSURE_MODE_HOLD;
        return;
    }

    float applied_v = 0.0f;
    uint32_t dur = FINE_REVERSE_MS;
    if (pulse_voltage > 0.0f) {
        /* 正向精调延续最后10N找到的有效1-2N/脉冲，不重新从低电压爬坡。 */
        if (!PressureControl_PrecisionForward(sys, &applied_v, &dur)) return;
        s_pulse_ref_pressure = -1.0f;
        s_pulse_boost = 0.0f;
        s_pulse_stall_cnt = 0U;
    } else {
        PressureControl_ResetPrecision();
        float boost = PressureControl_ReverseBoost(sys->control.current_pressure,
                                                    FINE_REVERSE_BOOST_V);
        bool low_target = (sys->control.target_pressure <= PRECISION_LOW_TARGET_N);
        applied_v = (low_target ? 1.0f : FINE_REVERSE_BASE_V) + boost;
        float reverse_max = low_target ? 1.5f : FINE_REVERSE_MAX_V;
        if (applied_v > reverse_max) applied_v = reverse_max;
        if (low_target) dur = 3U;
    }
    s_cur_pulse_duration = dur;
    s_pulse_fb_snapshot = source_feedback_stamp;
    s_low_feedback_stamp = s_pulse_fb_snapshot;
    s_low_settle_frames = 0U;

    sys->control.pulse_start_time = SourceTime();
    sys->control.pulse_active = true;
    SourceOutput_SetMode(pulse_mode);
    s_last_pulse_positive = (pulse_voltage > 0.0f);
    if (pulse_voltage > 0.0f) {
        s_reverse_ref_pressure = -1.0f;
        s_reverse_boost = 0.0f;
        s_reverse_stall_cnt = 0;
    } else {
        s_pulse_ref_pressure = -1.0f;
        s_pulse_boost = 0.0f;
        s_pulse_stall_cnt = 0;
        s_forward_pulse_ms = PULSE_DURATION_MS;
    }
    if (pulse_voltage >= 0.0f) {
        pulse_voltage = applied_v;
    } else {
        pulse_voltage = -applied_v;
    }
    SourceOutput_SetVoltage(pulse_voltage);
    PressureControl_StartPulseTimer(sys);
}

static void PressureControl_CoarseMode(PressureControlSystem_t *sys)
{
    if (sys == NULL) return;
    if (!s_has_contacted && sys->control.current_pressure < 10.0f &&
        sys->control.pressure_error > PRESSURE_ERROR_MICRO_HIGH) {
        SourceOutput_SetVoltage(APPROACH_VOLTAGE);
    } else {
        SourceOutput_SetVoltage(0.0f);
        sys->control.mode = PRESSURE_MODE_MICRO;
    }
}

static void PressureControl_Update(PressureControlSystem_t *sys)
{
    if (sys == NULL || !sys->system_ready || !sys->control.enable) {
        return;
    }
    
    /* 检查紧急停止 */
    if (sys->control.emergency_stop) {
        SourceOutput_EmergencyStop();
        sys->control.mode = PRESSURE_MODE_EMERGENCY;
        return;
    }

    /* 压力反馈超时保护: 传感器断线/停发时禁止继续输出，防止盲压导致过冲 */
    if (SourceTime() - source_feedback_stamp > PRESSURE_FB_TIMEOUT_MS) {
        SourceOutput_SetVoltage(0.0f);
        sys->control.pulse_active = false;
        return;
    }

    /* 控制任务10ms节奏只负责模式判断；定时器按当前设定脉宽到期制动。 */
    uint32_t current_time = SourceTime();
    /* TIM5 supplies actual pulse end, without task polling. */
    /* 模式/反馈判断仍按约10ms执行。 */
    if (current_time - sys->control.last_control_time < sys->control.control_interval_ms) {
        return;
    }
    sys->control.last_control_time = current_time;

    /* 反馈跳变滤波: 必须按新的传感器帧计数，不能按5ms控制循环重复累计同一帧。 */
    float raw_pressure = (float)pressure_sys.actual_pressure;
    if (source_feedback_stamp != s_glitch_feedback_stamp) {
        s_glitch_feedback_stamp = source_feedback_stamp;

        if (s_last_valid_pressure >= 0.0f &&
            fabsf(raw_pressure - s_last_valid_pressure) > PRESSURE_GLITCH_JUMP_N) {
            /* 异常值还必须连续且彼此接近，才视为真实的压力突变。 */
            if (s_glitch_candidate < 0.0f ||
                fabsf(raw_pressure - s_glitch_candidate) > 5.0f) {
                s_glitch_candidate = raw_pressure;
                s_glitch_cnt = 1;
            } else {
                s_glitch_cnt++;
            }

            if (s_glitch_cnt < 3U) {
                raw_pressure = s_last_valid_pressure;
            } else {
                s_last_valid_pressure = raw_pressure;
                s_glitch_candidate = -1.0f;
                s_glitch_cnt = 0;
            }
        } else {
            s_last_valid_pressure = raw_pressure;
            s_glitch_candidate = -1.0f;
            s_glitch_cnt = 0;
        }
    } else {
        /* 没有新反馈时沿用上次有效值，避免同一异常帧被重复计数。 */
        if (s_last_valid_pressure >= 0.0f) {
            raw_pressure = s_last_valid_pressure;
        }
    }
    /* 增大远段脉冲能量后，未确认的大幅压力跳变期间禁止继续盲压。 */
    if (s_glitch_cnt > 0U) {
        s_near_wait = true;
        s_near_pause_start = SourceTime();
        SourceOutput_Brake();
        sys->control.pulse_active = false;
        sys->control.pulse_end_time = SourceTime();
        s_pulse_ref_pressure = -1.0f;
        s_pulse_boost = 0.0f;
        s_pulse_stall_cnt = 0;
        s_near_stall_level = 0;
        PressureControl_ResetPrecision();
        s_forward_pulse_ms = PULSE_DURATION_MS;
        s_near_forward_active = false;
        return;
    }
    sys->control.current_pressure = raw_pressure;

    if (sys->control.current_pressure >= 10.0f) {
        s_has_contacted = true;
    }

    /* 计算压力误差 */
    sys->control.pressure_error = sys->control.target_pressure - sys->control.current_pressure;

    /* 闭环安全门：接近目标后，只用新反馈帧判断压力速度；大幅跳变先制动等待。 */
    if (source_feedback_stamp != s_near_feedback_stamp) {
        s_near_feedback_stamp = source_feedback_stamp;
        if (sys->control.pulse_active &&
            fabsf(sys->control.pressure_error) <= PRESSURE_ERROR_MICRO_HIGH) {
            SourceOutput_Brake();
            sys->control.pulse_active = false;
            sys->control.pulse_end_time = SourceTime();
        }
        if (s_near_last_pressure >= 0.0f &&
            sys->control.current_pressure >= sys->control.target_pressure * 0.40f &&
            fabsf(sys->control.current_pressure - s_near_last_pressure) > NEAR_JUMP_LIMIT_N) {
            s_near_wait = true;
            s_near_pause_start = SourceTime();
            SourceOutput_Brake();
            sys->control.pulse_active = false;
            sys->control.pulse_end_time = SourceTime();
            s_pulse_ref_pressure = -1.0f;
            s_pulse_boost = 0.0f;
            s_pulse_stall_cnt = 0;
            s_near_stall_level = 0;
            PressureControl_ResetPrecision();
            s_forward_pulse_ms = PULSE_DURATION_MS;
        }
        s_near_last_pressure = sys->control.current_pressure;
    }
    if (s_near_wait) {
        if (SourceTime() - s_near_pause_start < NEAR_SETTLE_WAIT_MS) {
            SourceOutput_Brake();
            return;
        }
        s_near_wait = false;
    }
    
    /* 更新统计信息 */
    sys->control_cycles++;
    float error_abs = fabsf(sys->control.pressure_error);
    if (error_abs > sys->max_error) {
        sys->max_error = error_abs;
    }
    sys->avg_error = (sys->avg_error * (sys->control_cycles - 1) + error_abs) / sys->control_cycles;
    
    /* 保存上次控制模式 */
    sys->control.last_mode = sys->control.mode;
    
    /* 根据逻辑确定控制模式 */
    /* 仅首次未接触时5V连续接近；接触后下压只用短脉冲。 */
    if (!s_has_contacted && sys->control.current_pressure < 10.0f &&
        sys->control.pressure_error > PRESSURE_ERROR_MICRO_HIGH) {
        sys->control.mode = PRESSURE_MODE_COARSE;
    }
    else if (PressureControl_IsInHoldRange(sys->control.pressure_error)) {
        sys->control.mode = PRESSURE_MODE_HOLD;
        if (sys->control.last_mode != PRESSURE_MODE_HOLD) {
            s_hold_enter_time = SourceTime();
            s_hold_feedback_stamp = source_feedback_stamp;
            s_hold_stable_frames = 0;
            s_hold_last_pressure = -1.0f;
        }
    } 
    /* 误差1-3N，精调脉冲 */
    else if (PressureControl_IsInFineRange(sys->control.pressure_error)) {
        sys->control.mode = PRESSURE_MODE_FINE;
    } 
    /* 误差>3N用微调短脉冲，一次脉冲后等待新反馈帧。 */
    else {
        sys->control.mode = PRESSURE_MODE_MICRO;
    }

    if (sys->control.mode != PRESSURE_MODE_HOLD) {
        PressureControl_EnsureMotorEnabled();
        s_hold_stable_frames = 0;
        s_hold_feedback_stamp = source_feedback_stamp;
        s_hold_last_pressure = -1.0f;
        sys->pressure_stable = false;
        pressure_substate = PRESSURE_AUTO_RUN;
    }
    if (sys->control.mode != PRESSURE_MODE_MICRO) {
        /* 精调不能沿用微调停滞状态或0.5V助推步长。 */
        s_near_forward_active = false;
        s_near_stall_level = 0U;
    }
    if (sys->control.mode != PRESSURE_MODE_MICRO &&
        sys->control.mode != PRESSURE_MODE_FINE) {
        PressureControl_ResetPrecision();
    }
    
    /* 检查模式回退 (保护逻辑): 精调误差变大时退回微调脉冲 */
    if (sys->control.mode == PRESSURE_MODE_FINE) {
        /* 在精调过程中，如果误差超出微调范围，回退到微调 */
        if (!PressureControl_IsInFineRange(sys->control.pressure_error) && 
            !PressureControl_IsInHoldRange(sys->control.pressure_error)) {
            sys->control.mode = PRESSURE_MODE_MICRO;
            sys->control.pulse_active = false;
        }
    }
   
    
    /* 根据控制模式执行相应动作 */
    switch (sys->control.mode) {
        case PRESSURE_MODE_STOP:
            PressureControl_StopMode(sys);
            Num_OK = 0;
            break;

        case PRESSURE_MODE_COARSE:
            PressureControl_CoarseMode(sys);
            Num_OK = 0;
            break;
            
        case PRESSURE_MODE_MICRO:
            PressureControl_MicroMode(sys);
            Num_OK = 0;
            break;
            
        case PRESSURE_MODE_FINE:
            PressureControl_FineMode(sys);
             Num_OK = 0;
            break;
            
        case PRESSURE_MODE_HOLD:
            PressureControl_HoldMode(sys);
            Num_OK++;
            break;
            
        case PRESSURE_MODE_EMERGENCY:
            SourceOutput_EmergencyStop();
            Num_OK = 0;
            break;
            
        default:
            break;
    }
    
     if (Num_OK >=200)
    {
        current_interface = INTERFACE_PRESSURE;
        interface_press_init();
        Num_OK = 0;
    }   
}

void ForceBuildSource_Reset(float target,uint32_t now)
{
 memset(&source,0,sizeof(source)); PressureControl_ResetAdaptive();
 source.control.target_pressure=target; source.control.max_pressure=3000;
 source.control.control_interval_ms=10; source.control.last_control_time=now-10;
 source.control.mode=PRESSURE_MODE_STOP; source.control.enable=true; source.system_ready=true;
 source_have_sequence=false; source_now=now;
}
ForceBuildDecision ForceBuildSource_Step(float measured,uint64_t sequence,uint32_t received,uint32_t now,
 bool pulse_active,uint32_t ended_ms)
{
 decision=(ForceBuildDecision){0};
 if (!isfinite(measured) || measured<0 || now-received>20 ||
     (source_have_sequence && (int64_t)(sequence-source_sequence)<=0)) return decision;
 source_have_sequence=true; source_sequence=sequence; source_now=now;
 source_feedback_stamp=received; pressure_sys.actual_pressure=measured;
 source.control.pulse_active=pulse_active; source.control.pulse_end_time=ended_ms;
 if (measured>=source.control.target_pressure) { decision.action=BUILD_DECISION_OFF; return decision; }
 PressureControl_Update(&source);
 if (source.control.mode==PRESSURE_MODE_COARSE && decision.command==5000)
     decision.action=BUILD_DECISION_APPROACH;
 decision.mode=source.control.mode==PRESSURE_MODE_COARSE ? BUILD_MODE_COARSE :
     source.control.mode==PRESSURE_MODE_FINE ? BUILD_MODE_FINE : BUILD_MODE_MICRO;
 decision.phase=decision.mode==BUILD_MODE_COARSE ? BUILD_PHASE_APPROACH :
     source.control.pressure_error<=50 ? BUILD_PHASE_TAPER : BUILD_PHASE_BUILD;
 decision.settle_ms=s_last_pulse_positive && s_precision_tightest_band>=2 ? 400 : 30;
 decision.precision_level=s_precision_level; decision.precision_extra_ms=s_precision_extra_ms;
 decision.precision_escape=s_precision_escape_steps; decision.precision_band=s_precision_tightest_band;
 decision.boost_command=(uint32_t)lroundf(s_pulse_boost*1000);
 decision.stall_count=s_pulse_stall_cnt; decision.filtered=source.control.current_pressure;
 decision.precision_latched=s_precision_latched; decision.near_wait=s_near_wait;
 return decision;
}
