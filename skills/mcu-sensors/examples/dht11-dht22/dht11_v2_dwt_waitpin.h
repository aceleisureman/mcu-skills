#ifndef __DHT11_H
#define __DHT11_H

#include "stm32f1xx_hal.h"

#define DHT11_PORT      GPIOC
#define DHT11_PIN       GPIO_PIN_15

typedef struct {
    uint8_t temperature;  /* integer degrees C */
    uint8_t humidity;     /* integer RH% */
    uint8_t valid;        /* 1 = last read successful */
} DHT11_Data_t;

void DHT11_Init(void);
uint8_t DHT11_Read(uint8_t *temperature, uint8_t *humidity);

#endif /* __DHT11_H */
