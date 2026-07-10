/* 软件 I2C（OLED 与 CCS811 共用总线）+ DWT 微秒延时 */
#include "soft_i2c.h"
#include "board_config.h"

#define SCL_H() HAL_GPIO_WritePin(SI2C_SCL_PORT, SI2C_SCL_PIN, GPIO_PIN_SET)
#define SCL_L() HAL_GPIO_WritePin(SI2C_SCL_PORT, SI2C_SCL_PIN, GPIO_PIN_RESET)
#define SDA_H() HAL_GPIO_WritePin(SI2C_SDA_PORT, SI2C_SDA_PIN, GPIO_PIN_SET)
#define SDA_L() HAL_GPIO_WritePin(SI2C_SDA_PORT, SI2C_SDA_PIN, GPIO_PIN_RESET)
#define SDA_READ() (HAL_GPIO_ReadPin(SI2C_SDA_PORT, SI2C_SDA_PIN) == GPIO_PIN_SET)

void delay_us_init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void delay_us(uint32_t us)
{
  uint32_t start = DWT->CYCCNT;
  uint32_t ticks = us * (SystemCoreClock / 1000000U);
  while ((DWT->CYCCNT - start) < ticks);
}

static void i2c_delay(void) { delay_us(4); }

static void sda_out_od(void)
{
  GPIO_InitTypeDef g = {0};
  g.Pin = SI2C_SDA_PIN;
  g.Mode = GPIO_MODE_OUTPUT_OD;
  g.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(SI2C_SDA_PORT, &g);
}

void SI2C_Init(void)
{
  GPIO_InitTypeDef g = {0};
  g.Pin = SI2C_SCL_PIN;
  g.Mode = GPIO_MODE_OUTPUT_OD;
  g.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(SI2C_SCL_PORT, &g);
  sda_out_od();
  SCL_H(); SDA_H();
}

void SI2C_Start(void)
{
  SDA_H(); SCL_H(); i2c_delay();
  SDA_L(); i2c_delay();
  SCL_L();
}

void SI2C_Stop(void)
{
  SCL_L(); SDA_L(); i2c_delay();
  SCL_H(); i2c_delay();
  SDA_H(); i2c_delay();
}

uint8_t SI2C_WriteByte(uint8_t b)
{
  uint8_t i, nack;
  for (i = 0; i < 8; i++) {
    if (b & 0x80) SDA_H(); else SDA_L();
    b <<= 1;
    i2c_delay();
    SCL_H(); i2c_delay();
    SCL_L();
  }
  SDA_H(); i2c_delay();          /* 释放 SDA 读 ACK */
  SCL_H(); i2c_delay();
  nack = SDA_READ() ? 1 : 0;
  SCL_L();
  return nack;
}

uint8_t SI2C_ReadByte(uint8_t ack)
{
  uint8_t i, b = 0;
  SDA_H();
  for (i = 0; i < 8; i++) {
    b <<= 1;
    SCL_H(); i2c_delay();
    if (SDA_READ()) b |= 1;
    SCL_L(); i2c_delay();
  }
  if (ack) SDA_L(); else SDA_H();
  i2c_delay();
  SCL_H(); i2c_delay();
  SCL_L(); SDA_H();
  return b;
}

uint8_t SI2C_WriteRegs(uint8_t addr, uint8_t reg, const uint8_t *buf, uint16_t len)
{
  uint16_t i;
  SI2C_Start();
  if (SI2C_WriteByte(addr)) { SI2C_Stop(); return 1; }
  if (SI2C_WriteByte(reg))  { SI2C_Stop(); return 1; }
  for (i = 0; i < len; i++)
    if (SI2C_WriteByte(buf[i])) { SI2C_Stop(); return 1; }
  SI2C_Stop();
  return 0;
}

uint8_t SI2C_ReadRegs(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
  uint16_t i;
  SI2C_Start();
  if (SI2C_WriteByte(addr)) { SI2C_Stop(); return 1; }
  if (SI2C_WriteByte(reg))  { SI2C_Stop(); return 1; }
  SI2C_Start();
  if (SI2C_WriteByte(addr | 0x01)) { SI2C_Stop(); return 1; }
  for (i = 0; i < len; i++)
    buf[i] = SI2C_ReadByte(i < (len - 1));
  SI2C_Stop();
  return 0;
}
