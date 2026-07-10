/**
  * @file  dht11.c
  * @brief DHT11 单总线读取（PA8，开漏 + 上拉）
  */
#include "dht11.h"
#include "bsp.h"

#define DHT_H()  HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET)
#define DHT_L()  HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET)
#define DHT_IN() HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN)

/* 等待引脚到达指定电平，超时(us)返回 0xFFFF */
static uint16_t wait_level(GPIO_PinState level, uint16_t timeout_us)
{
  uint16_t t = 0;
  while (DHT_IN() != level) {
    BSP_DelayUs(1);
    if (++t > timeout_us) return 0xFFFF;
  }
  return t;
}

uint8_t DHT11_Read(uint8_t *temp, uint8_t *humi)
{
  uint8_t data[5] = {0};

  /* 主机起始信号：拉低 >=18ms，再释放 */
  DHT_L();
  HAL_Delay(20);
  DHT_H();
  BSP_DelayUs(30);

  /* 从机响应：拉低 80us，再拉高 80us */
  if (wait_level(GPIO_PIN_RESET, 100) == 0xFFFF) return 0;
  if (wait_level(GPIO_PIN_SET, 100)   == 0xFFFF) return 0;
  if (wait_level(GPIO_PIN_RESET, 100) == 0xFFFF) return 0;

  /* 读取 40 位 */
  for (uint8_t i = 0; i < 40; i++) {
    if (wait_level(GPIO_PIN_SET, 100) == 0xFFFF) return 0;  /* 50us 低电平结束 */
    BSP_DelayUs(35);                                        /* 采样点：26~28us=0, 70us=1 */
    data[i / 8] <<= 1;
    if (DHT_IN() == GPIO_PIN_SET) {
      data[i / 8] |= 1;
      if (wait_level(GPIO_PIN_RESET, 100) == 0xFFFF) return 0;
    }
  }

  DHT_H();

  /* 校验和 */
  if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4]) return 0;

  *humi = data[0];
  *temp = data[2];
  return 1;
}
