/**
  * @file  hx711.c
  * @brief HX711 24 位称重 ADC（GPIO 位操作，通道 A 增益 128）
  */
#include "hx711.h"
#include "bsp.h"

static int32_t s_offset = 0;
static float   s_scale  = 100.0f;   /* 默认每克 100 计数，需按实物校准 */

#define SCK_H()  HAL_GPIO_WritePin(HX711_SCK_PORT, HX711_SCK_PIN, GPIO_PIN_SET)
#define SCK_L()  HAL_GPIO_WritePin(HX711_SCK_PORT, HX711_SCK_PIN, GPIO_PIN_RESET)
#define DT_READ() HAL_GPIO_ReadPin(HX711_DT_PORT, HX711_DT_PIN)

void HX711_Init(void)
{
  SCK_L();
}

void HX711_Reset(void)
{
  SCK_L();
  HAL_Delay(1);
  SCK_H();
  BSP_DelayUs(100);
  SCK_L();
  HAL_Delay(500);
}

uint8_t HX711_IsReady(void)
{
  return (DT_READ() == GPIO_PIN_RESET);
}

uint8_t HX711_WaitReady(uint32_t timeout_ms)
{
  uint32_t start = HAL_GetTick();
  while ((HAL_GetTick() - start) < timeout_ms) {
    if (HX711_IsReady()) return 1;
    HAL_Delay(1);
  }
  return HX711_IsReady();
}

int32_t HX711_ReadRaw(void)
{
  uint32_t val = 0;

  /* 等待数据就绪（DOUT 变低），超时保护 */
  uint32_t timeout = 100000;
  while (DT_READ() == GPIO_PIN_SET && --timeout) { }
  if (timeout == 0) return s_offset;   /* 未就绪返回零点 */

  for (uint8_t i = 0; i < 24; i++) {
    SCK_H();
    BSP_DelayUs(1);
    val = (val << 1);
    if (DT_READ() == GPIO_PIN_SET) val++;
    SCK_L();
    BSP_DelayUs(1);
  }
  /* 第 25 个脉冲：设置通道 A 增益 128 */
  SCK_H(); BSP_DelayUs(1); SCK_L(); BSP_DelayUs(1);

  /* 24 位符号扩展 */
  if (val & 0x800000) val |= 0xFF000000;
  return (int32_t)val;
}

static int32_t read_avg(uint8_t times)
{
  if (times == 0) times = 1;
  int64_t sum = 0;
  for (uint8_t i = 0; i < times; i++) sum += HX711_ReadRaw();
  return (int32_t)(sum / times);
}

void HX711_Tare(uint8_t times)
{
  s_offset = read_avg(times);
}

void HX711_SetScale(float counts_per_gram)
{
  if (counts_per_gram != 0.0f) s_scale = counts_per_gram;
}

int32_t HX711_GetOffset(void)
{
  return s_offset;
}

void HX711_SetOffset(int32_t offset)
{
  s_offset = offset;
}

int32_t HX711_GetWeight(uint8_t times)
{
  int32_t raw = read_avg(times);
  int32_t delta = raw - s_offset;
  if (delta < 0) delta = -delta;
  float g = (float)delta / s_scale;
  if (g < 0) g = 0;
  return (int32_t)(g + 0.5f);
}
