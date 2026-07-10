#ifndef __DHT11_H
#define __DHT11_H

#include "stm32f1xx_hal.h"
#include "config.h"

typedef struct {
    uint8_t temperature;
    uint8_t humidity;
} DHT11_Data;

uint8_t DHT11_Init(void);
uint8_t DHT11_Read(DHT11_Data *data);

#endif
