#include "stm32f10x.h"
#include "MPU6050.h"
#include "Delay.h"
#include <math.h>

/* PB6=SCL, PB7=SDA (software I2C, separate from OLED on PB8/PB9) */
#define MPU_SCL_PORT	GPIOB
#define MPU_SCL_PIN		GPIO_Pin_6
#define MPU_SDA_PORT	GPIOB
#define MPU_SDA_PIN		GPIO_Pin_7

#define MPU_W_SCL(x)	GPIO_WriteBit(MPU_SCL_PORT, MPU_SCL_PIN, (BitAction)(x))
#define MPU_W_SDA(x)	GPIO_WriteBit(MPU_SDA_PORT, MPU_SDA_PIN, (BitAction)(x))
#define MPU_R_SDA()		GPIO_ReadInputDataBit(MPU_SDA_PORT, MPU_SDA_PIN)

#define MPU6050_ADDR	0xD0

#define MPU6050_SMPLRT_DIV		0x19
#define MPU6050_CONFIG			0x1A
#define MPU6050_GYRO_CONFIG		0x1B
#define MPU6050_ACCEL_CONFIG	0x1C
#define MPU6050_ACCEL_XOUT_H	0x3B
#define MPU6050_TEMP_OUT_H		0x41
#define MPU6050_GYRO_XOUT_H	0x43
#define MPU6050_PWR_MGMT_1		0x6B
#define MPU6050_PWR_MGMT_2		0x6C
#define MPU6050_WHO_AM_I		0x75

static void MPU_I2C_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = MPU_SCL_PIN;
	GPIO_Init(MPU_SCL_PORT, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = MPU_SDA_PIN;
	GPIO_Init(MPU_SDA_PORT, &GPIO_InitStructure);

	MPU_W_SCL(1);
	MPU_W_SDA(1);
}

static void MPU_I2C_Start(void)
{
	MPU_W_SDA(1);
	MPU_W_SCL(1);
	Delay_us(2);
	MPU_W_SDA(0);
	Delay_us(2);
	MPU_W_SCL(0);
}

static void MPU_I2C_Stop(void)
{
	MPU_W_SDA(0);
	MPU_W_SCL(1);
	Delay_us(2);
	MPU_W_SDA(1);
	Delay_us(2);
}

static void MPU_I2C_SendByte(uint8_t Byte)
{
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		MPU_W_SDA(!!(Byte & (0x80 >> i)));
		Delay_us(1);
		MPU_W_SCL(1);
		Delay_us(2);
		MPU_W_SCL(0);
		Delay_us(1);
	}
	MPU_W_SDA(1);
	Delay_us(1);
	MPU_W_SCL(1);
	Delay_us(2);
	MPU_W_SCL(0);
}

static uint8_t MPU_I2C_RecvByte(uint8_t ack)
{
	uint8_t i, byte = 0;
	MPU_W_SDA(1);
	for (i = 0; i < 8; i++)
	{
		MPU_W_SCL(1);
		Delay_us(2);
		byte = (byte << 1) | MPU_R_SDA();
		MPU_W_SCL(0);
		Delay_us(2);
	}
	if (ack)
		MPU_W_SDA(0);
	else
		MPU_W_SDA(1);
	Delay_us(1);
	MPU_W_SCL(1);
	Delay_us(2);
	MPU_W_SCL(0);
	MPU_W_SDA(1);
	return byte;
}

static void MPU6050_WriteReg(uint8_t reg, uint8_t data)
{
	MPU_I2C_Start();
	MPU_I2C_SendByte(MPU6050_ADDR);
	MPU_I2C_SendByte(reg);
	MPU_I2C_SendByte(data);
	MPU_I2C_Stop();
}

static uint8_t MPU6050_ReadReg(uint8_t reg)
{
	uint8_t data;
	MPU_I2C_Start();
	MPU_I2C_SendByte(MPU6050_ADDR);
	MPU_I2C_SendByte(reg);
	MPU_I2C_Start();
	MPU_I2C_SendByte(MPU6050_ADDR | 0x01);
	data = MPU_I2C_RecvByte(0);
	MPU_I2C_Stop();
	return data;
}

void MPU6050_Init(void)
{
	MPU_I2C_Init();
	MPU6050_WriteReg(MPU6050_PWR_MGMT_1, 0x01);	/* wake up, use X-axis gyro PLL */
	MPU6050_WriteReg(MPU6050_PWR_MGMT_2, 0x00);
	MPU6050_WriteReg(MPU6050_SMPLRT_DIV, 0x09);	/* sample rate = 1kHz / (1+9) = 100Hz */
	MPU6050_WriteReg(MPU6050_CONFIG, 0x06);			/* DLPF 5Hz bandwidth */
	MPU6050_WriteReg(MPU6050_GYRO_CONFIG, 0x18);	/* ±2000°/s */
	MPU6050_WriteReg(MPU6050_ACCEL_CONFIG, 0x00);	/* ±2g */
}

uint8_t MPU6050_GetID(void)
{
	return MPU6050_ReadReg(MPU6050_WHO_AM_I);
}

void MPU6050_GetData(MPU6050_Data_t *data)
{
	data->AccX  = (MPU6050_ReadReg(MPU6050_ACCEL_XOUT_H) << 8) | MPU6050_ReadReg(MPU6050_ACCEL_XOUT_H + 1);
	data->AccY  = (MPU6050_ReadReg(MPU6050_ACCEL_XOUT_H + 2) << 8) | MPU6050_ReadReg(MPU6050_ACCEL_XOUT_H + 3);
	data->AccZ  = (MPU6050_ReadReg(MPU6050_ACCEL_XOUT_H + 4) << 8) | MPU6050_ReadReg(MPU6050_ACCEL_XOUT_H + 5);
	data->Temp  = (MPU6050_ReadReg(MPU6050_TEMP_OUT_H) << 8) | MPU6050_ReadReg(MPU6050_TEMP_OUT_H + 1);
	data->GyroX = (MPU6050_ReadReg(MPU6050_GYRO_XOUT_H) << 8) | MPU6050_ReadReg(MPU6050_GYRO_XOUT_H + 1);
	data->GyroY = (MPU6050_ReadReg(MPU6050_GYRO_XOUT_H + 2) << 8) | MPU6050_ReadReg(MPU6050_GYRO_XOUT_H + 3);
	data->GyroZ = (MPU6050_ReadReg(MPU6050_GYRO_XOUT_H + 4) << 8) | MPU6050_ReadReg(MPU6050_GYRO_XOUT_H + 5);
}

void MPU6050_GetAngle(MPU6050_Data_t *data, MPU6050_Angle_t *angle)
{
	float ax = data->AccX / 16384.0f;
	float ay = data->AccY / 16384.0f;
	float az = data->AccZ / 16384.0f;

	angle->Pitch = atan2f(ax, sqrtf(ay * ay + az * az)) * 57.2957795f;
	angle->Roll  = atan2f(ay, sqrtf(ax * ax + az * az)) * 57.2957795f;
}
