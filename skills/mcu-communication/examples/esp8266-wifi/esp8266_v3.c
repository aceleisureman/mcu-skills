/* ESP8266 WiFi 模块：AT 指令联网 + MQTT 3.1.1 over raw TCP
   USART2: PA2-TX PA3-RX，115200 */
#include "esp8266.h"
#include "board_config.h"
#include "debug_uart.h"
#include <stdio.h>
#include <string.h>

static UART_HandleTypeDef huart1;

#define RX_BUF_SIZE 256
#define RESP_BUF_SIZE 64
static volatile uint8_t rx_buf[RX_BUF_SIZE];
static volatile uint16_t rx_len;
static uint8_t online;
static uint8_t wifi_online;
static char last_step[16] = "INIT";
static char last_resp[RESP_BUF_SIZE];

static const char *debug_cmd(const char *cmd)
{
  return (cmd != NULL && strstr(cmd, "CWJAP") != NULL) ? "AT+CWJAP=\"***\",\"***\"" : cmd;
}

void ESP8266_Init(void)
{
  GPIO_InitTypeDef g = {0};

  __HAL_RCC_USART2_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  g.Pin = GPIO_PIN_2;                 /* TX */
  g.Mode = GPIO_MODE_AF_PP;
  g.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &g);
  g.Pin = GPIO_PIN_3;                 /* RX */
  g.Mode = GPIO_MODE_INPUT;
  g.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &g);

  huart1.Instance = ESP_UART;
  huart1.Init.BaudRate = ESP_UART_BAUD;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  HAL_UART_Init(&huart1);

  __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
  HAL_NVIC_SetPriority(USART2_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(USART2_IRQn);

  Debug_Printf("[WIFI] ESP8266 UART init USART2 %lu\r\n", (unsigned long)ESP_UART_BAUD);

  /* 等待 ESP8266 上电启动完成后再发 AT，避免与模块开机抢时间导致首连失败 */
  HAL_Delay(1500);
}

void ESP8266_UART_IRQHandler(void)
{
  if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE)) {
    uint8_t c = (uint8_t)(huart1.Instance->DR & 0xFF);
    if (rx_len < RX_BUF_SIZE - 1) rx_buf[rx_len++] = c;
  }
  if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_ORE)) {
    __HAL_UART_CLEAR_OREFLAG(&huart1);
  }
}

static void rx_clear(void) { rx_len = 0; memset((void *)rx_buf, 0, RX_BUF_SIZE); }

static void set_last_step(const char *step)
{
  if (step == NULL) step = "";
  strncpy(last_step, step, sizeof(last_step) - 1u);
  last_step[sizeof(last_step) - 1u] = '\0';
}

static void snapshot_last_resp(void)
{
  uint16_t i;
  uint16_t j = 0u;

  memset(last_resp, 0, sizeof(last_resp));
  for (i = 0; i < rx_len && j < (RESP_BUF_SIZE - 1u); i++) {
    uint8_t c = rx_buf[i];
    last_resp[j++] = (c >= 32u && c <= 126u) ? (char)c : ' ';
  }
  last_resp[j] = '\0';
}

static void debug_resp(const char *tag, const char *step, uint8_t ok)
{
  Debug_Printf("%s %s %s resp=\"%s\"\r\n", tag, step, ok ? "OK" : "FAIL", last_resp);
}

static void uart_send(const char *s)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)strlen(s), 1000);
}

static void uart_send_raw(const uint8_t *buf, uint16_t len)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)buf, len, 1000);
}

/* 二进制安全查找：MQTT 包内含 0x00，strstr 会被截断 */
static const uint8_t *rx_find(const char *s)
{
  uint16_t slen = (uint16_t)strlen(s);
  uint16_t i;
  if (slen == 0u || rx_len < slen) return NULL;
  for (i = 0; i + slen <= rx_len; i++) {
    if (memcmp((const void *)&rx_buf[i], s, slen) == 0) return (const uint8_t *)&rx_buf[i];
  }
  return NULL;
}

static uint8_t rx_has_str(const char *s)
{
  return (rx_find(s) != NULL) ? 1u : 0u;
}

static uint8_t rx_has_bytes(const uint8_t *pat, uint16_t pat_len)
{
  uint16_t i, j;
  if (pat_len == 0u) return 1u;
  if (rx_len < pat_len) return 0u;
  for (i = 0; i + pat_len <= rx_len; i++) {
    for (j = 0; j < pat_len; j++) {
      if (rx_buf[i + j] != pat[j]) break;
    }
    if (j == pat_len) return 1u;
  }
  return 0u;
}

static uint8_t wait_for_any_str(const char *a, const char *b, uint32_t timeout_ms)
{
  uint32_t t0 = HAL_GetTick();
  while (HAL_GetTick() - t0 < timeout_ms) {
    if (rx_has_str(a) || rx_has_str(b)) return 0;
    if (rx_has_str("ERROR")) return 1;
    HAL_Delay(10);
  }
  return 1;
}

static uint8_t wait_for_bytes(const uint8_t *pat, uint16_t pat_len, uint32_t timeout_ms)
{
  uint32_t t0 = HAL_GetTick();
  while (HAL_GetTick() - t0 < timeout_ms) {
    if (rx_has_bytes(pat, pat_len)) return 0;
    HAL_Delay(10);
  }
  return 1;
}

/* 发送 AT 命令并等待关键字，0=成功 */
static uint8_t at_cmd(const char *step, const char *cmd, const char *ack, uint32_t timeout_ms)
{
  uint32_t t0;
  set_last_step(step);
  rx_clear();
  Debug_Printf("[AT> %s] %s\r\n", step, debug_cmd(cmd));
  uart_send(cmd);
  uart_send("\r\n");
  t0 = HAL_GetTick();
  while (HAL_GetTick() - t0 < timeout_ms) {
    if (rx_has_str(ack)) {
      snapshot_last_resp();
      debug_resp("[AT<", step, 1u);
      return 0;
    }
    if (rx_has_str("ERROR")) {
      snapshot_last_resp();
      debug_resp("[AT<", step, 0u);
      return 1;
    }
    HAL_Delay(10);
  }
  snapshot_last_resp();
  debug_resp("[AT<", step, 0u);
  return 1;
}

static uint8_t at_cmd_ok_or_connect(const char *step, const char *cmd, uint32_t timeout_ms)
{
  uint32_t t0;
  set_last_step(step);
  rx_clear();
  Debug_Printf("[AT> %s] %s\r\n", step, debug_cmd(cmd));
  uart_send(cmd);
  uart_send("\r\n");
  t0 = HAL_GetTick();
  while (HAL_GetTick() - t0 < timeout_ms) {
    if (rx_has_str("OK") || rx_has_str("CONNECT")) {
      snapshot_last_resp();
      debug_resp("[TCP]", step, 1u);
      return 0;
    }
    if (rx_has_str("ERROR") || rx_has_str("FAIL")) {
      snapshot_last_resp();
      debug_resp("[TCP]", step, 0u);
      return 1;
    }
    HAL_Delay(10);
  }
  snapshot_last_resp();
  debug_resp("[TCP]", step, 0u);
  return 1;
}

static uint8_t at_cmd_wifi_join(const char *step, const char *cmd, uint32_t timeout_ms)
{
  uint32_t t0;
  uint8_t got_ip = 0u;
  set_last_step(step);
  rx_clear();
  Debug_Printf("[AT> %s] %s\r\n", step, debug_cmd(cmd));
  uart_send(cmd);
  uart_send("\r\n");
  t0 = HAL_GetTick();
  while (HAL_GetTick() - t0 < timeout_ms) {
    if (!got_ip && rx_has_str("WIFI GOT IP")) {
      got_ip = 1u;
      Debug_Printf("[WIFI] got ip, waiting final OK\r\n");
    }
    if (rx_has_str("OK")) {
      snapshot_last_resp();
      debug_resp("[WIFI]", step, 1u);
      return 0;
    }
    if (rx_has_str("ERROR") || rx_has_str("FAIL")) {
      snapshot_last_resp();
      debug_resp("[WIFI]", step, 0u);
      return 1;
    }
    HAL_Delay(10);
  }
  snapshot_last_resp();
  debug_resp("[WIFI]", step, 0u);
  return 1;
}

static uint8_t mqtt_encode_remaining(uint8_t *out, uint16_t len)
{
  uint8_t used = 0u;
  do {
    uint8_t byte = (uint8_t)(len % 128u);
    len /= 128u;
    if (len > 0u) byte |= 0x80u;
    out[used++] = byte;
  } while (len > 0u);
  return used;
}

static uint16_t mqtt_write_u16(uint8_t *out, uint16_t v)
{
  out[0] = (uint8_t)(v >> 8);
  out[1] = (uint8_t)(v & 0xFFu);
  return 2u;
}

static uint16_t mqtt_write_str(uint8_t *out, const char *s)
{
  uint16_t len = (uint16_t)strlen(s);
  uint16_t pos = 0u;
  pos += mqtt_write_u16(out + pos, len);
  memcpy(out + pos, s, len);
  return (uint16_t)(pos + len);
}

static uint16_t mqtt_build_connect(uint8_t *pkt, uint16_t max_len)
{
  uint8_t flags = 0x02u;
  uint16_t pos = 0u;
  uint16_t vh_len = 10u;
  uint16_t pl_len = 0u;
  uint16_t cid_len = (uint16_t)strlen(MQTT_CLIENT_ID);
  uint16_t user_len = (uint16_t)strlen(MQTT_USER);
  uint16_t pass_len = (uint16_t)strlen(MQTT_PASS);
  uint16_t rem;

  if (user_len > 0u) {
    flags = 0xC2u;
    pl_len = (uint16_t)(pl_len + 2u + user_len + 2u + pass_len);
  }
  pl_len = (uint16_t)(pl_len + 2u + cid_len);
  rem = (uint16_t)(vh_len + pl_len);

  pkt[pos++] = 0x10u;
  pos += mqtt_encode_remaining(pkt + pos, rem);
  if (pos + rem > max_len) return 0u;

  pkt[pos++] = 0x00u; pkt[pos++] = 0x04u;
  pkt[pos++] = 'M'; pkt[pos++] = 'Q'; pkt[pos++] = 'T'; pkt[pos++] = 'T';
  pkt[pos++] = 0x04u;
  pkt[pos++] = flags;
  pos += mqtt_write_u16(pkt + pos, 60u);
  pos += mqtt_write_str(pkt + pos, MQTT_CLIENT_ID);
  if (user_len > 0u) {
    pos += mqtt_write_str(pkt + pos, MQTT_USER);
    pos += mqtt_write_str(pkt + pos, MQTT_PASS);
  }
  return pos;
}

static uint16_t mqtt_build_publish(uint8_t *pkt, uint16_t max_len, const char *topic, const uint8_t *payload, uint16_t payload_len)
{
  uint16_t pos = 0u;
  uint16_t topic_len = (uint16_t)strlen(topic);
  uint16_t rem = (uint16_t)(2u + topic_len + payload_len);

  pkt[pos++] = 0x30u;
  pos += mqtt_encode_remaining(pkt + pos, rem);
  if (pos + rem > max_len) return 0u;
  pos += mqtt_write_str(pkt + pos, topic);
  memcpy(pkt + pos, payload, payload_len);
  pos = (uint16_t)(pos + payload_len);
  return pos;
}

static uint16_t mqtt_build_subscribe(uint8_t *pkt, uint16_t max_len, const char *topic)
{
  uint16_t pos = 0u;
  uint16_t topic_len = (uint16_t)strlen(topic);
  uint16_t rem = (uint16_t)(2u + 2u + topic_len + 1u);

  pkt[pos++] = 0x82u;
  pos += mqtt_encode_remaining(pkt + pos, rem);
  if (pos + rem > max_len) return 0u;
  pkt[pos++] = 0x00u;
  pkt[pos++] = 0x01u;
  pos += mqtt_write_str(pkt + pos, topic);
  pkt[pos++] = 0x00u;
  return pos;
}

static uint8_t mqtt_send_packet(const char *step, const uint8_t *pkt, uint16_t pkt_len);

uint8_t ESP8266_PublishJSON(const char *topic, const char *json)
{
  uint8_t pkt[220];
  uint16_t len;

  if (!online) return 1u;
  if (topic == NULL || json == NULL) return 1u;

  len = mqtt_build_publish(pkt, sizeof(pkt), topic, (const uint8_t *)json, (uint16_t)strlen(json));
  if (len == 0u) return 1u;
  if (mqtt_send_packet("PUBLISH", pkt, len)) {
    online = 0;
    return 1u;
  }
  return 0u;
}

static uint8_t mqtt_send_packet(const char *step, const uint8_t *pkt, uint16_t pkt_len)
{
  char cmd[24];

  set_last_step(step);
  rx_clear();
  snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", (unsigned)pkt_len);
  Debug_Printf("[MQTT] send packet step=%s len=%u\r\n", step, (unsigned)pkt_len);
  if (at_cmd("CIPSEND", cmd, ">", 500)) {
    /* 可能只是上一包尚未发完，ESP 短暂忙：重试一次再判失败 */
    rx_clear();
    if (at_cmd("CIPSEND", cmd, ">", 800)) {
      Debug_Printf("[MQTT] CIPSEND failed step=%s\r\n", step);
      return 1;
    }
  }
  uart_send_raw(pkt, pkt_len);
  /* QoS0 准异步：短等 SEND OK，超时不计失败，链路异常由 PollLink 检测 */
  (void)wait_for_any_str("SEND OK", "OK", 300);
  snapshot_last_resp();
  return 0;
}

static uint8_t mqtt_send_connect(void)
{
  uint8_t pkt[200];
  uint16_t len = mqtt_build_connect(pkt, sizeof(pkt));
  uint8_t connack[] = {0x20u, 0x02u, 0x00u, 0x00u};
  if (len == 0u) return 1;
  Debug_Printf("[MQTT] CONNECT send client=%s\r\n", MQTT_CLIENT_ID);
  if (mqtt_send_packet("MQTT", pkt, len)) return 1;
  set_last_step("MQTT");
  /* 冷启动 TCP+broker 往返较慢，CONNACK 等待放宽到 3000ms */
  if (wait_for_bytes(connack, sizeof(connack), 3000)) {
    snapshot_last_resp();
    debug_resp("[MQTT]", "CONNACK", 0u);
    return 1;
  }
  if (rx_len >= 4u && rx_buf[0] == 0x20u && rx_buf[1] == 0x02u && rx_buf[3] != 0x00u) {
    snapshot_last_resp();
    Debug_Printf("[MQTT] CONNACK reject code=%u resp=\"%s\"\r\n", (unsigned)rx_buf[3], last_resp);
    return 1;
  }
  snapshot_last_resp();
  Debug_Printf("[MQTT] CONNACK OK resp=\"%s\"\r\n", last_resp);
  HAL_Delay(200);
  return 0;
}

uint8_t ESP8266_ConnectWiFi(void)
{
  char cmd[128];

  uint8_t i;

  Debug_Printf("[WIFI] connect start ssid=%s\r\n", WIFI_SSID);
  online = 0;
  wifi_online = 0;
  set_last_step("AT");
  rx_clear();
  uart_send("+++");                            /* 退出可能残留的透传 */
  snapshot_last_resp();
  HAL_Delay(1000);
  /* 冷启动时模块可能尚未就绪，重试若干次 AT 直到回 OK */
  for (i = 0u; i < 5u; i++) {
    Debug_Printf("[WIFI] AT probe %u\r\n", (unsigned)(i + 1u));
    if (at_cmd("AT", "AT", "OK", 1000) == 0) break;
    HAL_Delay(500);
  }
  if (i >= 5u) {
    Debug_Printf("[WIFI] ESP not responding\r\n");
    return 1;
  }
  at_cmd("ATE0", "ATE0", "OK", 1000);
  if (at_cmd("CWMODE", "AT+CWMODE=1", "OK", 1000)) {
    Debug_Printf("[WIFI] CWMODE failed\r\n");
    return 1;
  }
  at_cmd("CWQAP", "AT+CWQAP", "OK", 1000);

  snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", WIFI_SSID, WIFI_PASSWORD);
  if (at_cmd_wifi_join("CWJAP", cmd, 20000)) {
    Debug_Printf("[WIFI] join failed rc=2\r\n");
    return 2;
  }
  wifi_online = 1;
  Debug_Printf("[WIFI] online\r\n");
  return 0;
}

uint8_t ESP8266_ConnectMQTT(void)
{
  char cmd[128];
  uint8_t pkt[96];
  uint16_t len;

  Debug_Printf("[MQTT] connect start host=%s port=%s\r\n", MQTT_HOST, MQTT_PORT);
  if (online) {
    online = 0;
  }

  if (at_cmd("CIPMUX", "AT+CIPMUX=0", "OK", 1000)) {
    Debug_Printf("[TCP] CIPMUX busy/fail, retry\r\n");
    HAL_Delay(500);
    if (at_cmd("CIPMUX", "AT+CIPMUX=0", "OK", 1500)) {
      wifi_online = 0;
      Debug_Printf("[TCP] CIPMUX failed rc=3\r\n");
      return 3;
    }
  }
  snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%s", MQTT_HOST, MQTT_PORT);
  if (at_cmd_ok_or_connect("CIPSTART", cmd, 8000)) {
    wifi_online = 0;
    Debug_Printf("[TCP] CIPSTART failed rc=3\r\n");
    return 3;
  } /* 连 MQTT Broker，失败视为 WiFi 异常 */

  if (mqtt_send_connect()) {
    Debug_Printf("[MQTT] connect failed rc=4\r\n");
    return 4;
  }

  online = 1;
  Debug_Printf("[MQTT] online\r\n");
  len = mqtt_build_subscribe(pkt, sizeof(pkt), MQTT_TOPIC_CMD);
  if (len != 0u) {
    (void)mqtt_send_packet("SUB", pkt, len);
    Debug_Printf("[MQTT] subscribed %s\r\n", MQTT_TOPIC_CMD);
  }
  HAL_Delay(100);
  return 0;
}

uint8_t ESP8266_Connect(void)
{
  uint8_t rc = ESP8266_ConnectWiFi();
  if (rc != 0) return rc;
  return ESP8266_ConnectMQTT();
}

uint8_t ESP8266_IsOnline(void) { return online; }
uint8_t ESP8266_WifiOnline(void) { return wifi_online; }

/* 主循环轻量轮询：从接收缓冲区发现链路异常 */
void ESP8266_PollLink(void)
{
  if (online && rx_has_str("CLOSED")) {
    online = 0;
  }
  if (wifi_online && rx_has_str("WIFI DISCONNECT")) {
    wifi_online = 0;
    online = 0;
  }
}
const char *ESP8266_LastStep(void) { return last_step; }
const char *ESP8266_LastResp(void) { return last_resp; }

uint8_t ESP8266_PollCommand(char *out, uint16_t out_size)
{
  char *topic;
  char *json_start;
  char *json_end;
  uint16_t len;

  if (!online || out == NULL || out_size == 0u) return 0u;

  topic = (char *)rx_find(MQTT_TOPIC_CMD);
  if (topic == NULL) return 0u;

  json_start = memchr(topic, '{', (size_t)(rx_len - (uint16_t)(topic - (char *)rx_buf)));
  if (json_start == NULL) return 0u;

  json_end = memchr(json_start, '}', (size_t)(rx_len - (uint16_t)(json_start - (char *)rx_buf)));
  if (json_end == NULL) return 0u;

  len = (uint16_t)(json_end - json_start + 1);
  if (len >= out_size) len = (uint16_t)(out_size - 1u);
  memcpy(out, json_start, len);
  out[len] = '\0';
  rx_clear();
  return 1u;
}

/* MQTT JSON 上报 */
void ESP8266_Report(float temp, float humi, uint16_t co2,
                    float ph, float flow, const uint8_t dev_state[4],
                    uint8_t alarm, uint8_t manual)
{
  char payload[176];
  uint8_t pkt[220];
  uint16_t len;

  if (!online) return;

  snprintf(payload, sizeof(payload),
           "{\"temp\":%.1f,\"humi\":%.1f,\"co2\":%u,\"ph\":%.2f,\"flow\":%.1f,\"heater\":%u,\"cool\":%u,\"humidifier\":%u,\"vent\":%u,\"alarm\":%u,\"mode\":%u}",
           temp, humi, (unsigned)co2, ph, flow,
           dev_state[0], dev_state[1], dev_state[2], dev_state[3], alarm, manual);

  len = mqtt_build_publish(pkt, sizeof(pkt), MQTT_TOPIC, (const uint8_t *)payload, (uint16_t)strlen(payload));
  if (len == 0u) return;
  if (mqtt_send_packet("PUBLISH", pkt, len)) {
    online = 0;
  }
}
