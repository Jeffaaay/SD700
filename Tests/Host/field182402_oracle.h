#ifndef FIELD182402_ORACLE_H
#define FIELD182402_ORACLE_H
#include <stdint.h>
void FieldOracle_Reset(float target,int force,uint32_t now);
void FieldOracle_Time(uint32_t now);
void FieldOracle_Feed(int force,uint32_t now);
int32_t FieldOracle_Command(void);
uint32_t FieldOracle_Duration(void);
uint32_t FieldOracle_Level(void);
uint32_t FieldOracle_Escape(void);
uint32_t FieldOracle_Boost(void);
int FieldOracle_Braking(void);
#endif
