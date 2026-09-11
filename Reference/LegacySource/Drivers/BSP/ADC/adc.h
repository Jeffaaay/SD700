#ifndef __ADC_H
#define __ADC_H
#include "./SYSTEM/sys/sys.h"


void Adc_Init(void);				//ADC通道初始化
uint16_t  Get_Adc(uint32_t ch); 		        //获得某个通道值 
uint16_t Get_Adc_Average(uint32_t ch,uint8_t times);//得到某个通道给定次数采样的平均值

#endif 
