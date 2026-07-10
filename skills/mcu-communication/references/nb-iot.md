# NB-IoT/蜂窝模块开发规范

## 1. 概述与选型指南

NB-IoT/Cellular 模块使 MCU 接入运营商蜂窝网络，适用于广域物联网数据上报、远程监控等场景。按网络制式分为 NB-IoT（窄带物联网，低速率、超低功耗）、GPRS（2G 数据，逐步退网）、4G LTE（高速率）等。

### 常见型号对比

| 型号 | 厂商 | 网络制式 | 接口 | 协议 | 发射功率 | 供电 | 特点 | 价格 |
|------|------|----------|------|------|----------|------|------|------|
| BC95-G | 移远 | NB-IoT (B5/B8) | UART | UDP/TCP/CoAP/MQTT | +23dBm | 3.1~4.2V | 低功耗, 广覆盖 | ¥15~25 |
| BC26 | 移远 | NB-IoT (B1/B3/B5/B8) | UART | UDP/TCP/MQTT/CoAP | +23dBm | 3.1~4.2V | 多频段 | ¥18~30 |
| ME3616 | 中移 | NB-IoT | UART | UDP/TCP/CoAP/MQTT | +23dBm | 3.1~4.2V | 国产替代 | ¥15~25 |
| SIM800C | SIMCom | GPRS (2G) | UART | TCP/UDP/HTTP/FTP | +30dBm | 3.4~4.4V | 成熟, 2G退网中 | ¥15~25 |
| SIM7600CE | SIMCom | 4G Cat-4 (LTE) | UART/USB | TCP/UDP/HTTP/MQTT/SSL | +23dBm | 3.4~5.5V | 高速, 多模 | ¥40~70 |
| SIM7600C | SIMCom | 4G Cat-4 | UART/USB | TCP/UDP/HTTP | +23dBm | 3.4~5.5V | 国内全网通 | ¥40~70 |
| Air724UG | 合宙 | 4G Cat-1 | UART | TCP/UDP/MQTT/HTTP | +23dBm | 3.3~4.3V | Cat-1 中速 | ¥25~40 |
| EC600S | 移远 | 4G Cat-1 | UART/USB | TCP/UDP/MQTT/HTTP | +23dBm | 3.3~4.3V | Cat-1 低功耗 | ¥25~40 |

### 选型决策树

```
需要高速率(>1Mbps)？ → 是 → SIM7600 (4G Cat-4)
需要中速率(100Kbps~1Mbps)？ → 是 → Air724 / EC600S (4G Cat-1)
需要超低功耗 + 小数据量？ → 是 → BC95-G / BC26 (NB-IoT)
仍有 2G 网络覆盖？ → 是 → SIM800C (不推荐新设计使用)
需要定位功能？ → 是 → SIM7600C (含GPS) / EC600S
国内项目？ → 是 → 优先选国产芯片 (移远/合宙/中移)
新项目通用推荐 → BC26 (NB-IoT, 低功耗) 或 Air724 (Cat-1, 中速)
```

### 适用场景

- **BC95-G/BC26**：智能水表/燃气表、环境传感器、资产追踪（低频上报）
- **SIM800C**：短信通知、GPRS 数据上报（仅限仍有 2G 网络的区域）
- **SIM7600**：视频传输、工业网关、车联网（需要高速率）
- **Air724/EC600S**：中等数据量 IoT 设备、智能家居网关

---

## 2. 硬件设计规范

### BC95 引脚定义与电路

```
BC95-G 模块引脚定义 (LCC+LGA 封装):
              ┌────────────────────┐
   VCC   ──┤1              2├── GND
   GND   ──┤3              4├── GND
   TXD   ──┤5              6├── RXD
   RST   ──┤7              8├── PWRKEY
   STATUS──┤9             10├── SIM_VDD
   SIM_DATA─┤11            12├── SIM_CLK
   SIM_RST──┤13            14├── SIM_GND
   ANT   ──┤15            16├── GND
              └────────────────────┘
```

```
典型应用电路 (BC95 + STM32):

 VBAT(3.8V) ──┬── 100μF ──┬── 100nF ──┬── VCC(1)
              │            │           │
              │    DC-DC (MP1584)      │
              │    5V → 3.8V           │
              │    峰值电流 2A+         │
              │                        │
              │    STM32 PA9(TX) ──────┼── 1kΩ ── RXD(6)
              │    STM32 PA10(RX) ─────┼── TXD(5)
              │                        │
              │    STM32 PA8 ── PWRKEY(8) [开机: 拉低 ≥500ms]
              │                        │
              │    STM32 PB0 ── RST(7) [复位: 拉低 ≥100ms]
              │                        │
              │    STATUS(9) ── STM32 PB1 [网络注册状态]
              │                        │
              │    SIM 卡座:            │
              │    SIM_VDD(10) ──────── │
              │    SIM_DATA(11) ─────── │
              │    SIM_CLK(12) ──────── │
              │    SIM_RST(13) ──────── │
              │                        │
              │    ANT(15) ── 匹配 ── 天线
              │                        │
 GND ─────────┴────────────────────────┘
```

**要点**：
- VBAT 供电 3.1~4.2V，推荐 3.8V，发射时峰值电流可达 2A+
- 电源必须用 DC-DC，LDO 无法满足峰值电流需求
- VBAT 旁放 100μF + 100nF 去耦电容，应对射频突发电流
- PWRKEY 拉低 ≥ 500ms 触发开机
- UART 电平 2.8V（BC95），与 3.3V MCU 通信建议串 1kΩ 电阻
- SIM 卡座 VDD 旁放 220nF 去耦
- SIM 卡走线尽量短，远离射频信号
- 天线接口处做 50Ω 阻抗匹配

### SIM7600 电路要点

```
SIM7600 电源设计:

 VBAT(4.0V) ──┬── 100μF × 2 ──┬── 100nF ──┬── VBAT
              │                │           │
              │    DC-DC (MP1584)         │
              │    5V → 4.0V              │
              │    峰值电流 2A+            │
              │                           │
              │    PWRKEY ── 10kΩ ── VBAT │
              │           │               │
              │     STM32 GPIO ── 开机    │
              │     (拉低 ≥1s 触发开机)   │
              │                           │
 GND ─────────┴───────────────────────────┘
```

**要点**：
- SIM7600 VBAT 范围 3.4~5.5V，推荐 4.0V
- 发射时峰值电流可达 2A，DC-DC 需 ≥ 3A 输出能力
- PWRKEY 开机：拉低 ≥ 1 秒后释放
- USB 接口可选（用于固件升级和高速数据）
- 天线接口有主天线和 GPS 天线（如有 GPS 功能）
- 散热设计：模块底部 PCB 需有散热焊盘

### 电源设计关键参数

| 参数 | BC95(NB-IoT) | SIM800C(GPRS) | SIM7600(4G) | 说明 |
|------|-------------|---------------|-------------|------|
| 工作电压 | 3.1~4.2V | 3.4~4.4V | 3.4~5.5V | 推荐 3.8~4.0V |
| 峰值电流 | 2A | 2A | 2A | TX 发射时 |
| 平均电流 | 3~5mA(PSM) | 200mA | 150mA | 连接状态 |
| 睡眠电流 | 5μA(PSM) | 1mA | 1.5mA | - |
| 去耦电容 | 100μF+100nF | 100μF+100nF | 100μF×2+100nF | 必须 |
| DC-DC 要求 | ≥ 2A | ≥ 2A | ≥ 3A | - |

### SIM 卡座电路

```
SIM 卡座 (6pin / 8pin):

 SIM_VDD ──┬── 220nF ── GND
           │
           ├── SIM卡 VDD
 SIM_DATA ─┤── SIM卡 DATA ── 22Ω 串联电阻
 SIM_CLK  ─┤── SIM卡 CLK  ── 22Ω 串联电阻
 SIM_RST  ─┤── SIM卡 RST
           │
 GND ──────┴── SIM卡 GND
```

---

## 3. 驱动开发规范

### 通信协议说明

NB-IoT/蜂窝模块均使用 AT 指令集通过 UART 通信：
- 指令格式：`AT+<CMD>=<params>\r\n`
- 响应格式：`+<CMD>: <data>\r\n\r\nOK\r\n` 或 `ERROR\r\n`
- URC（Unsolicited Result Code）：模块主动上报消息，如 `+NSMI` 收到短信

### 统一接口定义

```c
typedef enum {
    CELL_OK = 0,
    CELL_ERR_TIMEOUT,
    CELL_ERR_NO_RESPONSE,
    CELL_ERR_CMD_FAIL,
    CELL_ERR_NO_SIM,
    CELL_ERR_NO_NETWORK,
    CELL_ERR_SEND,
    CELL_ERR_INVALID_PARAM,
} cell_status_t;

typedef enum {
    CELL_NET_NONE = 0,    /* 未注册 */
    CELL_NET_SEARCHING,   /* 搜索中 */
    CELL_NET_REGISTERED,  /* 已注册 */
} cell_net_state_t;

typedef struct {
    uint8_t  *rx_buf;
    uint16_t  rx_buf_size;
    volatile uint16_t rx_len;
    volatile bool rx_ready;
    volatile bool urc_pending;
    cell_net_state_t net_state;
    int8_t rssi;         /* 信号强度 dBm */
    void *uart;
} cell_handle_t;

cell_status_t cell_init(cell_handle_t *h, void *uart_config);
cell_status_t cell_power_on(cell_handle_t *h, void *pwrkey_gpio);
cell_status_t cell_check_sim(cell_handle_t *h);
cell_status_t cell_check_network(cell_handle_t *h, uint32_t timeout_ms);
cell_status_t cell_udp_send(cell_handle_t *h, const char *ip, uint16_t port,
                             const uint8_t *data, uint16_t len);
cell_status_t cell_udp_recv(cell_handle_t *h, uint8_t *buf, uint16_t *len, uint32_t timeout_ms);
cell_status_t cell_coap_send(cell_handle_t *h, const char *server, uint16_t port,
                              const uint8_t *data, uint16_t len);
cell_status_t cell_mqtt_connect(cell_handle_t *h, const char *broker, uint16_t port,
                                const char *client_id);
void cell_deinit(cell_handle_t *h);
```

### BC95 AT 指令驱动

```c
/* BC95-G NB-IoT AT 指令驱动
 * 硬件: STM32 + BC95-G
 * 接口: UART 9600 8N1 (NB-IoT 默认波特率较低)
 */

#define CELL_RX_BUF_SIZE  4096
#define CELL_DEFAULT_TIMEOUT  10000

/* 开机: PWRKEY 拉低 ≥ 500ms */
cell_status_t cell_power_on(cell_handle_t *h, void *pwrkey_gpio) {
    gpio_set_low(pwrkey_gpio);
    sys_delay_ms(500);
    gpio_set_high(pwrkey_gpio);

    /* 等待模块启动 (RDY 或 AT 响应) */
    sys_delay_ms(3000);

    /* 测试 AT 通信 */
    for (int i = 0; i < 5; i++) {
        if (cell_send_at(h, "AT", 2000) == CELL_OK)
            return CELL_OK;
        sys_delay_ms(500);
    }
    return CELL_ERR_NO_RESPONSE;
}

/* 通用 AT 指令发送 */
static cell_status_t cell_send_at(cell_handle_t *h, const char *cmd, uint32_t timeout_ms) {
    h->rx_len = 0;
    h->rx_ready = false;

    uart_send_string(h->uart, cmd);
    uart_send_string(h->uart, "\r\n");

    uint32_t start = sys_get_tick();
    while ((sys_get_tick() - start) < timeout_ms) {
        if (h->rx_ready) {
            if (strstr((char *)h->rx_buf, "OK") != NULL)    return CELL_OK;
            if (strstr((char *)h->rx_buf, "ERROR") != NULL)  return CELL_ERR_CMD_FAIL;
        }
        sys_delay_ms(10);
    }
    return CELL_ERR_TIMEOUT;
}

/* 初始化 */
cell_status_t cell_init(cell_handle_t *h, void *uart_config) {
    uart_init(uart_config, 9600);

    /* 关闭回显 */
    cell_send_at(h, "ATE0", 2000);

    /* 查询模块型号 */
    cell_send_at(h, "ATI", 2000);

    /* 查询 IMSI (SIM 卡识别) */
    if (cell_send_at(h, "AT+CIMI", 5000) != CELL_OK)
        return CELL_ERR_NO_SIM;

    return CELL_OK;
}

/* 检查 SIM 卡 */
cell_status_t cell_check_sim(cell_handle_t *h) {
    /* AT+NPING? 或 AT+CIMI 查询 IMSI */
    if (cell_send_at(h, "AT+CIMI", 5000) != CELL_OK)
        return CELL_ERR_NO_SIM;
    return CELL_OK;
}

/* 检查网络注册状态 */
cell_status_t cell_check_network(cell_handle_t *h, uint32_t timeout_ms) {
    /* AT+CGATT? 查询网络附着状态
     * 返回: +CGATT: 1 (已附着) 或 +CGATT: 0 (未附着)
     */
    uint32_t start = sys_get_tick();
    while ((sys_get_tick() - start) < timeout_ms) {
        h->rx_ready = false;
        h->rx_len = 0;
        uart_send_string(h->uart, "AT+CGATT?\r\n");

        sys_delay_ms(500);
        if (h->rx_ready) {
            char *pos = strstr((char *)h->rx_buf, "+CGATT:");
            if (pos != NULL) {
                int state = atoi(pos + 7);
                if (state == 1) {
                    h->net_state = CELL_NET_REGISTERED;
                    return CELL_OK;
                }
            }
        }
        sys_delay_ms(2000);  /* 每 2 秒查询一次 */
    }
    return CELL_ERR_NO_NETWORK;
}

/* 查询信号强度 */
int8_t cell_get_rssi(cell_handle_t *h) {
    /* AT+CSQ 返回: +CSQ: <rssi>,<ber>
     * rssi: 0~31, 99=未知
     * rssi(dBm) = -113 + 2*rssi
     */
    h->rx_ready = false;
    h->rx_len = 0;
    uart_send_string(h->uart, "AT+CSQ\r\n");
    sys_delay_ms(1000);

    if (h->rx_ready) {
        char *pos = strstr((char *)h->rx_buf, "+CSQ:");
        if (pos != NULL) {
            int rssi = atoi(pos + 5);
            if (rssi != 99 && rssi >= 0 && rssi <= 31) {
                h->rssi = -113 + 2 * rssi;
                return h->rssi;
            }
        }
    }
    return -128;  /* 未知 */
}

/* UDP 通信 - 创建 Socket + 发送 + 接收 */
cell_status_t cell_udp_send(cell_handle_t *h, const char *ip, uint16_t port,
                             const uint8_t *data, uint16_t len) {
    char cmd[128];

    /* 1. 创建 UDP Socket: AT+NSOCR=DGRAM,17,<local_port>,1 */
    snprintf(cmd, sizeof(cmd), "AT+NSOCR=DGRAM,17,%d,1", 5000);
    if (cell_send_at(h, cmd, 5000) != CELL_OK) {
        /* 解析返回的 socket id */
        /* +NSOCR: <socket_id> */
    }

    /* 2. 发送数据: AT+NSOST=<socket_id>,<ip>,<port>,<length>,<hex_data> */
    /* 将数据转为十六进制字符串 */
    char hex_data[512 * 2 + 1];
    for (uint16_t i = 0; i < len && i < 256; i++) {
        snprintf(hex_data + i * 2, 3, "%02X", data[i]);
    }

    snprintf(cmd, sizeof(cmd), "AT+NSOST=0,\"%s\",%d,%d,%s",
             ip, port, len, hex_data);
    if (cell_send_at(h, cmd, 5000) != CELL_OK)
        return CELL_ERR_SEND;

    return CELL_OK;
}

/* UDP 接收 (监听 +NSONMI URC) */
cell_status_t cell_udp_recv(cell_handle_t *h, uint8_t *buf, uint16_t *len, uint32_t timeout_ms) {
    /* 模块收到数据时主动上报: +NSONMI: <socket_id>,<data_len> */
    uint32_t start = sys_get_tick();
    while ((sys_get_tick() - start) < timeout_ms) {
        char *urc = strstr((char *)h->rx_buf, "+NSONMI:");
        if (urc != NULL) {
            int sock_id, data_len;
            if (sscanf(urc, "+NSONMI: %d,%d", &sock_id, &data_len) == 2) {
                /* 读取数据: AT+NSORF=<socket_id>,<length> */
                char cmd[64];
                snprintf(cmd, sizeof(cmd), "AT+NSORF=%d,%d", sock_id, data_len);
                h->rx_ready = false;
                h->rx_len = 0;
                uart_send_string(h->uart, cmd);
                uart_send_string(h->uart, "\r\n");
                sys_delay_ms(2000);

                /* 解析返回: +NSORF: <socket_id>,<ip>,<port>,<length>,<hex_data>,<remaining> */
                char *data_start = strstr((char *)h->rx_buf, ",");
                if (data_start) {
                    /* 跳过 ip 和 port，找到 hex 数据 */
                    for (int i = 0; i < 3; i++) {
                        data_start = strchr(data_start + 1, ',');
                        if (!data_start) return CELL_ERR_INVALID_PARAM;
                    }
                    data_start++;  /* 跳过最后的逗号 */
                    /* 解析十六进制数据 */
                    for (int i = 0; i < data_len && i < *len; i++) {
                        char hex[3] = {data_start[i*2], data_start[i*2+1], 0};
                        buf[i] = (uint8_t)strtol(hex, NULL, 16);
                    }
                    *len = data_len;
                    return CELL_OK;
                }
            }
        }
        sys_delay_ms(10);
    }
    *len = 0;
    return CELL_ERR_TIMEOUT;
}

/* CoAP 通信 */
cell_status_t cell_coap_send(cell_handle_t *h, const char *server,
                              uint16_t port, const uint8_t *data, uint16_t len) {
    char cmd[256];

    /* 1. 创建 CoAP 上下文: AT+NMICREATE=<ip>,<port> */
    snprintf(cmd, sizeof(cmd), "AT+NMICREATE=\"%s\",%d", server, port);
    if (cell_send_at(h, cmd, 5000) != CELL_OK)
        return CELL_ERR_CMD_FAIL;

    /* 2. 发送 CoAP 请求: AT+NMISEND=<msg_id>,<method>,<uri_path>,<payload> */
    /* hex 编码 payload */
    char hex_data[256 * 2 + 1];
    for (uint16_t i = 0; i < len && i < 128; i++) {
        snprintf(hex_data + i * 2, 3, "%02X", data[i]);
    }

    snprintf(cmd, sizeof(cmd), "AT+NMISEND=1,0,\"/data\",\"%s\"", hex_data);
    return cell_send_at(h, cmd, 5000);
}

/* MQTT 连接 */
cell_status_t cell_mqtt_connect(cell_handle_t *h, const char *broker,
                                uint16_t port, const char *client_id) {
    char cmd[256];

    /* 1. 创建 TCP 连接 */
    snprintf(cmd, sizeof(cmd), "AT+NSOCR=STREAM,6,%d,1", 5001);
    if (cell_send_at(h, cmd, 5000) != CELL_OK) return CELL_ERR_CMD_FAIL;

    /* 2. 连接 MQTT Broker */
    snprintf(cmd, sizeof(cmd), "AT+NSOCO=0,\"%s\",%d", broker, port);
    if (cell_send_at(h, cmd, 15000) != CELL_OK) return CELL_ERR_NO_NETWORK;

    /* 3. 发送 MQTT CONNECT 报文 (手动构建 MQTT 协议帧) */
    /* 省略 MQTT 协议帧构建，实际应使用 MQTT 库或模块内置 MQTT AT 指令 */

    return CELL_OK;
}
```

### NB-IoT 完整通信流程

```c
void nb_iot_demo(void) {
    cell_handle_t cell;
    uint8_t rx_buffer[CELL_RX_BUF_SIZE];
    cell.rx_buf = rx_buffer;
    cell.rx_buf_size = CELL_RX_BUF_SIZE;
    cell.rx_len = 0;
    cell.rx_ready = false;
    cell.net_state = CELL_NET_NONE;

    /* Step 1: 开机 */
    printf("开机中...\n");
    if (cell_power_on(&cell, &pwrkey_gpio) != CELL_OK) {
        printf("模块开机失败\n");
        return;
    }

    /* Step 2: 初始化 + 检查 SIM */
    if (cell_init(&cell, &uart_config) != CELL_OK) {
        printf("SIM 卡检查失败\n");
        return;
    }
    printf("SIM 卡正常\n");

    /* Step 3: 查询信号 */
    int8_t rssi = cell_get_rssi(&cell);
    printf("信号强度: %d dBm\n", rssi);

    /* Step 4: 等待网络注册 */
    printf("等待网络注册...\n");
    if (cell_check_network(&cell, 60000) != CELL_OK) {
        printf("网络注册失败\n");
        return;
    }
    printf("网络已注册\n");

    /* Step 5: UDP 发送数据 */
    uint8_t payload[] = "Hello NB-IoT!";
    if (cell_udp_send(&cell, "47.100.100.100", 5683, payload, sizeof(payload)-1) == CELL_OK) {
        printf("UDP 发送成功\n");
    }

    /* Step 6: 等待接收 */
    uint8_t rx_data[256];
    uint16_t rx_len = sizeof(rx_data);
    if (cell_udp_recv(&cell, rx_data, &rx_len, 30000) == CELL_OK) {
        printf("收到 %d 字节: %.*s\n", rx_len, rx_len, rx_data);
    }
}
```

---

## 4. 调试与测试规范

### 硬件验证清单

- [ ] 万用表测量 VBAT 电压 3.8~4.0V
- [ ] 确认 DC-DC 电源能力 ≥ 2A（峰值）
- [ ] 示波器检查 VBAT 在发射时纹波 < 300mV
- [ ] SIM 卡座 VDD 有 220nF 去耦
- [ ] PWRKEY 引脚受 MCU 控制
- [ ] UART TX/RX 交叉连接正确
- [ ] 天线已连接（未接天线发射可能损坏模块）
- [ ] USIM 卡已正确插入

### 通信验证

1. **模块开机**：发 AT 确认 `OK`，等待 `RDY`
2. **SIM 卡**：`AT+CIMI` 返回 IMSI 号码
3. **信号强度**：`AT+CSQ` 返回 rssi ≥ 10（-93dBm 以上）
4. **网络注册**：`AT+CGATT?` 返回 1（已附着）
5. **IP 获取**：`AT+CGPADDR` 返回 IP 地址
6. **UDP/TCP 测试**：向已知服务器发送数据确认可达

### 数据校验

- UDP 通信无可靠性保证，需应用层重传机制
- TCP 通信需确认连接建立和断开
- MQTT 使用 QoS 1/2 确保消息送达

### 性能测试指标

| 指标 | BC95(NB-IoT) | SIM800C(GPRS) | SIM7600(4G) |
|------|-------------|---------------|-------------|
| 上行速率 | 20~60 Kbps | 20~85 Kbps | 5~50 Mbps |
| 下行速率 | 20~60 Kbps | 20~85 Kbps | 5~50 Mbps |
| 网络注册时间 | 10~60s | 10~30s | 10~30s |
| 发射延迟 | 1~10s | 0.5~2s | 0.1~1s |
| RSRP(好) | -70~-90 dBm | -50~-80 dBm | -60~-90 dBm |
| RSRP(可用) | -90~-120 dBm | -80~-100 dBm | -90~-110 dBm |
| 功耗(PSM) | 5μA | - | - |
| 功耗(连接) | 3~50mA | 200mA | 150mA |

### 调试工具

- **串口助手**：直接发送 AT 指令调试
- **QNavigator (移远)**：可视化调试工具
- **QCOM (SIMCom)**：SIMCom 模块调试工具
- **网络测试服务器**：PC 上搭建 UDP/TCP/MQTT 服务端

---

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| AT 无响应 | 波特率不匹配 | NB-IoT 默认 9600，GPRS/4G 默认 115200 |
| AT 无响应 | PWRKEY 未正确触发 | 确认拉低时间 ≥ 500ms (BC95) / 1s (SIM7600) |
| AT 无响应 | 电源不足 | 发射时电压跌落导致复位，加大 DC-DC 电流 |
| CIMI 返回错误 | SIM 卡未插入或方向错误 | 检查 SIM 卡方向，确认 SIM 卡座焊接 |
| CIMI 返回错误 | SIM 卡未激活 | 确认 SIM 卡已开通数据业务 |
| 网络注册超时 | 信号弱 | 更换天线位置，检查 RSRP < -120dBm 不可用 |
| 网络注册超时 | 频段不支持 | 确认模块频段与运营商一致 |
| 网络注册超时 | NB-IoT 未覆盖 | 确认当地有 NB-IoT 基站覆盖 |
| 发送数据失败 | 未获取 IP | 确认 CGATT=1 且已获取 IP 地址 |
| 发送数据失败 | Socket 未创建 | 先 AT+NSOCR 创建 socket |
| UDP 接收不到 | 未监听 URC | 开启 URC 上报: AT+NSONMI=1 |
| TCP 连接失败 | APN 未设置 | 设置正确的 APN: AT+CGDCONT=1,"IP","cmnet" |
| MQTT 连接失败 | 未配置 MQTT 参数 | 检查 broker 地址和端口 |
| 频繁掉线 | 信号不稳定 | 加大天线增益或调整安装位置 |
| 模块发热 | 发射功率大 + 散热不足 | 增加 PCB 散热焊盘，降低发射频率 |
| 2G 模块无网络 | 2G 退网 | SIM800C 已不推荐新项目使用，换 NB-IoT 或 Cat-1 |
| NB-IoT PSM 不生效 | 未配置 PSM 参数 | AT+CPSMS 配置 PSM 周期 |
| NB-IoT 功耗高 | 未进入 PSM | 确认基站支持 PSM 且已协商 |
| SIM7600 USB 不识别 | 驱动未安装 | 安装 SIMCom USB 驱动 |

## 相关文档

- `skill://mcu-driver-core/templates/driver-template-uart.c` — UART 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
- `skill://mcu-driver-core/guides/pitfalls.md` — 跨类别常见问题与避坑指南
