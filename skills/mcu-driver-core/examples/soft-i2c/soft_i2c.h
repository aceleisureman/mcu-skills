#ifndef __SOFT_I2C_H
#define __SOFT_I2C_H
#include "stm32f1xx_hal.h"
#include <stdint.h>

void SI2C_Init(void);
void SI2C_Start(void);
void SI2C_Stop(void);
uint8_t SI2C_WriteByte(uint8_t b);          /* 返回 0=ACK 1=NACK */
uint8_t SI2C_ReadByte(uint8_t ack);
uint8_t SI2C_WriteRegs(uint8_t addr, uint8_t reg, const uint8_t *buf, uint16_t len);
uint8_t SI2C_ReadRegs(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);

void delay_us(uint32_t us);
void delay_us_init(void);
#endif
