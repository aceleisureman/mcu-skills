# SD 卡开发规范

## 1. 概述与选型指南

### 器件简介

SD 卡（Secure Digital Card）是基于闪存的大容量存储介质，支持 SPI 和 SDIO 两种接口模式。SD 卡体积小、容量大、使用方便，是 MCU 系统中最常用的大容量存储方案，广泛应用于数据记录、文件系统（FATFS）、固件 OTA 升级等场景。

### SD/SDHC/SDXC 区分

| 类型 | 容量范围 | 文件系统 | 寻址方式 | 说明 |
|------|----------|----------|----------|------|
| SDSC | 0~2GB | FAT16/FAT32 | 字节寻址 (CMD17/24) | 标准容量，已淘汰 |
| SDHC | 2GB~32GB | FAT32 | 块寻址 (CMD17/24, 512B块) | 高容量，最常用 |
| SDXC | 32GB~2TB | exFAT | 块寻址 | 超大容量，MCU支持有限 |
| SDUC | 2TB~128TB | exFAT | 块寻址 | 超极速，MCU不支持 |

### 接口模式对比

| 特性 | SPI 模式 | SDIO 模式 |
|------|----------|-----------|
| 接口线数 | 4 线 (CS/SCK/MOSI/MISO) | 6 线 (CMD/CLK/DAT0~DAT3) |
| 最大时钟 | 25MHz (默认) | 50MHz (高速) |
| 最大带宽 | ~3MB/s | ~50MB/s (4位总线) |
| 总线宽度 | 1 位 | 1 位 或 4 位 |
| MCU 支持 | 所有带 SPI 的 MCU | 需 SDIO 外设 |
| 复杂度 | 低 | 中~高 |
| 适用场景 | 低速数据记录 | 高速读写、音视频 |

### 常见 SD 卡模块对比

| 模块/接口 | 电压 | 模式 | 适用 | 备注 |
|-----------|------|------|------|------|
| MicroSD SPI 模块 | 3.3V/5V | SPI | STM32/ESP32/Arduino | 最常用，带电平转换 |
| MicroSD 原生座 | 3.3V | SDIO/SPI | 带 SDIO 外设的 MCU | 直接焊接 MicroSD 卡座 |
| 标准 SD 卡座 | 3.3V | SDIO/SPI | 开发板 | 体积大，已少用 |

### 选型决策树

```
需要 > 5MB/s 读写速度？ → 是 → MCU 有 SDIO 外设？ → 是 → SDIO 4位模式
                                                   否 → SPI 模式 (限速)
仅做低速数据记录？ → 是 → SPI 模式 (最简单)
需要文件系统？ → 是 → 移植 FatFs (支持 SPI/SDIO)
容量需求？
  ≤ 2GB  → SDSC (字节寻址, 兼容性好)
  ≤ 32GB → SDHC (块寻址, 推荐)
  > 32GB → SDXC (需 exFAT, MCU 支持有限)
通用推荐 → SDHC 8GB/16GB + SPI 模式 + FatFs
```

### 适用场景

- **SPI 模式**：数据记录仪、配置文件存储、固件升级（低速场景）
- **SDIO 4 位模式**：音频播放、图像存储、视频录制（高速场景）
- **FatFs 文件系统**：与 PC 互通数据、多文件管理

## 2. 硬件设计规范

### SPI 模式电路

```
VCC(3.3V)
  │
  ├── 10kΩ ── CS  ── MCU CS   (上拉, 防止悬空选中)
  │
  │     ┌──────────────────────────┐
  │     │      MicroSD 卡座        │
  │     │                          │
  VCC ──┤ VCC(4)                   │
        │                          │
  MCU CS   ── CS/DAT3(1) ── 10kΩ── VCC
  MCU MOSI ── CMD/DI(2)  ── 10kΩ── VCC
  GND      ── GND(3)              │
  VCC      ── VCC(4)              │
  MCU SCK  ── CLK/SCK(5)          │
  GND      ── GND(6)              │
  MCU MISO ── DAT0/DO(7) ── 10kΩ── VCC
        │                          │
        │ DAT1(8) ── NC (SPI不使用) │
        │ DAT2(9) ── NC (SPI不使用) │
        └──────────────────────────┘

  VCC ── 10μF ── GND  (电源滤波)
  VCC ── 100nF ── GND (高频去耦)
```

**MicroSD 卡座引脚定义**：

| 引脚 | SPI 模式 | SDIO 模式 | 上拉电阻 |
|------|----------|-----------|----------|
| 1 | CS (DAT3) | DAT3 | 10kΩ |
| 2 | DI (CMD) | CMD | 10kΩ |
| 3 | GND | GND | - |
| 4 | VCC | VCC | - |
| 5 | SCK (CLK) | CLK | - |
| 6 | GND | GND | - |
| 7 | DO (DAT0) | DAT0 | 10kΩ |
| 8 | NC (DAT1) | DAT1 | 10kΩ (SDIO) |
| 9 | NC (DAT2) | DAT2 | 10kΩ (SDIO) |

### SDIO 模式电路

```
VCC(3.3V) ── 10μF + 100nF ── GND  (电源滤波)

        ┌──────────────────────────┐
        │      MicroSD 卡座        │
        │                          │
MCU DAT3── CS/DAT3(1) ── 10kΩ ── VCC
MCU CMD ── CMD/DI(2)  ── 10kΩ ── VCC
GND     ── GND(3)
VCC     ── VCC(4)
MCU CLK ── CLK/SCK(5)
GND     ── GND(6)
MCU DAT0── DAT0/DO(7) ── 10kΩ ── VCC
MCU DAT1── DAT1(8)    ── 10kΩ ── VCC
MCU DAT2── DAT2(9)    ── 10kΩ ── VCC
        └──────────────────────────┘

  SDIO 模式: 4位数据总线, 所有 DAT 线和 CMD 线需上拉
```

### PCB 设计要点

- **电源滤波**：VCC 旁放置 10μF + 100nF，SD 卡瞬时电流可达 100mA+
- **上拉电阻**：SPI 模式 DAT0/CS/CMD 各接 10kΩ 上拉；SDIO 模式所有 DAT 和 CMD 线接 10kΩ
- **信号完整性**：SDIO 高速模式（50MHz）走线需等长，阻抗控制 50Ω
- **卡座选择**：推荐带卡检测（CD）和写保护（WP）开关的卡座
- **ESD 保护**：暴露在外的 SD 卡接口需加 TVS 阵列（如 USBLC6-2）
- **走线长度**：SPI 模式 < 10cm，SDIO 高速模式 < 5cm

### 电气参数限制

| 参数 | SPI 模式 | SDIO 模式 |
|------|----------|-----------|
| 工作电压 | 2.7~3.6V | 2.7~3.6V |
| 默认时钟 | ≤ 400kHz | ≤ 400kHz |
| 高速时钟 | ≤ 25MHz | ≤ 50MHz |
| 最大工作电流 | 100mA | 200mA (4位) |
| 最大待机电流 | 0.5mA | 0.5mA |

## 3. 驱动开发规范

### SD 卡 SPI 命令表

| 命令 | 编号 | 参数 | 响应 | 功能 |
|------|------|------|------|------|
| CMD0 | 0 | 0x00000000 | R1 | 软件复位，进入 SPI 模式 |
| CMD8 | 8 | 0x000001AA | R7 | 检查电压范围 (SD v2.0+) |
| CMD9 | 9 | 0x00000000 | R1 | 读取 CSD 寄存器 |
| CMD10 | 10 | 0x00000000 | R1 | 读取 CID 寄存器 |
| CMD16 | 16 | 0x00000200 | R1 | 设置块大小为 512 字节 |
| CMD17 | 17 | 块地址 | R1 | 读取单块 |
| CMD24 | 24 | 块地址 | R1 | 写入单块 |
| CMD55 | 55 | 0x00000000 | R1 | ACMD 前缀命令 |
| ACMD41 | 41 | 0x40000000 | R1 | 初始化 (HCS 位) |
| CMD58 | 58 | 0x00000000 | R3 | 读取 OCR 寄存器 |
| CMD13 | 13 | 0x00000000 | R2 | 读取卡状态 |

### R1 响应标志位

```
R1 响应字节 (Bit 定义):
  Bit 7: 0  - 始终为 0
  Bit 6: 参数错误
  Bit 5: 地址错误
  Bit 4: 擦除序列错误
  Bit 3: CRC 错误
  Bit 2: 非法命令
  Bit 1: 擦除复位
  Bit 0: 闲状态 (idle=1, 表示正在初始化)
  
R1 = 0x00: 正常 (初始化完成)
R1 = 0x01: 空闲状态 (初始化中)
R1 = 0x05: 空闲 + 非法命令 (CMD8 不支持, SD v1.x)
```

### 驱动接口定义

```c
typedef struct {
    spi_bus_t *spi;
    gpio_pin_t cs_pin;
    uint8_t card_type;    /* 卡类型: SD v1 / SD v2 / SDHC */
    uint32_t block_count; /* 总块数 */
    uint32_t block_size;  /* 块大小 (512) */
} sd_card_handle_t;

typedef enum {
    SD_OK = 0,
    SD_ERR_NO_CARD,
    SD_ERR_TIMEOUT,
    SD_ERR_CMD_RESPONSE,
    SD_ERR_INIT,
    SD_ERR_WRITE_FAIL,
    SD_ERR_READ_FAIL,
    SD_ERR_CRC,
    SD_ERR_INVALID_PARAM,
} sd_status_t;

typedef enum {
    SD_TYPE_UNKNOWN = 0,
    SD_TYPE_V1,       /* SDSC v1.x, 字节寻址 */
    SD_TYPE_V2,       /* SDSC v2.0, 字节寻址 */
    SD_TYPE_SDHC,     /* SDHC/SDXC, 块寻址 */
} sd_card_type_t;

sd_status_t sd_init(sd_card_handle_t *h, spi_bus_t *spi, gpio_pin_t cs);
sd_status_t sd_read_block(sd_card_handle_t *h, uint32_t block, uint8_t *buf);
sd_status_t sd_write_block(sd_card_handle_t *h, uint32_t block, const uint8_t *buf);
sd_status_t sd_read_multi(sd_card_handle_t *h, uint32_t block, uint8_t *buf, uint32_t count);
sd_status_t sd_write_multi(sd_card_handle_t *h, uint32_t block, const uint8_t *buf, uint32_t count);
```

### SD 卡 SPI 模式初始化代码

```c
/* === 底层 SPI 传输 === */

static void sd_cs_low(sd_card_handle_t *h) { gpio_write(h->cs_pin, 0); }
static void sd_cs_high(sd_card_handle_t *h) { gpio_write(h->cs_pin, 1); }

static uint8_t sd_spi_xfer(sd_card_handle_t *h, uint8_t data) {
    uint8_t rx;
    spi_transfer(h->spi, &data, &rx, 1);
    return rx;
}

/* 发送 8 个时钟 (保持 CS 高) */
static void sd_send_dummy_clocks(sd_card_handle_t *h, int count) {
    sd_cs_high(h);
    for (int i = 0; i < count; i++) sd_spi_xfer(h, 0xFF);
}

/* === 发送命令并接收 R1 响应 === */
static uint8_t sd_send_cmd(sd_card_handle_t *h, uint8_t cmd,
                            uint32_t arg, uint8_t crc) {
    uint8_t r1;
    uint8_t retry = 0;

    /* CMD0 和 CMD8 需要正确 CRC, 其他命令 CRC 可用 0x95/0xFF */
    sd_cs_low(h);

    /* 等待卡就绪 (非忙) */
    if (cmd != CMD0 && cmd != ACMD41) {
        do {
            r1 = sd_spi_xfer(h, 0xFF);
            if (++retry > 100) break;
        } while (r1 != 0xFF);
    }

    /* 发送命令帧: 01 + CMD(6bit) + ARG(32bit) + CRC(7bit) + 1 */
    sd_spi_xfer(h, 0x40 | cmd);
    sd_spi_xfer(h, (arg >> 24) & 0xFF);
    sd_spi_xfer(h, (arg >> 16) & 0xFF);
    sd_spi_xfer(h, (arg >> 8) & 0xFF);
    sd_spi_xfer(h, arg & 0xFF);
    sd_spi_xfer(h, crc);

    /* CMD12 (STOP_TRANSMISSION) 需要额外读一个字节 */
    if (cmd == 12) sd_spi_xfer(h, 0xFF);

    /* 等待 R1 响应 (最多 10 次重试) */
    retry = 0;
    do {
        r1 = sd_spi_xfer(h, 0xFF);
    } while ((r1 & 0x80) && (++retry < 10));

    return r1;
}

/* === SD 卡初始化 === */
sd_status_t sd_init(sd_card_handle_t *h, spi_bus_t *spi, gpio_pin_t cs) {
    h->spi = spi;
    h->cs_pin = cs;
    h->card_type = SD_TYPE_UNKNOWN;

    /* 1. SPI 时钟降至 100~400kHz */
    spi_set_clock(spi, 400000);  /* 400kHz */

    /* 2. 发送 80+ 个时钟 (CS 高), 唤醒 SD 卡 */
    sd_send_dummy_clocks(h, 10);  /* 10 x 8 = 80 clocks */

    /* 3. CMD0: 软件复位, 进入 SPI 模式 */
    sd_cs_low(h);
    uint8_t r1 = sd_send_cmd(h, 0, 0x00000000, 0x95);
    if (r1 != 0x01) {  /* 期望 R1=0x01 (idle) */
        sd_cs_high(h);
        return SD_ERR_INIT;
    }

    /* 4. CMD8: 检查电压范围 (区分 SD v1.x 和 v2.0) */
    r1 = sd_send_cmd(h, 8, 0x000001AA, 0x87);
    if (r1 == 0x05) {
        /* SD v1.x: 不支持 CMD8 */
        h->card_type = SD_TYPE_V1;
    } else if (r1 == 0x01) {
        /* SD v2.0+: 读取 CMD8 响应剩余 4 字节 */
        uint8_t ocr[4];
        for (int i = 0; i < 4; i++) ocr[i] = sd_spi_xfer(h, 0xFF);
        if (ocr[2] != 0x01 || ocr[3] != 0xAA) {
            sd_cs_high(h);
            return SD_ERR_INIT;  /* 电压不兼容 */
        }
        h->card_type = SD_TYPE_V2;
    } else {
        sd_cs_high(h);
        return SD_ERR_INIT;
    }
    sd_cs_high(h);

    /* 5. CMD55 + ACMD41: 初始化 (启动卡) */
    uint32_t retry = 0;
    do {
        sd_cs_low(h);
        sd_send_cmd(h, 55, 0x00000000, 0xFF);
        /* SDHC: HCS=1 (bit30); SDSC: HCS=0 */
        uint32_t acmd_arg = (h->card_type == SD_TYPE_V2) ? 0x40000000 : 0x00000000;
        r1 = sd_send_cmd(h, 41, acmd_arg, 0xFF);
        sd_cs_high(h);
        if (++retry > 1000) return SD_ERR_INIT;  /* 超时 */
    } while (r1 != 0x00);  /* 等待退出 idle 状态 */

    /* 6. CMD58: 读取 OCR, 确认 SDHC/SDSC */
    if (h->card_type == SD_TYPE_V2) {
        sd_cs_low(h);
        r1 = sd_send_cmd(h, 58, 0x00000000, 0xFF);
        if (r1 != 0x00) {
            sd_cs_high(h);
            return SD_ERR_INIT;
        }
        uint8_t ocr[4];
        for (int i = 0; i < 4; i++) ocr[i] = sd_spi_xfer(h, 0xFF);
        sd_cs_high(h);

        /* Bit30 (CCS) = 1 → SDHC/SDXC */
        if (ocr[0] & 0x40) {
            h->card_type = SD_TYPE_SDHC;
        }
    }

    /* 7. 提高 SPI 时钟至正常速度 */
    spi_set_clock(spi, 12000000);  /* 12MHz */

    /* 8. 设置块大小 512 字节 (CMD16) */
    sd_cs_low(h);
    r1 = sd_send_cmd(h, 16, 0x00000200, 0xFF);
    sd_cs_high(h);
    if (r1 != 0x00) return SD_ERR_INIT;

    h->block_size = 512;
    return SD_OK;
}
```

### 扇区读写代码

```c
/* === 读取单个扇区 (512 字节) === */
sd_status_t sd_read_block(sd_card_handle_t *h, uint32_t block, uint8_t *buf) {
    /* SDHC 使用块地址, SDSC 使用字节地址 */
    uint32_t addr = (h->card_type == SD_TYPE_SDHC) ? block : block * 512;

    sd_cs_low(h);

    /* CMD17: READ_SINGLE_BLOCK */
    uint8_t r1 = sd_send_cmd(h, 17, addr, 0xFF);
    if (r1 != 0x00) {
        sd_cs_high(h);
        return SD_ERR_READ_FAIL;
    }

    /* 等待数据起始令牌 0xFE */
    uint16_t retry = 0;
    uint8_t token;
    do {
        token = sd_spi_xfer(h, 0xFF);
        if (++retry > 5000) {
            sd_cs_high(h);
            return SD_ERR_TIMEOUT;
        }
    } while (token != 0xFE);

    /* 读取 512 字节数据 */
    for (int i = 0; i < 512; i++) {
        buf[i] = sd_spi_xfer(h, 0xFF);
    }

    /* 读取 2 字节 CRC (丢弃) */
    sd_spi_xfer(h, 0xFF);
    sd_spi_xfer(h, 0xFF);

    sd_cs_high(h);
    return SD_OK;
}

/* === 写入单个扇区 (512 字节) === */
sd_status_t sd_write_block(sd_card_handle_t *h, uint32_t block,
                            const uint8_t *buf) {
    uint32_t addr = (h->card_type == SD_TYPE_SDHC) ? block : block * 512;

    sd_cs_low(h);

    /* CMD24: WRITE_BLOCK */
    uint8_t r1 = sd_send_cmd(h, 24, addr, 0xFF);
    if (r1 != 0x00) {
        sd_cs_high(h);
        return SD_ERR_WRITE_FAIL;
    }

    /* 发送数据起始令牌 0xFE + 512 字节数据 + 2 字节 CRC */
    sd_spi_xfer(h, 0xFE);  /* 数据起始令牌 */
    for (int i = 0; i < 512; i++) {
        sd_spi_xfer(h, buf[i]);
    }
    sd_spi_xfer(h, 0xFF);  /* CRC 字节 1 (假) */
    sd_spi_xfer(h, 0xFF);  /* CRC 字节 2 (假) */

    /* 读取数据响应令牌: xxx0SSS1 (SSS=010 表示接受) */
    uint8_t resp;
    uint16_t retry = 0;
    do {
        resp = sd_spi_xfer(h, 0xFF);
        if (++retry > 100) {
            sd_cs_high(h);
            return SD_ERR_WRITE_FAIL;
        }
    } while ((resp & 0x1F) != 0x05);  /* 0x05 = 数据被接受 */

    /* 等待写入完成 (卡忙时返回 0x00) */
    retry = 0;
    while (sd_spi_xfer(h, 0xFF) == 0x00) {
        if (++retry > 50000) {  /* 最大 500ms */
            sd_cs_high(h);
            return SD_ERR_TIMEOUT;
        }
    }

    sd_cs_high(h);
    return SD_OK;
}

/* === 读取 CSD 寄存器 (获取卡容量) === */
sd_status_t sd_read_csd(sd_card_handle_t *h, uint8_t *csd) {
    sd_cs_low(h);
    uint8_t r1 = sd_send_cmd(h, 9, 0x00000000, 0xFF);
    if (r1 != 0x00) {
        sd_cs_high(h);
        return SD_ERR_CMD_RESPONSE;
    }

    /* 等待数据起始令牌 0xFE */
    uint16_t retry = 0;
    while (sd_spi_xfer(h, 0xFF) != 0xFE) {
        if (++retry > 1000) {
            sd_cs_high(h);
            return SD_ERR_TIMEOUT;
        }
    }

    /* 读取 16 字节 CSD */
    for (int i = 0; i < 16; i++) csd[i] = sd_spi_xfer(h, 0xFF);

    /* CRC */
    sd_spi_xfer(h, 0xFF);
    sd_spi_xfer(h, 0xFF);

    sd_cs_high(h);

    /* 计算容量 */
    if ((csd[0] >> 6) == 0) {
        /* CSD v1.0 (SDSC) */
        uint32_t c_size = ((csd[6] & 0x03) << 10) | (csd[7] << 2) |
                          (csd[8] >> 6);
        uint32_t c_mult = ((csd[9] & 0x03) << 1) | (csd[10] >> 7);
        uint32_t read_bl_len = csd[5] & 0x0F;
        h->block_count = (c_size + 1) * (1 << (c_mult + 2)) *
                         (1 << (read_bl_len - 9));
    } else {
        /* CSD v2.0 (SDHC/SDXC) */
        uint32_t c_size = ((csd[7] & 0x3F) << 16) | (csd[8] << 8) | csd[9];
        h->block_count = (c_size + 1) * 1024;
    }

    return SD_OK;
}
```

## 4. 调试与测试规范

### 硬件验证清单

- [ ] 万用表确认 VCC 电压 2.7~3.6V（3.3V 典型）
- [ ] CS、MOSI、MISO、SCK 四线连接正确
- [ ] DAT0、CS、CMD 上拉电阻（10kΩ）已安装
- [ ] 电源滤波电容 10μF + 100nF 已贴装
- [ ] 卡检测开关（CD）引脚已连接（如使用带 CD 的卡座）
- [ ] 示波器检查 SPI 波形：初始化阶段时钟 ≤ 400kHz

### 通信验证

```c
/* SD 卡连接验证 */
sd_status_t sd_verify(sd_card_handle_t *h) {
    /* 读取 CID 寄存器 (卡身份信息) */
    sd_cs_low(h);
    uint8_t r1 = sd_send_cmd(h, 10, 0x00000000, 0xFF);
    if (r1 != 0x00) {
        sd_cs_high(h);
        printf("SD card not responding\n");
        return SD_ERR_NO_CARD;
    }

    /* 等待数据令牌 */
    while (sd_spi_xfer(h, 0xFF) != 0xFE);
    uint8_t cid[16];
    for (int i = 0; i < 16; i++) cid[i] = sd_spi_xfer(h, 0xFF);
    sd_spi_xfer(h, 0xFF);
    sd_spi_xfer(h, 0xFF);
    sd_cs_high(h);

    /* 解析 CID */
    printf("SD Card Info:\n");
    printf("  Manufacturer ID: 0x%02X\n", cid[0]);
    printf("  OEM ID: %c%c\n", cid[1], cid[2]);
    printf("  Product: %c%c%c%c%c\n", cid[3], cid[4], cid[5], cid[6], cid[7]);
    printf("  Serial: %08X\n", (cid[9]<<24)|(cid[10]<<16)|(cid[11]<<8)|cid[12]);
    printf("  Type: %s\n",
           h->card_type == SD_TYPE_SDHC ? "SDHC/SDXC" :
           h->card_type == SD_TYPE_V2  ? "SD v2.0 (SDSC)" : "SD v1.x");
    printf("  Capacity: %lu MB\n", h->block_count / 2048);
    return SD_OK;
}
```

### 数据校验

```c
/* 读写回环测试 */
sd_status_t sd_rw_test(sd_card_handle_t *h) {
    uint8_t write_buf[512], read_buf[512];
    uint32_t test_block = 0;  /* 使用第 0 块测试 */

    /* 准备测试数据 */
    for (int i = 0; i < 512; i++) write_buf[i] = (uint8_t)(i ^ 0x5A);

    /* 写入 */
    sd_status_t st = sd_write_block(h, test_block, write_buf);
    if (st != SD_OK) { printf("Write failed: %d\n", st); return st; }

    /* 读回 */
    st = sd_read_block(h, test_block, read_buf);
    if (st != SD_OK) { printf("Read failed: %d\n", st); return st; }

    /* 校验 */
    for (int i = 0; i < 512; i++) {
        if (write_buf[i] != read_buf[i]) {
            printf("Mismatch at offset %d: wrote 0x%02X, read 0x%02X\n",
                   i, write_buf[i], read_buf[i]);
            return SD_ERR_CRC;
        }
    }
    printf("SD card RW test PASSED\n");
    return SD_OK;
}
```

### 性能测试指标

| 指标 | SPI 模式 (12MHz) | SDIO 1-bit (25MHz) | SDIO 4-bit (50MHz) |
|------|-------------------|---------------------|---------------------|
| 读取速度 | ~1.0 MB/s | ~2.5 MB/s | ~10 MB/s |
| 写入速度 | ~0.7 MB/s | ~1.5 MB/s | ~5 MB/s |
| 初始化时间 | ~500ms | ~300ms | ~300ms |
| 单扇区读取延迟 | ~2ms | ~1ms | ~0.5ms |
| 单扇区写入延迟 | ~5ms | ~3ms | ~2ms |

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| CMD0 返回 0xFF | SPI 接线错误或卡未插入 | 检查 CS/MOSI/MISO/SCK 接线 |
| CMD0 返回非 0x01 | 初始时钟过高 | 初始化时 SPI 时钟必须 ≤ 400kHz |
| ACMD41 一直返回 0x01 | 初始化未完成 | 增加重试次数（>1000），检查 CMD55 前缀 |
| ACMD41 超时 | 卡不兼容或损坏 | 检查卡类型，更换品牌卡测试 |
| SDHC 卡地址计算错误 | 使用了字节地址 | SDHC 使用块地址，不需乘 512 |
| 写入失败返回 0x0C | 块未对齐或卡满 | 检查块地址是否在有效范围内 |
| 写入后读取数据不对 | 写入未完成就读取 | 等待忙标志清除（返回 0xFF） |
| FatFs 挂载失败 FR_DISK_ERR | 底层驱动错误 | 检查 sd_read_block 返回值 |
| FatFs 挂载失败 FR_NO_FILESYS | 卡未格式化 | 用 PC 格式化为 FAT32 |
| 读取速度很慢 | SPI 时钟未提升 | 初始化后提高 SPI 时钟到 12~25MHz |
| 偶发读取错误 | 电源不稳或线长 | 加大电源滤波电容，缩短走线 |
| 高温下卡掉线 | 卡过热 | 改善散热，使用工业级 SD 卡 |
| 卡寿命短 | 频繁擦写 | 实现磨损均衡，使用工业级卡 |

## 相关文档

- `skill://mcu-driver-core/templates/driver-template-spi.c` — SPI 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
- `skill://mcu-driver-core/guides/pitfalls.md` — 跨类别常见问题与避坑指南
