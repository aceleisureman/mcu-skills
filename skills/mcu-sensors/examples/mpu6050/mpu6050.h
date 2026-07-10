#ifndef __MPU6050_H
#define __MPU6050_H

#include "stm32f10x.h"

typedef struct {
	int16_t AccX;
	int16_t AccY;
	int16_t AccZ;
	int16_t GyroX;
	int16_t GyroY;
	int16_t GyroZ;
	int16_t Temp;
} MPU6050_Data_t;

typedef struct {
	float Pitch;
	float Roll;
} MPU6050_Angle_t;

void MPU6050_Init(void);
uint8_t MPU6050_GetID(void);
void MPU6050_GetData(MPU6050_Data_t *data);
void MPU6050_GetAngle(MPU6050_Data_t *data, MPU6050_Angle_t *angle);

#endif
