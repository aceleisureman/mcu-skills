#ifndef __PIR_H
#define __PIR_H

#include "stm32f1xx_hal.h"

/* 热释红外人体感应模块 -> PA8 (输出高电平表示检测到人体) */
#define PIR_PORT  GPIOA
#define PIR_PIN   GPIO_PIN_8

void Pir_Init(void);

/* 去抖读取: 连续多次(约 30ms)检测到高电平才返回 1, 避免毛刺误触发.
 * 需在主循环周期性调用 (建议每 ~10ms).
 */
uint8_t Pir_Triggered(void);

/* 调试用: 返回当前去抖计数(0..PIR_DEBOUNCE). 定位完可删 */
uint8_t Pir_DbgCount(void);

#endif /* __PIR_H */
