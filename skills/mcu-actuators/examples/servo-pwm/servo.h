#ifndef __SERVO_H
#define __SERVO_H

#include "stm32f1xx_hal.h"

/*
 * PWM parameters for SG90 servo at 64MHz TIM3 clock (APB1=32MHz, TIM x2 = 64MHz)
 * PSC=63, ARR=19999 → 64MHz / (64 * 20000) = 50Hz
 * Pulse: 500(@0°) ... 2500(@180°) — SG90 180° range at 50Hz
 */
#define SERVO_PSC       63
#define SERVO_ARR       19999
#define SERVO_MIN_PULSE 500
#define SERVO_MAX_PULSE 2500

void Servo_Init(void);
void Servo_SetAngle(uint8_t angle);  /* angle: 0-180 degrees */
void Servo_SetAngle2(uint8_t angle); /* PB0 servo, angle: 0-180 degrees */
void Servo_Feed(void);               /* open gate, hold 1s, close */
void Servo_Water(void);              /* PB0 servo action, hold 1s, close */

#endif /* __SERVO_H */
