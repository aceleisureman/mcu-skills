/**
  * @file  mq135.c
  * @brief MQ135 气体传感器 ADC 采集
  */
#include "mq135.h"
#include "bsp.h"

uint16_t MQ135_ReadRaw(void)
{
  uint16_t v = 0;
  HAL_ADC_Start(&hadc1);
  if (HAL_ADC_PollForConversion(&hadc1, 20) == HAL_OK) {
    v = (uint16_t)HAL_ADC_GetValue(&hadc1);
  }
  HAL_ADC_Stop(&hadc1);
  return v;
}

uint8_t MQ135_ReadPercent(void)
{
  uint32_t raw = MQ135_ReadRaw();
  return (uint8_t)(raw * 100U / 4095U);
}

/* 简化的相对浓度估算（非标定绝对 ppm，用于阈值比较/演示） */
uint16_t MQ135_ReadPPM(void)
{
  uint32_t raw = MQ135_ReadRaw();
  return (uint16_t)(raw * 2000U / 4095U);   /* 映射到 0~2000 相对值 */
}
