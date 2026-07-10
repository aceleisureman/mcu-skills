#ifndef __KEYBOARD_H
#define __KEYBOARD_H

#include "stm32f1xx_hal.h"

/* 4x4 矩阵键盘引脚定义
 * 行: PB12, PB13, PB14, PB15 (输出)
 * 列: PA0, PA1, PB8, PB9 (输入)
 */

/* 行引脚 (输出, 默认高电平, 逐行拉低扫描) */
#define KB_ROW0_PORT    GPIOB
#define KB_ROW0_PIN     GPIO_PIN_12
#define KB_ROW1_PORT    GPIOB
#define KB_ROW1_PIN     GPIO_PIN_13
#define KB_ROW2_PORT    GPIOB
#define KB_ROW2_PIN     GPIO_PIN_14
#define KB_ROW3_PORT    GPIOB
#define KB_ROW3_PIN     GPIO_PIN_15

/* 列引脚 (输入, 上拉) */
#define KB_COL0_PORT    GPIOA
#define KB_COL0_PIN     GPIO_PIN_0
#define KB_COL1_PORT    GPIOA
#define KB_COL1_PIN     GPIO_PIN_1
#define KB_COL2_PORT    GPIOB
#define KB_COL2_PIN     GPIO_PIN_8
#define KB_COL3_PORT    GPIOB
#define KB_COL3_PIN     GPIO_PIN_9

/* 按键值定义 */
#define KEY_NONE        0xFF
#define KEY_0           '0'
#define KEY_1           '1'
#define KEY_2           '2'
#define KEY_3           '3'
#define KEY_4           '4'
#define KEY_5           '5'
#define KEY_6           '6'
#define KEY_7           '7'
#define KEY_8           '8'
#define KEY_9           '9'
#define KEY_A           'A'
#define KEY_B           'B'
#define KEY_C           'C'
#define KEY_D           'D'
#define KEY_STAR        '*'
#define KEY_HASH        '#'

/* 函数声明 */
void Keyboard_Init(void);
uint8_t Keyboard_Scan(void);
uint8_t Keyboard_GetKey(void);

#endif
