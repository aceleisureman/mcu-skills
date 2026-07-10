/**
  * @file  dht11.h
  * @brief DHT11 温湿度传感器驱动（单总线）
  */
#ifndef __DHT11_H
#define __DHT11_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

/* 读取成功返回 1，温度(℃)与湿度(%RH)写入指针；失败返回 0 */
uint8_t DHT11_Read(uint8_t *temp, uint8_t *humi);

#ifdef __cplusplus
}
#endif
#endif
