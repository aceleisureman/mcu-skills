#ifndef __CCS811_H
#define __CCS811_H
#include <stdint.h>

uint8_t CCS811_Init(void);                       /* 0=成功 */
uint8_t CCS811_Read(uint16_t *eco2, uint16_t *tvoc); /* 0=有新数据 */
void CCS811_SetEnvData(float temp, float humi);  /* 温湿度补偿 */
#endif
