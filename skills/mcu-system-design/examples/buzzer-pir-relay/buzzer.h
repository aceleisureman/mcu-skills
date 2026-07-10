#ifndef __BUZZER_H
#define __BUZZER_H

#include "stm32f1xx_hal.h"

/* 蜂鸣器 -> PA1 (推挽输出)
 * 默认高电平鸣响(有源蜂鸣器). 若你的板子是低电平有效,
 * 把 BUZZER_ON_LEVEL 改成 GPIO_PIN_RESET 即可.
 */
#define BUZZER_PORT     GPIOA
#define BUZZER_PIN      GPIO_PIN_1
#define BUZZER_ON_LEVEL  GPIO_PIN_SET

void Buzzer_Init(void);
void Buzzer_On(void);
void Buzzer_Off(void);

#endif /* __BUZZER_H */
