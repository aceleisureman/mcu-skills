#ifndef __DHT22_H
#define __DHT22_H
#include <stdint.h>

void DHT22_Init(void);
/* 返回 0 成功；temp ℃，humi %RH */
uint8_t DHT22_Read(float *temp, float *humi);
#endif
