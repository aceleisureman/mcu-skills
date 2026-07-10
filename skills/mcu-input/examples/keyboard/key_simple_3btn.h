#ifndef __KEY_H
#define __KEY_H

#include "stm32f1xx_hal.h"

/* 三个独立按键 (按键接 GND, 上拉输入, 按下为低电平)
 *   PB12 -> 布防 (ARM)
 *   PB13 -> 撤防 (DISARM)
 *   PB15 -> 紧急报警 (SOS)
 * 复位: 使用单片机 NRST 复位脚 (物理按钮接 NRST 到 GND), 不占用 GPIO.
 */
#define KEY_NONE    0U
#define KEY_ARM     1U
#define KEY_SOS     2U
#define KEY_DISARM  3U
#define KEY_RESET   4U   /* 保留(已改用 NRST 硬复位, 不再由按键产生) */

void Key_Init(void);

/* 非阻塞扫描, 返回本次新按下的键码 (仅按下沿触发一次); 无按键返回 KEY_NONE.
 * 需在主循环中周期性调用 (建议每 ~10ms).
 */
uint8_t Key_Scan(void);

#endif /* __KEY_H */
