#include "stm32f10x.h"
#include "Delay.h"

/*
 * AT8236 双路电机驱动 x2 模块，驱动四轮
 *
 * PWM引脚 (TIM2):
 *   PA0 (TIM2_CH1) -> 模块1 PWMA (左前轮)
 *   PA1 (TIM2_CH2) -> 模块1 PWMB (右前轮)
 *   PA2 (TIM2_CH3) -> 模块2 PWMA (左后轮)
 *   PA3 (TIM2_CH4) -> 模块2 PWMB (右后轮)
 *
 * 方向引脚 - 模块1 (前轮):
 *   PA4 -> AIN1, PA5 -> AIN2  (左前)
 *   PA6 -> BIN1, PA7 -> BIN2  (右前)
 *
 * 方向引脚 - 模块2 (后轮):
 *   PB0 -> AIN1, PB1 -> AIN2  (左后)
 *   PB10 -> BIN1, PB11 -> BIN2 (右后)
 *
 * STBY: 两个模块的STBY引脚直接接3.3V
 */

/* 模块1方向引脚 (前轮) */
#define M1_AIN1_PORT  GPIOA
#define M1_AIN1_PIN   GPIO_Pin_4
#define M1_AIN2_PORT  GPIOA
#define M1_AIN2_PIN   GPIO_Pin_5
#define M1_BIN1_PORT  GPIOA
#define M1_BIN1_PIN   GPIO_Pin_6
#define M1_BIN2_PORT  GPIOA
#define M1_BIN2_PIN   GPIO_Pin_7

/* 模块2方向引脚 (后轮) */
#define M2_AIN1_PORT  GPIOB
#define M2_AIN1_PIN   GPIO_Pin_0
#define M2_AIN2_PORT  GPIOB
#define M2_AIN2_PIN   GPIO_Pin_1
#define M2_BIN1_PORT  GPIOB
#define M2_BIN1_PIN   GPIO_Pin_10
#define M2_BIN2_PORT  GPIOB
#define M2_BIN2_PIN   GPIO_Pin_11

static void Motor_SetLeftDir(int8_t dir)
{
	if (dir > 0)
	{
		GPIO_SetBits(M1_AIN1_PORT, M1_AIN1_PIN);
		GPIO_ResetBits(M1_AIN2_PORT, M1_AIN2_PIN);
		GPIO_ResetBits(M2_AIN1_PORT, M2_AIN1_PIN);	/* 后轮反装，方向取反 */
		GPIO_SetBits(M2_AIN2_PORT, M2_AIN2_PIN);
	}
	else if (dir < 0)
	{
		GPIO_ResetBits(M1_AIN1_PORT, M1_AIN1_PIN);
		GPIO_SetBits(M1_AIN2_PORT, M1_AIN2_PIN);
		GPIO_SetBits(M2_AIN1_PORT, M2_AIN1_PIN);
		GPIO_ResetBits(M2_AIN2_PORT, M2_AIN2_PIN);
	}
	else
	{
		GPIO_ResetBits(M1_AIN1_PORT, M1_AIN1_PIN);
		GPIO_ResetBits(M1_AIN2_PORT, M1_AIN2_PIN);
		GPIO_ResetBits(M2_AIN1_PORT, M2_AIN1_PIN);
		GPIO_ResetBits(M2_AIN2_PORT, M2_AIN2_PIN);
	}
}

static void Motor_SetRightDir(int8_t dir)
{
	if (dir > 0)
	{
		GPIO_SetBits(M1_BIN1_PORT, M1_BIN1_PIN);
		GPIO_ResetBits(M1_BIN2_PORT, M1_BIN2_PIN);
		GPIO_ResetBits(M2_BIN1_PORT, M2_BIN1_PIN);	/* 后轮反装，方向取反 */
		GPIO_SetBits(M2_BIN2_PORT, M2_BIN2_PIN);
	}
	else if (dir < 0)
	{
		GPIO_ResetBits(M1_BIN1_PORT, M1_BIN1_PIN);
		GPIO_SetBits(M1_BIN2_PORT, M1_BIN2_PIN);
		GPIO_SetBits(M2_BIN1_PORT, M2_BIN1_PIN);
		GPIO_ResetBits(M2_BIN2_PORT, M2_BIN2_PIN);
	}
	else
	{
		GPIO_ResetBits(M1_BIN1_PORT, M1_BIN1_PIN);
		GPIO_ResetBits(M1_BIN2_PORT, M1_BIN2_PIN);
		GPIO_ResetBits(M2_BIN1_PORT, M2_BIN1_PIN);
		GPIO_ResetBits(M2_BIN2_PORT, M2_BIN2_PIN);
	}
}

void Motor_Init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	
	/* PWM引脚 PA0~PA3 复用推挽 */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	/* 模块1方向引脚 PA4~PA7 推挽输出 */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	/* 模块2方向引脚 PB0, PB1, PB10, PB11 推挽输出 */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_10 | GPIO_Pin_11;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	/* TIM2: 72MHz / (71+1) / (99+1) = 10kHz PWM */
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_TimeBaseStructure.TIM_Period = 99;
	TIM_TimeBaseStructure.TIM_Prescaler = 71;
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
	
	TIM_OCInitTypeDef TIM_OCInitStructure;
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 0;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	
	TIM_OC1Init(TIM2, &TIM_OCInitStructure);
	TIM_OC2Init(TIM2, &TIM_OCInitStructure);
	TIM_OC3Init(TIM2, &TIM_OCInitStructure);
	TIM_OC4Init(TIM2, &TIM_OCInitStructure);
	
	TIM_Cmd(TIM2, ENABLE);
	
	Motor_SetLeftDir(0);
	Motor_SetRightDir(0);
}

void Motor_SetSpeed(int16_t SpeedL, int16_t SpeedR)
{
	uint16_t pwmL, pwmR;
	
	if (SpeedL > 100) SpeedL = 100;
	if (SpeedL < -100) SpeedL = -100;
	if (SpeedR > 100) SpeedR = 100;
	if (SpeedR < -100) SpeedR = -100;
	
	if (SpeedL > 0)
	{
		Motor_SetLeftDir(1);
		pwmL = (uint16_t)SpeedL;
	}
	else if (SpeedL < 0)
	{
		Motor_SetLeftDir(-1);
		pwmL = (uint16_t)(-SpeedL);
	}
	else
	{
		Motor_SetLeftDir(0);
		pwmL = 0;
	}
	
	if (SpeedR > 0)
	{
		Motor_SetRightDir(1);
		pwmR = (uint16_t)SpeedR;
	}
	else if (SpeedR < 0)
	{
		Motor_SetRightDir(-1);
		pwmR = (uint16_t)(-SpeedR);
	}
	else
	{
		Motor_SetRightDir(0);
		pwmR = 0;
	}
	
	TIM_SetCompare1(TIM2, pwmL);   /* 左前 */
	TIM_SetCompare2(TIM2, pwmR);   /* 右前 */
	TIM_SetCompare3(TIM2, pwmL);   /* 左后 */
	TIM_SetCompare4(TIM2, pwmR);   /* 右后 */
}

void Motor_Forward(uint16_t Speed)
{
	Motor_SetSpeed(-(int16_t)Speed, -(int16_t)Speed);
}

void Motor_Backward(uint16_t Speed)
{
	Motor_SetSpeed((int16_t)Speed, (int16_t)Speed);
}

void Motor_TurnLeft(uint16_t Speed)
{
	Motor_SetSpeed(-(int16_t)Speed, (int16_t)Speed);
}

void Motor_TurnRight(uint16_t Speed)
{
	Motor_SetSpeed((int16_t)Speed, -(int16_t)Speed);
}

void Motor_Stop(void)
{
	Motor_SetSpeed(0, 0);
}

/*
 * 逐个轮子测试：左前->右前->左后->右后，每个转1秒
 * 用于排查哪个通道/模块没接好
 */
void Motor_Test(void)
{
	/* 左前轮: TIM2_CH1(PA0) + M1_AIN */
	Motor_SetLeftDir(1);
	Motor_SetRightDir(0);
	TIM_SetCompare1(TIM2, 50); TIM_SetCompare2(TIM2, 0);
	TIM_SetCompare3(TIM2, 0);  TIM_SetCompare4(TIM2, 0);
	Delay_ms(1000);
	
	/* 右前轮: TIM2_CH2(PA1) + M1_BIN */
	Motor_SetLeftDir(0);
	Motor_SetRightDir(1);
	TIM_SetCompare1(TIM2, 0);  TIM_SetCompare2(TIM2, 50);
	TIM_SetCompare3(TIM2, 0);  TIM_SetCompare4(TIM2, 0);
	Delay_ms(1000);
	
	/* 左后轮: TIM2_CH3(PA2) + M2_AIN */
	Motor_SetLeftDir(1);
	Motor_SetRightDir(0);
	TIM_SetCompare1(TIM2, 0);  TIM_SetCompare2(TIM2, 0);
	TIM_SetCompare3(TIM2, 50); TIM_SetCompare4(TIM2, 0);
	Delay_ms(1000);
	
	/* 右后轮: TIM2_CH4(PA3) + M2_BIN */
	Motor_SetLeftDir(0);
	Motor_SetRightDir(1);
	TIM_SetCompare1(TIM2, 0);  TIM_SetCompare2(TIM2, 0);
	TIM_SetCompare3(TIM2, 0);  TIM_SetCompare4(TIM2, 50);
	Delay_ms(1000);
	
	Motor_Stop();
}
