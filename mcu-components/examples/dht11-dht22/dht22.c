/* DHT22 单总线驱动 */
#include "dht22.h"
#include "board_config.h"
#include "soft_i2c.h"   /* delay_us */

static void pin_out(void)
{
  GPIO_InitTypeDef g = {0};
  g.Pin = DHT22_GPIO_PIN;
  g.Mode = GPIO_MODE_OUTPUT_OD;
  g.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DHT22_GPIO_PORT, &g);
}

static void pin_in(void)
{
  GPIO_InitTypeDef g = {0};
  g.Pin = DHT22_GPIO_PIN;
  g.Mode = GPIO_MODE_INPUT;
  g.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(DHT22_GPIO_PORT, &g);
}

#define PIN_H() HAL_GPIO_WritePin(DHT22_GPIO_PORT, DHT22_GPIO_PIN, GPIO_PIN_SET)
#define PIN_L() HAL_GPIO_WritePin(DHT22_GPIO_PORT, DHT22_GPIO_PIN, GPIO_PIN_RESET)
#define PIN_READ() (HAL_GPIO_ReadPin(DHT22_GPIO_PORT, DHT22_GPIO_PIN) == GPIO_PIN_SET)

void DHT22_Init(void)
{
  pin_out();
  PIN_H();
}

/* 等待电平，超时返回 1 */
static uint8_t wait_level(uint8_t level, uint32_t timeout_us)
{
  uint32_t t = 0;
  while (PIN_READ() != level) {
    if (++t > timeout_us) return 1;
    delay_us(1);
  }
  return 0;
}

static uint8_t DHT22_ReadOnce(float *temp, float *humi)
{
  uint8_t data[5] = {0};
  uint8_t i, j;

  pin_out();
  PIN_L();
  HAL_Delay(20);             /* DHT11 起始信号 >=18ms */
  PIN_H();
  delay_us(30);
  pin_in();

  if (wait_level(0, 100)) return 1;   /* 从机响应低 80us */
  if (wait_level(1, 100)) return 1;   /* 响应高 80us */
  if (wait_level(0, 100)) return 1;   /* 开始输出数据 */

  for (j = 0; j < 5; j++) {
    for (i = 0; i < 8; i++) {
      if (wait_level(1, 80)) return 1;   /* 50us 低电平结束 */
      delay_us(40);                       /* 高电平 26~28us=0, 70us=1 */
      data[j] <<= 1;
      if (PIN_READ()) {
        data[j] |= 1;
        if (wait_level(0, 60)) return 1;
      }
    }
  }

  if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4]) return 2;

  *humi = (float)data[0];
  *temp = (float)data[2];
  return 0;
}

uint8_t DHT22_Read(float *temp, float *humi)
{
  uint8_t attempt;

  for (attempt = 0; attempt < 3u; attempt++) {
    if (DHT22_ReadOnce(temp, humi) == 0u) {
      return 0;
    }
    if (attempt < 2u) {
      HAL_Delay(40);
    }
  }
  return 1;
}
