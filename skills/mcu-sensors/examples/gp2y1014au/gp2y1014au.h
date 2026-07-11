#ifndef __GP2Y1014AU_H
#define __GP2Y1014AU_H

#include "stm32f10x.h"
#include "delay.h"

/* 初始化 */
void     gp2y_adc_init(void);
void     gp2y_iled_init(void);

/* 滤波 */
uint16_t gp2y_filter(uint16_t new_val);

/* ADC 读取 */
uint16_t gp2y_adc_read(uint8_t channel);

/* 读取粉尘浓度 */
void     gp2y_read(uint16_t *raw, uint16_t *voltage, float *density);

#endif /* __GP2Y1014AU_H */
