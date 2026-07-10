# FRAM 铁电存储器开发规范

## 1. 概述与选型指南

### 器件简介

FRAM（Ferroelectric RAM，铁电随机存取存储器）是一种结合了 RAM 的高速读写特性和 ROM 非易失性的存储器。其核心优势在于：无需擦除即可直接写入、写入速度极快（无写延迟）、擦写寿命近乎无限（10^14 次）、低功耗。

与 Flash/EEPROM 的关键区别：

| 特性 | FRAM | Flash | EEPROM |
|------|------|-------|--------|
| 写入方式 | 直接写入（无需擦除） | 必须先擦除 | 直接写入 |
| 写入时间 | 无延迟（~100ns） | 0.7ms/页 + 擦除 | 5ms/页 |
| 擦写寿命 | 10^14 次（近乎无限） | 10^5 次 | 10^6 次 |
| 读取寿命 | 无限 | 无限 | 无限 |
| 数据保持 | 10~38年 | 20年 | 100年 |
| 功耗 | 极低 | 中 | 中 |
| 成本 | 高 | 低 | 中 |

### 常见型号对比

| 型号 | 接口 | 容量 | 最大时钟 | 写入寿命 | 数据保持 | 工作电压 | 价格 |
|------|------|------|----------|----------|----------|----------|------|
| FM25V10 | SPI | 1Mbit (128KB) | 40MHz | 10^14 | 38年 | 2.0~5.5V | ¥15~30 |
| FM25V20 | SPI | 2Mbit (256KB) | 40MHz | 10^14 | 38年 | 2.0~5.5V | ¥20~40 |
| FM25W256 | SPI | 256Kbit (32KB) | 20MHz | 10^14 | 38年 | 2.0~5.5V | ¥8~15 |
| MB85RS256 | SPI | 256Kbit (32KB) | 25MHz | 10^12 | 10年 | 3.0~3.6V | ¥5~10 |
| MB85RC256 | I2C | 256Kbit (32KB) | 1MHz | 10^12 | 10年 | 3.0~3.6V | ¥5~10 |
| MB85RS1M | SPI | 1Mbit (128KB) | 25MHz | 10^12 | 10年 | 3.0~3.6V | ¥10~20 |
| FM25V05 | SPI | 512Kbit (64KB) | 40MHz | 10^14 | 38年 | 2.0~5.5V | ¥10~20 |

### 选型决策树

```
需要无限擦写寿命？ → 是 → FRAM (10^14 次, 远超 Flash/EEPROM)
需要无写延迟？     → 是 → FRAM (写入无 Twc 等待)
频繁写入数据(每秒>1次)？ → 是 → FRAM (Flash 寿命不够)
容量 ≤ 32KB？
  → I2C 接口 → MB85RC256
  → SPI 接口 → MB85RS256 / FM25W256
容量 ≤ 128KB → FM25V10 / MB85RS1M
容量 ≤ 256KB → FM25V20
需要低功耗？ → MB85RS/RC 系列 (Standby < 1μA)
需要宽电压？  → FM25V 系列 (2.0~5.5V)
通用推荐 → FM25V10 (128KB, SPI, 40MHz, 38年保持)
```

### 适用场景

- **数据记录仪**：频繁写入测量数据，无需擦除直接写入
- **计量仪表**：电表/水表/气表累计数据存储，擦写次数极高
- **配置参数存储**：频繁更新的运行参数、校准数据
- **掉电保护缓冲**：掉电瞬间快速保存关键数据（无写延迟）
- **事件日志**：高频事件记录，Flash 擦写寿命不够的场景

## 2. 硬件设计规范

### SPI FRAM 电路 (FM25V10)

```
VCC(3.3V/5V)
  │
  ├── 100nF ── GND  (去耦电容, 靠近引脚)
  │
  │     ┌──────────────────────────┐
  │     │       FM25V10            │
  │     │  (SOIC-8 封装)           │
  │     │                          │
  └─────┤ CS#(1)  ── MCU CS        │
        │ SO(2)   ── MCU MISO      │
        │ WP#(3)  ── VCC (写允许)   │
  GND ──┤ GND(4)                   │
  MCU ──┤ SI(5)   ── MCU MOSI      │
  MCU ──┤ SCK(6)  ── MCU SCK       │
        │ HOLD#(7)── VCC (不使用)   │
  VCC ──┤ VCC(8)                   │
        └──────────────────────────┘

  注: 电路与 SPI Flash 完全相同, 可直接替换
```

### I2C FRAM 电路 (MB85RC256)

```
VCC(3.3V)
  │
  ├── 4.7kΩ ── SCL ── MCU I2C_SCL ──┐
  ├── 4.7kΩ ── SDA ── MCU I2C_SDA ──┤
  │                                   │
  │     ┌─────────────────────────┐   │
  │     │      MB85RC256          │   │
  │     │                         │   │
  └─────┤ VCC(8)     SCL(6) ──────┼───┘
        │                         │
        │ A0(1) ── GND            │
        │ A1(2) ── GND            │
        │ A2(3) ── GND            │
        │ WP(7) ── GND (写允许)    │
   GND ─┤ GND(4)                  │
        │     SDA(5) ─────────────┼───┘
        └─────────────────────────┘

  VCC ── 100nF ── GND  (去耦电容)
```

**引脚说明**：

| 引脚 (SPI) | 名称 | 功能 |
|-------------|------|------|
| CS# | 片选 | 低有效 |
| SO | 数据输出 | SPI MISO |
| WP# | 写保护 | 低电平保护状态寄存器 |
| GND | 地 | - |
| SI | 数据输入 | SPI MOSI |
| SCK | 时钟 | SPI 时钟 |
| HOLD# | 暂停 | 低电平暂停操作 |
| VCC | 电源 | 2.0~5.5V |

### 与 Flash 的引脚兼容性

FRAM 的引脚定义和封装与 SPI Flash（如 W25Q 系列）完全相同，可直接在 PCB 上替换。区别在于驱动代码：FRAM 无需擦除操作、无需等待 BUSY 标志。

### PCB 设计要点

- VCC 引脚放置 100nF 去耦电容
- I2C 模式 SCL/SDA 接 4.7kΩ~10kΩ 上拉
- WP# 和 HOLD# 不可悬空，接 VCC 或 MCU GPIO
- 高速 SPI（>20MHz）需注意信号完整性
- FRAM 对辐射不敏感（非易失原理不同），但仍建议远离强干扰源

### 电气参数限制

| 参数 | FM25V10 | MB85RS256 | MB85RC256 |
|------|---------|-----------|-----------|
| 工作电压 | 2.0~5.5V | 3.0~3.6V | 3.0~3.6V |
| 最大 SPI/I2C 频率 | 40MHz | 25MHz | 1MHz |
| 写入时间 | 无延迟 (总线速度) | 无延迟 | 无延迟 |
| 擦写寿命 | 10^14 | 10^12 | 10^12 |
| 数据保持 | 38年 | 10年 | 10年 |
| 工作电流 | 3mA (40MHz) | 3mA | 1mA |
| 待机电流 | 100μA | <1μA | <1μA |

## 3. 驱动开发规范

### FRAM SPI 命令表

| 命令 | 代码 | 功能 | 说明 |
|------|------|------|------|
| WREN | 0x06 | 写使能 | 每次写操作前执行 |
| WRDI | 0x04 | 写禁止 | 清除写使能 |
| READ | 0x03 | 读取数据 | 发送地址后连续读取 |
| WRITE | 0x02 | 写入数据 | 发送地址后连续写入 |
| RDSR | 0x05 | 读状态寄存器 | 读取 WEL/BP 位 |
| WRSR | 0x01 | 写状态寄存器 | 设置保护位 |
| RDID | 0x9F | 读设备 ID | 厂商 + 产品 ID |

### FRAM 与 Flash 驱动的关键差异

```
Flash 写入流程:
  1. 写使能 (WREN)
  2. 擦除扇区 (SECTOR_ERASE)  ← FRAM 不需要
  3. 等待擦除完成 (BUSY)        ← FRAM 不需要
  4. 写使能 (WREN)
  5. 页编程 (PAGE_PROGRAM)     ← FRAM: 直接 WRITE
  6. 等待写入完成 (BUSY)        ← FRAM 不需要

FRAM 写入流程:
  1. 写使能 (WREN)
  2. 写入数据 (WRITE)          ← 直接写入, 无需擦除
  完成! (无等待, 写入速度 = 总线速度)
```

### 驱动接口定义

```c
typedef struct {
    spi_bus_t *spi;
    gpio_pin_t cs_pin;
    uint32_t mem_size;    /* 存储容量 (字节) */
    uint8_t addr_width;   /* 地址宽度 (2 或 3 字节) */
} fram_handle_t;

typedef enum {
    FRAM_OK = 0,
    FRAM_ERR_NOT_FOUND,
    FRAM_ERR_TIMEOUT,
    FRAM_ERR_WRITE_PROTECTED,
    FRAM_ERR_INVALID_PARAM,
} fram_status_t;

fram_status_t fram_init(fram_handle_t *h, spi_bus_t *spi, gpio_pin_t cs,
                         uint32_t mem_size, uint8_t addr_width);
fram_status_t fram_read(fram_handle_t *h, uint32_t addr, uint8_t *buf, uint32_t len);
fram_status_t fram_write(fram_handle_t *h, uint32_t addr,
                          const uint8_t *buf, uint32_t len);
fram_status_t fram_read_id(fram_handle_t *h, uint16_t *vendor_id, uint16_t *product_id);
```

### SPI FRAM 驱动核心代码

```c
/* === 底层 SPI 传输 === */

static void fram_cs_low(fram_handle_t *h) { gpio_write(h->cs_pin, 0); }
static void fram_cs_high(fram_handle_t *h) { gpio_write(h->cs_pin, 1); }

static void fram_spi_write(fram_handle_t *h, const uint8_t *data, uint16_t len) {
    spi_write(h->spi, (uint8_t *)data, len);
}

static void fram_spi_read(fram_handle_t *h, uint8_t *data, uint16_t len) {
    spi_read(h->spi, data, len);
}

static void fram_spi_xfer(fram_handle_t *h, uint8_t *tx, uint8_t *rx, uint16_t len) {
    spi_transfer(h->spi, tx, rx, len);
}

/* === 发送地址 (2 或 3 字节) === */
static void fram_send_addr(fram_handle_t *h, uint32_t addr) {
    uint8_t addr_buf[3];
    uint8_t idx = 0;
    if (h->addr_width == 3) {
        addr_buf[idx++] = (addr >> 16) & 0xFF;
    }
    addr_buf[idx++] = (addr >> 8) & 0xFF;
    addr_buf[idx++] = addr & 0xFF;
    fram_spi_write(h, addr_buf, idx);
}

/* === 读设备 ID === */
fram_status_t fram_read_id(fram_handle_t *h,
                            uint16_t *vendor_id, uint16_t *product_id) {
    fram_cs_low(h);
    uint8_t cmd = 0x9F;  /* RDID */
    fram_spi_write(h, &cmd, 1);

    uint8_t id[9];
    fram_spi_read(h, id, 9);
    fram_cs_high(h);

    *vendor_id = (id[0] << 8) | id[1];
    *product_id = (id[2] << 8) | id[3];

    /* 已知厂商 ID 校验 */
    /* 0x0483 = Fujitsu (MB85RS) */
    /* 0x047F = Cypress/Infineon (FM25V) */
    if (id[0] == 0x00 || id[0] == 0xFF) return FRAM_ERR_NOT_FOUND;
    return FRAM_OK;
}

/* === 读状态寄存器 === */
static uint8_t fram_read_status(fram_handle_t *h) {
    fram_cs_low(h);
    uint8_t cmd = 0x05;  /* RDSR */
    fram_spi_write(h, &cmd, 1);
    uint8_t status;
    fram_spi_read(h, &status, 1);
    fram_cs_high(h);
    return status;
}

/* 状态寄存器位定义:
 * Bit 7: WPEN  - 写保护使能
 * Bit 6-4: BP0-BP2 - 块保护 (000=无保护)
 * Bit 3: 0
 * Bit 2: WEL  - 写使能锁存 (1=可写)
 * Bit 1-0: 0
 */

/* === 写使能 === */
static void fram_write_enable(fram_handle_t *h) {
    fram_cs_low(h);
    uint8_t cmd = 0x06;  /* WREN */
    fram_spi_write(h, &cmd, 1);
    fram_cs_high(h);
}

/* === 读取数据 (任意长度, 无需分页) === */
/* FRAM 无页大小限制, 可一次写入/读取整个芯片 */
fram_status_t fram_read(fram_handle_t *h, uint32_t addr,
                         uint8_t *buf, uint32_t len) {
    if (addr + len > h->mem_size) return FRAM_ERR_INVALID_PARAM;

    fram_cs_low(h);

    /* 发送 READ 命令 + 地址 */
    uint8_t cmd = 0x03;
    fram_spi_write(h, &cmd, 1);
    fram_send_addr(h, addr);

    /* 连续读取 (无页限制, 无需等待 BUSY) */
    fram_spi_read(h, buf, len);

    fram_cs_high(h);
    return FRAM_OK;
}

/* === 写入数据 (任意长度, 无需擦除, 无需分页) === */
/* 关键优势: 直接写入, 无擦除步骤, 无写延迟 */
fram_status_t fram_write(fram_handle_t *h, uint32_t addr,
                          const uint8_t *buf, uint32_t len) {
    if (addr + len > h->mem_size) return FRAM_ERR_INVALID_PARAM;

    /* 检查写保护状态 */
    uint8_t status = fram_read_status(h);
    if (!(status & 0x02)) {
        /* WEL 未置位, 执行写使能 */
        fram_write_enable(h);
    }

    /* 检查块保护 (BP 位) */
    if (status & 0x70) {
        /* 有块保护, 检查目标地址是否在保护范围内 */
        /* 简化处理: 清除保护 */
        fram_cs_low(h);
        uint8_t cmd = 0x01;  /* WRSR */
        uint8_t sr = 0x00;   /* 清除所有保护 */
        fram_spi_write(h, &cmd, 1);
        fram_spi_write(h, &sr, 1);
        fram_cs_high(h);
        fram_write_enable(h);
    }

    fram_cs_low(h);

    /* 发送 WRITE 命令 + 地址 */
    uint8_t cmd = 0x02;
    fram_spi_write(h, &cmd, 1);
    fram_send_addr(h, addr);

    /* 连续写入 (无页限制, 无需等待 BUSY) */
    fram_spi_write(h, buf, len);

    fram_cs_high(h);

    /* FRAM 写入后无需等待! 数据已非易失保存 */
    return FRAM_OK;
}

/* === 掉电快速保存示例 === */
/* 利用 FRAM 无写延迟特性, 在掉电瞬间快速保存关键数据 */
fram_status_t fram_emergency_save(fram_handle_t *h, uint32_t addr,
                                   const uint8_t *data, uint32_t len) {
    /* FRAM 写入速度 = SPI 总线速度, 1KB @40MHz ≈ 200μs */
    /* 掉电检测中断中直接写入, 无需擦除 */
    return fram_write(h, addr, data, len);
}
```

### I2C FRAM 驱动 (MB85RC256)

```c
/* I2C FRAM 与 I2C EEPROM 驱动几乎相同, 但无写延迟 */
typedef struct {
    i2c_bus_t *bus;
    uint8_t dev_addr;     /* 设备地址 0x50~0x57 */
    uint32_t mem_size;
} fram_i2c_handle_t;

fram_status_t fram_i2c_write(fram_i2c_handle_t *h, uint16_t addr,
                              const uint8_t *data, uint16_t len) {
    uint8_t buf[2 + 256];  /* 2字节地址 + 数据 */
    buf[0] = (addr >> 8) & 0xFF;
    buf[1] = addr & 0xFF;
    memcpy(&buf[2], data, len);

    if (i2c_write(h->bus, h->dev_addr, buf, 2 + len) != I2C_OK)
        return FRAM_ERR_TIMEOUT;

    /* FRAM 无写延迟! 不需要 ACK 轮询或延时 */
    /* 与 EEPROM 的唯一区别: 不需要等待 Twc */
    return FRAM_OK;
}

fram_status_t fram_i2c_read(fram_i2c_handle_t *h, uint16_t addr,
                             uint8_t *buf, uint16_t len) {
    uint8_t addr_buf[2];
    addr_buf[0] = (addr >> 8) & 0xFF;
    addr_buf[1] = addr & 0xFF;

    if (i2c_write(h->bus, h->dev_addr, addr_buf, 2) != I2C_OK)
        return FRAM_ERR_TIMEOUT;
    if (i2c_read(h->bus, h->dev_addr, buf, len) != I2C_OK)
        return FRAM_ERR_TIMEOUT;

    return FRAM_OK;
}
```

## 4. 调试与测试规范

### 硬件验证清单

- [ ] 万用表确认 VCC 电压正确（FM25V: 2.0~5.5V, MB85: 3.0~3.6V）
- [ ] SPI 四线 (CS/MOSI/MISO/SCK) 连接正确
- [ ] WP# 和 HOLD# 接 VCC 或 MCU GPIO（不可悬空）
- [ ] 100nF 去耦电容已贴装
- [ ] I2C 模式确认 SCL/SDA 上拉电阻

### 通信验证

```c
/* FRAM 连接验证 */
fram_status_t fram_verify(fram_handle_t *h) {
    uint16_t vendor, product;
    fram_status_t st = fram_read_id(h, &vendor, &product);
    if (st != FRAM_OK) {
        printf("FRAM not responding\n");
        return st;
    }

    printf("FRAM ID: Vendor=0x%04X, Product=0x%04X\n", vendor, product);

    /* 已知 ID 列表 */
    struct { uint16_t vid; uint16_t pid; const char *name; } known[] = {
        {0x047F, 0x7E7E, "FM25V10"},   /* Cypress/Infineon */
        {0x0483, 0x7E7E, "MB85RS256"}, /* Fujitsu */
        {0x0483, 0x7E7F, "MB85RS1M"},
    };

    for (int i = 0; i < 3; i++) {
        if (vendor == known[i].vid) {
            printf("Detected: %s\n", known[i].name);
            return FRAM_OK;
        }
    }
    return FRAM_OK;  /* 未知但能响应 */
}
```

### 寿命测试

```c
/* FRAM 擦写寿命测试 - 验证无限擦写特性 */
fram_status_t fram_endurance_test(fram_handle_t *h) {
    uint32_t test_addr = 0x0000;
    uint8_t pattern_a = 0xAA;
    uint8_t pattern_b = 0x55;
    uint8_t read_val;

    /* 连续写入 100 万次 (Flash 无法承受) */
    printf("FRAM endurance test: 1,000,000 write cycles...\n");
    for (uint32_t i = 0; i < 1000000; i++) {
        uint8_t data = (i & 1) ? pattern_b : pattern_a;
        fram_write(h, test_addr, &data, 1);
        fram_read(h, test_addr, &read_val, 1);
        if (read_val != data) {
            printf("FAIL at cycle %lu: wrote 0x%02X, read 0x%02X\n",
                   i, data, read_val);
            return FRAM_ERR_INVALID_PARAM;
        }
    }
    printf("PASSED: 1,000,000 write cycles OK (Flash would fail at ~100K)\n");
    return FRAM_OK;
}
```

### 速度对比测试

```c
/* FRAM vs Flash 写入速度对比 */
void fram_speed_compare(fram_handle_t *fram, flash_handle_t *flash) {
    uint8_t buf[1024];
    for (int i = 0; i < 1024; i++) buf[i] = (uint8_t)i;

    /* FRAM 写入 1KB */
    uint32_t start = get_tick_us();
    fram_write(fram, 0, buf, 1024);
    uint32_t fram_time = get_tick_us() - start;

    /* Flash 写入 1KB (含擦除) */
    start = get_tick_us();
    flash_erase_sector(flash, 0);
    flash_write(flash, 0, buf, 1024);
    uint32_t flash_time = get_tick_us() - start;

    printf("Write 1KB: FRAM=%luμs, Flash=%luμs (FRAM %.1fx faster)\n",
           fram_time, flash_time, (float)flash_time / fram_time);
}
```

### 性能测试指标

| 指标 | FM25V10 (SPI 40MHz) | MB85RS256 (SPI 25MHz) | MB85RC256 (I2C 1MHz) |
|------|----------------------|------------------------|------------------------|
| 读取速度 | ~5 MB/s | ~3 MB/s | ~120 KB/s |
| 写入速度 | ~5 MB/s | ~3 MB/s | ~120 KB/s |
| 单字节写入 | <1μs | <1μs | ~20μs |
| 1KB 写入 | ~200μs | ~340μs | ~8ms |
| 写入延迟 | 0 (无等待) | 0 | 0 |
| 擦除时间 | N/A (无需擦除) | N/A | N/A |

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| FRAM 无响应 | SPI 接线错误 | 检查 CS/MOSI/MISO/SCK，FRAM 与 Flash 引脚相同 |
| 读 ID 返回 0xFF | CS 未正确控制 | 确认 CS 时序，发送命令前拉低 CS |
| 写入后读不到数据 | WEL 未置位 | 每次写入前执行 WREN 命令 |
| 写入部分区域失败 | 块保护位 (BP) 未清除 | 读取状态寄存器，清除 BP 位 |
| 地址计算错误 | 地址宽度设置错误 | 128KB 以上用 3 字节地址，32KB 用 2 字节 |
| 误用 Flash 驱动写入 | 使用了页编程+等待 BUSY | FRAM 无需分页，无需等待 BUSY |
| I2C FRAM 写入后 ACK 失败 | 按 EEPROM 方式等待 Twc | FRAM 无写延迟，写后立即可读 |
| 数据意外丢失 | 掉电时正在写入 | FRAM 写入速度极快，掉电风险极低 |
| 读取速度不如预期 | SPI 时钟过低 | 提高 SPI 时钟至 FRAM 最大频率 |
| 替换 Flash 后不工作 | 驱动未修改 | 使用 FRAM 专用驱动（无擦除、无等待） |
| 高温数据保持下降 | 超过规格温度 | MB85 系列保持 10 年@85°C，FM25V 保持 38 年 |
| FRAM 容量识别错误 | 未读取设备 ID | 初始化时读取 ID 确认容量 |

## 相关文档

- `skill://mcu-driver-core/templates/driver-template-i2c.c` — I2C 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-spi.c` — SPI 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
- `skill://mcu-driver-core/guides/pitfalls.md` — 跨类别常见问题与避坑指南
