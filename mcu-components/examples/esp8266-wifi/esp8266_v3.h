#ifndef __ESP8266_H
#define __ESP8266_H
#include <stdint.h>

/* ====== WiFi 与 MQTT 配置 ====== */
#define WIFI_SSID        "55555555"
#define WIFI_PASSWORD    "55555555"
#define MQTT_HOST        "47.92.236.139"
#define MQTT_PORT        "1883"          /* 18083 是 EMQX 仪表盘端口，MQTT TCP 为 1883 */
#define MQTT_CLIENT_ID   "cellar_stm32"
#define MQTT_TOPIC       "cellar/data"
#define MQTT_TOPIC_CMD   "cellar/cmd"    /* App 下发控制指令主题 */
#define MQTT_TOPIC_PARAM  "cellar/param"  /* 当前阈值参数主题 */
#define MQTT_USER        ""
#define MQTT_PASS        ""

void ESP8266_Init(void);                 /* 串口初始化 */
uint8_t ESP8266_ConnectWiFi(void);       /* 联网，0=成功 */
uint8_t ESP8266_ConnectMQTT(void);       /* MQTT 连接，0=成功 */
uint8_t ESP8266_Connect(void);           /* 联网+连接 MQTT，0=成功 */
uint8_t ESP8266_IsOnline(void);
uint8_t ESP8266_WifiOnline(void);
void ESP8266_PollLink(void);             /* 主循环轻量检测链路断开 */
const char *ESP8266_LastStep(void);
const char *ESP8266_LastResp(void);
/* 上报：温 湿 CO2 PH 气流 + 4 个设备状态 + alarm + mode(1=手动) */
void ESP8266_Report(float temp, float humi, uint16_t co2,
                    float ph, float flow, const uint8_t dev_state[4],
                    uint8_t alarm, uint8_t manual);
uint8_t ESP8266_PublishJSON(const char *topic, const char *json);
void ESP8266_UART_IRQHandler(void);      /* 在 USART1_IRQHandler 中调用 */
/* 轮询接收 App 下发的控制指令：收到则把 JSON 写入 out 并返回 1，否则返回 0 */
uint8_t ESP8266_PollCommand(char *out, uint16_t out_size);
#endif
