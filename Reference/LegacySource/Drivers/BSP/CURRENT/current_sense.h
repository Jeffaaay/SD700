#ifndef __CURRENT_SENSE_H
#define __CURRENT_SENSE_H

#include "./BSP/ADC/adc.h"
#include "./SYSTEM/delay/delay.h"
#include "./SYSTEM/sys/sys.h"
#include "stdio.h"

/* INA240配置参数 */
#define INA240_GAIN            20.0f      /* 增益 (V/V)，根据实际型号：20, 50, 100, 200 */
#define SHUNT_RESISTOR         0.001f      /* 分流电阻值 (欧姆) */
#define ADC_REFERENCE_VOLTAGE  3.3f       /* ADC参考电压 (V) */
#define ADC_RESOLUTION         4095.0f    /* 12位ADC最大值为4095 */

/* 校准参数 */
#define CURRENT_SCALE          1.0f       /* 电流缩放校准系数 */

/* ADC配置 */
#define ADC_CHANNEL            ADC_CHANNEL_4  /* PB1对应ADC1通道9 */
#define ADC_AVERAGE_TIMES      10         /* 采样平均次数 */

/* 滤波器设置 */
#define FILTER_ALPHA           0.1f       /* 一阶低通滤波器系数 (0.0-1.0) */

/* 电流数据结构体 */
typedef struct {
    uint16_t raw_adc_value;    /* 原始ADC值 */
    float adc_voltage;         /* ADC输入电压 (V) */
    float shunt_voltage;       /* 分流电阻电压 (V) */
    float current;             /* 计算得到的电流 (A) */
    float filtered_current;    /* 滤波后的电流 (A) */
    float max_current;         /* 最大电流记录 (A) */
    float min_current;         /* 最小电流记录 (A) */
    float avg_current;         /* 平均电流 (A) */
    uint32_t sample_count;     /* 采样计数 */
} CurrentData_t;

/* 电流检测状态枚举 */
typedef enum {
    CURRENT_SENSE_OK = 0,
    CURRENT_SENSE_OVERCURRENT,
    CURRENT_SENSE_UNDERCURRENT,
    CURRENT_SENSE_ADC_ERROR
} CurrentSenseStatus_t;

/* 函数声明 */
void CurrentSense_Init(void);
CurrentSenseStatus_t CurrentSense_Update(void);
float CurrentSense_GetCurrent(void);
float CurrentSense_GetFilteredCurrent(void);
float CurrentSense_GetAverageCurrent(void);
float CurrentSense_GetMaxCurrent(void);
float CurrentSense_GetMinCurrent(void);
void CurrentSense_ResetStatistics(void);
void CurrentSense_SetOvercurrentThreshold(float threshold);
uint8_t CurrentSense_CheckOvercurrent(float threshold);
void CurrentSense_CalibrateOffset(uint16_t num_samples);

/* 调试函数 */
void CurrentSense_PrintData(void);
void CurrentSense_PrintCalibrationData(void);

/* 外部变量声明 */
extern CurrentData_t g_current_data;
extern float g_overcurrent_threshold;

#endif /* __CURRENT_SENSE_H */

