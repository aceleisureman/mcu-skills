#ifndef __SERIAL_GPS_H
#define __SERIAL_GPS_H

#include "stm32f10x.h"

/*
 * GPS：MCU TX PB15 -> 接模块 RX；MCU RX PB14 <- 接模块 TX
 * F103C8 的 PB14/PB15 无 USART 复用，此处为 GPIO 软件串口 8N1。
 */
void    Serial_GPS_Init(void);
void    Serial_GPS_SetBaud(uint32_t baud);
uint32_t Serial_GPS_GetBaud(void);
void    Serial_GPS_SendByte(uint8_t byte);
uint8_t Serial_GPS_ReceiveByte(void);
uint8_t Serial_GPS_TryReceiveByte(uint8_t *out, uint16_t timeout_ms);

#endif
