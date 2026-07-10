# WiFi 模块开发规范

## 1. 概述与选型指南

WiFi 模块是 MCU 接入局域网和互联网的核心通信组件，广泛应用于物联网数据上报、远程控制、OTA 升级等场景。按主控架构可分为 AT 指令模式（MCU 主控 + WiFi 协处理器）和 SOC 模式（WiFi 芯片同时运行应用逻辑）。

### 常见型号对比

| 型号 | 芯片 | 接口 | WiFi | 蓝牙 | Flash/SRAM | 供电 | 开发生态 | 价格 |
|------|------|------|------|------|------------|------|----------|------|
| ESP-01S | ESP8266EX | UART(AT) | b/g/n | 无 | 1MB/50KB | 3.3V | AT 指令 | ¥6~10 |
| ESP8266 MOD | ESP8266EX | UART/SPI | b/g/n | 无 | 4~16MB/50KB | 3.3V | AT / Non-OS / RTOS | ¥8~15 |
| ESP32-WROOM-32 | ESP32-D0WD | UART/SPI/I2C | b/g/n | BLE 4.2 | 4~16MB/520KB | 3.3V | ESP-IDF / Arduino | ¥15~30 |
| ESP32-C3 | ESP32-C3 | UART/SPI/I2C | b/g/n | BLE 5.0 | 4MB/400KB | 3.3V | ESP-IDF / Arduino | ¥10~20 |
| ESP32-S3 | ESP32-S3 | UART/SPI/I2C | b/g/n | BLE 5.0 | 8~16MB/512KB | 3.3V | ESP-IDF / Arduino | ¥18~35 |
| ESP8285 | ESP8285 | UART/SPI | b/g/n | 无 | 1MB(内嵌)/50KB | 3.3V | Non-OS / RTOS | ¥8~12 |

### 选型决策树

```
需要 SOC 运行业务逻辑？ → 是 → 需要蓝牙？ → 是 → ESP32-WROOM-32 / ESP32-C3 / ESP32-S3
                                         否 → ESP8266 (Non-OS / RTOS SDK)
仅需 WiFi 透传 / AT 指令？ → 是 → 空间受限？ → 是 → ESP-01S (最小系统)
                                            否 → ESP8266 MOD (引脚丰富)
需要超低功耗？ → 是 → ESP32-C3 (支持 Deep Sleep 5μA)
预算极低 + 基础联网？ → 是 → ESP-01S (AT 指令模式)
通用推荐 → ESP32-C3 (RISC-V, WiFi+BLE5.0, 性价比最优)
```

### 适用场景

- **ESP-01S**：简单物联网传感器节点、远程开关、基础数据上报
- **ESP8266 MOD**：中复杂度 IoT 应用、智能家居设备
- **ESP32 系列**：需要丰富外设、蓝牙共存、音视频流、AI 推理的场景
- **AT 指令模式**：已有 MCU 主控（STM32 等），WiFi 仅作网络透传

---

## 2. 硬件设计规范

### ESP-01S 引脚定义与电路

```
ESP-01S 引脚定义:
                    ┌──────┐
         VCC(3.3V) ─┤1    8├─ GND
        GPIO2     ─┤2    7├─ GPIO0(FLASH)
        GPIO0     ─┤3    6├─ EN/CH_PD(使能)
          RXD     ─┤4    5├─ TXD
                    └──────┘
```

```
典型应用电路 (ESP-01S + STM32):

 3.3V ──┬── 10μF ──┬── 100nF ──┬── VCC(1)
        │          │           │
        │      ┌───┴───┐       │
        │      │ AMS1117│      │
        │      │  3.3V  │      │
        │      └───┬───┘       │
 5V ────┼──────────┘           │
        │                      │
        ├── 10kΩ ── EN(6)      │
        │                      │
        │    STM32 PA9(TX) ────┼── 1kΩ ── RXD(4)
        │                      │
        │    STM32 PA10(RX) ───┼── TXD(5)
        │                      │
        │    STM32 PA8(GPIO) ──┼── 10kΩ ── GPIO0(3)
        │                      │
 GND ───┴──────────────────────┴── GND(8)
```

**要点**：
- ESP-01S 峰值电流可达 300mA+，AMS1117-3.3 最小封装 SOT-23 仅 300mA，必须用 SOT-223 封装
- VCC 旁必须放 10μF 钽电容/陶瓷电容 + 100nF 去耦，应对射频突发电流
- EN/CH_PD 引脚必须拉高（10kΩ），不可悬空
- GPIO0 上电时为低进入下载模式，为高进入正常模式
- UART 电平 3.3V，与 5V MCU 连接时必须加电平转换（分压电阻或专用芯片）
- GPIO2 上电时必须为高电平

### ESP32-WROOM-32 最小系统电路

```
                    ESP32-WROOM-32
         ┌────────────────────────────┐
  GND ──┤1                       38├── GND
  3V3 ──┤2   ┌──────────────┐    37├── GPIO23
  EN  ──┤3   │              │    36├── GPIO22
 GPIO36┤4   │   ESP32      │    35├── TXD0
 GPIO39┤5   │   ANTENNA    │    34├── RXD0
 GPIO34┤6   │              │    33├── GPIO21
 GPIO35┤7   │              │    32├── GPIO19
 GPIO32┤8   └──────────────┘    31├── GPIO18
 GPIO33┤9                      30├── GPIO5
 GPIO25┤10                     29├── GPIO17
 GPIO26┤11                     28├── GPIO16
 GPIO27┤12                     27├── GPIO4
 GPIO14┤13                     26├── GPIO0
 GPIO12┤14                     25├── GPIO2
 GND  ┤15                     24├── GPIO15
 GPIO13┤16                     23├── SD1
 GPIO9 ─ SD2                  22├── SD0
 GPIO10─ SD3                  21├── CLK
 GPIO11─ CMD                  20├── 3V3
  3V3 ─┤18                     19├── GND
         └────────────────────────────┘
```

```
ESP32 最小系统:

 3.3V ──┬── 22μF ──┬── 100nF ──┬── 3V3(2)
        │          │           │
        ├── 10kΩ ── EN(3)      │
        │          │           │
        │      1μF │           │
        │     ──── ┘           │
        │      │               │
        ├── 10kΩ ── GPIO0      │
        │          │           │
        │     Boot按钮         │
        │          │           │
        │   TXD0(35)── CP2102 ── USB
        │   RXD0(34)── CP2102 ── USB
        │                      │
 GND ───┴──────────────────────┘
```

**要点**：
- 3.3V 供电需 > 500mA 能力，推荐 AMS1117-3.3 SOT-223 或 DC-DC
- EN 引脚拉高 10kΩ，并联 1μF 电容到 GND 实现上电延时复位
- GPIO0 上电时为低进入下载模式，正常运行接 10kΩ 上拉
- GPIO6~GPIO11 连接内部 Flash，不可使用
- GPIO34/35/36/39 为输入专用引脚，无内部上拉/下拉
- 射频天线区域 PCB 禁止铺铜，保持净空区
- WROOM 天线方向朝向 PCB 边缘，避免被金属遮挡

### 电源设计关键参数

| 参数 | ESP8266 | ESP32 | 说明 |
|------|---------|-------|------|
| 工作电压 | 3.0~3.6V | 3.0~3.6V | 超出范围永久损坏 |
| 峰值电流 | 300mA | 500mA | TX 发射时 |
| 平均电流 | 80mA | 120mA | STA 连接状态 |
| 深睡眠电流 | 20μA | 10μA | Deep Sleep 模式 |
| 关断电流 | <1μA | <5μA | 睡眠模式 |
| 去耦电容 | 10μF + 100nF | 22μF + 100nF | 必须 |

### PCB 设计要点

1. **电源走线**：电源线宽度 ≥ 20mil，尽量短
2. **去耦电容**：VCC 引脚 3mm 内放置 10μF + 100nF
3. **天线净空区**：天线正下方和周围 5mm 内禁止铺铜
4. **晶振区域**：远离高速信号线，周围不铺铜
5. **USB 串口**：CH340/CP2102 靠近 USB 接口放置

---

## 3. 驱动开发规范

### 通信协议说明

ESP8266 AT 指令集通过 UART 通信，默认波特率 115200。指令格式：
- 命令前缀：`AT+`
- 参数分隔：`,`
- 结束符：`\r\n`
- 响应前缀：`+` 或 `OK` / `ERROR`

### 统一接口定义

```c
typedef enum {
    WIFI_MODE_STA = 1,       /* Station 模式 */
    WIFI_MODE_AP = 2,        /* AP 模式 */
    WIFI_MODE_STA_AP = 3,    /* STA+AP 混合模式 */
} wifi_mode_t;

typedef enum {
    WIFI_OK = 0,
    WIFI_ERR_TIMEOUT,        /* AT 指令超时 */
    WIFI_ERR_NO_RESPONSE,    /* 模块无响应 */
    WIFI_ERR_CMD_FAIL,       /* AT 指令返回 ERROR */
    WIFI_ERR_CONNECT,        /* WiFi 连接失败 */
    WIFI_ERR_DISCONNECTED,   /* 连接断开 */
    WIFI_ERR_SEND,           /* 数据发送失败 */
    WIFI_ERR_INVALID_PARAM,
    WIFI_ERR_BUF_OVERFLOW,
} wifi_status_t;

typedef enum {
    WIFI_LINK_DISCONNECTED = 0,
    WIFI_LINK_CONNECTED,      /* WiFi 已连接 */
    WIFI_LINK_GOT_IP,        /* 已获取 IP */
    WIFI_LINK_TCP_CONNECTED, /* TCP 连接已建立 */
} wifi_link_state_t;

typedef struct {
    uint8_t           *rx_buf;
    uint16_t           rx_buf_size;
    volatile uint16_t  rx_len;
    volatile bool      rx_ready;
    wifi_link_state_t  state;
    uint8_t            ip[4];
    uint8_t            mac[6];
} wifi_handle_t;

wifi_status_t wifi_init(wifi_handle_t *h, void *uart_config);
wifi_status_t wifi_set_mode(wifi_handle_t *h, wifi_mode_t mode);
wifi_status_t wifi_connect_ap(wifi_handle_t *h, const char *ssid, const char *password);
wifi_status_t wifi_tcp_connect(wifi_handle_t *h, uint8_t id, const char *host, uint16_t port);
wifi_status_t wifi_tcp_send(wifi_handle_t *h, uint8_t id, const uint8_t *data, uint16_t len);
wifi_status_t wifi_tcp_recv(wifi_handle_t *h, uint8_t id, uint8_t *buf, uint16_t *len, uint32_t timeout);
wifi_status_t wifi_mqtt_connect(wifi_handle_t *h, const char *broker, uint16_t port, const char *client_id);
wifi_status_t wifi_mqtt_publish(wifi_handle_t *h, const char *topic, const char *data, uint8_t qos);
wifi_status_t wifi_mqtt_subscribe(wifi_handle_t *h, const char *topic, uint8_t qos);
```

### AT 指令驱动核心

```c
#define WIFI_RX_BUF_SIZE  2048
#define WIFI_DEFAULT_TIMEOUT_MS  5000

/* 内部 AT 发送函数 */
static wifi_status_t wifi_send_at(wifi_handle_t *h, const char *cmd, uint32_t timeout_ms) {
    h->rx_len = 0;
    h->rx_ready = false;

    uart_send_string(h->uart, cmd);
    uart_send_string(h->uart, "\r\n");

    uint32_t start = sys_get_tick();
    while ((sys_get_tick() - start) < timeout_ms) {
        if (h->rx_ready) {
            if (strstr((char *)h->rx_buf, "OK") != NULL)   return WIFI_OK;
            if (strstr((char *)h->rx_buf, "ERROR") != NULL) return WIFI_ERR_CMD_FAIL;
            if (strstr((char *)h->rx_buf, "FAIL") != NULL) return WIFI_ERR_CMD_FAIL;
        }
        sys_delay_ms(10);
    }
    return WIFI_ERR_TIMEOUT;
}

/* UART 接收回调 (在中断或 DMA 回调中调用) */
void wifi_uart_rx_callback(wifi_handle_t *h, uint8_t byte) {
    if (h->rx_len < h->rx_buf_size - 1) {
        h->rx_buf[h->rx_len++] = byte;
        if (byte == '\n') {
            h->rx_buf[h->rx_len] = '\0';
            h->rx_ready = true;
        }
    } else {
        h->rx_len = 0;  /* 缓冲溢出, 丢弃 */
    }
}

/* 初始化 */
wifi_status_t wifi_init(wifi_handle_t *h, void *uart_config) {
    uart_init(uart_config, 115200);
    sys_delay_ms(500);  /* 等待模块上电稳定 */

    if (wifi_send_at(h, "AT", 2000) != WIFI_OK)
        return WIFI_ERR_NO_RESPONSE;

    wifi_send_at(h, "ATE0", 2000);  /* 关闭回显 */
    wifi_send_at(h, "AT+GMR", 2000); /* 查询固件版本 */
    return WIFI_OK;
}

/* 设置工作模式 */
wifi_status_t wifi_set_mode(wifi_handle_t *h, wifi_mode_t mode) {
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CWMODE=%d", (int)mode);
    return wifi_send_at(h, cmd, 3000);
}

/* 连接 AP (阻塞, 超时 15s) */
wifi_status_t wifi_connect_ap(wifi_handle_t *h, const char *ssid, const char *password) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
    wifi_status_t st = wifi_send_at(h, cmd, 15000);
    if (st == WIFI_OK) {
        h->state = WIFI_LINK_CONNECTED;
        wifi_send_at(h, "AT+CIFSR", 3000); /* 查询 IP */
    }
    return st;
}

/* TCP 连接 */
wifi_status_t wifi_tcp_connect(wifi_handle_t *h, uint8_t id, const char *host, uint16_t port) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=%d,\"TCP\",\"%s\",%d", id, host, port);
    wifi_status_t st = wifi_send_at(h, cmd, 10000);
    if (st == WIFI_OK) h->state = WIFI_LINK_TCP_CONNECTED;
    return st;
}

/* TCP 发送数据 */
wifi_status_t wifi_tcp_send(wifi_handle_t *h, uint8_t id, const uint8_t *data, uint16_t len) {
    char cmd[64];

    /* 1. 发送长度指令 */
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d,%d", id, len);
    if (wifi_send_at(h, cmd, 3000) != WIFI_OK) return WIFI_ERR_SEND;

    /* 2. 等待 ">" 提示符 */
    uint32_t start = sys_get_tick();
    h->rx_ready = false;
    while ((sys_get_tick() - start) < 3000) {
        if (strstr((char *)h->rx_buf, ">") != NULL) break;
        sys_delay_ms(10);
    }

    /* 3. 发送原始数据 */
    uart_send_data(h->uart, data, len);

    /* 4. 等待 SEND OK */
    start = sys_get_tick();
    h->rx_ready = false;
    while ((sys_get_tick() - start) < 5000) {
        if (h->rx_ready && strstr((char *)h->rx_buf, "SEND OK") != NULL)
            return WIFI_OK;
        sys_delay_ms(10);
    }
    return WIFI_ERR_TIMEOUT;
}

/* TCP 接收数据 (基于 +IPD 响应解析) */
wifi_status_t wifi_tcp_recv(wifi_handle_t *h, uint8_t id, uint8_t *buf, uint16_t *len, uint32_t timeout) {
    uint32_t start = sys_get_tick();
    while ((sys_get_tick() - start) < timeout) {
        char *ipd = strstr((char *)h->rx_buf, "+IPD,");
        if (ipd != NULL) {
            uint8_t recv_id;
            uint16_t data_len;
            char *colon = strchr(ipd, ':');
            if (colon != NULL) {
                sscanf(ipd, "+IPD,%d,%d", (int *)&recv_id, (int *)&data_len);
                if (recv_id == id && data_len <= *len) {
                    memcpy(buf, colon + 1, data_len);
                    *len = data_len;
                    return WIFI_OK;
                }
            }
        }
        sys_delay_ms(10);
    }
    *len = 0;
    return WIFI_ERR_TIMEOUT;
}
```

### MQTT 驱动（AT 指令扩展）

```c
/* 需 ESP8266 AT 固件 v1.7.5+ 或 ESP32 AT v2.2+ 支持 MQTT 扩展指令 */

wifi_status_t wifi_mqtt_connect(wifi_handle_t *h, const char *broker,
                                 uint16_t port, const char *client_id) {
    char cmd[256];

    /* 设置 MQTT 用户配置: scheme=1(MQTT over TCP) */
    snprintf(cmd, sizeof(cmd),
        "AT+MQTTUSERCFG=0,1,\"%s\",\"\",\"\",0,0,\"\",120", client_id);
    if (wifi_send_at(h, cmd, 3000) != WIFI_OK) return WIFI_ERR_CMD_FAIL;

    /* 连接 MQTT Broker */
    snprintf(cmd, sizeof(cmd), "AT+MQTTCONN=0,\"%s\",%d,1", broker, port);
    return wifi_send_at(h, cmd, 15000);
}

wifi_status_t wifi_mqtt_publish(wifi_handle_t *h, const char *topic,
                                 const char *data, uint8_t qos) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "AT+MQTTPUB=0,\"%s\",\"%s\",%d,0", topic, data, qos);
    return wifi_send_at(h, cmd, 5000);
}

wifi_status_t wifi_mqtt_subscribe(wifi_handle_t *h, const char *topic, uint8_t qos) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "AT+MQTTSUB=0,\"%s\",%d", topic, qos);
    return wifi_send_at(h, cmd, 5000);
}

/* MQTT 消息接收回调 */
typedef void (*mqtt_msg_cb_t)(const char *topic, const char *data, uint16_t len);

void wifi_process_mqtt_msg(wifi_handle_t *h, mqtt_msg_cb_t cb) {
    char *msg = strstr((char *)h->rx_buf, "+MQTTSUBRECV:");
    if (msg != NULL && cb != NULL) {
        char topic[128] = {0};
        int data_len = 0;
        char *t_start = strchr(msg, '\"');
        if (t_start) {
            char *t_end = strchr(t_start + 1, '\"');
            if (t_end) {
                uint16_t tlen = t_end - t_start - 1;
                if (tlen < sizeof(topic)) memcpy(topic, t_start + 1, tlen);
            }
        }
        char *last_comma = strrchr(msg, ',');
        if (last_comma) {
            data_len = atoi(last_comma + 1);
            char *data_start = strchr(last_comma + 1, ',');
            if (data_start) cb(topic, data_start + 1, data_len);
        }
    }
}
```

### WiFi 连接完整流程

```c
void wifi_connect_flow(void) {
    wifi_handle_t wifi;
    uint8_t rx_buffer[WIFI_RX_BUF_SIZE];
    wifi.rx_buf = rx_buffer;
    wifi.rx_buf_size = WIFI_RX_BUF_SIZE;
    wifi.rx_len = 0;
    wifi.rx_ready = false;
    wifi.state = WIFI_LINK_DISCONNECTED;

    /* Step 1: 初始化 */
    if (wifi_init(&wifi, &uart_config) != WIFI_OK) {
        printf("WiFi 模块初始化失败\n");
        return;
    }

    /* Step 2: 设置 STA 模式 */
    if (wifi_set_mode(&wifi, WIFI_MODE_STA) != WIFI_OK) return;

    /* Step 3: 连接 AP */
    printf("正在连接 WiFi...\n");
    if (wifi_connect_ap(&wifi, "MySSID", "MyPassword123") != WIFI_OK) {
        printf("WiFi 连接失败\n");
        return;
    }
    printf("WiFi 连接成功\n");

    /* Step 4: TCP 通信 */
    if (wifi_tcp_connect(&wifi, 0, "192.168.1.100", 8080) == WIFI_OK) {
        wifi_tcp_send(&wifi, 0, (uint8_t *)"Hello MCU!", 10);
        uint8_t rx[256];
        uint16_t rlen = sizeof(rx);
        if (wifi_tcp_recv(&wifi, 0, rx, &rlen, 5000) == WIFI_OK)
            printf("收到: %.*s\n", rlen, rx);
    }

    /* Step 5: MQTT 通信 */
    if (wifi_mqtt_connect(&wifi, "broker.emqx.io", 1883, "mcu_01") == WIFI_OK) {
        wifi_mqtt_subscribe(&wifi, "sensor/data", 0);
        wifi_mqtt_publish(&wifi, "sensor/data", "online", 0);
    }
}
```

### ESP32 ESP-IDF 原生开发

```c
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)
        esp_wifi_connect();
    else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi 断开, 重试...");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void wifi_sta_init(void) {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "MySSID",
            .password = "MyPassword123",
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}
```

---

## 4. 调试与测试规范

### 硬件验证清单

- [ ] 万用表测量 VCC 引脚电压 3.3V ±5%
- [ ] 确认 GND 连接可靠，无虚焊
- [ ] 示波器检查 3.3V 电源纹波 < 100mV（发射时）
- [ ] 确认 EN/CH_PD 引脚已上拉 10kΩ
- [ ] 确认 GPIO0 上拉 10kΩ（正常模式）
- [ ] UART TX/RX 交叉连接正确（MCU TX → 模块 RX）
- [ ] 10μF + 100nF 去耦电容已贴装且靠近 VCC 引脚
- [ ] 天线区域 PCB 净空，无金属遮挡

### 通信验证

1. **AT 响应测试**：发送 `AT\r\n`，确认收到 `OK\r\n`
2. **固件版本**：发送 `AT+GMR`，确认固件版本号
3. **波特率验证**：如需修改 `AT+UART_DEF=115200,8,1,0,0`
4. **WiFi 扫描**：发送 `AT+CWLAP` 确认可扫描到 AP
5. **IP 获取**：连接 AP 后 `AT+CIFSR` 确认获取 IP 地址

### 数据校验

- TCP 通信使用应用层校验（CRC16/校验和）
- MQTT 消息确认 QoS 1/2 的 PUBACK/PUBREC 响应
- 长时间运行监控连接状态，断线自动重连

### 性能测试指标

| 指标 | ESP8266(AT) | ESP32(IDF) | 说明 |
|------|-------------|------------|------|
| AT 指令延迟 | 50~200ms | - | 指令发送到响应 |
| TCP 吞吐(上行) | 5~10 KB/s | 50~100 KB/s | AT 模式受 UART 限制 |
| TCP 吞吐(下行) | 5~8 KB/s | 40~80 KB/s | - |
| 连接建立时间 | 3~15s | 2~8s | 取决于 AP |
| MQTT 连接时间 | 5~10s | 3~8s | - |
| 平均功耗 | 80mA | 120mA | STA 持续连接 |
| Deep Sleep | 20μA | 10μA | 定时唤醒场景 |

### 网络调试工具

- **串口助手**：SSCOM / minicom（直接发 AT 指令调试）
- **TCP 测试服务器**：PC 上运行 `nc -l -p 8080` 或 Python socket
- **MQTT Broker**：Mosquitto 本地部署或使用 EMQX 公共 broker
- **抓包工具**：Wireshark 抓 WiFi 网络包

---

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| AT 无响应 | UART 波特率不匹配 | 尝试 9600/38400/115200；确认 TX/RX 未接反 |
| AT 无响应 | EN/CH_PD 未上拉 | EN 引脚接 10kΩ 上拉到 3.3V |
| AT 无响应 | 电源不足 | 发射时电压跌落导致复位，加大电容至 100μF |
| 连接 WiFi 后重启 | 电源供电不足 | 峰值电流 300mA+，LDO 需 ≥ 500mA |
| 连接 WiFi 失败 | 密码含特殊字符 | AT 指令中转义特殊字符 |
| TCP 发送失败 | 未等待 `>` 提示 | 发送 CIPSEND 后等 `>` 再发数据 |
| TCP 数据乱码 | AT 指令和数据混入 | 发送数据时禁用 AT 回显 `ATE0` |
| TCP 接收丢数据 | +IPD 解析不完整 | 加大接收缓冲；使用固定长度模式 |
| MQTT 连接失败 | 固件不支持 MQTT AT | 升级到支持 MQTT 的 AT 固件 |
| MQTT 频繁断线 | 心跳间隔太长 | keepalive 设置为 60~120s |
| 烧录后不启动 | GPIO0 未拉高 | 检查 GPIO0 上拉电阻和 Boot 电路 |
| ESP8266 看门狗复位 | 阻塞时间过长 | WiFi 操作分片执行，及时喂看门狗 |
| ESP8266 SmartConfig 失败 | 固件版本过低 | 升级 AT 固件至 v1.7+ |
| ESP32 启动循环 | Flash 配置错误 | 检查 esptool flash 参数 |
| ESP32 WiFi 不稳定 | 电源不足 | 增加 VCC 去耦电容至 22μF+100nF |
| ESP32 Deep Sleep 唤醒后 WiFi 异常 | 未正确初始化 | 唤醒后重新调用 esp_wifi_init |
| 2.4GHz WiFi 被 BT 干扰 | 同频干扰 | 分时调度或关闭不使用的 BT |

## 相关文档

- `../../templates/driver-template-i2c.c` — I2C 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-spi.c` — SPI 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-uart.c` — UART 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `../../guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `../../guides/debugging-testing.md` — 调试与测试规范
- `../../guides/pitfalls.md` — 跨类别常见问题与避坑指南
