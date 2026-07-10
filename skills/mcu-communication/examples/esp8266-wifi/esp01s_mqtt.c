/**
  * @file  esp01s.c
  * @brief ESP-01S AT 指令驱动 + OneNET 物模型 MQTT 上报
  *        —— 手工构造 MQTT CONNECT/PUBLISH 报文，经 AT 透传发送 ——
  */
#include "esp01s.h"
#include "app_config.h"
#include "bsp.h"
#include <string.h>
#include <stdio.h>

/* 物模型属性上报主题：$sys/{产品ID}/{设备名}/thing/property/post */
#define MQTT_TOPIC_POST  "$sys/" CFG_MQTT_PRODUCT_ID "/" CFG_MQTT_DEV_NAME "/thing/property/post"
/* 云端属性设置下发主题（订阅）与回复主题 */
#define MQTT_TOPIC_SET       "$sys/" CFG_MQTT_PRODUCT_ID "/" CFG_MQTT_DEV_NAME "/thing/property/set"
#define MQTT_TOPIC_SET_REPLY "$sys/" CFG_MQTT_PRODUCT_ID "/" CFG_MQTT_DEV_NAME "/thing/property/set_reply"

static EspPropSetCb s_prop_set_cb = 0;
void ESP_SetOnPropertySet(EspPropSetCb cb) { s_prop_set_cb = cb; }

static uint8_t mqtt_subscribe(const char *topic);

/* ---------------- 接收环形缓冲（USART2 中断填充） ---------------- */
#define RXBUF_SZ 512
static volatile uint8_t  s_rx[RXBUF_SZ];
static volatile uint16_t s_head = 0, s_tail = 0;
static uint8_t s_wifi_ok = 0;
static uint8_t s_mqtt_ok = 0;
static uint8_t s_sub_ok = 0;   /* SUBSCRIBE 是否已被 SUBACK 确认 */
static volatile uint32_t s_set_rx = 0;  /* 累计解析到的下发 set 条数（诊断）*/
static volatile uint32_t s_topic_seen = 0; /* 缓冲中出现 set 主题的次数（诊断）*/
static volatile uint32_t s_ipd_seen = 0;   /* 收到非 set 的 +IPD 次数（诊断）*/
static volatile uint32_t s_rx_total = 0;   /* 累计收到的字节数（诊断用） */

void ESP_RxByte(uint8_t b)
{
  s_rx_total++;
  uint16_t n = (uint16_t)((s_head + 1) % RXBUF_SZ);
  if (n != s_tail) { s_rx[s_head] = b; s_head = n; }
}

static void rx_flush(void) { s_tail = s_head; }

/* 采集应答文本到 out（基于空闲判定，收到字节后重置计时），返回字节数 */
static uint16_t esp_collect(char *out, uint16_t max, uint32_t idle_ms, uint32_t total_ms)
{
  uint32_t t0 = HAL_GetTick();
  uint32_t last = t0;
  uint16_t k = 0;
  while ((HAL_GetTick() - t0) < total_ms) {
    if (s_tail != s_head) {
      if (k < max - 1) out[k++] = (char)s_rx[s_tail];
      s_tail = (uint16_t)((s_tail + 1) % RXBUF_SZ);
      last = HAL_GetTick();
    } else if ((HAL_GetTick() - last) > idle_ms && k > 0) {
      break;
    }
  }
  out[k] = 0;
  return k;
}

/* 在 timeout(ms) 内等待缓冲区出现字符串 token；命中返回 1 */
static uint8_t wait_token(const char *token, uint32_t timeout_ms)
{
  uint32_t t0 = HAL_GetTick();
  uint16_t match = 0;
  uint16_t len = (uint16_t)strlen(token);
  while ((HAL_GetTick() - t0) < timeout_ms) {
    while (s_tail != s_head) {
      uint8_t c = s_rx[s_tail];
      s_tail = (uint16_t)((s_tail + 1) % RXBUF_SZ);
      if (c == (uint8_t)token[match]) {
        if (++match == len) return 1;
      } else {
        match = (c == (uint8_t)token[0]) ? 1 : 0;
      }
    }
  }
  return 0;
}

/* 在 timeout(ms) 内等待二进制序列 seq[len]（用于匹配 CONNACK 等含 0x00 的报文） */
static uint8_t wait_seq(const uint8_t *seq, uint8_t len, uint32_t timeout_ms)
{
  uint32_t t0 = HAL_GetTick();
  uint8_t match = 0;
  while ((HAL_GetTick() - t0) < timeout_ms) {
    while (s_tail != s_head) {
      uint8_t c = s_rx[s_tail];
      s_tail = (uint16_t)((s_tail + 1) % RXBUF_SZ);
      if (c == seq[match]) {
        if (++match == len) return 1;
      } else {
        match = (c == seq[0]) ? 1 : 0;
      }
    }
  }
  return 0;
}

static void esp_send(const char *s)
{
  HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 500);
}

static void esp_send_bin(const uint8_t *d, uint16_t n)
{
  HAL_UART_Transmit(&huart2, (uint8_t *)d, n, 1000);
}

/* 发送 AT 命令并等待 expect */
static uint8_t at_cmd(const char *cmd, const char *expect, uint32_t timeout_ms)
{
  rx_flush();
  esp_send(cmd);
  esp_send("\r\n");
  return wait_token(expect, timeout_ms);
}

/* 重新配置 USART2 波特率（用于自适应探测） */
static void esp_set_baud(uint32_t baud)
{
  huart2.Init.BaudRate = baud;
  HAL_UART_Init(&huart2);
  __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
}

/* 探测 ESP-01S 实际波特率（常见 AT 固件为 115200 或 9600）；命中返回该波特率，否则 0 */
static uint32_t esp_probe_baud(void)
{
  static const uint32_t bauds[] = {115200, 9600, 74880, 57600, 38400, 921600};
  for (uint8_t b = 0; b < sizeof(bauds)/sizeof(bauds[0]); b++) {
    esp_set_baud(bauds[b]);
    HAL_Delay(80);
    for (uint8_t i = 0; i < 3; i++) {
      if (at_cmd("AT", "OK", 500)) return bauds[b];
    }
  }
  return 0;
}

/* MQTT 剩余长度变长编码，返回占用字节数 */
static uint8_t mqtt_enc_rl(uint8_t *p, uint32_t x)
{
  uint8_t i = 0;
  do {
    uint8_t e = (uint8_t)(x % 128);
    x /= 128;
    if (x) e |= 0x80;
    p[i++] = e;
  } while (x);
  return i;
}

/* 经 AT+CIPSEND 透传发送一段原始字节，等待 "SEND OK" */
static uint8_t cipsend(const uint8_t *pkt, uint16_t len)
{
  char cmd[32];
  snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", (unsigned)len);
  if (!at_cmd(cmd, ">", 3000)) return 0;
  rx_flush();
  esp_send_bin(pkt, len);
  return wait_token("SEND OK", 5000);
}

/* 采集原始字节（不做字符串处理，CONNACK 含 0x00），返回字节数 */
static uint16_t esp_collect_raw(uint8_t *out, uint16_t max, uint32_t idle_ms, uint32_t total_ms)
{
  uint32_t t0 = HAL_GetTick(), last = t0;
  uint16_t k = 0;
  while ((HAL_GetTick() - t0) < total_ms) {
    if (s_tail != s_head) {
      if (k < max) out[k++] = (uint8_t)s_rx[s_tail];
      s_tail = (uint16_t)((s_tail + 1) % RXBUF_SZ);
      last = HAL_GetTick();
    } else if ((HAL_GetTick() - last) > idle_ms && k > 0) {
      break;
    }
  }
  return k;
}

/* 组装并发送 MQTT CONNECT，返回 CONNACK 返回码：0=成功，1..5=拒绝原因，-1=无 CONNACK */
static int16_t mqtt_connect(void)
{
  const char *cid = CFG_MQTT_DEV_NAME;
  const char *usr = CFG_MQTT_PRODUCT_ID;
  const char *pwd = CFG_MQTT_TOKEN;
  uint16_t cl = (uint16_t)strlen(cid);
  uint16_t ul = (uint16_t)strlen(usr);
  uint16_t pl = (uint16_t)strlen(pwd);

  /* 可变头：协议名"MQTT" + level4 + flags(用户名+密码+cleansession) + keepalive 60s */
  const uint8_t vh[10] = {0x00,0x04,'M','Q','T','T',0x04,0xC2,0x00,0x3C};
  uint32_t rl = 10U + (2U+cl) + (2U+ul) + (2U+pl);

  static uint8_t pkt[320];
  uint16_t i = 0;
  pkt[i++] = 0x10;                       /* CONNECT */
  i += mqtt_enc_rl(&pkt[i], rl);
  memcpy(&pkt[i], vh, 10); i += 10;
  pkt[i++] = (uint8_t)(cl >> 8); pkt[i++] = (uint8_t)(cl & 0xFF); memcpy(&pkt[i], cid, cl); i += cl;
  pkt[i++] = (uint8_t)(ul >> 8); pkt[i++] = (uint8_t)(ul & 0xFF); memcpy(&pkt[i], usr, ul); i += ul;
  pkt[i++] = (uint8_t)(pl >> 8); pkt[i++] = (uint8_t)(pl & 0xFF); memcpy(&pkt[i], pwd, pl); i += pl;

  char cmd[32];
  snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", (unsigned)i);
  if (!at_cmd(cmd, ">", 3000)) return -2;   /* CIPSEND 未就绪 */
  rx_flush();
  esp_send_bin(pkt, i);

  /* 读取 CONNACK：0x20 0x02 <sp> <rc> */
  uint8_t rb[64];
  uint16_t n = esp_collect_raw(rb, sizeof(rb), 600, 6000);
  for (uint16_t j = 0; j + 3 < n; j++) {
    if (rb[j] == 0x20 && rb[j + 1] == 0x02) return (int16_t)rb[j + 3];
  }
  return -1;
}

/* 建立 TCP 到 MQTT broker 并完成 MQTT 连接 */
static uint8_t mqtt_link_up(void)
{
  char cmd[96];
  at_cmd("AT+CIPCLOSE", "OK", 2000);      /* 清理旧连接（忽略结果） */
  at_cmd("AT+CIPMUX=0", "OK", 1000);
  snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%s",
           CFG_ONENET_HOST, CFG_ONENET_PORT);
  if (!at_cmd(cmd, "OK", 8000) && !wait_token("ALREADY CONNECTED", 100))
    return 0;
  s_mqtt_ok = (mqtt_connect() == 0) ? 1 : 0;
  s_sub_ok = 0;
  if (s_mqtt_ok) s_sub_ok = mqtt_subscribe(MQTT_TOPIC_SET);   /* 重连后重新订阅下发主题 */
  return s_mqtt_ok;
}

/* 发送 MQTT PUBLISH (QoS0) */
static uint8_t mqtt_publish(const char *topic, const char *payload)
{
  uint16_t tl = (uint16_t)strlen(topic);
  uint16_t dl = (uint16_t)strlen(payload);
  uint32_t rl = (2U + tl) + dl;           /* QoS0 无报文标识符 */

  static uint8_t pkt[512];
  uint16_t i = 0;
  pkt[i++] = 0x30;                        /* PUBLISH QoS0 */
  i += mqtt_enc_rl(&pkt[i], rl);
  pkt[i++] = (uint8_t)(tl >> 8); pkt[i++] = (uint8_t)(tl & 0xFF);
  memcpy(&pkt[i], topic, tl); i += tl;
  memcpy(&pkt[i], payload, dl); i += dl;

  return cipsend(pkt, i);
}

/* 发送 MQTT SUBSCRIBE (QoS0)，订阅单个主题 */
static uint8_t mqtt_subscribe(const char *topic)
{
  uint16_t tl = (uint16_t)strlen(topic);
  uint32_t rl = 2U + (2U + tl) + 1U;      /* pktid(2) + topiclen(2)+topic + qos(1) */

  static uint8_t pkt[256];
  uint16_t i = 0;
  pkt[i++] = 0x82;                        /* SUBSCRIBE (需置 bit1) */
  i += mqtt_enc_rl(&pkt[i], rl);
  pkt[i++] = 0x00; pkt[i++] = 0x01;       /* 报文标识符 = 1 */
  pkt[i++] = (uint8_t)(tl >> 8); pkt[i++] = (uint8_t)(tl & 0xFF);
  memcpy(&pkt[i], topic, tl); i += tl;
  pkt[i++] = 0x00;                        /* 请求 QoS0 */

  /* 发送并确认 SUBACK(0x90 0x03 0x00 0x01 <qos>)，失败重试，确保订阅真正生效 */
  static const uint8_t suback[4] = {0x90, 0x03, 0x00, 0x01};
  for (uint8_t attempt = 0; attempt < 4; attempt++) {
    if (cipsend(pkt, i) && wait_seq(suback, 4, 2000)) return 1;
    HAL_Delay(300);
  }
  return 0;
}

static char s_ntp_dbg[40];   /* 授时诊断：最后一次 SNTP 响应片段 */
const char *ESP_NtpDebug(void) { return s_ntp_dbg; }

/* 在字符串里找第一个 HH:MM:SS，成功写入并返回1 */
static uint8_t parse_hms(const char *p, uint8_t *ph, uint8_t *pm, uint8_t *ps)
{
  for (const char *q = p; q[0] && q[1] && q[2] && q[3] && q[4] && q[5] && q[6] && q[7]; q++) {
    if (q[2] == ':' && q[5] == ':' &&
        q[0] >= '0' && q[0] <= '9' && q[1] >= '0' && q[1] <= '9' &&
        q[3] >= '0' && q[3] <= '9' && q[4] >= '0' && q[4] <= '9' &&
        q[6] >= '0' && q[6] <= '9' && q[7] >= '0' && q[7] <= '9') {
      uint8_t hh = (uint8_t)((q[0]-'0')*10 + (q[1]-'0'));
      uint8_t mm = (uint8_t)((q[3]-'0')*10 + (q[4]-'0'));
      uint8_t ss = (uint8_t)((q[6]-'0')*10 + (q[7]-'0'));
      if (hh < 24 && mm < 60 && ss < 60) { *ph=hh; *pm=mm; *ps=ss; return 1; }
    }
  }
  return 0;
}

/* 授时：用 ESP8266 内置 SNTP 从公网 NTP 服务器取北京时间(UTC+8)。
   海外 OneNET 平台无 NTP 主题，故不走 MQTT，改用模块自身 SNTP。成功返回1 */
uint8_t ESP_SyncTimeUTC8(uint8_t *ph, uint8_t *pm, uint8_t *ps)
{
  s_ntp_dbg[0] = '\0';
  if (!s_wifi_ok) { snprintf(s_ntp_dbg, sizeof(s_ntp_dbg), "no wifi"); return 0; }

  /* 使能 SNTP：时区 +8，指定 NTP 服务器 */
  at_cmd("AT+CIPSNTPCFG=1,8,\"ntp.aliyun.com\",\"pool.ntp.org\"", "OK", 2000);

  /* 轮询等待同步（首次约需数秒，未同步时年份为 1970） */
  for (uint8_t i = 0; i < 12; i++) {
    HAL_Delay(1000);
    rx_flush();
    esp_send("AT+CIPSNTPTIME?\r\n");
    char resp[160];
    esp_collect(resp, sizeof(resp), 300, 2500);

    char *p = strstr(resp, "+CIPSNTPTIME:");
    if (!p) continue;

    /* 年份 >=2020 才算同步成功（避免拿到默认 1970） */
    uint8_t synced = 0;
    for (char *q = p; q[0] && q[1] && q[2] && q[3]; q++) {
      if (q[0]>='2'&&q[0]<='9'&&q[1]>='0'&&q[1]<='9'&&q[2]>='0'&&q[2]<='9'&&q[3]>='0'&&q[3]<='9') {
        int y = (q[0]-'0')*1000 + (q[1]-'0')*100 + (q[2]-'0')*10 + (q[3]-'0');
        if (y >= 2020 && y <= 2099) { synced = 1; break; }
      }
    }

    /* 诊断：保留响应片段 */
    uint16_t k = 0;
    for (char *q = p + 13; *q && *q != '\r' && *q != '\n' && k < sizeof(s_ntp_dbg) - 1; q++)
      s_ntp_dbg[k++] = (*q >= 0x20 && *q < 0x7F) ? *q : '.';
    s_ntp_dbg[k] = '\0';

    if (synced && parse_hms(p, ph, pm, ps)) return 1;
  }
  return 0;
}

/* 快速单次查询当前 SNTP 时间（供周期性校时用，不重复使能/不长等待）。成功返回1 */
uint8_t ESP_QueryTimeUTC8(uint8_t *ph, uint8_t *pm, uint8_t *ps)
{
  if (!s_wifi_ok) return 0;

  rx_flush();
  esp_send("AT+CIPSNTPTIME?\r\n");
  char resp[160];
  esp_collect(resp, sizeof(resp), 300, 1500);

  char *p = strstr(resp, "+CIPSNTPTIME:");
  if (!p) return 0;

  uint8_t synced = 0;
  for (char *q = p; q[0] && q[1] && q[2] && q[3]; q++) {
    if (q[0]>='2'&&q[0]<='9'&&q[1]>='0'&&q[1]<='9'&&q[2]>='0'&&q[2]<='9'&&q[3]>='0'&&q[3]<='9') {
      int y = (q[0]-'0')*1000 + (q[1]-'0')*100 + (q[2]-'0')*10 + (q[3]-'0');
      if (y >= 2020 && y <= 2099) { synced = 1; break; }
    }
  }
  return (synced && parse_hms(p, ph, pm, ps)) ? 1 : 0;
}

EspStatus ESP_Init(EspStatusCb cb)
{
  s_wifi_ok = 0;
  s_mqtt_ok = 0;
  s_sub_ok = 0;
  if (cb) cb("conn", "-", "");

  HAL_Delay(500);
  s_rx_total = 0;
  /* 自适应波特率探测（应对模块默认 9600 等情况） */
  uint32_t baud = esp_probe_baud();
  if (!baud) {
    static char rbuf[24];
    /* rx=0 说明单片机一个字节都没收到（多半 TX->PA3 未通/EN 未拉高）；
       rx>0 说明收到但对不上 OK（波特率/干扰） */
    snprintf(rbuf, sizeof(rbuf), "noresp rx=%lu", (unsigned long)s_rx_total);
    if (cb) cb("fail", "-", rbuf);
    return ESP_ERR_AT;
  }

  /* 若不是 115200，尝试永久切到 115200（失败也无妨，继续用当前波特率） */
  if (baud != 115200) {
    if (at_cmd("AT+UART_CUR=115200,8,1,0,0", "OK", 800) ||
        at_cmd("AT+UART=115200,8,1,0,0",     "OK", 800)) {
      esp_set_baud(115200);
      HAL_Delay(50);
      if (!at_cmd("AT", "OK", 500)) esp_set_baud(baud);   /* 切换未生效则回退 */
    }
  }

  at_cmd("ATE0", "OK", 1000);
  if (!at_cmd("AT+CWMODE=1", "OK", 1000)){ if (cb) cb("fail", "-", "CWMODE err");  return ESP_ERR_AT; }

  char buf[96];
  snprintf(buf, sizeof(buf), "AT+CWJAP=\"%s\",\"%s\"", CFG_WIFI_SSID, CFG_WIFI_PASS);
  if (!at_cmd(buf, "OK", 15000))         { if (cb) cb("fail", "-", "WiFi join fail"); return ESP_ERR_WIFI; }
  s_wifi_ok = 1;
  if (cb) cb("ok", "conn", "");

  /* 建立 TCP 到 MQTT broker */
  char cmd[96];
  at_cmd("AT+CIPCLOSE", "OK", 2000);
  at_cmd("AT+CIPMUX=0", "OK", 1000);
  snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%s", CFG_ONENET_HOST, CFG_ONENET_PORT);
  char resp[160];
  uint8_t tcp_ok = 0;
  const char *r = "TCP:no reply";
  for (uint8_t attempt = 0; attempt < 3 && !tcp_ok; attempt++) {
    if (attempt) { at_cmd("AT+CIPCLOSE", "OK", 1500); HAL_Delay(800); }
    rx_flush();
    esp_send(cmd); esp_send("\r\n");
    esp_collect(resp, sizeof(resp), 800, 9000);
    if (strstr(resp, "CONNECT") || strstr(resp, "ALREADY")) { tcp_ok = 1; break; }
    r = strstr(resp, "DNS")    ? "TCP:DNS fail(no net?)" :
        strstr(resp, "CLOSED") ? "TCP:closed"            :
        strstr(resp, "ERROR")  ? "TCP:ERROR(port?)"      :
        (resp[0] ? "TCP:no CONNECT" : "TCP:no reply");
  }
  if (!tcp_ok) {
    if (cb) cb("ok", "fail", r);
    return ESP_ERR_TCP;
  }
  /* MQTT CONNECT */
  int16_t rc = mqtt_connect();
  if (rc != 0) {
    static char mbuf[24];
    /* CONNACK rc: 1=协议版本 2=ClientID被拒 3=服务不可用 4=用户名/密码错 5=未授权 */
    if (rc < 0) snprintf(mbuf, sizeof(mbuf), "MQTT no CONNACK");
    else        snprintf(mbuf, sizeof(mbuf), "MQTT rej rc=%d", (int)rc);
    if (cb) cb("ok", "fail", mbuf);
    return ESP_ERR_SEND;
  }
  s_mqtt_ok = 1;
  s_sub_ok = mqtt_subscribe(MQTT_TOPIC_SET);   /* 订阅云端属性设置下发 */
  if (cb) cb("ok", "ok", "");
  return ESP_OK;
}

uint8_t ESP_IsWifiConnected(void) { return s_wifi_ok; }
uint8_t ESP_IsCloudConnected(void) { return s_mqtt_ok; }
uint8_t ESP_IsSubscribed(void) { return s_sub_ok; }
uint32_t ESP_SetRxCount(void) { return s_set_rx; }
uint32_t ESP_TopicSeen(void) { return s_topic_seen; }
uint32_t ESP_IpdSeen(void) { return s_ipd_seen; }

EspStatus ESP_UploadOneNet(const char *params_json)
{
  if (!s_wifi_ok) return ESP_ERR_WIFI;
  if (!s_mqtt_ok && !mqtt_link_up()) return ESP_ERR_TCP;

  /* 物模型属性上报负载：{"id":"1","params":{ ... }} */
  char payload[384];
  snprintf(payload, sizeof(payload), "{\"id\":\"1\",\"params\":{%s}}", params_json);

  if (!mqtt_publish(MQTT_TOPIC_POST, payload)) {
    s_mqtt_ok = 0; s_sub_ok = 0;          /* 下次上报前重连 */
    return ESP_ERR_SEND;
  }
  return ESP_OK;
}

/* 轮询处理 ESP 上行数据：解析云端属性设置下发（thing/property/set）。
 * AT 非透传模式下 TCP 数据以 "+IPD,<len>:<data>" 形式到达，PUBLISH 报文中含
 * 主题串与 JSON 负载；此处用字符串扫描方式提取 params 与 id（无需完整 MQTT 解帧）。 */
/* 在 hay[0..hlen) 内二进制安全查找 ASCII 子串 needle（可越过 0x00） */
static char *mem_find(char *hay, uint16_t hlen, const char *needle)
{
  uint16_t nl = (uint16_t)strlen(needle);
  if (nl == 0 || hlen < nl) return 0;
  for (uint16_t i = 0; i + nl <= hlen; i++)
    if (memcmp(hay + i, needle, nl) == 0) return hay + i;
  return 0;
}

#define POLLBUF_SZ 512
void ESP_Poll(void)
{
  static char buf[POLLBUF_SZ + 1];
  static uint16_t len = 0;
  static uint32_t t_resub = 0;

  /* 订阅未确认时定期补订阅，自愈开机/重连时的订阅失败
     (未订阅时不会有下发到达，此处 cipsend 内的 rx_flush 无损) */
  if (s_mqtt_ok && !s_sub_ok) {
    uint32_t nowt = HAL_GetTick();
    if (nowt - t_resub >= 5000U) { t_resub = nowt; s_sub_ok = mqtt_subscribe(MQTT_TOPIC_SET); }
  }

  /* 1) 排空环形缓冲到线性缓冲 */
  while (s_tail != s_head) {
    uint8_t c = s_rx[s_tail];
    s_tail = (uint16_t)((s_tail + 1) % RXBUF_SZ);
    if (len >= POLLBUF_SZ) {              /* 溢出：保留后半段（下发报文较短） */
      uint16_t keep = POLLBUF_SZ / 2;
      memmove(buf, buf + (len - keep), keep);
      len = keep;
    }
    buf[len++] = (char)c;
  }
  buf[len] = 0;
  if (len == 0) return;
  uint16_t end = len;                      /* 有效字节数（二进制安全，含 0x00） */

  /* 2) 查找属性设置下发主题。+IPD 内是二进制 MQTT PUBLISH 报文，主题串前有
     0x00 长度字节，必须用长度受限的二进制查找，不能用会被 0x00 截断的 strstr。 */
  char *tp = mem_find(buf, end, "thing/property/set");
  if (!tp) {
    if (mem_find(buf, end, "+IPD") && !mem_find(buf, end, "thing/property")) {
      s_ipd_seen++; len = 0; return;       /* 无关 +IPD */
    }
    if (len > POLLBUF_SZ - 32) { len = 0; }  /* 无关数据，防止长期占用 */
    return;
  }
  s_topic_seen++;

  /* 3) 定位 params 对象并等待其大括号闭合（未收全则保留缓冲，下次继续） */
  char *pp = mem_find(tp, (uint16_t)(end - (uint16_t)(tp - buf)), "\"params\"");
  if (!pp) return;
  char *lb = 0;
  for (char *q = pp; q < buf + end; q++) { if (*q == '{') { lb = q; break; } }
  if (!lb) return;
  int depth = 0;
  char *pe = 0;
  for (char *q = lb; q < buf + end; q++) {
    if (*q == '{') depth++;
    else if (*q == '}' && --depth == 0) { pe = q; break; }
  }
  if (!pe) return;                         /* JSON 尚未收全 */

  /* 4) 提取 id（消息标识，用于回复） */
  static char id[32];
  id[0] = 0;
  char *ip = mem_find(buf, end, "\"id\"");
  if (ip) {
    char *c = ip;
    while (c < buf + end && *c != ':') c++;
    if (c < buf + end) {
      c++;
      while (c < buf + end && (*c == ' ' || *c == '\"')) c++;
      uint16_t k = 0;
      while (c < buf + end && *c != '\"' && *c != ',' && *c != '}' && k < sizeof(id) - 1) id[k++] = *c++;
      id[k] = 0;
    }
  }

  /* 5) 提取 params 内部文本并回调 */
  static char params[256];
  uint16_t plen = (uint16_t)(pe - lb - 1);
  if (plen >= sizeof(params)) plen = sizeof(params) - 1;
  memcpy(params, lb + 1, plen);
  params[plen] = 0;
  s_set_rx++;
  if (s_prop_set_cb) s_prop_set_cb(params);

  /* 6) 回复 set_reply */
  char reply[96];
  snprintf(reply, sizeof(reply), "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}",
           id[0] ? id : "0");
  mqtt_publish(MQTT_TOPIC_SET_REPLY, reply);

  /* 7) 消费已处理数据 */
  len = 0; buf[0] = 0;
}
