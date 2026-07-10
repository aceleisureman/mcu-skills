#ifndef __DOOR_IO_H
#define __DOOR_IO_H

#include "stm32f1xx_hal.h"

/* ============================================================
 * 声光输出模块(开门由舵机 PA10 模拟, 无继电器)
 *   蜂鸣器:       PA2   —— 低电平响(有源蜂鸣器, 低电平触发)
 *   蓝灯(状态/成功): PA7
 *   红灯(报警/失败): PB0
 * 这些引脚均避开了键盘(PA0/1,PB8/9,PB12-15)、OLED(PB6/7)、舵机(PA10)。
 * ============================================================ */

#define BUZZER_PORT         GPIOA
#define BUZZER_PIN          GPIO_PIN_2
#define BUZZER_ACTIVE_HIGH  0U          /* 有源蜂鸣器: 0=低电平触发 */

#define LED_OK_PORT         GPIOA
#define LED_OK_PIN          GPIO_PIN_7   /* 蓝灯 */
#define LED_ALARM_PORT      GPIOB
#define LED_ALARM_PIN       GPIO_PIN_0   /* 红灯 */
#define LED_ACTIVE_HIGH     1U

void DoorIO_Init(void);

void Buzzer_On(void);
void Buzzer_Off(void);
void Led_Ok(uint8_t on);
void Led_Alarm(uint8_t on);
void DoorIO_AllOff(void);

void DoorIO_BeepKey(void);     /* 按键短提示音 */
void DoorIO_BeepOk(void);      /* 成功提示(绿灯+两声) */
void DoorIO_ErrorAlarm(void);  /* 错误声光报警(红灯+长鸣) */
void DoorIO_AlarmPulse(uint8_t on); /* 锁定期间脉冲报警(红灯+蜂鸣同步) */

#endif
