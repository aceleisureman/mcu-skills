#include "stm32f10x.h"
#include "HCSR04.h"
#include "Delay.h"

/* PA6=Trig, PA7=Echo */
#define TRIG_PORT	GPIOA
#define TRIG_PIN	GPIO_Pin_6
#define ECHO_PORT	GPIOA
#define ECHO_PIN	GPIO_Pin_7

#define TRIG_HIGH()	GPIO_SetBits(TRIG_PORT, TRIG_PIN)
#define TRIG_LOW()	GPIO_ResetBits(TRIG_PORT, TRIG_PIN)
#define ECHO_READ()	GPIO_ReadInputDataBit(ECHO_PORT, ECHO_PIN)

void HCSR04_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = TRIG_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(TRIG_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_InitStructure.GPIO_Pin = ECHO_PIN;
	GPIO_Init(ECHO_PORT, &GPIO_InitStructure);

	TRIG_LOW();
}

float HCSR04_GetDistance(void)
{
	uint32_t timeout;
	volatile uint32_t count = 0;

	TRIG_LOW();
	Delay_us(2);
	TRIG_HIGH();
	Delay_us(10);
	TRIG_LOW();

	timeout = 100000;
	while (ECHO_READ() == 0)
	{
		if (--timeout == 0) return -1.0f;
	}

	/*
	 * Count loop iterations while echo is high.
	 * At 72MHz, each loop iteration is ~10 cycles -> ~0.139us per iteration.
	 * Calibration factor derived empirically; adjust if needed.
	 */
	timeout = 1800000;
	while (ECHO_READ() == 1)
	{
		count++;
		if (--timeout == 0) return -1.0f;
	}

	/* Convert count to microseconds: ~7.2 iterations per us at 72MHz (rough) */
	float time_us = (float)count / 7.2f;
	float distance = time_us / 58.0f;

	if (distance > 400.0f || distance < 2.0f)
		return -1.0f;

	return distance;
}
