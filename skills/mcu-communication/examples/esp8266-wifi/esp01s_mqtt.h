/**
  * @file  esp01s.h
  * @brief ESP-01S WiFi 模块驱动（USART1，AT 指令），HTTP 上传 OneNET 数据流
  */
#ifndef __ESP01S_H
#define __ESP01S_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>

typedef enum {
  ESP_OK = 0,
  ESP_ERR_AT,        /* 模块无响应 */
  ESP_ERR_WIFI,      /* 连接热点失败 */
  ESP_ERR_TCP,       /* 建立 TCP 失败 */
  ESP_ERR_SEND       /* 发送/上传失败 */
} EspStatus;

/* 连接进度回调：wifi/mqtt 为 "conn"/"ok"/"fail"/"-"，reason 为失败原因(ASCII) */
typedef void (*EspStatusCb)(const char *wifi, const char *mqtt, const char *reason);

void      ESP_RxByte(uint8_t b);    /* 由 USART2 中断调用 */
EspStatus ESP_Init(EspStatusCb cb); /* 复位、连 WiFi 并建立 MQTT 连接；cb 可为 NULL */
uint8_t   ESP_IsWifiConnected(void);
uint8_t   ESP_IsCloudConnected(void);  /* MQTT 是否已连接 */

/* SNTP 授时：使能 ESP 内置 SNTP 并等待首次同步，输出北京时间(UTC+8)；成功返回1 */
uint8_t     ESP_SyncTimeUTC8(uint8_t *h, uint8_t *m, uint8_t *s);
/* 快速单次查询当前 SNTP 时间（供周期性重校时用，不长等待）；成功返回1 */
uint8_t     ESP_QueryTimeUTC8(uint8_t *h, uint8_t *m, uint8_t *s);
const char *ESP_NtpDebug(void);   /* 授时失败时的诊断串 */

/* 通过物模型 MQTT 上报属性到 OneNET
 * 传入 params 内部键值（如 "\"temp\":{\"value\":25}"），
 * 内部封装成 {"id":"1","params":{...}} 并 PUBLISH 到属性上报主题 */
EspStatus ESP_UploadOneNet(const char *params_json);

/* 属性设置下发回调：收到云端 thing/property/set 时被调用，
 * params 为 params 对象内部文本（如 "\"m1_hour\":9,\"m1_min\":30"） */
typedef void (*EspPropSetCb)(const char *params_json);
void ESP_SetOnPropertySet(EspPropSetCb cb);

/* 轮询处理 ESP 上行数据（含云端属性设置下发）；在主循环中周期调用（非阻塞） */
void ESP_Poll(void);

/* 诊断：是否已确认订阅 set 主题；累计收到的下发条数 */
uint8_t  ESP_IsSubscribed(void);
uint32_t ESP_SetRxCount(void);
uint32_t ESP_TopicSeen(void);
uint32_t ESP_IpdSeen(void);

#ifdef __cplusplus
}
#endif
#endif
