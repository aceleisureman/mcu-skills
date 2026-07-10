# EEPROM 存储器开发规范

## 1. 概述与选型指南

### 器件简介

EEPROM（Electrically Erasable Programmable Read-Only Memory）是一种电可擦除可编程只读存储器，支持按字节随机读写、掉电数据不丢失，适用于存储配置参数、校准数据、设备序列号等小容量非易失数据。主要接口类型为 I2C 和 SPI。

### 常见型号对比

| 型号 | 接口 | 容量 | 页写大小 | 写循环时间 | 擦写寿命 | 工作电压 | 价格 |
|------|------|------|----------|------------|----------|----------|------|
| AT24C02 | I2C | 2Kbit (256B) | 4 字节 | 5ms | 100万次 | 1.8~5.5V | ¥0.3~0.8 |
| AT24C04 | I2C | 4Kbit (512B) | 8 字节 | 5ms | 100万次 | 1.8~5.5V | ¥0.3~0.8 |
| AT24C16 | I2C | 16Kbit (2KB) | 16 字节 | 5ms | 100万次 | 1.8~5.5V | ¥0.4~1.0 |
| 24LC256 | I2C | 256Kbit (32KB) | 64 字节 | 5ms | 100万次 | 1.8~5.5V | ¥1.5~3.0 |
| AT25HP512 | SPI | 512Kbit (64KB) | 256 字节 | 3ms | 100万次 | 1.8~5.5V | ¥2.0~4.0 |
| AT25M01 | SPI | 1Mbit (128KB) | 256 字节 | 5ms | 100万次 | 1.8~5.5V | ¥3.0~5.0 |

### 选型决策树

```
容量 ≤ 256B？ → 是 → AT24C02 (I2C, 最廉价)
容量 ≤ 2KB？  → 是 → AT24C16 (I2C, 性价比高)
容量 ≤ 32KB？ → 是 → 24LC256 (I2C, 页写64字节效率高)
容量 ≤ 64KB？ → 是 → AT25HP512 (SPI, 速度更快)
容量 ≤ 128KB？→ 是 → AT25M01 (SPI)
需要高速写入？ → 是 → 选 SPI 接口型号 (页写256字节)
引脚数量受限？ → 是 → 选 I2C 接口 (仅需2根线)
通用推荐 → 24LC256 (32KB, I2C, 页写64字节, 综合最优)
```

### 适用场景

- **AT24C02**：设备序列号、MAC 地址、小型配置参数（< 256 字节）
- **24LC256**：校准数据表、日志缓冲区、多组配置参数
- **AT25HP512**：字库存储、固件备份、大容量参数存储

## 2. 硬件设计规范

### I2C EEPROM 电路 (AT24C02 / 24LC256)

```
VCC(3.3V/5V)
  │
  ├── 4.7kΩ ── SCL ── MCU I2C_SCL ──┐
  ├── 4.7kΩ ── SDA ── MCU I2C_SDA ──┤
  │                                   │
  │     ┌─────────────────────────┐   │
  │     │      AT24C02/24LC256    │   │
  │     │                         │   │
  └─────┤ VCC(8)     SCL(6) ──────┼───┘
        │                         │
        │ A0(1) ── GND            │
        │ A1(2) ── GND            │
        │ A2(3) ── GND            │
        │                         │
        │ WP(7) ── GND (写允许)    │
   GND ─┤ GND(4)                  │
        │     SDA(5) ─────────────┼───┘
        └─────────────────────────┘

  VCC ── 100nF ── GND  (去耦电容，靠近VCC引脚)
```

**引脚说明**：

| 引脚 | 名称 | 功能 |
|------|------|------|
| A0-A2 | 地址输入 | I2C 设备地址低 3 位（AT24C16 仅 A2 有效） |
| GND | 地 | 电源地 |
| SDA | 串行数据 | I2C 数据线（开漏，需上拉） |
| SCL | 串行时钟 | I2C 时钟线（开漏，需上拉） |
| WP | 写保护 | WP=H 时写保护（只读），WP=L 时可写 |
| VCC | 电源 | 1.8V~5.5V |

**设备地址格式**：

```
  Bit:  7    6    5    4    3    2    1    0
       1    0    1    0   A2   A1   A0   R/W

AT24C02:  1010 + A2A1A0 + R/W  (可寻址8个器件, 地址 0x50~0x57)
24LC256:  1010 + A2A1A0 + R/W  (可寻址8个器件, 地址 0x50~0x57)
AT24C16:  1010 + A2A1  + R/W   (仅A2A1有效, 地址 0x50~0x57, 共4个)
```

### SPI EEPROM 电路 (AT25HP512)

```
VCC(3.3V)
  │
  ├── 100nF ── GND  (去耦电容)
  │
  │     ┌─────────────────────────┐
  │     │      AT25HP512          │
  │     │                         │
  └─────┤ VCC(8)     HOLD(7) ────┼── VCC (不使用HOLD时接VCC)
        │                         │
  MCU CS   ── CS(1)               │
  MCU MISO ── SO(2)               │
  MCU ──────── WP(3) ── VCC (写允许)
  GND ──────── GND(4)             │
  MCU MOSI ── SI(5)               │
  MCU SCK  ── SCK(6)              │
        └─────────────────────────┘
```

### PCB 设计要点

- I2C 总线 SCL/SDA 必须接 4.7kΩ~10kΩ 上拉电阻
- VCC 引脚旁放置 100nF 去耦电容，尽量靠近引脚
- WP 引脚不可悬空，接 GND（允许写入）或接 MCU GPIO（软件控制保护）
- I2C 总线长度建议 < 30cm，长线需降低上拉阻值
- 多片 EEPROM 共享总线时，A0-A2 设为不同地址
- SPI 总线走线尽量短直，避免平行走线干扰

### 电气参数限制

| 参数 | AT24C02 | 24LC256 | AT25HP512 |
|------|---------|---------|-----------|
| 工作电压 | 1.8~5.5V | 1.8~5.5V | 1.8~5.5V |
| 最大 SCL 频率 | 400kHz | 400kHz | 10MHz |
| 写循环时间 (Twc) | 5ms | 5ms | 3ms |
| 数据保持时间 | 100年 | 100年 | 100年 |
| 擦写寿命 | 100万次 | 100万次 | 100万次 |
| 页写大小 | 4 字节 | 64 字节 | 256 字节 |

## 3. 驱动开发规范

### I2C EEPROM 驱动接口定义

```c
typedef struct {
    i2c_bus_t *bus;        /* I2C 总线句柄 */
    uint8_t dev_addr;      /* 设备地址 (7bit, 如 0x50) */
    uint16_t page_size;    /* 页写大小 (AT24C02=4, 24LC256=64) */
    uint16_t mem_size;     /* 存储容量 (字节) */
    uint8_t addr_width;    /* 地址宽度 (1 或 2 字节) */
} eeprom_i2c_handle_t;

typedef enum {
    EEPROM_OK = 0,
    EEPROM_ERR_NOT_FOUND,
    EEPROM_ERR_TIMEOUT,
    EEPROM_ERR_WRITE_BUSY,
    EEPROM_ERR_INVALID_PARAM,
    EEPROM_ERR_PAGE_OVERFLOW,
} eeprom_status_t;

eeprom_status_t eeprom_i2c_init(eeprom_i2c_handle_t *h, i2c_bus_t *bus,
                                 uint8_t dev_addr, uint16_t page_size,
                                 uint16_t mem_size, uint8_t addr_width);
eeprom_status_t eeprom_i2c_write_byte(eeprom_i2c_handle_t *h,
                                       uint16_t addr, uint8_t data);
eeprom_status_t eeprom_i2c_read_byte(eeprom_i2c_handle_t *h,
                                      uint16_t addr, uint8_t *data);
eeprom_status_t eeprom_i2c_write_page(eeprom_i2c_handle_t *h,
                                       uint16_t addr, const uint8_t *data,
                                       uint16_t len);
eeprom_status_t eeprom_i2c_read(eeprom_i2c_handle_t *h,
                                 uint16_t addr, uint8_t *buf,
                                 uint16_t len);
bool eeprom_i2c_is_ready(eeprom_i2c_handle_t *h);
```

### I2C EEPROM 驱动核心代码

```c
#include <string.h>

/* 检查 EEPROM 是否就绪（ACK 轮询法） */
bool eeprom_i2c_is_ready(eeprom_i2c_handle_t *h) {
    /* 写循环完成后，EEPROM 会响应设备地址 ACK */
    return i2c_check_ack(h->bus, h->dev_addr);
}

/* 等待写循环完成，超时返回错误 */
static eeprom_status_t eeprom_wait_write_complete(eeprom_i2c_handle_t *h,
                                                   uint32_t timeout_ms) {
    uint32_t elapsed = 0;
    while (!eeprom_i2c_is_ready(h)) {
        i2c_delay_ms(1);
        if (++elapsed >= timeout_ms) return EEPROM_ERR_WRITE_BUSY;
    }
    return EEPROM_OK;
}

/* 写单个字节 */
eeprom_status_t eeprom_i2c_write_byte(eeprom_i2c_handle_t *h,
                                       uint16_t addr, uint8_t data) {
    uint8_t buf[3];
    uint8_t idx = 0;

    /* 构造地址 + 数据 */
    if (h->addr_width == 2) {
        buf[idx++] = (addr >> 8) & 0xFF;   /* 高地址字节 */
    }
    buf[idx++] = addr & 0xFF;              /* 低地址字节 */
    buf[idx++] = data;

    if (i2c_write(h->bus, h->dev_addr, buf, idx) != I2C_OK)
        return EEPROM_ERR_TIMEOUT;

    /* 等待写循环完成 (典型 5ms, 超时 10ms) */
    return eeprom_wait_write_complete(h, 10);
}

/* 页写 - 一次写入一页数据 */
eeprom_status_t eeprom_i2c_write_page(eeprom_i2c_handle_t *h,
                                       uint16_t addr,
                                       const uint8_t *data,
                                       uint16_t len) {
    /* 检查页边界: 不能跨页写入 */
    uint16_t page_offset = addr % h->page_size;
    uint16_t space_in_page = h->page_size - page_offset;

    if (len > space_in_page) {
        /* 数据跨越页边界，截断到当前页 */
        len = space_in_page;
        /* 注意: 调用者需自行处理剩余数据 */
    }

    uint8_t buf[2 + 64];  /* 最大: 2字节地址 + 64字节数据 */
    uint8_t idx = 0;

    if (h->addr_width == 2) {
        buf[idx++] = (addr >> 8) & 0xFF;
    }
    buf[idx++] = addr & 0xFF;
    memcpy(&buf[idx], data, len);
    idx += len;

    if (i2c_write(h->bus, h->dev_addr, buf, idx) != I2C_OK)
        return EEPROM_ERR_TIMEOUT;

    return eeprom_wait_write_complete(h, 10);
}

/* 任意长度写入 (自动分页) */
eeprom_status_t eeprom_i2c_write(eeprom_i2c_handle_t *h,
                                  uint16_t addr,
                                  const uint8_t *data,
                                  uint16_t len) {
    uint16_t offset = 0;
    while (offset < len) {
        uint16_t page_offset = (addr + offset) % h->page_size;
        uint16_t chunk = h->page_size - page_offset;
        if (chunk > (len - offset)) chunk = len - offset;

        eeprom_status_t status = eeprom_i2c_write_page(h, addr + offset,
                                                        data + offset, chunk);
        if (status != EEPROM_OK) return status;
        offset += chunk;
    }
    return EEPROM_OK;
}

/* 随机读 - 先写入地址，再重新读 */
eeprom_status_t eeprom_i2c_read_byte(eeprom_i2c_handle_t *h,
                                      uint16_t addr, uint8_t *data) {
    uint8_t addr_buf[2];
    uint8_t idx = 0;

    if (h->addr_width == 2) {
        addr_buf[idx++] = (addr >> 8) & 0xFF;
    }
    addr_buf[idx++] = addr & 0xFF;

    /* Dummy write 设置当前地址 */
    if (i2c_write(h->bus, h->dev_addr, addr_buf, idx) != I2C_OK)
        return EEPROM_ERR_TIMEOUT;

    /* 读取数据 */
    if (i2c_read(h->bus, h->dev_addr, data, 1) != I2C_OK)
        return EEPROM_ERR_TIMEOUT;

    return EEPROM_OK;
}

/* 顺序读 - 从指定地址连续读取 */
eeprom_status_t eeprom_i2c_read(eeprom_i2c_handle_t *h,
                                 uint16_t addr,
                                 uint8_t *buf,
                                 uint16_t len) {
    uint8_t addr_buf[2];
    uint8_t idx = 0;

    if (h->addr_width == 2) {
        addr_buf[idx++] = (addr >> 8) & 0xFF;
    }
    addr_buf[idx++] = addr & 0xFF;

    /* 设置起始地址 */
    if (i2c_write(h->bus, h->dev_addr, addr_buf, idx) != I2C_OK)
        return EEPROM_ERR_TIMEOUT;

    /* 连续读取 */
    if (i2c_read(h->bus, h->dev_addr, buf, len) != I2C_OK)
        return EEPROM_ERR_TIMEOUT;

    return EEPROM_OK;
}
```

### 写循环时间管理

```c
/*
 * EEPROM 写循环管理策略:
 *
 * 1. ACK 轮询法 (推荐): 写操作后持续发送设备地址,
 *    器件内部写完成后返回 ACK, 无需固定延时。
 *
 * 2. 固定延时法 (简单): 每次写操作后延时 Twc(5ms),
 *    简单但效率低。
 *
 * 3. 状态管理法 (高效): 记录上次写入时间, 下次写入前
 *    检查是否已过 Twc, 适用于低写入频率场景。
 */

/* 固定延时法示例 */
eeprom_status_t eeprom_i2c_write_byte_fixed_delay(eeprom_i2c_handle_t *h,
                                                    uint16_t addr,
                                                    uint8_t data) {
    uint8_t buf[3];
    uint8_t idx = 0;
    if (h->addr_width == 2) buf[idx++] = (addr >> 8) & 0xFF;
    buf[idx++] = addr & 0xFF;
    buf[idx++] = data;

    if (i2c_write(h->bus, h->dev_addr, buf, idx) != I2C_OK)
        return EEPROM_ERR_TIMEOUT;

    i2c_delay_ms(5);  /* 固定等待写循环完成 */
    return EEPROM_OK;
}
```

### 平台适配层

```c
/* STM32 HAL 适配 */
#if defined(STM32_HAL)
#include "stm32f4xx_hal.h"

typedef I2C_HandleTypeDef i2c_bus_t;

int i2c_write(i2c_bus_t *bus, uint8_t addr, uint8_t *data, uint16_t len) {
    return HAL_I2C_Master_Transmit(bus, (addr << 1), data, len, 100) == HAL_OK
           ? 0 : -1;
}

int i2c_read(i2c_bus_t *bus, uint8_t addr, uint8_t *data, uint16_t len) {
    return HAL_I2C_Master_Receive(bus, (addr << 1) | 1, data, len, 100) == HAL_OK
           ? 0 : -1;
}

bool i2c_check_ack(i2c_bus_t *bus, uint8_t addr) {
    return HAL_I2C_IsDeviceReady(bus, (addr << 1), 1, 1) == HAL_OK;
}

void i2c_delay_ms(uint32_t ms) { HAL_Delay(ms); }

#elif defined(ESP32)
#include "driver/i2c.h"

int i2c_write(i2c_port_t port, uint8_t addr, uint8_t *data, uint16_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    return ret == ESP_OK ? 0 : -1;
}

#elif defined(ARDUINO)
#include <Wire.h>

int i2c_write(TwoWire *wire, uint8_t addr, uint8_t *data, uint16_t len) {
    wire->beginTransmission(addr);
    wire->write(data, len);
    return wire->endTransmission() == 0 ? 0 : -1;
}

bool i2c_check_ack(TwoWire *wire, uint8_t addr) {
    wire->beginTransmission(addr);
    return wire->endTransmission() == 0;
}

void i2c_delay_ms(uint32_t ms) { delay(ms); }
#endif
```

## 4. 调试与测试规范

### 硬件验证清单

- [ ] 万用表确认 VCC 电压在 1.8~5.5V 范围内
- [ ] SCL/SDA 上拉电阻已正确安装（4.7kΩ）
- [ ] A0-A2 地址脚已接到确定电平（不可悬空）
- [ ] WP 引脚已接 GND 或 MCU GPIO（不可悬空）
- [ ] 100nF 去耦电容已贴装且靠近 VCC 引脚
- [ ] 示波器检查 I2C 波形：SCL 频率、电平幅值、上升沿时间

### 通信验证

```c
/* EEPROM 连接检测 */
eeprom_status_t eeprom_verify(eeprom_i2c_handle_t *h) {
    /* 方法1: ACK 轮询 */
    if (!i2c_check_ack(h->bus, h->dev_addr)) {
        printf("EEPROM not found at addr 0x%02X\n", h->dev_addr);
        return EEPROM_ERR_NOT_FOUND;
    }

    /* 方法2: 写入并读回验证 */
    uint8_t test_data = 0x55;
    eeprom_status_t st = eeprom_i2c_write_byte(h, 0x00, test_data);
    if (st != EEPROM_OK) return st;

    uint8_t read_back;
    st = eeprom_i2c_read_byte(h, 0x00, &read_back);
    if (st != EEPROM_OK) return st;

    if (read_back != test_data) {
        printf("EEPROM verify failed: wrote 0x%02X, read 0x%02X\n",
               test_data, read_back);
        return EEPROM_ERR_INVALID_PARAM;
    }
    printf("EEPROM verified OK\n");
    return EEPROM_OK;
}
```

### 数据校验方法

```c
/* 全区域写入测试 */
eeprom_status_t eeprom_full_test(eeprom_i2c_handle_t *h) {
    uint8_t write_buf[256], read_buf[256];

    /* 写入递增模式 */
    for (uint16_t i = 0; i < sizeof(write_buf); i++) {
        write_buf[i] = (uint8_t)(i & 0xFF);
    }

    /* 分页写入 */
    for (uint16_t addr = 0; addr < h->mem_size; addr += sizeof(write_buf)) {
        uint16_t len = sizeof(write_buf);
        if (addr + len > h->mem_size) len = h->mem_size - addr;

        if (eeprom_i2c_write(h, addr, write_buf, len) != EEPROM_OK)
            return EEPROM_ERR_TIMEOUT;
    }

    /* 读回校验 */
    for (uint16_t addr = 0; addr < h->mem_size; addr += sizeof(read_buf)) {
        uint16_t len = sizeof(read_buf);
        if (addr + len > h->mem_size) len = h->mem_size - addr;

        if (eeprom_i2c_read(h, addr, read_buf, len) != EEPROM_OK)
            return EEPROM_ERR_TIMEOUT;

        for (uint16_t i = 0; i < len; i++) {
            if (read_buf[i] != (uint8_t)((addr + i) & 0xFF)) {
                printf("Mismatch at 0x%04X: expected 0x%02X, got 0x%02X\n",
                       addr + i, (uint8_t)((addr + i) & 0xFF), read_buf[i]);
                return EEPROM_ERR_INVALID_PARAM;
            }
        }
    }
    printf("EEPROM full test passed (%d bytes)\n", h->mem_size);
    return EEPROM_OK;
}
```

### 性能测试指标

| 指标 | AT24C02 | 24LC256 | AT25HP512 |
|------|---------|---------|-----------|
| 单字节写入 | ~5ms | ~5ms | ~3ms |
| 页写 (满页) | ~5ms | ~5ms | ~3ms |
| 连续读取速度 | ~340KB/s(400kHz) | ~340KB/s(400kHz) | ~1.25MB/s(10MHz) |
| 1KB 数据写入 | ~1.3s (4B/页) | ~85ms (64B/页) | ~12ms (256B/页) |

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| I2C 扫描找不到设备 | A0-A2 悬空或接错 | A0-A2 必须接确定电平，检查地址计算 |
| 写入数据丢失 | 页写跨页边界 | 计算页边界，跨页时分多次写入 |
| 写入数据错乱 | 未等待写循环完成 | 每次 WRITE 后等待 Twc 或用 ACK 轮询 |
| WP 引脚悬空导致无法写入 | WP 悬空可能浮高 | WP 必须接 GND 或 MCU GPIO 控制 |
| 读取数据全 0xFF | 地址字节顺序错误 | 检查 addr_width 设置（8bit/16bit 地址） |
| 多片 EEPROM 地址冲突 | A0-A2 设为相同值 | 每片设置不同的 A0-A2 组合 |
| 写入后立即读取失败 | 写循环未完成 | 读取前确认 ACK 轮询通过或延时 5ms |
| AT24C16 地址异常 | AT24C16 地址在设备地址中 | AT24C16 高 3 位地址在设备地址位 A2/A1/P0 中 |
| 数据保持时间不足 | 超过擦写寿命 | 监控擦写次数，达到上限更换器件 |
| 高温下写入失败 | 温度超规格 | 确认工作温度范围，工业级 -40~+85°C |

## 相关文档

- `skill://mcu-driver-core/templates/driver-template-i2c.c` — I2C 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-spi.c` — SPI 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
- `skill://mcu-driver-core/guides/pitfalls.md` — 跨类别常见问题与避坑指南
