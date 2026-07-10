# NFC/RFID 模块开发规范

## 1. 概述与选型指南

NFC/RFID 模块工作在 13.56MHz 频段，用于非接触式智能卡读写和近场通信。按协议分为 ISO14443（Mifare 系列）、ISO15693、FeliCa 等。应用包括门禁系统、公交卡读写、电子钱包、NFC 标签等。

### 常见型号对比

| 型号 | 厂商 | 接口 | 协议 | 卡类型 | 供电 | 特点 | 价格 |
|------|------|------|------|--------|------|------|------|
| RC522 | NXP | SPI | ISO14443A | Mifare 1K/4K | 2.5~3.3V | 经典廉价 | ¥3~8 |
| MFRC522 | NXP | SPI/I2C/UART | ISO14443A/B | Mifare 全系列 | 2.5~3.3V | 原厂 | ¥5~12 |
| PN532 | NXP | I2C/SPI/UART | ISO14443A/B,FeliCa,ISO18092 | Mifare,FeliCa,NFC P2P | 2.7~5V | 多协议NFC | ¥15~30 |
| PN5180 | NXP | SPI | ISO14443A/B,ISO15693 | Mifare+,ICODE | 2.7~5V | 高功率 | ¥25~50 |
| ST25R3911B | ST | SPI | 多协议 | 全系列 | 2.4~5.5V | 高性能 | ¥20~40 |

### 选型决策树

```
仅需读 Mifare 卡？ → 是 → RC522 (SPI, 廉价)
需要 NFC P2P？ → 是 → PN532 (支持 ISO18092)
需要 ISO15693？ → 是 → PN5180 / ST25R3911B
需要手机 NFC 交互？ → 是 → PN532 (读写/卡模拟/P2P)
通用推荐 → RC522 (简单) / PN532 (NFC)
```

### 适用场景

- **RC522**：门禁卡读写、Mifare 卡应用、教学实验
- **PN532**：NFC 交互、手机通信、卡模拟、多协议读写
- **PN5180**：高性能读卡器、ICODE 标签

---

## 2. 硬件设计规范

### RC522 引脚与电路

```
RC522 模块引脚:
         ┌──────────────┐
   SDA ─┤1 (CS/SS)    │
   SCK ─┤2            │
   MOSI┤3     RC522  │
   MISO┤4            │
   IRQ ─┤5            │
   GND ─┤6            │
   RST ─┤7            │
  3.3V ─┤8            │
         └──────────────┘
```

```
典型电路 (RC522 + STM32):

 3.3V ──┬── 10μF ──┬── 100nF ──┬── VCC(8)
        │           │           │
        │    STM32 SPI1:        │
        │    PA5(SCK)  ────────┼── SCK(2)
        │    PA7(MOSI) ────────┼── MOSI(3)
        │    PA6(MISO) ────────┼── MISO(4)
        │    PA4(CS)   ────────┼── SDA/CS(1)
        │                        │
        │    PB0(IRQ)  ─────────┼── IRQ(5)
        │    PB1(RST)  ─────────┼── RST(7)
 GND ───┴────────────────────────┘
```

**要点**：
- RC522 供电 3.3V，不可接 5V（会永久损坏）
- SPI 时钟最高 10MHz，建议 1~8MHz
- IRQ 用于卡片检测中断通知
- VCC 旁放 10μF + 100nF 去耦电容
- 天线区域避免金属遮挡

### PN532 电路 (I2C 模式)

```
 3.3V ──┬── 10μF ──┬── 100nF ──┬── VCC(1)
        │           │           │
        │    STM32 I2C:        │
        │    PB6(SCL) ─ 4.7kΩ ─┼── SCL(9)
        │    PB7(SDA) ─ 4.7kΩ ─┼── SDA(10)
        │    PB0(IRQ) ──────────┼── IRQ(7)
        │    PB1(RST) ──────────┼── RST(8)
        │    SEL0 ── GND (I2C)  │
        │    SEL1 ── VCC         │
 GND ───┴────────────────────────┘
```

### 天线匹配电路

```
RC522 天线匹配 (参考):
    TX1 ──┬── 1kΩ ──┬── EMC滤波 ──┬── 匹配 ── 天线
          │         │  (L+C)      │  22pF    线圈
    TX2 ──┬── 1kΩ ──┘            │
    RX  ──┬── 匹配电阻 ──────────┘
```

**天线设计要点**：
- 13.56MHz 天线为矩形 PCB 线圈
- EMC 滤波器：串联电感 + 电容低通滤波
- 匹配电容使天线谐振在 13.56MHz
- Q 值 30~50（过高影响带宽）
- 天线区域禁止铺铜

### 电源设计关键参数

| 参数 | RC522 | PN532 | PN5180 | ST25R3911B |
|------|-------|-------|--------|------------|
| 工作电压 | 2.5~3.3V | 2.7~5V | 2.7~5V | 2.4~5.5V |
| 工作电流 | 13~26mA | 50~100mA | 80~120mA | 60~100mA |
| 待机电流 | 10μA | 15μA | 5μA | 5μA |
| 读取距离 | ~3cm | ~5cm | ~7cm | ~6cm |

---

## 3. 驱动开发规范

### Mifare 卡通信流程

```
ISO14443A 通信流程:
1. REQA (0x26) → 卡返回 ATQA
2. Anticollision (0x93) → 读取 UID(4B) + BCC(1B)
3. Select (0x93) → 选中卡片, 返回 SAK
4. Authenticate (0x60/0x61) → 密钥验证 KeyA/KeyB
5. Read (0x30) / Write (0xA0) → 读写 16 字节数据块
6. Halt (0x50) → 休眠卡片

Mifare 1K 存储结构:
扇区 0~15, 每扇区 4 块:
┌──────────┬──────────┬──────────┬──────────────┐
│ Block 0  │ Block 1  │ Block 2  │ Block 3(尾块) │
│ 16 字节   │ 16 字节   │ 16 字节   │ KeyA+Ctrl+KeyB│
└──────────┴──────────┴──────────┴──────────────┘
扇区0 Block0: 厂商数据(含UID), 不可写
```

### 统一接口定义

```c
typedef enum {
    NFC_OK = 0, NFC_ERR_NO_CARD, NFC_ERR_TIMEOUT,
    NFC_ERR_CRC, NFC_ERR_AUTH, NFC_ERR_READ,
    NFC_ERR_WRITE, NFC_ERR_SPI, NFC_ERR_INVALID_PARAM,
} nfc_status_t;

typedef enum { NFC_KEY_A = 0x60, NFC_KEY_B = 0x61 } nfc_key_type_t;

typedef struct {
    uint8_t uid[10]; uint8_t uid_len; uint8_t sak;
    uint16_t atqa; uint8_t type;
} nfc_card_t;

nfc_status_t nfc_init(void);
nfc_status_t nfc_request_card(nfc_card_t *card);
nfc_status_t nfc_anticollision(nfc_card_t *card);
nfc_status_t nfc_select_card(const nfc_card_t *card);
nfc_status_t nfc_authenticate(uint8_t block, nfc_key_type_t type, const uint8_t *key, const uint8_t *uid);
nfc_status_t nfc_read_block(uint8_t block, uint8_t *data);
nfc_status_t nfc_write_block(uint8_t block, const uint8_t *data);
void nfc_halt(void);
```

### RC522 SPI 寄存器驱动

```c
/* RC522 SPI 驱动核心
 * 硬件: STM32 + RC522 (SPI)
 */

#define REG_COMMAND      0x01
#define REG_COMM_IRQ     0x04
#define REG_ERROR         0x06
#define REG_FIFO_DATA    0x09
#define REG_FIFO_LEVEL   0x0A
#define REG_BIT_FRAMING  0x0D
#define REG_TX_MODE      0x12
#define REG_RX_MODE      0x13
#define REG_TX_CONTROL   0x14
#define REG_T_MODE       0x2A
#define REG_T_PRESCALE   0x2B
#define REG_T_RELOAD_H   0x2C
#define REG_T_RELOAD_L   0x2D
#define REG_VERSION      0x37

#define CMD_IDLE        0x00
#define CMD_AUTHENT     0x0E
#define CMD_TRANSCEIVE  0x0C
#define CMD_RESET       0x0F

#define PICC_REQA       0x26
#define PICC_ANTICOLL   0x93
#define PICC_HALT       0x50
#define PICC_READ       0x30
#define PICC_WRITE      0xA0

static spi_handle_t *nfc_spi;

static uint8_t rc522_read_reg(uint8_t addr) {
    uint8_t tx[2] = {0x80 | (addr & 0x3F), 0x00};
    uint8_t rx[2] = {0};
    spi_cs_low(nfc_spi);
    spi_transfer(nfc_spi, tx, rx, 2);
    spi_cs_high(nfc_spi);
    return rx[1];
}

static void rc522_write_reg(uint8_t addr, uint8_t val) {
    uint8_t tx[2] = {(addr & 0x3F), val};
    spi_cs_low(nfc_spi);
    spi_transfer(nfc_spi, tx, NULL, 2);
    spi_cs_high(nfc_spi);
}

static void rc522_set_bit(uint8_t addr, uint8_t mask) {
    rc522_write_reg(addr, rc522_read_reg(addr) | mask);
}

static void rc522_clear_bit(uint8_t addr, uint8_t mask) {
    rc522_write_reg(addr, rc522_read_reg(addr) & ~mask);
}

void nfc_antenna_on(void)  { rc522_set_bit(REG_TX_CONTROL, 0x03); }
void nfc_antenna_off(void) { rc522_clear_bit(REG_TX_CONTROL, 0x03); }

/* 初始化 */
nfc_status_t nfc_init(void) {
    nfc_spi = spi_get_handle(SPI1);

    gpio_set_low(RST_PIN); sys_delay_ms(10);
    gpio_set_high(RST_PIN); sys_delay_ms(50);

    rc522_write_reg(REG_COMMAND, CMD_RESET);
    sys_delay_ms(50);

    /* 定时器 ~25ms */
    rc522_write_reg(REG_T_MODE, 0x8D);
    rc522_write_reg(REG_T_PRESCALE, 0x3E);
    rc522_write_reg(REG_T_RELOAD_H, 0x30);
    rc522_write_reg(REG_T_RELOAD_L, 0x00);

    /* CRC 使能, 106kbps */
    rc522_write_reg(REG_TX_MODE, 0x00);
    rc522_write_reg(REG_RX_MODE, 0x08);
    rc522_write_reg(0x26, 0x60);  /* RxGain */

    nfc_antenna_on();

    uint8_t ver = rc522_read_reg(REG_VERSION);
    if (ver != 0x91 && ver != 0x92) return NFC_ERR_SPI;
    return NFC_OK;
}

/* 与卡片通信 */
static nfc_status_t rc522_comm(uint8_t cmd, const uint8_t *send, uint8_t slen,
                                uint8_t *recv, uint8_t *rlen) {
    uint8_t irq_en = 0, wait_irq = 0;
    if (cmd == CMD_TRANSCEIVE)      { irq_en = 0x77; wait_irq = 0x30; }
    else if (cmd == CMD_AUTHENT)    { irq_en = 0x12; wait_irq = 0x10; }

    rc522_write_reg(0x02, irq_en | 0x80);
    rc522_write_reg(REG_COMM_IRQ, 0x7F);
    rc522_set_bit(REG_FIFO_LEVEL, 0x80);  /* 清 FIFO */

    for (int i = 0; i < slen; i++)
        rc522_write_reg(REG_FIFO_DATA, send[i]);

    rc522_write_reg(REG_COMMAND, cmd);
    if (cmd == CMD_TRANSCEIVE) rc522_set_bit(REG_BIT_FRAMING, 0x80);

    uint32_t start = sys_get_tick();
    uint8_t n;
    while ((sys_get_tick() - start) < 100) {
        n = rc522_read_reg(REG_COMM_IRQ);
        if (n & wait_irq) break;
        if (n & 0x01) { rc522_write_reg(REG_COMMAND, CMD_IDLE); return NFC_ERR_TIMEOUT; }
    }

    rc522_clear_bit(REG_BIT_FRAMING, 0x80);
    if (rc522_read_reg(REG_ERROR) & 0x1D) return NFC_ERR_CRC;

    if (cmd == CMD_TRANSCEIVE) {
        n = rc522_read_reg(REG_FIFO_LEVEL);
        if (n > *rlen) n = *rlen;
        for (int i = 0; i < n; i++) recv[i] = rc522_read_reg(REG_FIFO_DATA);
        *rlen = n;
        if (n == 0) return NFC_ERR_TIMEOUT;
    }
    return NFC_OK;
}

/* 寻卡 */
nfc_status_t nfc_request_card(nfc_card_t *card) {
    uint8_t reqa = PICC_REQA, atqa[2] = {0}, len = 2;
    rc522_write_reg(REG_BIT_FRAMING, 0x07);
    nfc_status_t st = rc522_comm(CMD_TRANSCEIVE, &reqa, 1, atqa, &len);
    rc522_write_reg(REG_BIT_FRAMING, 0x00);
    if (st != NFC_OK || len < 2) return NFC_ERR_NO_CARD;
    card->atqa = (atqa[1] << 8) | atqa[0];
    return NFC_OK;
}

/* 防冲突 (读 UID) */
nfc_status_t nfc_anticollision(nfc_card_t *card) {
    uint8_t send[2] = {PICC_ANTICOLL, 0x20}, recv[5] = {0}, len = 5;
    rc522_clear_bit(REG_TX_MODE, 0x80); rc522_clear_bit(REG_RX_MODE, 0x80);
    nfc_status_t st = rc522_comm(CMD_TRANSCEIVE, send, 2, recv, &len);
    rc522_set_bit(REG_TX_MODE, 0x80); rc522_set_bit(REG_RX_MODE, 0x80);
    if (st != NFC_OK) return st;
    if ((recv[0]^recv[1]^recv[2]^recv[3]) != recv[4]) return NFC_ERR_CRC;
    memcpy(card->uid, recv, 4);
    card->uid_len = 4;
    return NFC_OK;
}

/* 选卡 */
nfc_status_t nfc_select_card(const nfc_card_t *card) {
    uint8_t send[7] = {PICC_ANTICOLL, 0x70}, recv[3] = {0}, len = 3;
    memcpy(&send[2], card->uid, 4);
    send[6] = card->uid[0]^card->uid[1]^card->uid[2]^card->uid[3];
    if (rc522_comm(CMD_TRANSCEIVE, send, 7, recv, &len) != NFC_OK) return NFC_ERR_TIMEOUT;
    ((nfc_card_t *)card)->sak = recv[0];
    ((nfc_card_t *)card)->type = (recv[0]==0x08) ? 1 : (recv[0]==0x18) ? 2 : 0;
    return NFC_OK;
}

/* 密钥验证 */
nfc_status_t nfc_authenticate(uint8_t block, nfc_key_type_t type,
                                const uint8_t *key, const uint8_t *uid) {
    uint8_t send[12], recv[4] = {0}, len = 4;
    send[0] = type; send[1] = block;
    memcpy(&send[2], key, 6); memcpy(&send[8], uid, 4);
    return rc522_comm(CMD_AUTHENT, send, 12, recv, &len);
}

/* 读数据块 (16B) */
nfc_status_t nfc_read_block(uint8_t block, uint8_t *data) {
    uint8_t send[2] = {PICC_READ, block}, len = 16;
    if (rc522_comm(CMD_TRANSCEIVE, send, 2, data, &len) != NFC_OK) return NFC_ERR_READ;
    return (len == 16) ? NFC_OK : NFC_ERR_READ;
}

/* 写数据块 (16B) */
nfc_status_t nfc_write_block(uint8_t block, const uint8_t *data) {
    uint8_t cmd[2] = {PICC_WRITE, block}, recv[4] = {0}, len = 4;
    if (rc522_comm(CMD_TRANSCEIVE, cmd, 2, recv, &len) != NFC_OK) return NFC_ERR_WRITE;
    len = 4;
    if (rc522_comm(CMD_TRANSCEIVE, data, 16, recv, &len) != NFC_OK) return NFC_ERR_WRITE;
    return NFC_OK;
}

void nfc_halt(void) {
    uint8_t send[2] = {PICC_HALT, 0x00}, recv[4], len = 4;
    rc522_comm(CMD_TRANSCEIVE, send, 2, recv, &len);
}
```

### 完整读写流程

```c
static const uint8_t default_key[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

nfc_status_t nfc_read_mifare(uint8_t sector, uint8_t offset, uint8_t *data16) {
    nfc_card_t card;
    if (nfc_request_card(&card) != NFC_OK) return NFC_ERR_NO_CARD;
    if (nfc_anticollision(&card) != NFC_OK) return NFC_ERR_TIMEOUT;
    if (nfc_select_card(&card) != NFC_OK) return NFC_ERR_TIMEOUT;

    uint8_t block = sector * 4 + offset;
    if (nfc_authenticate(block, NFC_KEY_A, default_key, card.uid) != NFC_OK)
        return NFC_ERR_AUTH;

    nfc_status_t st = nfc_read_block(block, data16);
    nfc_halt();
    return st;
}

nfc_status_t nfc_write_mifare(uint8_t sector, uint8_t offset, const uint8_t *data16) {
    nfc_card_t card;
    if (nfc_request_card(&card) != NFC_OK) return NFC_ERR_NO_CARD;
    if (nfc_anticollision(&card) != NFC_OK) return NFC_ERR_TIMEOUT;
    if (nfc_select_card(&card) != NFC_OK) return NFC_ERR_TIMEOUT;

    uint8_t block = sector * 4 + offset;
    if (nfc_authenticate(block, NFC_KEY_A, default_key, card.uid) != NFC_OK)
        return NFC_ERR_AUTH;

    nfc_status_t st = nfc_write_block(block, data16);
    nfc_halt();
    return st;
}

void nfc_demo(void) {
    if (nfc_init() != NFC_OK) { printf("RC522 初始化失败\n"); return; }
    printf("等待卡片...\n");
    while (1) {
        uint8_t data[16];
        if (nfc_read_mifare(1, 0, data) == NFC_OK) {
            printf("读取: ");
            for (int i = 0; i < 16; i++) printf("%02X ", data[i]);
            printf("\n");
        }
        sys_delay_ms(500);
    }
}
```

---

## 4. 调试与测试规范

### 硬件验证清单

- [ ] 万用表测量 VCC 电压 3.3V（RC522 不可接 5V）
- [ ] SPI 通信正常（示波器检查 SCK/MOSI/MISO/CS）
- [ ] 读取版本寄存器确认 0x91/0x92
- [ ] RST 引脚受 MCU 控制
- [ ] VCC 旁放 10μF + 100nF 去耦
- [ ] 天线区域无金属遮挡

### 通信验证

1. **版本检测**：读取 `VersionReg`（0x37），返回 0x91/0x92
2. **寻卡测试**：放置 Mifare 卡，REQA 收到 ATQA
3. **UID 读取**：防冲突后 BCC 校验通过
4. **密钥验证**：默认密钥 FF×6 验证
5. **数据读写**：写入已知数据后读回比对

### 性能测试指标

| 指标 | RC522 | PN532 | PN5180 |
|------|-------|-------|--------|
| 读取距离 | ~3cm | ~5cm | ~7cm |
| 寻卡时间 | <50ms | <100ms | <50ms |
| 读取16字节 | ~10ms | ~15ms | ~10ms |
| 写入16字节 | ~15ms | ~20ms | ~15ms |
| SPI 时钟 | ≤10MHz | ≤5MHz | ≤8MHz |
| 工作电流 | 13~26mA | 50~100mA | 80~120mA |

### 调试工具

- **逻辑分析仪**：分析 SPI 时序和数据
- **NXP TagInfo APP**：手机验证卡片内容
- **Mifare Classic Tool**：Android 读写 Mifare 卡
- **NFC Tools APP**：NFC 标签读写验证

---

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 版本寄存器读 0x00 | SPI 通信异常 | 检查 SPI 接线，CPOL=0/CPHA=0 |
| 版本寄存器读 0xFF | CS 未拉低 | 确认 CS 片选时序 |
| 寻卡无响应 | 天线未开启 | 调用 nfc_antenna_on() |
| 寻卡无响应 | 无卡在场 | 确认 Mifare 卡在 3cm 内 |
| 寻卡无响应 | CRC 未关闭 | 防冲突阶段关闭 CRC |
| BCC 校验失败 | 防冲突时序错误 | 确认 7-bit 发送模式 (REQA) |
| 认证失败 | 密钥不匹配 | 使用正确密钥，默认 FF×6 |
| 认证失败 | 认证块号错误 | 认证块号 = 扇区内任意块 |
| 读数据返回错误 | 未认证 | 必须先认证再读写 |
| 读数据全 0 | 读取了尾块 | 尾块含密钥，不可直接读 |
| 写数据失败 | 写了只读块 | 扇区0块0不可写 |
| 写数据失败 | 控制位阻止写入 | 检查扇区尾块控制位设置 |
| 写后数据丢失 | 未正确休眠 | 写完后调用 nfc_halt() |
| RC522 损坏 | 接 5V 供电 | RC522 仅支持 3.3V |
| 读取距离变近 | 天线金属遮挡 | 远离金属物体 |
| 读取距离变近 | 去耦电容缺失 | VCC 旁补 10μF+100nF |
| Mifare 4K 读取错误 | 块号计算错误 | 4K 卡扇区 0~31 各 4 块，扇区 32~39 各 16 块 |
| PN532 I2C 无响应 | 地址错误 | PN532 I2C 地址 0x48 |
| PN532 模式错误 | SEL 跳线不对 | I2C: SEL0=GND, SEL1=VCC |
| 多卡同时在场 | 防冲突失败 | 一次只放一张卡 |
| 连续读卡不稳定 | 未 Halt | 每次操作后调用 nfc_halt() |
| 读 Ultralight 卡错误 | 协议不兼容 | Ultralight 无密钥认证，块大小 4 字节 |

## 相关文档

- `../../templates/driver-template-i2c.c` — I2C 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-spi.c` — SPI 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-uart.c` — UART 驱动模板（HAL 抽象层骨架）
- `../../guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `../../guides/debugging-testing.md` — 调试与测试规范
- `../../guides/pitfalls.md` — 跨类别常见问题与避坑指南
