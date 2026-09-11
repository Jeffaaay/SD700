#include "./BSP/CURRENT/current_sense.h"

/* 全局变量定义 */
CurrentData_t g_current_data = {0};
float g_overcurrent_threshold = 2.0f;  /* 默认过流阈值 10A */
uint32_t g_avg_offset = 0;               /* 电流偏置校准值 (A) */
    
/* 私有函数声明 */
static float ADC_To_Voltage(uint16_t adc_value);
static float Voltage_To_Current(float voltage);
static void ApplyLowPassFilter(float *filtered_value, float new_value, float alpha);

/**
  * @brief  初始化电流检测模块
  * @retval None
  */
void CurrentSense_Init(void)
{
    /* 初始化ADC（使用正点原子驱动） */
    Adc_Init();  /* 初始化ADC1，PB1设置为模拟输入 */
    
    /* 初始化电流数据结构 */
    g_current_data.min_current = -2.0f;  /* 初始化为极小值 */
    g_current_data.max_current = 2.0f;   /* 初始化为极大值 */
    g_current_data.avg_current = 0.0f;
    g_current_data.sample_count = 0;
    g_current_data.filtered_current = 0.0f;
    
    /* 执行零点校准 */
    CurrentSense_CalibrateOffset(100);
}

/**
  * @brief  更新电流数据（应在主循环中定期调用）
  * @retval CurrentSenseStatus_t 电流检测状态
  */
CurrentSenseStatus_t CurrentSense_Update(void)
{
    uint16_t adc_value = 0;
    
    uint16_t adc_value_PlusOffset = 0;
    
    /* 使用Get_Adc_Average函数获取ADC平均值 */
    adc_value = Get_Adc_Average(ADC_CHANNEL, ADC_AVERAGE_TIMES);
    
    /* 保存原始ADC值 */
    g_current_data.raw_adc_value = adc_value;
    
    /* 减去初始偏置后得到的采样值 */
    adc_value_PlusOffset = (g_current_data.raw_adc_value >= g_avg_offset) ? 
                       (g_current_data.raw_adc_value - g_avg_offset) : 
                       (g_avg_offset - g_current_data.raw_adc_value);
    /* 转换为电压 */
    g_current_data.adc_voltage = ADC_To_Voltage(adc_value_PlusOffset);
    
    /* 计算分流电阻电压（考虑INA240增益） */
    g_current_data.shunt_voltage = g_current_data.adc_voltage / INA240_GAIN;
    
    /* 计算电流 */
    g_current_data.current = Voltage_To_Current(g_current_data.shunt_voltage);
    
    /* 应用低通滤波器 */
    ApplyLowPassFilter(&g_current_data.filtered_current, 
                      g_current_data.current, FILTER_ALPHA);
    
    /* 更新统计信息 */
    g_current_data.sample_count++;
    
    if (g_current_data.current > g_current_data.max_current)
        g_current_data.max_current = g_current_data.current;
    else if (g_current_data.current < g_current_data.min_current)
        g_current_data.min_current = g_current_data.current;
    
    /* 更新移动平均 */
    g_current_data.avg_current = (g_current_data.avg_current * 
                                 (g_current_data.sample_count - 1) + 
                                 g_current_data.current) / 
                                 g_current_data.sample_count;
    
    /* 检查过流 */
    if (g_current_data.current > g_overcurrent_threshold)
    {
        return CURRENT_SENSE_OVERCURRENT;
    }
    
    return CURRENT_SENSE_OK;
}

/**
  * @brief  ADC值转换为电压
  * @param  adc_value: ADC原始值
  * @retval 电压值 (V)
  */
static float ADC_To_Voltage(uint16_t adc_value)
{
    return (adc_value * ADC_REFERENCE_VOLTAGE) / ADC_RESOLUTION;
}

/**
  * @brief  电压转换为电流
  * @param  voltage: 分流电阻两端电压 (V)
  * @retval 电流值 (A)
  */
static float Voltage_To_Current(float voltage)
{
    /* 欧姆定律: I = V / R */
    return voltage / SHUNT_RESISTOR;
}

/**
  * @brief  应用一阶低通滤波器
  * @param  filtered_value: 滤波后的值指针
  * @param  new_value: 新采样值
  * @param  alpha: 滤波器系数 (0.0-1.0)
  * @retval None
  */
static void ApplyLowPassFilter(float *filtered_value, float new_value, float alpha)
{
    *filtered_value = (alpha * new_value) + ((1.0f - alpha) * (*filtered_value));
}

/**
  * @brief  获取当前电流值
  * @retval 当前电流 (A)
  */
float CurrentSense_GetCurrent(void)
{
    return g_current_data.current;
}

/**
  * @brief  获取滤波后的电流值
  * @retval 滤波后的电流 (A)
  */
float CurrentSense_GetFilteredCurrent(void)
{
    return g_current_data.filtered_current;
}

/**
  * @brief  获取平均电流
  * @retval 平均电流 (A)
  */
float CurrentSense_GetAverageCurrent(void)
{
    return g_current_data.avg_current;
}

/**
  * @brief  获取最大电流记录
  * @retval 最大电流 (A)
  */
float CurrentSense_GetMaxCurrent(void)
{
    return g_current_data.max_current;
}

/**
  * @brief  获取最小电流记录
  * @retval 最小电流 (A)
  */
float CurrentSense_GetMinCurrent(void)
{
    return g_current_data.min_current;
}

/**
  * @brief  重置统计信息
  * @retval None
  */
void CurrentSense_ResetStatistics(void)
{
    g_current_data.max_current = g_current_data.current;
    g_current_data.min_current = g_current_data.current;
    g_current_data.avg_current = g_current_data.current;
    g_current_data.sample_count = 1;
}

/**
  * @brief  设置过流阈值
  * @param  threshold: 过流阈值 (A)
  * @retval None
  */
void CurrentSense_SetOvercurrentThreshold(float threshold)
{
    g_overcurrent_threshold = threshold;
}

/**
  * @brief  检查是否过流
  * @param  threshold: 过流阈值 (A)，如果为0则使用全局阈值
  * @retval 1: 过流, 0: 正常
  */
uint8_t CurrentSense_CheckOvercurrent(float threshold)
{
    float check_threshold = (threshold > 0) ? threshold : g_overcurrent_threshold;
    
    if (fabs(g_current_data.current) > check_threshold)
    {
        return 1;
    }
    
    return 0;
}

/**
  * @brief  校准零点偏移
  * @param  num_samples: 采样数量
  * @retval None
  */
void CurrentSense_CalibrateOffset(uint16_t num_samples)
{
    uint32_t sum = 0;
    uint16_t i;
    
    for (i = 0; i < num_samples; i++)
    {
        uint16_t adc_value = Get_Adc_Average(ADC_CHANNEL, 5);
        
        sum += adc_value;
        
        delay_ms(1);
    }
    
    g_avg_offset = sum / num_samples;
}

