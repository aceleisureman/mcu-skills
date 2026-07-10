#ifndef __OLED_H
#define __OLED_H

#include "stm32f1xx_hal.h"

/* SSD1306 I2C address (write) */
#define OLED_ADDR       0x78

/* Display dimensions */
#define OLED_WIDTH      128
#define OLED_HEIGHT     64
#define OLED_PAGES      8

/* GPIO pin definitions (software I2C) */
#define OLED_SCL_PORT   GPIOB
#define OLED_SCL_PIN    GPIO_PIN_4
#define OLED_SDA_PORT   GPIOB
#define OLED_SDA_PIN    GPIO_PIN_5

/* I2C bit-bang macros */
#define OLED_SCL_HIGH() HAL_GPIO_WritePin(OLED_SCL_PORT, OLED_SCL_PIN, GPIO_PIN_SET)
#define OLED_SCL_LOW()  HAL_GPIO_WritePin(OLED_SCL_PORT, OLED_SCL_PIN, GPIO_PIN_RESET)
#define OLED_SDA_HIGH() HAL_GPIO_WritePin(OLED_SDA_PORT, OLED_SDA_PIN, GPIO_PIN_SET)
#define OLED_SDA_LOW()  HAL_GPIO_WritePin(OLED_SDA_PORT, OLED_SDA_PIN, GPIO_PIN_RESET)

/* Public API */
void OLED_Init(void);
void OLED_Clear(void);
void OLED_Display_On(void);
void OLED_Display_Off(void);
void OLED_SetCursor(uint8_t page, uint8_t col);
void OLED_ShowChar(uint8_t x, uint8_t y, char ch, uint8_t size);
void OLED_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size);
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size);
void OLED_ShowChinese(uint8_t x, uint8_t y, uint8_t index);
void OLED_DrawBMP(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, const uint8_t *bmp);
void OLED_Refresh(void);
void OLED_ClearBuffer(void);

#endif /* __OLED_H */
