#ifndef __MOTOR_H
#define __MOTOR_H

void Motor_Init(void);
void Motor_SetSpeed(int16_t SpeedL, int16_t SpeedR);
void Motor_Forward(uint16_t Speed);
void Motor_Backward(uint16_t Speed);
void Motor_TurnLeft(uint16_t Speed);
void Motor_TurnRight(uint16_t Speed);
void Motor_Stop(void);
void Motor_Test(void);

#endif
