#ifndef __RELAYS_H
#define __RELAYS_H
#include <stdint.h>

typedef enum {
  DEV_HEATER = 0,   /* 电加热器 */
  DEV_COOL_VALVE,   /* 冷水电磁阀 */
  DEV_HUMIDIFIER,   /* 加湿器 */
  DEV_AIR_VALVE,    /* 电动风阀 */
  DEV_COUNT
} device_t;

void Relays_Init(void);
void Relay_Set(device_t dev, uint8_t on);
uint8_t Relay_Get(device_t dev);
void LED_Init(void);
void LED_Set(uint8_t on);
#endif
