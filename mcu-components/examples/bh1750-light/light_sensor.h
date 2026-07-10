#ifndef __LIGHT_SENSOR_H
#define __LIGHT_SENSOR_H

#include "stm32f1xx_hal.h"
#include "config.h"

void LightSensor_Init(void);
uint16_t LightSensor_Read(void);

#endif
