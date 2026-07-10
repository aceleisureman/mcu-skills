#ifndef __OLED_H
#define __OLED_H

#include "stm32f1xx_hal.h"

/* OLED I2C 引脚定义 - 软件I2C */
#define OLED_SCL_GPIO   GPIOB
#define OLED_SCL_PIN    GPIO_PIN_7
#define OLED_SDA_GPIO   GPIOB
#define OLED_SDA_PIN    GPIO_PIN_6

#define OLED_ADDRESS    0x78

/* 函数声明 */
void OLED_Init(void);
void OLED_Clear(void);
void OLED_Display_On(void);
void OLED_Display_Off(void);
void OLED_SetPos(uint8_t x, uint8_t y);
void OLED_ShowChar(uint8_t x, uint8_t y, char chr, uint8_t size);
void OLED_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size);
void OLED_ShowChinese(uint8_t x, uint8_t y, uint8_t index);
void OLED_DrawBMP(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, const uint8_t *bmp);
void OLED_Fill(uint8_t data);
void OLED_ClearArea(uint8_t x, uint8_t y, uint8_t w, uint8_t h);
void OLED_FillArea(uint8_t x, uint8_t page, uint8_t w, uint8_t pages, uint8_t data);
void OLED_ShowChineseInv(uint8_t x, uint8_t y, uint8_t index);
void OLED_ShowCharInv(uint8_t x, uint8_t y, char chr, uint8_t size);

/* 软件I2C底层 */
void OLED_IIC_Start(void);
void OLED_IIC_Stop(void);
void OLED_IIC_WriteByte(uint8_t data);
void OLED_IIC_WriteCmd(uint8_t cmd);
void OLED_IIC_WriteData(uint8_t data);
void OLED_IIC_WriteDataBurst(const uint8_t *data, uint8_t len);

#endif

