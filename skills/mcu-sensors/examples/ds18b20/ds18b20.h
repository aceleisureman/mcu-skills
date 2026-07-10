#ifndef __DS18B20_H
#define __DS18B20_H

#include "stm32f1xx_hal.h"

// OneWire Pin Definition (PA8)
#define DS18B20_PORT    GPIOA
#define DS18B20_PIN     GPIO_PIN_8

// Function Prototypes
void     DS18B20_Init(void);
uint8_t  DS18B20_Reset(void);
void     DS18B20_WriteByte(uint8_t data);
uint8_t  DS18B20_ReadByte(void);
void     DS18B20_StartConvert(void);
float    DS18B20_ReadTemp(void);

#endif /* __DS18B20_H */
