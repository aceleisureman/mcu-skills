#ifndef __WATER_SENSOR_H
#define __WATER_SENSOR_H

#include "stm32f1xx_hal.h"

#define WATER_SENSOR_PORT GPIOA
#define WATER_SENSOR_PIN  GPIO_PIN_2

void WaterSensor_Init(void);
uint16_t WaterSensor_ReadRaw(void);
uint8_t WaterSensor_ReadPercent(void);

#endif /* __WATER_SENSOR_H */
