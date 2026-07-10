# Flash 存储器开发规范

## 1. 概述与选型指南

### 器件简介

NOR Flash 是一种非易失性存储器，支持随机读取、快速访问和片上执行（XIP），广泛应用于存储固件、字库、图片资源、日志数据等。SPI NOR Flash 因接口简单、容量适中、性价比高，成为 MCU 系统中最常用的外部 Flash 存储方案。

与 EEPROM 的关键区别：Flash 必须先擦除才能写入，擦除以扇区（4KB）或块（64KB）为单位，写入以页（256B）为单位。擦写寿命通常为 10 万次，远低于 EEPROM 的 100 万次。

### 常见型号对比

| 型号 | 接口 | 容量 | 扇区/块 | 页大小 | 最大时钟 | 擦写寿命 | 数据保持 | 价格 |
|------|------|------|---------|--------|----------|----------|----------|------|
| W25Q64 | SPI | 8MB | 4KB/64KB | 256B | 104MHz | 10万次 | 20年 | ¥3~6 |
| W25Q128 | SPI | 16MB | 4KB/64KB | 256B | 104MHz | 10万次 | 20年 | ¥5~10 |
| W25Q256 | SPI/QSPI | 32MB | 4KB/64KB | 256B | 104MHz | 10万次 | 20年 | ¥8~15 |
| W25Q80 | SPI | 1MB | 4KB/64KB | 256B | 104MHz | 10万次 | 20年 | ¥1.5~3 |
| W25Q16 | SPI | 2MB | 4KB/64KB | 256B | 104MHz | 10万次 | 20年 | ¥2~4 |
| GD25Q16 | SPI | 2MB | 4KB | 256B | 120MHz | 10万次 | 20年 | ¥1~2 |

### 选型决策树

```
容量 ≤ 1MB？   → 是 → W25Q80 (8Mbit, 廉价)
容量 ≤ 2MB？   → 是 → W25Q16 / GD25Q16
容量 ≤ 8MB？   → 是 → W25Q64 (最常用, 性价比高)
容量 ≤ 16MB？  → 是 → W25Q128
容量 ≤ 32MB？  → 是 → W25Q256 (支持QSPI, 高速读取)
需要 XIP 执行代码？ → 是 → QSPI 模式 (W25Q256)
需要低成本？   → 是 → GD25Q系列 (国产替代)
通用推荐 → W25Q64 (8MB, SPI, 覆盖大多数应用)
```

### 适用场景

- **W25Q80/W25Q16**：小容量字库、图标资源、配置备份
- **W25Q64**：固件存储、OTA 升级镜像、数据日志（最通用）
- **W25Q128/W25Q256**：大容量存储、字库、图片资源、音频文件

## 2. 硬件设计规范

### SPI Flash 电路 (W25Q64)

```
VCC(3.3V)
  │
  ├── 100nF ── GND  (去耦电容, 靠近引脚)
  │
  │     ┌──────────────────────────┐
  │     │        W25Q64            │
  │     │  (SOIC-8 封装)           │
  │     │                          │
  └─────┤ CS#(1)  ── MCU CS        │
        │ DO(2)   ── MCU MISO      │
        │ WP#(3)  ── VCC (写允许)   │
  GND ──┤ GND(4)                   │
  MCU ──┤ DI(5)   ── MCU MOSI      │
  MCU ──┤ CLK(6)  ── MCU SCK       │
        │ HOLD#(7)── VCC (不使用)   │
  VCC ──┤ VCC(8)                   │
        └──────────────────────────┘

  可选: WP# 和 HOLD# 接 MCU GPIO 以便软件控制
```

**引脚说明**：

| 引脚 | 名称 | 功能 |
|------|------|------|
| CS# | 片选 | 低电平有效，选中器件 |
| DO (SO) | 数据输出 | SPI MISO |
| WP# | 写保护 | 低电平保护状态寄存器，高电平允许写 |
| GND | 地 | 电源地 |
| DI (SI) | 数据输入 | SPI MOSI |
| CLK (SCK) | 时钟 | SPI 时钟 |
| HOLD# | 暂停 | 低电平暂停当前操作，不用时接 VCC |
| VCC | 电源 | 2.7V~3.6V |

### QSPI 模式电路 (W25Q256)

```
VCC(3.3V) ── 100nF ── GND

        ┌──────────────────────────┐
        │        W25Q256           │
        │  (SOIC-16/WSON-8 封装)   │
        │                          │
MCU CS  ── CS#(1)                  │
MCU IO0 ── IO0/DI(2)  (MOSI)      │
MCU IO1 ── IO1/DO(3)  (MISO)      │
MCU IO2 ── IO2/WP#(4) (QSPI双向)  │
MCU IO3 ── IO3/HOLD#(5)(QSPI双向) │
MCU SCK ── CLK(6)                 │
VCC    ── VCC(8)                  │
GND    ── GND(4/9)                │
        └──────────────────────────┘

QSPI 模式: IO0~IO3 均为双向数据线
WP# 和 HOLD# 在 QSPI 模式下变为 IO2/IO3 数据线
```

### SPI 模式选择

| 模式 | CPOL | CPHA | 说明 |
|------|------|------|------|
| Mode 0 | 0 | 0 | 时钟空闲低，上升沿采样（最常用） |
| Mode 3 | 1 | 1 | 时钟空闲高，下降沿采样 |
| Mode 0/3 | - | - | W25Q 系列同时支持 Mode 0 和 Mode 3 |

### 写状态寄存器控制

```
状态寄存器 (Status Register):
  Bit 7: SRP0 - 状态寄存器保护位0
  Bit 6: BP3  - 块保护位3
  Bit 5: BP2  - 块保护位2
  Bit 4: BP0  - 块保护位0
  Bit 3: BP1  - 块保护位1
  Bit 2: WEL  - 写使能锁存 (只读)
  Bit 1: QB   - Quad SPI 使能 (W25Q256)
  Bit 0: BUSY - 忙标志 (只读, 擦写期间为1)

WP# 引脚:
  WP#=L 且 SRP=1 → 状态寄存器写保护 (硬件+软件)
  WP#=H          → 可写状态寄存器
```

### PCB 设计要点

- VCC 引脚放置 100nF 去耦电容，尽量靠近
- SPI 走线尽量短直，CS 信号线靠近 Flash
- 高速 SPI（>50MHz）需考虑信号完整性，走线等长
- QSPI 模式下 4 根数据线需等长走线
- 大容量 Flash 需考虑散热，底部铺铜
- 避免在 Flash 下方走高速信号

### 电气参数限制

| 参数 | W25Q64 | W25Q128 | W25Q256 |
|------|--------|---------|---------|
| 工作电压 | 2.7~3.6V | 2.7~3.6V | 2.7~3.6V |
| 最大 SPI 时钟 | 104MHz | 104MHz | 104MHz |
| 最大 QSPI 时钟 | - | - | 104MHz (4x传输) |
| 扇区擦除时间 | 45ms (typ) | 45ms (typ) | 45ms (typ) |
| 块擦除时间(64KB) | 150ms (typ) | 150ms (typ) | 150ms (typ) |
| 整片擦除时间 | 20s (typ) | 20s (typ) | 40s (typ) |
| 页编程时间 | 0.7ms (typ) | 0.7ms (typ) | 0.7ms (typ) |
| 擦写寿命 | 100,000次 | 100,000次 | 100,000次 |
| 数据保持 | 20年 | 20年 | 20年 |

## 3. 驱动开发规范

### Flash 命令表

| 命令 | 代码 | 功能 | 说明 |
|------|------|------|------|
| READ | 0x03 | 读取数据 | 标准读取，最大 50MHz |
| FAST_READ | 0x0B | 快速读取 | 带虚拟字节，最大 104MHz |
| WRITE_ENABLE | 0x06 | 写使能 | 每次写/擦除前必须执行 |
| WRITE_DISABLE | 0x04 | 写禁止 | 清除 WEL 位 |
| READ_SR1 | 0x05 | 读状态寄存器1 | 读取 BUSY/WEL/BP 位 |
| WRITE_SR1 | 0x01 | 写状态寄存器1 | 设置保护位 |
| PAGE_PROGRAM | 0x02 | 页编程 | 写入数据，最大 256 字节 |
| SECTOR_ERASE | 0x20 | 扇区擦除 | 擦除 4KB |
| BLOCK_ERASE_32K | 0x52 | 块擦除 | 擦除 32KB |
| BLOCK_ERASE_64K | 0xD8 | 块擦除 | 擦除 64KB |
| CHIP_ERASE | 0xC7 | 整片擦除 | 擦除全部 |
| JEDEC_ID | 0x9F | 读取 JEDEC ID | 厂商 ID + 芯片 ID |
| READ_ID | 0x90 | 读取设备 ID | 兼容模式 |

### 驱动接口定义

```c
typedef struct {
    spi_bus_t *spi;       /* SPI 总线句柄 */
    gpio_pin_t cs_pin;    /* CS 片选引脚 */
    uint32_t flash_size;  /* Flash 容量 (字节) */
    uint16_t page_size;   /* 页大小 (通常 256) */
    uint16_t sector_size; /* 扇区大小 (通常 4096) */
    uint16_t block_size;  /* 块大小 (通常 65536) */
} flash_handle_t;

typedef enum {
    FLASH_OK = 0,
    FLASH_ERR_NOT_FOUND,
    FLASH_ERR_TIMEOUT,
    FLASH_ERR_BUSY,
    FLASH_ERR_PROTECTED,
    FLASH_ERR_INVALID_PARAM,
    FLASH_ERR_ERASE_NEEDED,
} flash_status_t;

flash_status_t flash_init(flash_handle_t *h, spi_bus_t *spi, gpio_pin_t cs);
flash_status_t flash_read_jedec_id(flash_handle_t *h, uint8_t *mid, uint16_t *did);
flash_status_t flash_read(flash_handle_t *h, uint32_t addr, uint8_t *buf, uint32_t len);
flash_status_t flash_write(flash_handle_t *h, uint32_t addr, const uint8_t *buf, uint32_t len);
flash_status_t flash_erase_sector(flash_handle_t *h, uint32_t addr);
flash_status_t flash_erase_block_64k(flash_handle_t *h, uint32_t addr);
flash_status_t flash_erase_chip(flash_handle_t *h);
flash_status_t flash_read_status(flash_handle_t *h, uint8_t *status);
bool flash_is_busy(flash_handle_t *h);
```

### W25Q64 SPI 驱动核心代码

```c
#include <string.h>

/* === 底层 SPI 传输 === */

static void flash_cs_low(flash_handle_t *h) {
    gpio_write(h->cs_pin, 0);
}

static void flash_cs_high(flash_handle_t *h) {
    gpio_write(h->cs_pin, 1);
}

/* 发送命令 + 地址 (3字节地址) */
static void flash_send_cmd_addr(flash_handle_t *h, uint8_t cmd,
                                 uint32_t addr) {
    uint8_t buf[4];
    buf[0] = cmd;
    buf[1] = (addr >> 16) & 0xFF;
    buf[2] = (addr >> 8) & 0xFF;
    buf[3] = addr & 0xFF;
    spi_write(h->spi, buf, 4);
}

/* === 读 JEDEC ID === */
/* 返回: 厂商ID (如 0xEF=Winbond), 设备ID (如 0x4017=W25Q64) */
flash_status_t flash_read_jedec_id(flash_handle_t *h,
                                    uint8_t *mid, uint16_t *did) {
    flash_cs_low(h);
    uint8_t cmd = 0x9F;  /* JEDEC ID 命令 */
    spi_write(h->spi, &cmd, 1);

    uint8_t id[3];
    spi_read(h->spi, id, 3);
    flash_cs_high(h);

    *mid = id[0];           /* Manufacturer ID */
    *did = (id[1] << 8) | id[2];  /* Device ID */

    /* 校验已知的厂商 ID */
    if (*mid == 0x00 || *mid == 0xFF) return FLASH_ERR_NOT_FOUND;
    return FLASH_OK;
}

/* === 读状态寄存器 === */
flash_status_t flash_read_status(flash_handle_t *h, uint8_t *status) {
    flash_cs_low(h);
    uint8_t cmd = 0x05;  /* READ_SR1 */
    spi_write(h->spi, &cmd, 1);
    spi_read(h->spi, status, 1);
    flash_cs_high(h);
    return FLASH_OK;
}

/* === 检查 BUSY 标志 === */
bool flash_is_busy(flash_handle_t *h) {
    uint8_t status;
    flash_read_status(h, &status);
    return (status & 0x01) != 0;  /* Bit0: BUSY */
}

/* 等待操作完成 */
static flash_status_t flash_wait_ready(flash_handle_t *h, uint32_t timeout_ms) {
    uint32_t elapsed = 0;
    while (flash_is_busy(h)) {
        spi_delay_ms(1);
        if (++elapsed >= timeout_ms) return FLASH_ERR_TIMEOUT;
    }
    return FLASH_OK;
}

/* === 写使能 === */
static flash_status_t flash_write_enable(flash_handle_t *h) {
    flash_cs_low(h);
    uint8_t cmd = 0x06;  /* WRITE_ENABLE */
    spi_write(h->spi, &cmd, 1);
    flash_cs_high(h);

    /* 验证 WEL 位已置位 */
    uint8_t status;
    flash_read_status(h, &status);
    if (!(status & 0x02)) return FLASH_ERR_PROTECTED;  /* WEL 未置位 */
    return FLASH_OK;
}

/* === 读取数据 === */
flash_status_t flash_read(flash_handle_t *h, uint32_t addr,
                           uint8_t *buf, uint32_t len) {
    if (addr + len > h->flash_size) return FLASH_ERR_INVALID_PARAM;

    flash_cs_low(h);
    flash_send_cmd_addr(h, 0x03, addr);  /* READ 命令 */
    spi_read(h->spi, buf, len);
    flash_cs_high(h);
    return FLASH_OK;
}

/* === 快速读取 (支持更高时钟) === */
flash_status_t flash_fast_read(flash_handle_t *h, uint32_t addr,
                                uint8_t *buf, uint32_t len) {
    if (addr + len > h->flash_size) return FLASH_ERR_INVALID_PARAM;

    flash_cs_low(h);
    uint8_t buf_cmd[5];
    buf_cmd[0] = 0x0B;  /* FAST_READ */
    buf_cmd[1] = (addr >> 16) & 0xFF;
    buf_cmd[2] = (addr >> 8) & 0xFF;
    buf_cmd[3] = addr & 0xFF;
    buf_cmd[4] = 0x00;  /* 虚拟字节 */
    spi_write(h->spi, buf_cmd, 5);
    spi_read(h->spi, buf, len);
    flash_cs_high(h);
    return FLASH_OK;
}

/* === 扇区擦除 (4KB) === */
flash_status_t flash_erase_sector(flash_handle_t *h, uint32_t addr) {
    /* 地址需扇区对齐 */
    if (addr % h->sector_size != 0) return FLASH_ERR_INVALID_PARAM;

    flash_status_t st = flash_write_enable(h);
    if (st != FLASH_OK) return st;

    flash_cs_low(h);
    flash_send_cmd_addr(h, 0x20, addr);  /* SECTOR_ERASE */
    flash_cs_high(h);

    /* 等待擦除完成 (典型 45ms, 最大 400ms) */
    return flash_wait_ready(h, 500);
}

/* === 块擦除 (64KB) === */
flash_status_t flash_erase_block_64k(flash_handle_t *h, uint32_t addr) {
    if (addr % h->block_size != 0) return FLASH_ERR_INVALID_PARAM;

    flash_status_t st = flash_write_enable(h);
    if (st != FLASH_OK) return st;

    flash_cs_low(h);
    flash_send_cmd_addr(h, 0xD8, addr);  /* BLOCK_ERASE_64K */
    flash_cs_high(h);

    /* 等待擦除完成 (典型 150ms, 最大 2000ms) */
    return flash_wait_ready(h, 2000);
}

/* === 整片擦除 === */
flash_status_t flash_erase_chip(flash_handle_t *h) {
    flash_status_t st = flash_write_enable(h);
    if (st != FLASH_OK) return st;

    flash_cs_low(h);
    uint8_t cmd = 0xC7;  /* CHIP_ERASE */
    spi_write(h->spi, &cmd, 1);
    flash_cs_high(h);

    /* 等待整片擦除完成 (典型 20s, 最大 80s) */
    return flash_wait_ready(h, 80000);
}

/* === 页编程 (写入, 最大 256 字节) === */
static flash_status_t flash_page_program(flash_handle_t *h, uint32_t addr,
                                          const uint8_t *data, uint16_t len) {
    /* 检查页边界: 单次写入不能跨页 */
    uint16_t page_offset = addr % h->page_size;
    uint16_t space = h->page_size - page_offset;
    if (len > space) len = space;

    flash_status_t st = flash_write_enable(h);
    if (st != FLASH_OK) return st;

    flash_cs_low(h);
    flash_send_cmd_addr(h, 0x02, addr);  /* PAGE_PROGRAM */
    spi_write(h->spi, (uint8_t *)data, len);
    flash_cs_high(h);

    /* 等待编程完成 (典型 0.7ms, 最大 3ms) */
    return flash_wait_ready(h, 5);
}

/* === 任意长度写入 (自动分页) === */
/* 注意: 目标区域必须已擦除 (全 0xFF) */
flash_status_t flash_write(flash_handle_t *h, uint32_t addr,
                            const uint8_t *buf, uint32_t len) {
    if (addr + len > h->flash_size) return FLASH_ERR_INVALID_PARAM;

    uint32_t offset = 0;
    while (offset < len) {
        uint16_t page_offset = (addr + offset) % h->page_size;
        uint16_t chunk = h->page_size - page_offset;
        if (chunk > (len - offset)) chunk = len - offset;

        flash_status_t st = flash_page_program(h, addr + offset,
                                                buf + offset, chunk);
        if (st != FLASH_OK) return st;
        offset += chunk;
    }
    return FLASH_OK;
}

/* === 带擦除的安全写入 (自动擦除后写入) === */
flash_status_t flash_write_with_erase(flash_handle_t *h, uint32_t addr,
                                       const uint8_t *buf, uint32_t len) {
    if (addr + len > h->flash_size) return FLASH_ERR_INVALID_PARAM;

    /* 计算需要擦除的扇区范围 */
    uint32_t start_sector = addr / h->sector_size;
    uint32_t end_sector = (addr + len - 1) / h->sector_size;

    /* 擦除涉及的扇区 */
    for (uint32_t s = start_sector; s <= end_sector; s++) {
        flash_status_t st = flash_erase_sector(h, s * h->sector_size);
        if (st != FLASH_OK) return st;
    }

    /* 写入数据 */
    return flash_write(h, addr, buf, len);
}
```

### 磨损均衡策略 (简单版)

```c
/* 简单磨损均衡: 记录写入位置, 均匀分布擦写 */
typedef struct {
    uint32_t write_ptr;       /* 当前写入指针 */
    uint32_t sector_count;    /* 总扇区数 */
    uint32_t sector_size;     /* 扇区大小 */
    uint32_t *erase_counts;   /* 各扇区擦除次数 */
} wear_level_ctx_t;

flash_status_t flash_write_balanced(flash_handle_t *h,
                                     wear_level_ctx_t *wl,
                                     const uint8_t *data, uint16_t len) {
    /* 找到擦除次数最少的扇区 */
    uint32_t min_count = 0xFFFFFFFF;
    uint32_t target_sector = 0;
    for (uint32_t i = 0; i < wl->sector_count; i++) {
        if (wl->erase_counts[i] < min_count) {
            min_count = wl->erase_counts[i];
            target_sector = i;
        }
    }

    uint32_t addr = target_sector * wl->sector_size;
    flash_status_t st = flash_erase_sector(h, addr);
    if (st != FLASH_OK) return st;
    wl->erase_counts[target_sector]++;

    return flash_write(h, addr, data, len);
}
```

## 4. 调试与测试规范

### 硬件验证清单

- [ ] 万用表确认 VCC 电压 2.7~3.6V（3.3V 典型）
- [ ] CS# 引脚连接正确，MCU 初始化为高电平输出
- [ ] WP# 和 HOLD# 接 VCC 或 MCU GPIO（不可悬空）
- [ ] 100nF 去耦电容已贴装且靠近 VCC 引脚
- [ ] 示波器检查 SPI 时钟波形（频率、相位、电平）
- [ ] SPI 模式配置为 Mode 0 (CPOL=0, CPHA=0)

### 通信验证

```c
/* Flash 连接验证 */
flash_status_t flash_verify(flash_handle_t *h) {
    uint8_t mid;
    uint16_t did;
    flash_status_t st = flash_read_jedec_id(h, &mid, &did);
    if (st != FLASH_OK) {
        printf("Flash not responding\n");
        return st;
    }

    /* 常见厂商 ID: 0xEF=Winbond, 0xC8=GigaDevice, 0x20=Micron */
    printf("Flash JEDEC ID: MID=0x%02X, DID=0x%04X\n", mid, did);

    /* 校验已知芯片 */
    struct { uint8_t mid; uint16_t did; const char *name; uint32_t size; } known[] = {
        {0xEF, 0x4017, "W25Q64",  8 * 1024 * 1024},
        {0xEF, 0x4018, "W25Q128", 16 * 1024 * 1024},
        {0xEF, 0x4019, "W25Q256", 32 * 1024 * 1024},
        {0xEF, 0x4015, "W25Q16",  2 * 1024 * 1024},
        {0xEF, 0x4014, "W25Q80",  1 * 1024 * 1024},
        {0xC8, 0x4017, "GD25Q64", 8 * 1024 * 1024},
    };

    for (int i = 0; i < (int)(sizeof(known)/sizeof(known[0])); i++) {
        if (mid == known[i].mid && did == known[i].did) {
            printf("Detected: %s (%d MB)\n", known[i].name,
                   known[i].size / (1024*1024));
            h->flash_size = known[i].size;
            return FLASH_OK;
        }
    }
    printf("Unknown Flash chip\n");
    return FLASH_ERR_NOT_FOUND;
}
```

### 数据校验

```c
/* 读写回环测试 */
flash_status_t flash_rw_test(flash_handle_t *h) {
    uint32_t test_addr = 0x000000;  /* 使用第一个扇区测试 */
    uint8_t write_buf[256], read_buf[256];

    /* 准备测试数据 */
    for (int i = 0; i < 256; i++) write_buf[i] = (uint8_t)(i ^ 0xA5);

    /* 擦除扇区 */
    flash_status_t st = flash_erase_sector(h, test_addr);
    if (st != FLASH_OK) return st;

    /* 写入 */
    st = flash_write(h, test_addr, write_buf, 256);
    if (st != FLASH_OK) return st;

    /* 读回 */
    st = flash_read(h, test_addr, read_buf, 256);
    if (st != FLASH_OK) return st;

    /* 校验 */
    if (memcmp(write_buf, read_buf, 256) != 0) {
        printf("Flash RW test FAILED\n");
        return FLASH_ERR_INVALID_PARAM;
    }
    printf("Flash RW test PASSED\n");
    return FLASH_OK;
}
```

### 性能测试指标

| 指标 | W25Q64 | W25Q128 | W25Q256 (QSPI) |
|------|--------|---------|----------------|
| 读取速度 (SPI) | ~13MB/s (104MHz) | ~13MB/s | ~13MB/s |
| 读取速度 (QSPI) | - | - | ~52MB/s |
| 页编程速度 | ~350KB/s | ~350KB/s | ~350KB/s |
| 扇区擦除时间 | 45ms (typ) | 45ms | 45ms |
| 整片擦除 | 20s | 20s | 40s |

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 读取 JEDEC ID 返回 0xFF | SPI 接线错误或 CS 未拉低 | 检查 MOSI/MISO/SCK/CS 接线，CS 初始为高 |
| 读取 ID 为 0x0000 或 0xFFFF | MISO 未连接 | 检查 MISO 线连接，确认 SPI Mode 正确 |
| 写入后数据全 0xFF | 未执行擦除或擦除失败 | 写入前必须先擦除目标区域 |
| 写入数据部分错误 | 页编程跨页 | 单次写入不超过页边界，使用自动分页函数 |
| flash_write 返回 BUSY 超时 | 等待时间不足 | 增大超时时间，检查 BUSY 标志轮询 |
| 擦除后仍为非 0xFF | 块保护位未清除 | 检查状态寄存器 BP 位，清除保护后重试 |
| 高温下写入失败 | 温度超规格 | 确认工作温度，工业级 -40~+85C |
| QSPI 模式无法切换 | QE 位未置位 | 先写状态寄存器置 QE 位再切换 QSPI |
| 数据意外丢失 | 掉电时正在写入 | 使用掉电检测 + 数据冗余存储 |
| 擦写寿命耗尽 | 擦写次数超 10 万 | 实现磨损均衡，频繁数据用 FRAM 替代 |

## 相关文档

- `../../templates/driver-template-spi.c` — SPI 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `../../guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `../../guides/debugging-testing.md` — 调试与测试规范
- `../../guides/pitfalls.md` — 跨类别常见问题与避坑指南
