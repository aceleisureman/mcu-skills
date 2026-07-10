/* 执行设备继电器控制 */
#include "relays.h"
#include "board_config.h"

typedef struct { GPIO_TypeDef *port; uint16_t pin; } relay_io_t;

static const relay_io_t io[DEV_COUNT] = {
  {HEATER_PORT,     HEATER_PIN},
  {COOL_VALVE_PORT, COOL_VALVE_PIN},
  {HUMIDIFIER_PORT, HUMIDIFIER_PIN},
  {AIR_VALVE_PORT,  AIR_VALVE_PIN},
};

/* 各设备有效电平：光耦继电器与加湿器直驱 IO 极性可能相反，按设备独立配置 */
static const GPIO_PinState active_level[DEV_COUNT] = {
  RELAY_ACTIVE_LEVEL,        /* DEV_HEATER */
  RELAY_ACTIVE_LEVEL,        /* DEV_COOL_VALVE */
  HUMIDIFIER_ACTIVE_LEVEL,   /* DEV_HUMIDIFIER */
  RELAY_ACTIVE_LEVEL,        /* DEV_AIR_VALVE */
};

static uint8_t state[DEV_COUNT];
static uint8_t led_state;

void LED_Init(void)
{
  GPIO_InitTypeDef g = {0};
  g.Mode = GPIO_MODE_OUTPUT_PP;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  g.Pin = LED_PIN;
  HAL_GPIO_Init(LED_PORT, &g);
  LED_Set(0);
}

void LED_Set(uint8_t on)
{
  led_state = on ? 1u : 0u;
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, led_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void Relays_Init(void)
{
  uint8_t i;
  GPIO_InitTypeDef g = {0};
  g.Mode = GPIO_MODE_OUTPUT_PP;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  for (i = 0; i < DEV_COUNT; i++) {
    g.Pin = io[i].pin;
    HAL_GPIO_Init(io[i].port, &g);
    Relay_Set((device_t)i, 0);
  }
  LED_Init();
}

void Relay_Set(device_t dev, uint8_t on)
{
  GPIO_PinState act;
  GPIO_PinState lvl;
  if (dev >= DEV_COUNT) return;
  state[dev] = on ? 1 : 0;
  act = active_level[dev];
  lvl = on ? act
           : (act == GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET);
  HAL_GPIO_WritePin(io[dev].port, io[dev].pin, lvl);
}

uint8_t Relay_Get(device_t dev)
{
  return (dev < DEV_COUNT) ? state[dev] : 0;
}
