#ifndef __ESP8266_H
#define __ESP8266_H

#include "stm32f1xx_hal.h"

/* Ring buffer size */
#define ESP_RX_BUF_SIZE  512
#define ESP_CMD_BUF_SIZE 256
#define ESP_IP_BUF_SIZE  16

void ESP8266_Init(void);
uint8_t ESP8266_SendCmd(const char *cmd, const char *expected, uint32_t timeout_ms);
uint8_t ESP8266_ConnectWiFi(const char *ssid, const char *password);
uint8_t ESP8266_ConnectMQTT(const char *host, uint16_t port, const char *client_id);
uint8_t ESP8266_MQTTSubscribe(const char *topic);
uint8_t ESP8266_MQTTPublish(const char *topic, const char *payload, uint8_t retain);
void ESP8266_GetIP(char *ip_buf);
uint8_t ESP8266_CheckConnection(void);
uint16_t ESP8266_GetReceivedData(char *buf, uint16_t max_len);
uint8_t ESP8266_GetTime(char *time_str);  /* HH:MM format */

/* Extern for IRQ handler access */
extern UART_HandleTypeDef huart1;
extern uint8_t esp_rx_buf[ESP_RX_BUF_SIZE];
extern volatile uint16_t esp_rx_head;
extern volatile uint16_t esp_rx_tail;

#endif /* __ESP8266_H */
