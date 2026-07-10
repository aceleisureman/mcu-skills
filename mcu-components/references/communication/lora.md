# LoRa 模块开发规范

## 1. 概述与选型指南

LoRa（Long Range Radio）是一种基于扩频技术的低功耗远距离无线通信技术，适用于 LPWAN（低功耗广域网）场景。LoRa 通信距离可达 2~15km（视距），功耗极低，适合电池供电的远距离传感器网络。按接口方式可分为 SPI 接口（底层芯片）和串口透传模块（封装好的成品模块）。

### 常见型号对比

| 型号 | 芯片 | 频段 | 接口 | 调制 | 发射功率 | 接收灵敏度 | 供电 | 价格 |
|------|------|------|------|------|----------|-----------|------|------|
| SX1278 | SX1278 | 433/470MHz | SPI | LoRa | +20dBm(100mW) | -148dBm | 1.8~3.7V | ¥8~15 |
| SX1276 | SX1276 | 868/915MHz | SPI | LoRa | +20dBm | -148dBm | 1.8~3.7V | ¥10~18 |
| SX1262 | SX1262 | 433/868/915MHz | SPI | LoRa | +22dBm(158mW) | -150dBm | 1.8~3.7V | ¥15~25 |
| SX1268 | SX1268 | 433/470MHz | SPI | LoRa | +22dBm | -150dBm | 1.8~3.7V | ¥15~25 |
| E22-400T22D | SX1268 | 433MHz | UART | LoRa | +22dBm | -150dBm | 2.3~5.5V | ¥25~40 |
| E22-900T22D | SX1262 | 868/915MHz | UART | LoRa | +22dBm | -150dBm | 2.3~5.5V | ¥25~40 |
| E32-433T20D | SX1278 | 433MHz | UART | LoRa | +20dBm | -148dBm | 2.3~5.5V | ¥15~25 |
| RA-01 | SX1278 | 433MHz | SPI | LoRa | +20dBm | -148dBm | 1.8~3.7V | ¥5~10 |
| SX1280 | SX1280 | 2.4GHz | SPI | LoRa/FLRC | +12.5dBm | -132dBm | 1.8~3.3V | ¥15~25 |

### 选型决策树

```
国内使用 (433/470MHz)？ → 是 → 需要串口透传？ → 是 → E22-400T22D / E32-433T20D
                                             否 → SX1278 / SX1268 (SPI 开发)
海外使用 (868/915MHz)？ → 是 → 需要串口透传？ → 是 → E22-900T22D
                                             否 → SX1276 / SX1262 (SPI 开发)
需要 2.4GHz LoRa？ → 是 → SX1280
需要最大发射功率？ → 是 → SX1262/SX1268 (+22dBm)
需要最低功耗接收？ → 是 → SX1262/SX1268 (-150dBm)
需要 LoRaWAN？ → 是 → 上述芯片均可，配合 LoRaWAN 协议栈
通用推荐 → SX1262/SX1268 (新一代, 性能更优, 功耗更低)
```

### 适用场景

- **SX1278**：LoRa 节点开发、中距离通信（2~8km）、低成本方案
- **SX1262/1268**：远距离通信（5~15km）、低功耗应用、LoRaWAN 节点
- **E22/E32 系列**：快速开发无需写驱动代码、串口透传即用、工程量大时加速开发
- **SX1280**：2.4GHz 高速 LoRa、测距应用、低延迟场景

---

## 2. 硬件设计规范

### SX1278 引脚定义与电路

```
SX1278 QFN 封装 (28pin) 关键引脚:
                ┌──────────────┐
       VDD  ──┤1    14├── DIO5
       DIO0 ──┤2    13├── DIO4
       DIO1 ──┤3    12├── DIO3
       DIO2 ──┤4    11├── DIO2
       SCK  ──┤5    10├── MISO
       MOSI ──┤6     9├── NSS(CS)
       GND  ──┤7     8├── RST
                └──────────────┘
```

```
典型应用电路 (SX1278 + STM32):

 3.3V ──┬── 10μF ──┬── 100nF ──┬── VDD(1)
        │          │           │
        │    STM32 SPI1:       │
        │    PA5(SCK)  ────────┼── SCK(5)
        │    PA7(MOSI) ────────┼── MOSI(6)
        │    PA6(MISO) ────────┼── MISO(10)
        │    PA4(CS)   ────────┼── NSS(9)
        │                       │
        │    PA0(RST)  ────────┼── RST(8)
        │                       │
        │    DIO0(2)  ── EXTI0  │  (发送完成/接收中断)
        │    DIO1(3)  ── EXTI1  │  (超时/CAD检测)
        │                       │
        │    ANT ──┬── 50Ω ── SMA
        │          │
        │    匹配网络 (LC)
        │          │
 GND ───┴──────────────────────┘
```

**要点**：
- SX1278 SPI 最高时钟 10MHz，建议使用 1~8MHz
- NSS（CS）必须在每次 SPI 通信前拉低，通信后拉高
- RST 引脚低电平复位，正常工作时拉高
- DIO0 用于 TxDone / RxDone 中断，连接到 MCU 外部中断引脚
- DIO1 用于 RxTimeout / FhssChangeChannel
- VDD 旁放 10μF + 100nF 去耦电容
- 射频走线需做 50Ω 阻抗匹配
- 天线区域 PCB 净空，远离数字信号线

### SX1262 引脚定义与电路

```
SX1262/SX1268 关键引脚:
                ┌──────────────┐
       VDD  ──┤        ├── BUSY
       NSS  ──┤        ├── DIO1
       SCK  ──┤        ├── DIO2
       MOSI ──┤        ├── DIO3
       MISO ──┤        ├── RST
       GND  ──┤        ├── ANT
                └──────────────┘
```

```
典型应用电路 (SX1262 + STM32):

 3.3V ──┬── 10μF ──┬── 100nF ──┬── VDD
        │          │           │
        │    STM32 SPI:        │
        │    SCK  ─────────────┼── SCK
        │    MOSI ─────────────┼── MOSI
        │    MISO ─────────────┼── MISO
        │    CS   ─────────────┼── NSS
        │                       │
        │    RST  ──────────────┼── RST (低电平复位)
        │    BUSY ── GPIO输入   │  (忙信号, 操作前需检测)
        │    DIO1 ── EXTI       │  (TxDone/RxDone/CadDone)
        │                       │
        │    ANT ── 匹配网络 ── 天线
        │          (SX1262 需外部 TX/RX 开关)
        │                       │
 GND ───┴───────────────────────┘
```

**要点**：
- SX1262 引入 BUSY 信号，每次 SPI 操作前必须检查 BUSY 为低
- SX1262 需要外部 RF 开关（如 SKY66122），SX1268 内部集成
- SX126x 系列在 TX 和 RX 之间需要切换开关状态
- 发射功率 +22dBm 时峰值电流约 118mA，LDO 需 ≥ 200mA

### 天线匹配电路

```
SX1278 射频输出匹配 (433MHz):
                      ┌── 27nH ──┐
    ANT ──┬── 100pF ──┤          ├── 50Ω 传输线 ── 天线
           │           └── 22nH ──┘
           │
          GND (220pF)

SX1262 射频输出匹配 (需要 TX/RX 开关):
    VR_PA ──┬── 匹配网络 ──┬── RX 路径 ── LNA IN
             │               │
            TX 路径          RF 开关
             │               │
            PA OUT ─────────┘
```

### 电源设计关键参数

| 参数 | SX1278 | SX1262 | 说明 |
|------|---------|--------|------|
| 工作电压 | 1.8~3.7V | 1.8~3.7V | 推荐 3.3V |
| 发射电流(+20dBm) | 120mA | 118mA | +22dBm 时 118mA |
| 接收电流 | 12~15mA | 4.6~5.5mA | SX1262 更低 |
| 睡眠电流 | 0.2μA | 0.16μA | 保留寄存器 |
| 峰值电流 | 120mA | 118mA | TX 时 |
| 去耦电容 | 10μF + 100nF | 10μF + 100nF | 必须 |
| SPI 时钟 | ≤ 10MHz | ≤ 16MHz | 建议 1~8MHz |

---

## 3. 驱动开发规范

### 通信协议说明

LoRa 调制参数：
- **SF（Spreading Factor）扩频因子**：SF7~SF12，SF 越大传输越远但速率越低
- **BW（Bandwidth）带宽**：7.8~500kHz，常用 125kHz/250kHz/500kHz
- **CR（Coding Rate）编码率**：4/5、4/6、4/7、4/8
- **PreambleLength 前导码长度**：通常 6~12 个符号

时间空比（Air Time）计算：发送时间随 SF 增大呈指数增长。

### 统一接口定义

```c
typedef enum {
    LORA_OK = 0,
    LORA_ERR_TIMEOUT,
    LORA_ERR_SPI,
    LORA_ERR_NOT_FOUND,    /* 芯片未响应 */
    LORA_ERR_TX_BUSY,
    LORA_ERR_RX_TIMEOUT,
    LORA_ERR_CRC,
    LORA_ERR_INVALID_PARAM,
    LORA_ERR_FREQ,
} lora_status_t;

typedef struct {
    uint32_t frequency;    /* Hz, 如 433000000 */
    uint8_t  spreading_factor;  /* SF7~SF12 */
    uint32_t bandwidth;    /* Hz, 如 125000 */
    uint8_t  coding_rate;   /* 5~8 (实际 4/5~4/8) */
    uint16_t preamble_len;  /* 前导码长度 */
    uint8_t  tx_power;      /* dBm, 2~20 */
    bool     crc_on;        /* CRC 校验使能 */
    bool     implicit_header;  /* 隐式报头模式 */
} lora_config_t;

typedef struct {
    uint8_t  rssi;          /* 接收信号强度 */
    int8_t   snr;           /* 信噪比 */
    uint8_t  *data;
    uint8_t  data_len;
} lora_rx_data_t;

lora_status_t lora_init(lora_config_t *cfg);
lora_status_t lora_tx(const uint8_t *data, uint8_t len, uint32_t timeout_ms);
lora_status_t lora_rx_continuous(void);
lora_status_t lora_rx_single(uint32_t timeout_ms);
bool lora_is_tx_done(void);
bool lora_is_rx_done(void);
lora_status_t lora_read_rx(lora_rx_data_t *rx);
void lora_sleep(void);
void lora_wakeup(void);
```

### SX1278 SPI 寄存器驱动

```c
/* SX1278 SPI 寄存器驱动核心
 * 硬件: STM32 + SX1278 (SPI 接口)
 */

/* SX1278 寄存器地址 */
#define REG_FIFO                 0x00
#define REG_OP_MODE              0x01
#define REG_FRF_MSB              0x06
#define REG_FRF_MID              0x07
#define REG_FRF_LSB              0x08
#define REG_PA_CONFIG            0x09
#define REG_FIFO_ADDR_PTR        0x0D
#define REG_FIFO_TX_BASE_ADDR    0x0E
#define REG_FIFO_RX_BASE_ADDR    0x0F
#define REG_FIFO_RX_CURRENT_ADDR 0x10
#define REG_IRQ_FLAGS            0x12
#define REG_RX_NB_BYTES          0x13
#define REG_PKT_SNR_VALUE        0x19
#define REG_PKT_RSSI_VALUE       0x1A
#define REG_MODEM_CONFIG_1       0x1D
#define REG_MODEM_CONFIG_2       0x1E
#define REG_MODEM_CONFIG_3      0x26
#define REG_PREAMBLE_MSB         0x20
#define REG_PREAMBLE_LSB         0x21
#define REG_PAYLOAD_LENGTH       0x22
#define REG_RSSI_WIDEBAND        0x2C
#define REG_DETECTION_OPTIMIZE   0x31
#define REG_DETECTION_THRESHOLD  0x37
#define REG_SYNC_WORD            0x39
#define REG_VERSION              0x42
#define REG_LNA                  0x0C
#define REG_INVERTIQ             0x33

/* 工作模式 */
#define MODE_LONG_RANGE_MODE     0x80
#define MODE_SLEEP               0x00
#define MODE_STDBY               0x01
#define MODE_TX                  0x03
#define MODE_RX_CONTINUOUS       0x05
#define MODE_RX_SINGLE           0x06

/* IRQ 标志 */
#define IRQ_TX_DONE_MASK         0x08
#define IRQ_RX_DONE_MASK         0x40
#define IRQ_CRC_ERROR_MASK       0x20
#define IRQ_RX_TIMEOUT_MASK      0x80

/* LNA 增益 */
#define LNA_MAX_GAIN             0x23  /* G1, 最高增益 */

static spi_handle_t *lora_spi;

/* SPI 读写单个寄存器 */
static uint8_t lora_read_reg(uint8_t addr) {
    uint8_t tx[2] = {addr & 0x7F, 0x00};
    uint8_t rx[2] = {0};
    spi_cs_low(lora_spi);
    spi_transfer(lora_spi, tx, rx, 2);
    spi_cs_high(lora_spi);
    return rx[1];
}

static void lora_write_reg(uint8_t addr, uint8_t val) {
    uint8_t tx[2] = {addr | 0x80, val};
    spi_cs_low(lora_spi);
    spi_transfer(lora_spi, tx, NULL, 2);
    spi_cs_high(lora_spi);
}

/* 读写 FIFO 批量数据 */
static void lora_read_fifo(uint8_t *buf, uint8_t len) {
    spi_cs_low(lora_spi);
    uint8_t cmd = REG_FIFO & 0x7F;
    spi_transfer(lora_spi, &cmd, NULL, 1);
    spi_transfer(lora_spi, NULL, buf, len);
    spi_cs_high(lora_spi);
}

static void lora_write_fifo(const uint8_t *buf, uint8_t len) {
    spi_cs_low(lora_spi);
    uint8_t cmd = REG_FIFO | 0x80;
    spi_transfer(lora_spi, &cmd, NULL, 1);
    spi_transfer(lora_spi, buf, NULL, len);
    spi_cs_high(lora_spi);
}

/* 设置频率 */
static void lora_set_frequency(uint32_t freq) {
    /* FRF = freq * 2^19 / 32MHz */
    uint64_t frf = ((uint64_t)freq << 19) / 32000000ULL;
    lora_write_reg(REG_FRF_MSB, (uint8_t)(frf >> 16));
    lora_write_reg(REG_FRF_MID, (uint8_t)(frf >> 8));
    lora_write_reg(REG_FRF_LSB, (uint8_t)(frf >> 0));
}

/* 设置发射功率
 * PA_BOOST 引脚输出, power: 2~20 dBm
 */
static void lora_set_tx_power(uint8_t power) {
    if (power > 17) {
        /* 高功率模式: PA_BOOST + 20dBm */
        lora_write_reg(REG_PA_CONFIG, 0x80 | 0x70 | (power - 2));
    } else {
        /* 普通模式: PA_BOOST */
        lora_write_reg(REG_PA_CONFIG, 0x80 | (power - 2));
    }
}

/* 设置扩频因子 */
static void lora_set_sf(uint8_t sf) {
    uint8_t modem_config_2 = lora_read_reg(REG_MODEM_CONFIG_2);
    modem_config_2 = (modem_config_2 & 0x0F) | ((sf << 4) & 0xF0);
    lora_write_reg(REG_MODEM_CONFIG_2, modem_config_2);

    /* SF12 需要特殊优化寄存器 */
    if (sf == 12) {
        lora_write_reg(REG_DETECTION_OPTIMIZE, 0xC3);
        lora_write_reg(REG_DETECTION_THRESHOLD, 0x0A);
    } else {
        lora_write_reg(REG_DETECTION_OPTIMIZE, 0xC5);
        lora_write_reg(REG_DETECTION_THRESHOLD, 0x0C);
    }
}

/* 设置带宽 */
static void lora_set_bw(uint32_t bw) {
    uint8_t bw_val;
    switch (bw) {
        case 7800:   bw_val = 0; break;
        case 10400:  bw_val = 1; break;
        case 15600:  bw_val = 2; break;
        case 20800:  bw_val = 3; break;
        case 31250:  bw_val = 4; break;
        case 41700:  bw_val = 5; break;
        case 62500:  bw_val = 6; break;
        case 125000: bw_val = 7; break;
        case 250000: bw_val = 8; break;
        case 500000: bw_val = 9; break;
        default:     bw_val = 7; break;  /* 默认 125kHz */
    }
    uint8_t mc1 = lora_read_reg(REG_MODEM_CONFIG_1);
    lora_write_reg(REG_MODEM_CONFIG_1, (mc1 & 0x0F) | (bw_val << 4));
}

/* 初始化 */
lora_status_t lora_init(lora_config_t *cfg) {
    lora_spi = spi_get_handle(SPI1);

    /* 硬件复位 */
    gpio_set_low(RST_PIN);
    sys_delay_ms(10);
    gpio_set_high(RST_PIN);
    sys_delay_ms(10);

    /* 检查芯片版本: 应为 0x12 */
    uint8_t version = lora_read_reg(REG_VERSION);
    if (version != 0x12) return LORA_ERR_NOT_FOUND;

    /* 进入睡眠模式 (可配置) */
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
    sys_delay_ms(10);

    /* 设置频率 */
    lora_set_frequency(cfg->frequency);

    /* FIFO 基地址 */
    lora_write_reg(REG_FIFO_TX_BASE_ADDR, 0x00);
    lora_write_reg(REG_FIFO_RX_BASE_ADDR, 0x00);

    /* 设置扩频因子 */
    lora_set_sf(cfg->spreading_factor);

    /* 设置带宽 */
    lora_set_bw(cfg->bandwidth);

    /* 设置编码率 + CRC (MODEM_CONFIG_1) */
    uint8_t mc1 = lora_read_reg(REG_MODEM_CONFIG_1);
    mc1 = (mc1 & 0xF0) | ((cfg->coding_rate - 4) & 0x03);
    if (cfg->implicit_header) mc1 |= 0x01;
    lora_write_reg(REG_MODEM_CONFIG_1, mc1);

    /* CRC 使能 (MODEM_CONFIG_2) */
    uint8_t mc2 = lora_read_reg(REG_MODEM_CONFIG_2);
    if (cfg->crc_on) mc2 |= 0x04;
    else mc2 &= ~0x04;
    lora_write_reg(REG_MODEM_CONFIG_2, mc2);

    /* MODEM_CONFIG_3: AGC auto on + LNA boost */
    lora_write_reg(REG_MODEM_CONFIG_3, 0x04);

    /* 前导码长度 */
    lora_write_reg(REG_PREAMBLE_MSB, (cfg->preamble_len >> 8) & 0xFF);
    lora_write_reg(REG_PREAMBLE_LSB, cfg->preamble_len & 0xFF);

    /* 发射功率 */
    lora_set_tx_power(cfg->tx_power);

    /* LNA 增益 */
    lora_write_reg(REG_LNA, LNA_MAX_GAIN);

    /* 同步字 (私有 LoRa 网络: 0x12, LoRaWAN: 0x34) */
    lora_write_reg(REG_SYNC_WORD, 0x12);

    /* 进入待机模式 */
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);

    /* 清除所有中断标志 */
    lora_write_reg(REG_IRQ_FLAGS, 0xFF);

    return LORA_OK;
}

/* 发送数据 */
lora_status_t lora_tx(const uint8_t *data, uint8_t len, uint32_t timeout_ms) {
    /* 进入待机模式 */
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);

    /* 设置 FIFO 写地址 */
    lora_write_reg(REG_FIFO_ADDR_PTR, 0x00);

    /* 写入数据到 FIFO */
    lora_write_fifo(data, len);

    /* 设置数据长度 */
    lora_write_reg(REG_PAYLOAD_LENGTH, len);

    /* 清除 TX Done 标志 */
    lora_write_reg(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);

    /* 进入发送模式 */
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);

    /* 等待 TX Done */
    uint32_t start = sys_get_tick();
    while ((sys_get_tick() - start) < timeout_ms) {
        if (lora_read_reg(REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK) {
            lora_write_reg(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK); /* 清标志 */
            return LORA_OK;
        }
    }
    return LORA_ERR_TX_BUSY;
}

/* 接收数据 (单次接收模式) */
lora_status_t lora_rx_single(uint32_t timeout_ms) {
    /* 清除中断标志 */
    lora_write_reg(REG_IRQ_FLAGS, 0xFF);

    /* 进入单次接收模式 */
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_RX_SINGLE);

    /* 等待 RxDone 或超时 */
    uint32_t start = sys_get_tick();
    while ((sys_get_tick() - start) < timeout_ms) {
        uint8_t irq = lora_read_reg(REG_IRQ_FLAGS);
        if (irq & IRQ_RX_DONE_MASK) {
            if (irq & IRQ_CRC_ERROR_MASK) {
                lora_write_reg(REG_IRQ_FLAGS, 0xFF);
                return LORA_ERR_CRC;
            }
            lora_write_reg(REG_IRQ_FLAGS, 0xFF);
            return LORA_OK;
        }
        if (irq & IRQ_RX_TIMEOUT_MASK) {
            lora_write_reg(REG_IRQ_FLAGS, 0xFF);
            return LORA_ERR_RX_TIMEOUT;
        }
    }
    return LORA_ERR_RX_TIMEOUT;
}

/* 读取接收到的数据 */
lora_status_t lora_read_rx(lora_rx_data_t *rx) {
    /* 读取数据长度 */
    uint8_t len = lora_read_reg(REG_RX_NB_BYTES);
    if (len == 0 || len > 255) return LORA_ERR_INVALID_PARAM;

    /* 读取当前 FIFO 地址 */
    uint8_t current_addr = lora_read_reg(REG_FIFO_RX_CURRENT_ADDR);

    /* 设置 FIFO 读取地址 */
    lora_write_reg(REG_FIFO_ADDR_PTR, current_addr);

    /* 读取数据 */
    lora_read_fifo(rx->data, len);
    rx->data_len = len;

    /* 读取 RSSI 和 SNR */
    rx->rssi = lora_read_reg(REG_PKT_RSSI_VALUE) - 157;  /* HF port: -157 */
    int8_t snr = (int8_t)lora_read_reg(REG_PKT_SNR_VALUE);
    rx->snr = snr / 4;

    return LORA_OK;
}

/* 持续接收模式 */
lora_status_t lora_rx_continuous(void) {
    lora_write_reg(REG_IRQ_FLAGS, 0xFF);
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_RX_CONTINUOUS);
    return LORA_OK;
}

/* 进入睡眠 */
void lora_sleep(void) {
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
}

/* 唤醒 (进入待机) */
void lora_wakeup(void) {
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
}
```

### LoRa 收发完整示例

```c
/* LoRa 点对点通信示例
 * 发送端: 定时发送传感器数据
 * 接收端: 持续接收并解析
 */

void lora_sender_task(void) {
    lora_config_t cfg = {
        .frequency = 433000000,    /* 433MHz */
        .spreading_factor = 9,     /* SF9 */
        .bandwidth = 125000,       /* 125kHz */
        .coding_rate = 5,          /* 4/5 */
        .preamble_len = 8,
        .tx_power = 17,            /* 17 dBm */
        .crc_on = true,
        .implicit_header = false,
    };

    if (lora_init(&cfg) != LORA_OK) {
        printf("LoRa 初始化失败\n");
        return;
    }

    while (1) {
        float temp = read_temperature();
        uint8_t data[4];
        memcpy(data, &temp, 4);

        lora_status_t st = lora_tx(data, 4, 5000);
        if (st == LORA_OK) {
            printf("发送成功: %.1f°C\n", temp);
        } else {
            printf("发送失败: %d\n", st);
        }
        sys_delay_ms(10000);  /* 10秒发送一次 */
    }
}

void lora_receiver_task(void) {
    lora_config_t cfg = {
        .frequency = 433000000,
        .spreading_factor = 9,
        .bandwidth = 125000,
        .coding_rate = 5,
        .preamble_len = 8,
        .tx_power = 17,
        .crc_on = true,
        .implicit_header = false,
    };

    if (lora_init(&cfg) != LORA_OK) {
        printf("LoRa 初始化失败\n");
        return;
    }

    uint8_t rx_buf[255];
    lora_rx_data_t rx_data = {.data = rx_buf};

    /* 进入持续接收模式 */
    lora_rx_continuous();

    while (1) {
        /* 检查是否收到数据 */
        uint8_t irq = lora_read_reg(REG_IRQ_FLAGS);
        if (irq & IRQ_RX_DONE_MASK) {
            if (irq & IRQ_CRC_ERROR_MASK) {
                printf("CRC 错误\n");
            } else {
                if (lora_read_rx(&rx_data) == LORA_OK) {
                    float temp;
                    if (rx_data.data_len == 4) {
                        memcpy(&temp, rx_data.data, 4);
                        printf("收到: %.1f°C, RSSI=%ddBm, SNR=%d\n",
                               temp, rx_data.rssi, rx_data.snr);
                    }
                }
            }
            lora_write_reg(REG_IRQ_FLAGS, 0xFF); /* 清标志 */
            /* 继续接收 */
            lora_rx_continuous();
        }
        sys_delay_ms(10);
    }
}
```

---

## 4. 调试与测试规范

### 硬件验证清单

- [ ] 万用表测量 VDD 电压 3.3V
- [ ] SPI 时钟波形正常（示波器检查 SCK、MOSI、MISO）
- [ ] CS 片选信号在通信时拉低
- [ ] RST 引脚上电时有复位脉冲
- [ ] DIO0 已连接到 MCU 外部中断
- [ ] 天线已连接（未接天线发射可能损坏芯片）
- [ ] 射频匹配网络元件值正确

### 通信验证

1. **芯片识别**：读取 REG_VERSION 应为 0x12（SX1278）
2. **频率验证**：设置频率后用频谱仪/SDR 检查载波
3. **发射验证**：发送时用 SDR 确认信号频率和功率
4. **接收验证**：两端互发测试数据包
5. **距离测试**：不同距离下测试 RSSI 和丢包率

### 数据校验

- LoRa 硬件 CRC 使能（CRC_ON）
- 应用层增加数据包序列号和校验和
- 测试不同 SF/BW 下的接收成功率

### 性能测试指标

| 指标 | SF7/BW125 | SF9/BW125 | SF12/BW125 | 说明 |
|------|-----------|-----------|------------|------|
| 空中速率 | 5470 bps | 1760 bps | 293 bps | - |
| 20字节传输时间 | 46ms | 113ms | 991ms | - |
| 通信距离(市区) | ~1km | ~3km | ~8km | 视环境 |
| 通信距离(空旷) | ~3km | ~8km | ~15km | 视距 |
| 接收灵敏度 | -125dBm | -132dBm | -148dBm | - |
| 接收电流 | 12mA | 12mA | 12mA | SX1278 |

### LoRa 参数选择指南

```
距离优先 → SF12, BW=125kHz, CR=4/8 (最远, 最慢)
速率优先 → SF7, BW=500kHz, CR=4/5 (最快, 距离近)
平衡推荐 → SF9, BW=125kHz, CR=4/5 (兼顾距离和速率)
低功耗 → SF7, BW=125kHz (空中时间短, 省电)
抗干扰 → SF10~SF12, BW=125kHz (高 SF 抗干扰强)
```

### 调试工具

- **SDR（软件定义无线电）**：HackRF/RTL-SDR 检查频率和功率
- **频谱分析仪**：检测发射频谱和杂散
- **LoRa 调试器**：Semtech SX1278 评估板
- **网络分析仪**：天线阻抗匹配调试

---

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 读取 REG_VERSION 不为 0x12 | SPI 通信异常 | 检查 SPI 接线、时钟极性/相位 (CPOL=0, CPHA=0) |
| 读取全 0 或 0xFF | SPI 接线错误 | 确认 MISO/MOSI/SCK/CS 连接正确 |
| 发送无信号输出 | 未接天线 | 必须接天线后再发射，否则可能损坏芯片 |
| 发射功率不足 | PA_CONFIG 配置错误 | 确认 PA_BOOST 引脚输出，设置正确功率值 |
| 接收灵敏度差 | LNA 增益低 | 设置 REG_LNA = 0x23 (最高增益) |
| 接收不到数据 | 频率不匹配 | 发送端和接收端频率必须完全一致 |
| 接收不到数据 | SF/BW 不匹配 | 两端 SF/BW/CR 必须相同 |
| 接收不到数据 | 同步字不匹配 | 两端 REG_SYNC_WORD 必须相同 |
| 接收偶发 CRC 错误 | 信号弱 | 增大 SF 或发射功率 |
| 接收超时 | 前导码长度不匹配 | 两端前导码长度需一致 |
| 通信距离近 | 天线不匹配 | 检查天线阻抗匹配网络，用驻波比仪测量 |
| 通信距离近 | 频率选择不当 | 433MHz 穿透好距离近，868/915MHz 距离远 |
| SX1262 BUSY 不清 | SPI 时序问题 | 确认 BUSY 为低后再操作，增加延时 |
| 发送后不能立即接收 | 未清中断标志 | 发送完成后清除 IRQ_FLAGS |
| 接收数据不完整 | 隐式报头未设置长度 | 隐式模式两端必须设置相同的 PAYLOAD_LENGTH |
| LoRaWAN 加网失败 | DevEUI/AppEUI/AppKey 错误 | 核对入网参数，确认频段与网关一致 |
| 多节点冲突 | 无防碰撞机制 | 增加 CAD 检测或实现简单 ALOHA 协议 |
| 发热严重 | 发射功率过大 | 降低 TX power 或增加散热 |

## 相关文档

- `../../templates/driver-template-spi.c` — SPI 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-uart.c` — UART 驱动模板（HAL 抽象层骨架）
- `../../guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `../../guides/debugging-testing.md` — 调试与测试规范
- `../../guides/pitfalls.md` — 跨类别常见问题与避坑指南
