/**
  * @file  mq135.h
  * @brief MQ135 空气质量/异味传感器（ADC1_IN1）
  */
#ifndef __MQ135_H
#define __MQ135_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

uint16_t MQ135_ReadRaw(void);    /* 0~4095 */
uint16_t MQ135_ReadPPM(void);    /* 估算等效浓度(相对值) */
uint8_t  MQ135_ReadPercent(void);/* 0~100，越大空气越差 */

#ifdef __cplusplus
}
#endif
#endif
