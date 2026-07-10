#ifndef __OLED_H
#define __OLED_H

#include "stm32f1xx_hal.h"
#include "config.h"

void OLED_Init(void);
void OLED_Clear(void);
void OLED_Fill(uint8_t data);
void OLED_SetCursor(uint8_t x, uint8_t y);
void OLED_ShowString(uint8_t x, uint8_t y, const char *str);
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len);
void OLED_Update(void);
void OLED_ClearArea(uint8_t x, uint8_t y, uint8_t width, uint8_t height);
void OLED_DisplayOn(void);
void OLED_DisplayOff(void);

#endif
