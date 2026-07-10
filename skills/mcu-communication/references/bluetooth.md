# 蓝牙模块开发规范

## 1. 概述与选型指南

蓝牙模块是 MCU 常用的短距离无线通信组件，分为经典蓝牙（BR/EDR）和低功耗蓝牙（BLE）。经典蓝牙适用于音频传输、大数据量透传等场景；BLE 适用于低功耗传感器、可穿戴设备等场景。

### 常见型号对比

| 型号 | 芯片 | 蓝牙版本 | 接口 | 协议 | 供电 | 特点 | 价格 |
|------|------|----------|------|------|------|------|------|
| HC-05 | BC417 | BT 2.0+EDR | UART | SPP | 3.3~5V | 经典蓝牙, 主从一体 | ¥10~20 |
| HC-06 | BC417 | BT 2.0+EDR | UART | SPP | 3.3~5V | 仅从机模式, 廉价 | ¥8~15 |
| JDY-31 | - | BT 2.1+EDR | UART | SPP | 3.3~5V | 兼容 HC-06, BLE 广播 | ¥5~10 |
| ESP32 | ESP32 | BLE 4.2 | SOC | GATT | 3.3V | WiFi+BLE 双模 | ¥15~30 |
| nRF52832 | nRF52832 | BLE 5.0 | SPI/I2C/UART | GATT | 1.7~3.6V | 超低功耗, 专业级 | ¥15~25 |
| nRF52840 | nRF52840 | BLE 5.0 | USB/SPI/UART | GATT | 1.7~5.5V | USB, Thread/Zigbee | ¥25~40 |
| CH582 | CH582 | BLE 5.0 | SOC | GATT | 2.0~3.6V | 国产替代 nRF52 | ¥8~15 |

### 选型决策树

```
需要音频/SPP 大数据传输？ → 是 → HC-05(主从) / HC-06(仅从机)
仅需低功耗传感器通信？ → 是 → nRF52832(专业级) / CH582(国产替代)
需要 WiFi + BLE 双模？ → 是 → ESP32-WROOM-32 / ESP32-C3
需要 USB 直连？ → 是 → nRF52840
预算极低 + 基础透传？ → 是 → JDY-31 / HC-06
通用推荐 → ESP32-C3(BLE 5.0, WiFi+BLE, 性价比最优)
```

### 适用场景

- **HC-05/HC-06**：手机 APP 通信、蓝牙串口透传、教学演示
- **ESP32 BLE**：智能家居、需要 WiFi 和 BLE 共存的 IoT 设备
- **nRF52832**：可穿戴设备、无线传感器网络、医疗设备等超低功耗场景

---

## 2. 硬件设计规范

### HC-05 引脚定义与电路

```
HC-05 模块引脚定义 (底板):
         ┌──────────────────────┐
   RXD ─┤1               2├─ TXD
   GND ─┤3               4├─ VCC(3.3~5V)
   KEY ─┤5               6├─ EN(使能)
  STATE─┤7               8├─ (NC)
         └──────────────────────┘
```

```
典型应用电路 (HC-05 + 5V MCU):

 VCC(5V) ──┬── 100nF ──┬── VCC(4) [HC-05 底板支持 3.3~5V]
           │            │
           │    5V MCU (如 Arduino)
           │    PA9(TX) ── 分压电阻 ── RXD(1)
           │          │           │
           │         1kΩ        2kΩ
           │          │           │
           │         GND          │
           │    PA10(RX) ──────── TXD(2)  [HC-05 TX 为 3.3V, 5V MCU 可直连]
           │
           │    PA8(KEY) ── 10kΩ ── KEY(5) [拉高进入 AT 模式]
           │
 GND ──────┴────────────────── GND(3)
```

**要点**：
- HC-05 RX 引脚电平 3.3V，5V MCU 需分压（1kΩ + 2kΩ 分压至 3.3V）
- HC-05 TX 输出 3.3V，5V MCU 输入通常可直连（大部分 5V MCU 高电平阈值 < 3.3V）
- KEY 引脚拉高上电进入 AT 模式（波特率 38400），悬空为透传模式（波特率 9600）
- STATE 引脚输出连接状态：高电平=已连接，低电平=未连接
- VCC 旁放 100nF 去耦电容
- 工作电流 30~40mA，配对传输时可达 50mA

### nRF52832 最小系统电路

```
                    nRF52832 QFN48
         ┌────────────────────────────┐
  VDD ──┤1    DECO ── 1μF ── GND   │
  VDD ──┤2                        │
  GND ──┤3     ┌──────────┐       │
 DEC ── 1μF    │  nRF52832 │  D+  ┤
  GND ──┤      │           │  D-  ┤
  P0.00┤      │  32MHz XTAL│      │
  P0.01┤      │  32.768kHz│      │
  P0.02┤      └──────────┘       │
  ...         SWDIO ── 调试器      │
  ...         SWCLK ── 调试器      │
         └────────────────────────────┘
```

```
nRF52832 最小系统:

 3.3V ──┬── 4.7μF ──┬── 100nF ──┬── VDD (所有 VDD 引脚)
        │           │           │
        │   DEC ─── 1μF ─── GND │ (内部 DC-DC 去耦)
        │   DECO ── 1μF ─── GND │
        │                       │
        │   XC1 ──┬── 32MHz 晶振 ──┬── XC2
        │         │   12pF    │   12pF
        │         │            │
        │   XL1 ──┬── 32.768kHz ──┬── XL2
        │         │   12pF    │   12pF
        │                       │
        │   ANT ── 匹配网络 ── 天线
        │                       │
 GND ───┴───────────────────────┘
```

**要点**：
- 所有 VDD 引脚必须连接到 3.3V，不可漏接
- DEC 和 DECO 引脚各接 1μF 去耦电容到 GND（DC-DC 内部稳压器）
- 32MHz 晶振负载电容 12pF（根据晶振规格调整）
- 32.768kHz 晶振用于低功耗 RTC，可省略但无法使用低功耗模式
- ANT 引脚需匹配网络（π 型 LC），将射频阻抗匹配至 50Ω
- 天线区域 PCB 净空，禁止铺铜
- SWDIO/SWCLK 用于下载和调试

### 电源设计关键参数

| 参数 | HC-05 | nRF52832 | ESP32 BLE |
|------|-------|----------|-----------|
| 工作电压 | 3.3~5V | 1.7~3.6V | 3.0~3.6V |
| 连接电流 | 30~50mA | 4.8mA(TX) | 120mA(WiFi+BLE) |
| 广播电流 | - | 5.3mA | - |
| 睡眠电流 | - | 1.5μA(System ON) | 10μA |
| 峰值电流 | 50mA | 7.7mA | 500mA |
| 去耦电容 | 100nF | 4.7μF + 100nF | 22μF + 100nF |

---

## 3. 驱动开发规范

### HC-05 AT 指令配置

HC-05 AT 模式：KEY 引脚拉高后上电，波特率固定 38400，指令以 `\r\n` 结尾。

```c
/* HC-05 AT 指令驱动
 * 硬件: STM32 + HC-05 (KEY 拉高进入 AT 模式)
 * 接口: UART 38400 8N1
 */

typedef enum {
    BT_OK = 0,
    BT_ERR_TIMEOUT,
    BT_ERR_NO_RESPONSE,
    BT_ERR_CMD_FAIL,
    BT_ERR_INVALID_PARAM,
} bt_status_t;

typedef enum {
    BT_ROLE_SLAVE = 0,    /* 从机模式 */
    BT_ROLE_MASTER = 1,   /* 主机模式 */
} bt_role_t;

typedef struct {
    uint8_t  *rx_buf;
    uint16_t  rx_buf_size;
    volatile uint16_t rx_len;
    volatile bool rx_ready;
    void     *uart;
} hc05_handle_t;

/* 发送 AT 指令并等待响应 */
static bt_status_t hc05_send_at(hc05_handle_t *h, const char *cmd, uint32_t timeout_ms) {
    h->rx_len = 0;
    h->rx_ready = false;

    uart_send_string(h->uart, cmd);
    uart_send_string(h->uart, "\r\n");

    uint32_t start = sys_get_tick();
    while ((sys_get_tick() - start) < timeout_ms) {
        if (h->rx_ready) {
            if (strstr((char *)h->rx_buf, "OK") != NULL)   return BT_OK;
            if (strstr((char *)h->rx_buf, "ERROR") != NULL) return BT_ERR_CMD_FAIL;
        }
        sys_delay_ms(10);
    }
    return BT_ERR_TIMEOUT;
}

/* 进入 AT 模式 (KEY 拉高 + 复位) */
bt_status_t hc05_enter_at_mode(hc05_handle_t *h, void *key_gpio) {
    /* KEY 引脚拉高 */
    gpio_set_high(key_gpio);
    sys_delay_ms(100);

    /* 复位模块 (如果有 EN 引脚) */
    /* gpio_set_low(en_pin); sys_delay_ms(100); gpio_set_high(en_pin); */
    sys_delay_ms(500);

    /* AT 模式波特率固定 38400 */
    uart_init(h->uart, 38400);

    /* 测试 AT */
    return hc05_send_at(h, "AT", 2000);
}

/* 退出 AT 模式 */
void hc05_exit_at_mode(hc05_handle_t *h, void *key_gpio) {
    gpio_set_low(key_gpio);
    sys_delay_ms(100);
    /* 透传模式波特率恢复 (默认 9600 或 AT 配置值) */
    uart_init(h->uart, 9600);
}

/* 设置蓝牙名称 */
bt_status_t hc05_set_name(hc05_handle_t *h, const char *name) {
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "AT+NAME=%s", name);
    return hc05_send_at(h, cmd, 2000);
}

/* 设置波特率 (透传模式)
 * AT+UART=<baud>,<stop_bit>,<parity>
 * stop_bit: 0=1位, 1=2位
 * parity: 0=None, 1=Odd, 2=Even
 */
bt_status_t hc05_set_baudrate(hc05_handle_t *h, uint32_t baud) {
    char cmd[48];
    snprintf(cmd, sizeof(cmd), "AT+UART=%d,0,0", (int)baud);
    return hc05_send_at(h, cmd, 2000);
}

/* 设置密码 */
bt_status_t hc05_set_password(hc05_handle_t *h, const char *pin) {
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+PSWD=%s", pin);
    return hc05_send_at(h, cmd, 2000);
}

/* 设置主从模式 */
bt_status_t hc05_set_role(hc05_handle_t *h, bt_role_t role) {
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+ROLE=%d", (int)role);
    return hc05_send_at(h, cmd, 2000);
}

/* 查询连接状态 */
bool hc05_is_connected(hc05_handle_t *h, void *state_gpio) {
    /* STATE 引脚: 高电平=已连接, 低电平=未连接 */
    return gpio_read(state_gpio);
}

/* 透传数据发送 (AT 模式退出后直接写 UART) */
bt_status_t hc05_send_data(hc05_handle_t *h, const uint8_t *data, uint16_t len) {
    uart_send_data(h->uart, data, len);
    return BT_OK;
}

/* 透传数据接收 */
uint16_t hc05_recv_data(hc05_handle_t *h, uint8_t *buf, uint16_t max_len, uint32_t timeout_ms) {
    uint16_t len = 0;
    uint32_t start = sys_get_tick();
    while ((sys_get_tick() - start) < timeout_ms && len < max_len) {
        if (uart_available(h->uart)) {
            buf[len++] = uart_read_byte(h->uart);
            start = sys_get_tick();  /* 重置超时 (帧间间隔) */
        }
    }
    return len;
}
```

### ESP32 BLE GATT 服务定义

```c
/* ESP32 BLE GATT 服务定义与读写
 * 框架: ESP-IDF v5.x
 */

#include "esp_gatts_api.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"

#define GATTS_SERVICE_UUID_SENSOR   0x181A  /* Environmental Sensing */
#define GATTS_CHAR_UUID_TEMP        0x2A6E  /* Temperature */
#define GATTS_CHAR_UUID_HUMIDITY    0x2A6F  /* Humidity */
#define GATTS_CHAR_UUID_CONFIG      0xFF01  /* Custom Config (自定义) */
#define GATTS_NUM_HANDLES           6

/* GATT 属性表 */
static esp_gatts_attr_db_t gatt_db[GATTS_NUM_HANDLES] = {
    /* Service */
    [0] = {
        {ESP_GATT_AUTO_RSP},
        {
            .uuid_length = ESP_UUID_LEN_16,
            .uuid_p = (uint8_t *)&(uint16_t){GATTS_SERVICE_UUID_SENSOR},
            .perm = ESP_GATT_PERM_READ,
            .max_length = sizeof(uint16_t),
            .length = sizeof(uint16_t),
            .value = (uint8_t *)&(uint16_t){GATTS_SERVICE_UUID_SENSOR},
        }
    },
    /* Characteristic: Temperature (Read | Notify) */
    [1] = {
        {ESP_GATT_AUTO_RSP},
        {
            .uuid_length = ESP_UUID_LEN_16,
            .uuid_p = (uint8_t *)&(uint16_t){GATTS_CHAR_UUID_TEMP},
            .perm = ESP_GATT_PERM_READ,
            .max_length = sizeof(uint16_t),
            .length = sizeof(uint16_t),
            .value = (uint8_t *)&(uint16_t){0},
        }
    },
    /* CCCD (Client Characteristic Configuration Descriptor) */
    [2] = {
        {ESP_GATT_AUTO_RSP},
        {
            .uuid_length = ESP_UUID_LEN_16,
            .uuid_p = (uint8_t *)&(uint16_t){0x2902},
            .perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .max_length = sizeof(uint16_t),
            .length = sizeof(uint16_t),
            .value = (uint8_t *)&(uint16_t){0},
        }
    },
    /* Characteristic: Config (Read | Write) */
    [3] = {
        {ESP_GATT_AUTO_RSP},
        {
            .uuid_length = ESP_UUID_LEN_16,
            .uuid_p = (uint8_t *)&(uint16_t){GATTS_CHAR_UUID_CONFIG},
            .perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            .max_length = 32,
            .length = 1,
            .value = (uint8_t *)&(uint8_t){0},
        }
    },
};

/* GATT 事件回调 */
static uint16_t conn_id = 0;
static uint16_t temp_handle = 0;
static bool notify_enabled = false;

static void gatts_event_handler(esp_gatts_cb_event_t event,
                                esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT:
            /* 创建属性表 */
            esp_ble_gatts_create_attr_tab(gatt_db, gatts_if,
                                          GATTS_NUM_HANDLES, 0);
            break;

        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            if (param->add_attr_tab.status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "创建属性表失败");
                break;
            }
            if (param->add_attr_tab.num_handle == GATTS_NUM_HANDLES) {
                esp_ble_gatts_start_service(
                    param->add_attr_tab.handles[0]);
                temp_handle = param->add_attr_tab.handles[1];
            }
            break;

        case ESP_GATTS_CONNECT_EVT:
            conn_id = param->connect.conn_id;
            ESP_LOGI(TAG, "BLE 客户端连接: conn_id=%d", conn_id);
            break;

        case ESP_GATTS_DISCONNECT_EVT:
            notify_enabled = false;
            /* 重新开始广播 */
            esp_ble_gap_start_advertising(&adv_params);
            break;

        case ESP_GATTS_WRITE_EVT:
            /* 处理 CCCD 写入 (Notify 开关) */
            if (param->write.handle == temp_handle + 1) {
                uint16_t cccd = param->write.value[0] |
                               (param->write.value[1] << 8);
                notify_enabled = (cccd & 0x0001) ? true : false;
                ESP_LOGI(TAG, "Notify %s", notify_enabled ? "开启" : "关闭");
            }
            /* 处理自定义 Config 写入 */
            if (param->write.handle == temp_handle + 2) {
                /* 读取客户端写入的配置数据 */
                process_config(param->write.value, param->write.len);
            }
            break;

        case ESP_GATTS_READ_EVT:
            /* 读取请求时更新最新传感器数据 */
            break;

        default:
            break;
    }
}

/* 广播参数 */
static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x0020,   /* 20ms */
    .adv_int_max = 0x0040,   /* 40ms */
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/* 广播数据 */
static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .service_uuid_len = 2,
    .p_service_uuid = (uint8_t *)&(uint16_t){GATTS_SERVICE_UUID_SENSOR},
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

/* 发送温度数据 (Notify) */
void ble_send_temperature(float temp_c) {
    if (!notify_enabled || conn_id == 0) return;

    /* 温度 UUID 0x2A6E: sint16, 0.01°C 单位 */
    int16_t temp_val = (int16_t)(temp_c * 100);

    esp_gatts_handle_t handle = temp_handle;
    esp_ble_gatts_send_indicate(
        0,             /* gatts_if */
        conn_id,
        handle,
        sizeof(temp_val),
        (uint8_t *)&temp_val,
        false           /* notify=false 表示 indication */
    );
}

/* BLE 初始化 */
void ble_init(void) {
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);

    esp_bluedroid_init();
    esp_bluedroid_enable();

    /* 注册 GATT 回调 */
    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gatts_app_register(0);

    /* 设置广播数据 */
    esp_ble_gap_config_adv_data(&adv_data);
    esp_ble_gap_set_device_name("MCU-BLE-Sensor");
}
```

### nRF52832 BLE 广播与连接 (SoftDevice)

```c
/* nRF52832 BLE 广播配置
 * SDK: nRF5 SDK v17.x
 */

#include "ble.h"
#include "ble_advdata.h"

#define APP_ADV_INTERVAL        300     /* 300 * 0.625ms = 187.5ms */
#define APP_ADV_DURATION        18000   /* 180s */

static ble_gap_adv_params_t m_adv_params;

void advertising_init(void) {
    ble_uuid_t adv_uuids[] = {{BLE_UUID_ENVIRONMENTAL_SERVICE, BLE_UUID_TYPE_BLE}};
    ble_advdata_t advdata;

    memset(&advdata, 0, sizeof(advdata));
    advdata.name_type               = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance      = true;
    advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    advdata.uuids_complete.uuid_cnt = 1;
    advdata.uuids_complete.p_uuids  = adv_uuids;

    ble_advdata_set(&advdata, NULL);

    memset(&m_adv_params, 0, sizeof(m_adv_params));
    m_adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
    m_adv_params.p_peer_addr      = NULL;
    m_adv_params.filter_policy    = BLE_GAP_ADV_FP_ANY;
    m_adv_params.interval         = APP_ADV_INTERVAL;
    m_adv_params.duration         = APP_ADV_DURATION;
    sd_ble_gap_adv_set_configure(&m_adv_handle, NULL, &m_adv_params);
}
```

---

## 4. 调试与测试规范

### 硬件验证清单

- [ ] HC-05: 确认 VCC 电压 3.3V 或 5V（看底板规格）
- [ ] HC-05: 测量 RX 引脚电平，5V MCU 确认有分压电路
- [ ] HC-05: KEY 引脚控制正确，STATE 引脚可读
- [ ] nRF52832: 所有 VDD 引脚连接到 3.3V
- [ ] nRF52832: DEC/DECO 引脚各接 1μF 到 GND
- [ ] nRF52832: 晶振负载电容值匹配晶振规格
- [ ] nRF52832: SWDIO/SWCLK 可连接调试器
- [ ] 天线区域 PCB 净空，无金属遮挡

### 通信验证

- **HC-05**：AT 模式下发送 `AT`，确认收到 `OK`；手机蓝牙搜索可发现模块
- **ESP32 BLE**：使用 nRF Connect APP 扫描可发现设备并连接
- **nRF52832**：使用 nRF Connect APP 验证广播数据和服务发现

### 数据校验

- BLE 数据传输使用 CRC 校验或应用层校验和
- 验证 GATT 读写操作的完整性
- 测试长时间连接稳定性（断线重连）

### 性能测试指标

| 指标 | HC-05 | ESP32 BLE | nRF52832 |
|------|-------|-----------|----------|
| 连接距离 | ~10m | ~30m | ~50m |
| SPP/透传速率 | 50~100 KB/s | - | - |
| BLE 吞吐 | - | 10~20 KB/s | 20~50 KB/s |
| 连接间隔 | - | 7.5~4000ms | 7.5~4000ms |
| 广播间隔 | - | 20~10240ms | 20~10240ms |
| 唤醒延迟 | <1s | <1s | 3~20ms |
| 功耗(连接) | 30~50mA | 8~15mA(BLE) | 4.8~7mA |
| 功耗(睡眠) | - | 10μA | 1.5μA |

### 调试工具

- **串口助手**：HC-05 AT 指令调试
- **nRF Connect APP**：BLE 扫描、GATT 读写、Notify 测试
- **LightBlue APP**：iOS BLE 调试
- **nRF Sniffer**：抓取 BLE 空中包
- **Wireshark + nRF Sniffer**：协议分析

---

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| HC-05 AT 无响应 | 波特率不对 | AT 模式固定 38400，透传模式 9600(默认) |
| HC-05 AT 无响应 | KEY 未拉高 | 上电前将 KEY 拉高进入 AT 模式 |
| HC-05 无法被搜索 | 未进入可发现模式 | AT+ROLE=0(从机)，断开已连接设备 |
| HC-05 通信乱码 | 波特率不匹配 | 确认 AT 配置的波特率与 MCU 一致 |
| HC-05 配对失败 | 密码不匹配 | 默认密码 1234 或 0000，AT+PSWD 查询 |
| HC-05 RX 损坏 | 5V 直接接入 | RX 为 3.3V 电平，必须分压 |
| ESP32 BLE 无法广播 | 未初始化蓝牙控制器 | 检查 esp_bt_controller_init/enable 调用 |
| ESP32 BLE 连接断开 | MTU 未协商 | 主动请求 MTU 交换 esp_ble_gattc_send_mtu_req |
| ESP32 BLE 数据丢失 | Notify 频率过高 | 控制发送间隔 ≥ 连接间隔 |
| nRF52832 无法下载 | SWD 连接错误 | 检查 SWDIO/SWCLK/GND 连线 |
| nRF52832 功耗高 | 未关闭无用外设 | 空闲时关闭 UART/SPI，使用 RTC 唤醒 |
| nRF52832 功耗高 | 广播间隔太短 | 增大广播间隔至 1s+ |
| BLE 连接后立即断开 | 连接参数不兼容 | 检查 conn_params min/max interval |
| BLE 数据长度受限 | MTU 默认 23 | 请求增大 MTU 至 247 (BLE 4.2+) |
| BLE 多连接不稳定 | 连接间隔重叠 | 合理分配连接间隔，避免冲突 |
| BLE 隐私地址不匹配 | 使用了 RPA | 配置 IRK 或使用公共地址 |
| HC-05 STATE 引脚无变化 | 未连接 STATE | 确认底板有 STATE 引脚并正确接线 |

## 相关文档

- `skill://mcu-driver-core/templates/driver-template-i2c.c` — I2C 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-spi.c` — SPI 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-uart.c` — UART 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
- `skill://mcu-driver-core/guides/pitfalls.md` — 跨类别常见问题与避坑指南
