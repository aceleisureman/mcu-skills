/**
  * @file  hx711.h
  * @brief HX711 称重传感器 24 位 ADC 驱动
  */
#ifndef __HX711_H
#define __HX711_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

void    HX711_Init(void);
void    HX711_Reset(void);
uint8_t HX711_WaitReady(uint32_t timeout_ms);
int32_t HX711_ReadRaw(void);          /* 读取一次原始值（有符号 24 位） */
void    HX711_Tare(uint8_t times);    /* 去皮：以当前为零点 */
/* 返回重量(克)；需先设置校准系数 */
int32_t HX711_GetWeight(uint8_t times);
void    HX711_SetScale(float counts_per_gram);  /* 每克对应的原始计数 */
int32_t HX711_GetOffset(void);               /* 读取当前零点偏移(用于持久化) */
void    HX711_SetOffset(int32_t offset);     /* 设置零点偏移(掉电恢复用) */
uint8_t HX711_IsReady(void);

#ifdef __cplusplus
}
#endif
#endif
