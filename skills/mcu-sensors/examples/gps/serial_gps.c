#include "stm32f10x.h"
#include "Delay.h"
#include "Serial_GPS.h"

/*
 * 引脚分配（硬件已焊死）：
 *   PB15 = MCU TX → GPS 模块 RX
 *   PB14 = MCU RX ← GPS 模块 TX
 */
#define GPS_TX_PIN  GPIO_Pin_15
#define GPS_RX_PIN  GPIO_Pin_14

static uint32_t gBitUs = 104;  /* default 9600 baud */

void Serial_GPS_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	GPIO_InitTypeDef gpio;
	gpio.GPIO_Pin = GPS_TX_PIN;
	gpio.GPIO_Mode = GPIO_Mode_Out_PP;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &gpio);
	GPIO_SetBits(GPIOB, GPS_TX_PIN);

	gpio.GPIO_Pin = GPS_RX_PIN;
	gpio.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(GPIOB, &gpio);
}

void Serial_GPS_SetBaud(uint32_t baud)
{
	if (baud == 0) baud = 9600;
	gBitUs = 1000000u / baud;
	if (gBitUs < 8) gBitUs = 8;
}

uint32_t Serial_GPS_GetBaud(void)
{
	if (gBitUs == 0) return 0;
	return 1000000u / gBitUs;
}

void Serial_GPS_SendByte(uint8_t data)
{
	uint8_t i;

	GPIO_ResetBits(GPIOB, GPS_TX_PIN);
	Delay_us(gBitUs);
	for (i = 0; i < 8; i++) {
		if (data & 0x01)
			GPIO_SetBits(GPIOB, GPS_TX_PIN);
		else
			GPIO_ResetBits(GPIOB, GPS_TX_PIN);
		data >>= 1;
		Delay_us(gBitUs);
	}
	GPIO_SetBits(GPIOB, GPS_TX_PIN);
	Delay_us(gBitUs);
}

uint8_t Serial_GPS_ReceiveByte(void)
{
	uint8_t v;
	uint8_t i;

	while (GPIO_ReadInputDataBit(GPIOB, GPS_RX_PIN) == Bit_RESET)
		;
	while (GPIO_ReadInputDataBit(GPIOB, GPS_RX_PIN) == Bit_SET)
		;

	__disable_irq();

	Delay_us(gBitUs / 2);
	if (GPIO_ReadInputDataBit(GPIOB, GPS_RX_PIN) != Bit_RESET) {
		__enable_irq();
		return 0;
	}

	v = 0;
	for (i = 0; i < 8; i++) {
		Delay_us(gBitUs);
		if (GPIO_ReadInputDataBit(GPIOB, GPS_RX_PIN) == Bit_SET)
			v |= (uint8_t)(1u << i);
	}
	Delay_us(gBitUs);

	__enable_irq();
	return v;
}

uint8_t Serial_GPS_TryReceiveByte(uint8_t *out, uint16_t timeout_ms)
{
	uint32_t total_us;
	uint32_t elapsed_us;
	uint8_t i;

	total_us = (uint32_t)timeout_ms * 1000u;
	elapsed_us = 0;

	/*
	 * Wait for idle (HIGH) then start bit (LOW).
	 * Poll at 10us intervals for precise edge detection.
	 * At 9600 baud (104us/bit), 10us gives <10% sampling offset.
	 */
	while (GPIO_ReadInputDataBit(GPIOB, GPS_RX_PIN) == Bit_RESET) {
		Delay_us(10);
		elapsed_us += 10;
		if (elapsed_us >= total_us)
			return 0u;
	}
	while (GPIO_ReadInputDataBit(GPIOB, GPS_RX_PIN) == Bit_SET) {
		Delay_us(10);
		elapsed_us += 10;
		if (elapsed_us >= total_us)
			return 0u;
	}

	__disable_irq();

	/*
	 * We detected the falling edge with 0-10us latency.
	 * Wait 1.5 bit periods to center on the first DATA bit.
	 * (half-bit to verify start, then full bit to first data center)
	 */
	Delay_us(gBitUs / 2);
	if (GPIO_ReadInputDataBit(GPIOB, GPS_RX_PIN) != Bit_RESET) {
		__enable_irq();
		return 0u;
	}

	*out = 0;
	for (i = 0; i < 8; i++) {
		Delay_us(gBitUs);
		if (GPIO_ReadInputDataBit(GPIOB, GPS_RX_PIN) == Bit_SET)
			*out |= (uint8_t)(1u << i);
	}
	Delay_us(gBitUs);

	__enable_irq();

	return 1u;
}
