# CAN 总线开发规范

## 1. 概述与选型指南

CAN（Controller Area Network）总线是一种多主站串行通信总线，具有高可靠性、强抗干扰能力、实时性好的特点，广泛应用于汽车电子、工业控制等领域。CAN 通过差分信号传输（CAN_H/CAN_L），具有硬件级错误检测和自动重发机制。

### 常见型号对比

| 型号 | 厂商 | 接口 | 功能 | 供电 | 特点 | 价格 |
|------|------|------|------|------|------|------|
| MCP2515 | Microchip | SPI | CAN 控制器 | 3.0~5.5V | 独立控制器, CAN 2.0B | ¥3~8 |
| TJA1050 | NXP | - | CAN 收发器 | 4.75~5.25V | 高速, 5V | ¥3~5 |
| SN65HVD230 | TI | - | CAN 收发器 | 3.0~3.6V | 3.3V 低压 | ¥4~8 |
| ISO1042 | TI | - | 隔离收发器 | 3.0~5.5V | 5kV 隔离 | ¥15~25 |
| ADM3053 | ADI | - | 隔离收发器 | 4.5~5.5V | 5kV+DC-DC | ¥30~50 |
| STM32 bxCAN | ST | 内置 | CAN 控制器 | 3.3V | 集成在 MCU | 免费 |
| STM32 FDCAN | ST | 内置 | CAN-FD 控制器 | 3.3V | 支持 CAN FD | 免费 |
| SJA1000 | NXP | 并行 | CAN 控制器 | 5V | 经典独立控制器 | ¥5~10 |

### 选型决策树

```
MCU 有内置 CAN？ → 是 → 需要隔离？ → 是 → ISO1042 + STM32 bxCAN/FDCAN
                              否 → SN65HVD230(3.3V) / TJA1050(5V) + STM32
MCU 无 CAN？ → 是 → 需要 3.3V？ → 是 → MCP2515 + SN65HVD230
                              否 → MCP2515 + TJA1050
需要 CAN FD？ → 是 → STM32 FDCAN + TCAN1042
需要高压隔离？ → 是 → ISO1042 (5kV) / ADM3053 (集成电源隔离)
通用推荐 → MCP2515 + TJA1050 或 STM32 内置 CAN
```

### 适用场景

- **MCP2515 + TJA1050**：Arduino/51 等无内置 CAN 的 MCU 扩展 CAN
- **STM32 bxCAN**：STM32F1/F4 系列，内置 CAN 2.0B
- **STM32 FDCAN**：STM32G0/G4/H7 系列，支持 CAN FD
- **ISO1042**：电动汽车 BMS、工业控制等高压隔离场景

---

## 2. 硬件设计规范

### MCP2515 + TJA1050 电路

```
MCP2515 引脚定义 (SOIC-18):
              ┌──────────────┐
    TXCAN ──┤1     18├── VDD
    RXCAN ──┤2     17├── RESET#
      CS  ──┤3     16├── CLKOUT
     SCK  ──┤4     15├── INT
    MOSI ──┤5     14├── RX0BF
    MISO ──┤6     13├── RX1BF
     INT ──┤7     12├── TX0RTS
     GND ──┤8     11├── TX1RTS
   OSC1 ──┤9     10├── TX2RTS
   OSC2 ──┤        │  接 16MHz 晶振
              └──────────────┘
```

```
典型应用电路 (MCP2515 + TJA1050 + STM32):

 3.3V ──┬── 10μF ──┬── 100nF ──┬── VDD(18)
        │           │           │
        │    MCP2515 SPI:       │
        │    PA5(SCK)  ────────┼── SCK(4)
        │    PA7(MOSI) ────────┼── MOSI(5)
        │    PA6(MISO) ────────┼── MISO(6)
        │    PA4(CS)   ────────┼── CS(3)
        │                        │
        │    PB0(INT)  ─────────┼── INT(15)
        │    PB1(RST)  ─────────┼── RESET(16)
        │                        │
        │    OSC1(9) ─┬─ 16MHz ─┬─ OSC2(10)
        │             │  22pF   │  22pF
        │                        │
        │   TXCAN(1) ── TXD(1)  │  → TJA1050
        │   RXCAN(2) ── RXD(4)  │  ← TJA1050
        │                        │
        │    TJA1050 (5V):       │
        │    5V ── 100nF ── VCC  │
        │    CANH(7) ──┬─ 120Ω ─┼── CANH
        │    CANL(6) ──┤         ├── CANL
        │              └─────────┘
        │
 GND ───┴──────────────────────────┘
```

**要点**：
- MCP2515 使用 3.3V，SPI 接口直连 STM32
- TJA1050 需要 5V 供电（VCC=4.75~5.25V）
- MCP2515 TXCAN → TJA1050 TXD，RXCAN ← TJA1050 RXD
- MCP2515 需要 16MHz 外部晶振（两个 22pF 负载电容）
- INT 引脚连接 MCU 外部中断，触发接收中断
- RESET 引脚需 MCU 控制（或直接上拉）

### STM32 + SN65HVD230 (3.3V) 电路

```
 3.3V ──┬── 100nF ──┬── VCC(3)
        │            │
        │    STM32:   │
        │    PA12(CAN_TX) ── D(1)
        │    PA11(CAN_RX) ── R(2)
        │                │
        │    CANH(7) ────┬── 120Ω ──┬── CANH
        │    CANL(6) ────┤          ├── CANL
        │                │          │
        │    RS(8) ── 10kΩ ── GND   │
        │                │
 GND ───┴────────────────┘
```

**要点**：
- STM32 CAN_TX（PA12）连收发器 D，CAN_RX（PA11）连收发器 R
- SN65HVD230 RS 引脚接 GND 为高速模式，接 VCC 为待机
- 3.3V 供电省去 5V 电源
- PA11/PA12 可重映射到 PB8/PB9

### ISO1042 隔离 CAN 电路

```
   ┌── 逻辑侧 (MCU) ──┐    ┌── 总线侧 ──┐
   │  VCC1(3.3/5V)    │    │  VCC2(5V)   │
   │  GND1            │    │  GND2       │
   │  TXD ─── MCU TX  │    │  CANH ──┐   │
   │  RXD ─── MCU RX  │    │  CANL ──┤   │
   │  EN  ─── MCU GPIO│    │  120Ω ──┘   │
   │   5kV 隔离屏障   │    │              │
   └──────────────────┘    └──────────────┘
```

**要点**：
- 逻辑侧和总线侧使用独立电源（必须隔离供电）
- 总线侧可使用隔离 DC-DC（如 B0505S）供电

### CAN 总线物理层

```
CAN 总线拓扑 (总线型):

  节点1         节点2         节点3         节点N
  ┌───┐        ┌───┐        ┌───┐        ┌───┐
  │MCU│        │MCU│        │MCU│        │MCU│
  │CAN├──┬─────┤CAN├──┬─────┤CAN├──┬─────┤CAN│
  │TRX│  │     │TRX│  │     │TRX│  │     │TRX│
  └───┘  │     └───┘  │     └───┘  │     └───┘
         │             │             │
  ═══════╪═════════════╪═════════════╪════════
  CAN_H ─┘             │             │
  CAN_L ────────────────┴─────────────┘
       120Ω                          120Ω
       终端电阻                        终端电阻
```

**总线设计要点**：
- 总线两端必须各接一个 120Ω 终端电阻（并联后约 60Ω）
- 最大长度与波特率有关：1Mbps→40m，500Kbps→100m，125Kbps→500m
- 节点间距 ≥ 0.3m，支线（stub）< 0.3m
- 使用双绞线抑制共模干扰

### 电源设计关键参数

| 参数 | MCP2515 | TJA1050 | SN65HVD230 | ISO1042 |
|------|---------|---------|-------------|---------|
| 工作电压 | 3.0~5.5V | 4.75~5.25V | 3.0~3.6V | 3.0~5.5V |
| 静态电流 | 5mA | 5mA | 1.5mA | 3mA |
| 工作电流 | 5~10mA | 40~70mA | 2~5mA | 5~10mA |
| ESD 保护 | - | ±8kV | ±16kV | ±12kV |
| 隔离耐压 | - | - | - | 5kVrms |

---

## 3. 驱动开发规范

### CAN 帧结构说明

```
标准数据帧 (CAN 2.0A):
┌────────┬────────┬──────────┬──────────────────┬─────────┐
│ SOF    │ ID     │ Control  │ Data (0~8 bytes) │ CRC+ACK │
│ 1 bit  │ 11 bit │ RTR+IDE  │ DLC+数据          │ +EOF    │
│        │        │ +DLC     │                   │         │
└────────┴────────┴──────────┴──────────────────┴─────────┘

扩展数据帧 (CAN 2.0B): ID 为 29 bit

帧类型:
- 数据帧 (RTR=0): 携带数据
- 远程帧 (RTR=1): 请求数据
- 错误帧: 总线错误时发送
- 过载帧: 请求延迟下一帧
```

### 统一接口定义

```c
typedef enum {
    CAN_OK = 0,
    CAN_ERR_INIT,
    CAN_ERR_TX_BUSY,
    CAN_ERR_TX_FAIL,
    CAN_ERR_RX_EMPTY,
    CAN_ERR_BUS_OFF,
    CAN_ERR_NO_ACK,
    CAN_ERR_INVALID_PARAM,
} can_status_t;

typedef enum {
    CAN_FRAME_STD = 0,  /* 标准帧 11-bit ID */
    CAN_FRAME_EXT = 1,  /* 扩展帧 29-bit ID */
} can_frame_type_t;

typedef struct {
    uint32_t id;
    can_frame_type_t type;
    bool rtr;
    uint8_t dlc;        /* 数据长度 0~8 */
    uint8_t data[8];
} can_frame_t;

can_status_t can_init(uint32_t baudrate);
can_status_t can_set_filter(uint8_t filter_id, uint32_t can_id, uint32_t mask);
can_status_t can_tx(const can_frame_t *frame, uint32_t timeout_ms);
can_status_t can_rx(can_frame_t *frame, uint32_t timeout_ms);
can_status_t can_reset(void);
```

### MCP2515 SPI 驱动

```c
/* MCP2515 SPI 驱动核心
 * 硬件: STM32 + MCP2515 + TJA1050
 */

#define MCP_CANSTAT     0x0E
#define MCP_CANCTRL     0x0F
#define MCP_CNF1        0x2A
#define MCP_CNF2        0x29
#define MCP_CNF3        0x28
#define MCP_CANINTF     0x2C
#define MCP_CANINTE     0x2B
#define MCP_TXB0CTRL    0x30
#define MCP_TXB0DLC     0x35
#define MCP_TXB0DATA    0x36
#define MCP_RXB0CTRL    0x60
#define MCP_RXB0SIDH    0x61
#define MCP_RXB0SIDL    0x62
#define MCP_RXB0DLC     0x65
#define MCP_RXB0DATA    0x66

#define MCP_CMD_RESET       0xC0
#define MCP_CMD_READ        0x03
#define MCP_CMD_WRITE       0x02
#define MCP_CMD_RTS_TX0     0x81
#define MCP_CMD_READ_STATUS 0xA0
#define MCP_CMD_BITMOD      0x05

#define MODE_NORMAL     0x00
#define MODE_CONFIG     0x80

#define FLAG_RX0IF      0x01
#define FLAG_TX0IF      0x04
#define FLAG_ERRIF      0x20

static spi_handle_t *mcp_spi;

static uint8_t mcp_read_reg(uint8_t addr) {
    uint8_t tx[3] = {MCP_CMD_READ, addr, 0x00};
    uint8_t rx[3] = {0};
    spi_cs_low(mcp_spi);
    spi_transfer(mcp_spi, tx, rx, 3);
    spi_cs_high(mcp_spi);
    return rx[2];
}

static void mcp_write_reg(uint8_t addr, uint8_t val) {
    uint8_t tx[3] = {MCP_CMD_WRITE, addr, val};
    spi_cs_low(mcp_spi);
    spi_transfer(mcp_spi, tx, NULL, 3);
    spi_cs_high(mcp_spi);
}

static void mcp_bit_modify(uint8_t addr, uint8_t mask, uint8_t val) {
    uint8_t tx[4] = {MCP_CMD_BITMOD, addr, mask, val};
    spi_cs_low(mcp_spi);
    spi_transfer(mcp_spi, tx, NULL, 4);
    spi_cs_high(mcp_spi);
}

static uint8_t mcp_read_status(void) {
    uint8_t tx[2] = {MCP_CMD_READ_STATUS, 0x00};
    uint8_t rx[2] = {0};
    spi_cs_low(mcp_spi);
    spi_transfer(mcp_spi, tx, rx, 2);
    spi_cs_high(mcp_spi);
    return rx[1];
}

static void mcp_reset(void) {
    uint8_t cmd = MCP_CMD_RESET;
    spi_cs_low(mcp_spi);
    spi_transfer(mcp_spi, &cmd, NULL, 1);
    spi_cs_high(mcp_spi);
    sys_delay_ms(10);
}

static can_status_t mcp_set_mode(uint8_t mode) {
    mcp_bit_modify(MCP_CANCTRL, 0xE0, mode);
    sys_delay_ms(10);
    if ((mcp_read_reg(MCP_CANSTAT) & 0xE0) != (mode & 0xE0))
        return CAN_ERR_INIT;
    return CAN_OK;
}

/* 设置波特率 (16MHz 晶振)
 * Bit Rate = F_osc / (2 * (BRP+1) * (1 + BS1 + BS2))
 */
static can_status_t mcp_set_baudrate(uint32_t baud) {
    uint8_t brp, sjw, bs1, bs2;
    switch (baud) {
        case 1000000: brp=0;  sjw=1; bs1=5; bs2=4; break;  /* 采样点 75% */
        case 500000:  brp=1;  sjw=1; bs1=7; bs2=4; break;  /* 采样点 80% */
        case 250000:  brp=3;  sjw=1; bs1=7; bs2=4; break;
        case 125000:  brp=7;  sjw=1; bs1=7; bs2=4; break;
        case 100000:  brp=9;  sjw=1; bs1=7; bs2=4; break;
        case 50000:   brp=19; sjw=1; bs1=7; bs2=4; break;
        default:      return CAN_ERR_INVALID_PARAM;
    }
    mcp_write_reg(MCP_CNF1, ((sjw - 1) << 6) | brp);
    mcp_write_reg(MCP_CNF2, (1 << 7) | (bs1 - 1));  /* BTLMODE=1 */
    mcp_write_reg(MCP_CNF3, bs2 - 1);
    return CAN_OK;
}

/* 设置验收滤波器 (标准帧掩码模式) */
can_status_t can_set_filter(uint8_t filter_id, uint32_t can_id, uint32_t mask) {
    uint8_t sidh = (can_id >> 3) & 0xFF;
    uint8_t sidl = (can_id << 5) & 0xE0;
    uint8_t msh = (mask >> 3) & 0xFF;
    uint8_t msl = (mask << 5) & 0xE0;

    if (filter_id == 0) {
        mcp_write_reg(0x00, sidh);  /* RXF0SIDH */
        mcp_write_reg(0x01, sidl);  /* RXF0SIDL */
        mcp_write_reg(0x04, msh);   /* RXM0SIDH */
        mcp_write_reg(0x05, msl);  /* RXM0SIDL */
        mcp_write_reg(MCP_RXB0CTRL, 0x60);  /* 滤波器模式, 仅标准帧 */
    }
    return CAN_OK;
}

/* 初始化 */
can_status_t can_init(uint32_t baudrate) {
    mcp_spi = spi_get_handle(SPI1);
    mcp_reset();

    if (mcp_set_mode(MODE_CONFIG) != CAN_OK) return CAN_ERR_INIT;
    if (mcp_set_baudrate(baudrate) != CAN_OK) return CAN_ERR_INIT;

    /* 关闭滤波器, 接收所有帧 */
    mcp_write_reg(MCP_RXB0CTRL, 0x60);
    mcp_write_reg(MCP_RXB1CTRL, 0x60);

    mcp_write_reg(MCP_CANINTF, 0x00);       /* 清中断标志 */
    mcp_bit_modify(MCP_CANINTE, 0x01, 0x01); /* 启用 RXB0 中断 */

    return mcp_set_mode(MODE_NORMAL);
}

/* 发送 CAN 帧 */
can_status_t can_tx(const can_frame_t *frame, uint32_t timeout_ms) {
    uint32_t start = sys_get_tick();
    while ((sys_get_tick() - start) < timeout_ms) {
        if (!(mcp_read_status() & 0x04)) break;  /* TXB0 空闲 */
        sys_delay_ms(1);
    }
    if ((sys_get_tick() - start) >= timeout_ms) return CAN_ERR_TX_BUSY;

    /* 写入 ID */
    if (frame->type == CAN_FRAME_STD) {
        mcp_write_reg(MCP_TXB0CTRL + 1, (frame->id >> 3) & 0xFF);
        mcp_write_reg(MCP_TXB0CTRL + 2, (frame->id << 5) & 0xE0);
    } else {
        mcp_write_reg(MCP_TXB0CTRL + 1, (frame->id >> 21) & 0xFF);
        mcp_write_reg(MCP_TXB0CTRL + 2, ((frame->id >> 16) & 0x03) | 0x08);
        mcp_write_reg(MCP_TXB0CTRL + 3, (frame->id >> 8) & 0xFF);
        mcp_write_reg(MCP_TXB0CTRL + 4, frame->id & 0xFF);
    }

    /* 写入 DLC 和数据 */
    uint8_t dlc = frame->dlc & 0x0F;
    if (frame->rtr) dlc |= 0x40;
    mcp_write_reg(MCP_TXB0DLC, dlc);
    for (int i = 0; i < frame->dlc && i < 8; i++)
        mcp_write_reg(MCP_TXB0DATA + i, frame->data[i]);

    /* 请求发送 */
    uint8_t cmd = MCP_CMD_RTS_TX0;
    spi_cs_low(mcp_spi);
    spi_transfer(mcp_spi, &cmd, NULL, 1);
    spi_cs_high(mcp_spi);

    /* 等待发送完成 */
    start = sys_get_tick();
    while ((sys_get_tick() - start) < timeout_ms) {
        uint8_t ctrl = mcp_read_reg(MCP_TXB0CTRL);
        if (!(ctrl & 0x08)) {  /* TXREQ 清零 */
            if (ctrl & 0x30) return CAN_ERR_TX_FAIL;  /* MLOA/TXERR */
            return CAN_OK;
        }
        sys_delay_ms(1);
    }
    return CAN_ERR_NO_ACK;
}

/* 读取 CAN 帧 */
can_status_t can_rx(can_frame_t *frame, uint32_t timeout_ms) {
    uint32_t start = sys_get_tick();
    while ((sys_get_tick() - start) < timeout_ms) {
        if (mcp_read_status() & 0x01) {  /* RX0IF */
            uint8_t sidh = mcp_read_reg(MCP_RXB0SIDH);
            uint8_t sidl = mcp_read_reg(MCP_RXB0SIDL);

            if (sidl & 0x08) {  /* 扩展帧 */
                frame->type = CAN_FRAME_EXT;
                frame->id = ((uint32_t)sidh << 21) |
                            ((uint32_t)(sidl & 0xE0) << 13) |
                            ((uint32_t)(sidl & 0x03) << 16) |
                            ((uint32_t)mcp_read_reg(MCP_RXB0SIDL + 2) << 8) |
                            mcp_read_reg(MCP_RXB0SIDL + 3);
            } else {  /* 标准帧 */
                frame->type = CAN_FRAME_STD;
                frame->id = ((uint16_t)sidh << 3) | (sidl >> 5);
            }

            uint8_t dlc = mcp_read_reg(MCP_RXB0DLC);
            frame->rtr = (dlc & 0x40) ? true : false;
            frame->dlc = dlc & 0x0F;

            for (int i = 0; i < frame->dlc && i < 8; i++)
                frame->data[i] = mcp_read_reg(MCP_RXB0DATA + i);

            mcp_bit_modify(MCP_CANINTF, FLAG_RX0IF, 0x00);
            return CAN_OK;
        }
        sys_delay_ms(1);
    }
    return CAN_ERR_RX_EMPTY;
}

/* 接收中断回调 (在外部中断 ISR 中调用) */
void can_rx_irq_handler(void) {
    uint8_t intf = mcp_read_reg(MCP_CANINTF);
    if (intf & FLAG_RX0IF) {
        can_rx_flag = true;  /* 通知主循环读取 */
    }
    if (intf & FLAG_ERRIF) {
        uint8_t tec = mcp_read_reg(0x1C);  /* 错误发送计数器 */
        mcp_bit_modify(MCP_CANINTF, FLAG_ERRIF, 0x00);
    }
}
```

### STM32 bxCAN 配置示例

```c
/* STM32F103 内置 bxCAN 配置
 * 硬件: STM32F103 + SN65HVD230
 * 时钟: PCLK1 = 36MHz
 */
void stm32_bxcan_init_500kbps(void) {
    RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;

    /* PA11=CAN_RX, PA12=CAN_TX */
    GPIOA->CRH &= ~((0xF << 12) | (0xF << 16));
    GPIOA->CRH |= (0xB << 12);   /* PA11: 浮空输入 */
    GPIOA->CRH |= (0xA2 << 16);  /* PA12: 复用推挽 50MHz */

    CAN1->MCR |= CAN_MCR_INRQ;   /* 进入初始化模式 */
    while (!(CAN1->MSR & CAN_MSR_INAK));

    /* 500Kbps: PCLK1=36M, Presc=4, TS1=6, TS2=1 → 36M/(4*(1+7+2))=500K */
    CAN1->BTR = (4-1) | ((7-1) << 16) | ((2-1) << 20) | CAN_BTR_SILM;

    /* 滤波器0: 接收所有帧 */
    CAN1->FMR |= CAN_FMR_FINIT;
    CAN1->FA1R = 0x01;          /* 激活滤波器0 */
    CAN1->sFilterRegister[0].FR1 = 0;
    CAN1->sFilterRegister[0].FR2 = 0;
    CAN1->FM1R = 0;             /* 掩码模式 */
    CAN1->FS1R = 0x01;          /* 32位模式 */
    CAN1->FMR &= ~CAN_FMR_FINIT;

    CAN1->IER |= CAN_IER_FMPIE0;  /* FIFO0 消息挂起中断 */
    CAN1->MCR &= ~CAN_MCR_INRQ;   /* 退出初始化模式 */
    while (CAN1->MSR & CAN_MSR_INAK);

    NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
}

/* CAN1 RX0 中断服务 */
void USB_LP_CAN1_RX0_IRQHandler(void) {
    if (CAN1->RF0R & CAN_RF0R_FMP0) {
        can_frame_t frame;
        /* 读取 FIFO0 邮箱 */
        if (CAN1->sFIFOMailBox[0].RIR & CAN_RI0R_IDE) {
            frame.type = CAN_FRAME_EXT;
            frame.id = CAN1->sFIFOMailBox[0].RIR >> 3;
        } else {
            frame.type = CAN_FRAME_STD;
            frame.id = CAN1->sFIFOMailBox[0].RIR >> 21;
        }
        frame.rtr = (CAN1->sFIFOMailBox[0].RIR & CAN_RI0R_RTR) ? true : false;
        frame.dlc = CAN1->sFIFOMailBox[0].RDTR & CAN_RDT0R_DLC;
        frame.data[0] = CAN1->sFIFOMailBox[0].RDLR & 0xFF;
        frame.data[1] = (CAN1->sFIFOMailBox[0].RDLR >> 8) & 0xFF;
        frame.data[2] = (CAN1->sFIFOMailBox[0].RDLR >> 16) & 0xFF;
        frame.data[3] = (CAN1->sFIFOMailBox[0].RDLR >> 24) & 0xFF;
        frame.data[4] = CAN1->sFIFOMailBox[0].RDHR & 0xFF;
        frame.data[5] = (CAN1->sFIFOMailBox[0].RDHR >> 8) & 0xFF;
        frame.data[6] = (CAN1->sFIFOMailBox[0].RDHR >> 16) & 0xFF;
        frame.data[7] = (CAN1->sFIFOMailBox[0].RDHR >> 24) & 0xFF;

        CAN1->RF0R |= CAN_RF0R_RFOM0;  /* 释放 FIFO0 */
        can_rx_flag = true;
    }
}
```

---

## 4. 调试与测试规范

### 硬件验证清单

- [ ] 万用表测量 120Ω 终端电阻（两端并联后约 60Ω）
- [ ] 示波器检查 CAN_H/CAN_L 差分信号波形
- [ ] 确认 CAN_H/CAN_L 未接反
- [ ] MCP2515: 16MHz 晶振起振（示波器检查 OSC 引脚）
- [ ] MCP2515: SPI 通信正常（CS/SCK/MOSI/MISO 波形）
- [ ] MCP2515: INT 引脚连接到 MCU 外部中断
- [ ] 收发器供电电压正确（TJA1050 需 5V，SN65HVD230 需 3.3V）
- [ ] 总线双绞线质量良好

### 通信验证

1. **回路测试**：MCP2515 设为 Loopback 模式自发自收
2. **双节点测试**：两个节点互发数据帧
3. **远程帧测试**：发送远程帧请求特定 ID 数据
4. **总线负载测试**：高负载下测试帧丢失率

### 数据校验

- CAN 控制器硬件 CRC 保证数据完整性
- 应用层增加序列号和校验和
- 监控错误计数器（TEC/REC），超过阈值报警

### 性能测试指标

| 指标 | 1Mbps | 500Kbps | 125Kbps | 说明 |
|------|-------|---------|---------|------|
| 最大总线长度 | 40m | 100m | 500m | 理论值 |
| 单帧传输时间 | ~110μs | ~220μs | ~880μs | 8字节标准帧 |
| 有效数据速率 | ~728 Kbps | ~364 Kbps | ~91 Kbps | 8字节/帧 |
| 延迟 | <110μs | <220μs | <880μs | 最高优先级 |
| 总线利用率上限 | 70~80% | 70~80% | 70~80% | 超过会冲突增加 |

### 调试工具

- **CAN 分析仪**：PCAN-USB / CANalyst-II / Kvaser
- **软件**：PCAN-View、CANoe、Vehicle Spy
- **示波器**：差分探头测 CAN_H-CAN_L
- **万用表**：断电测总线电阻 60Ω（确认终端电阻）

---

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 无法通信 | 终端电阻缺失 | 总线两端各接 120Ω，万用表测 60Ω |
| 无法通信 | CAN_H/CAN_L 接反 | 检查收发器 CANH/CANL 连接 |
| 无法通信 | 波特率不匹配 | 所有节点波特率必须一致 |
| 无法通信 | 晶振频率不同 | MCP2515 必须用 16MHz，确认所有节点一致 |
| 偶发丢帧 | 总线负载过高 | 降低发送频率或提高波特率 |
| 偶发丢帧 | 采样点不匹配 | 统一各节点采样点（推荐 75%~87.5%） |
| 总线 Bus-Off | 错误过多 | 检查总线物理连接、终端电阻 |
| Bus-Off 后不恢复 | 未实现自动恢复 | MCR.ABOM=1 使能自动总线管理 |
| 发送无 ACK | 总线上无其他节点 | 至少需要 2 个节点才能收到 ACK |
| 发送无 ACK | ID 不匹配 | 接收节点滤波器未放行该 ID |
| MCP2515 读取全 0 | SPI 通信异常 | 检查 CPOL=0/CPHA=0，CS 时序 |
| MCP2515 无法进入配置模式 | RESET 未拉高 | RESET 引脚上拉或 MCU 控制 |
| 高优先级帧延迟大 | 邮箱优先级设置不当 | 合理配置邮箱优先级 |
| 隔离后通信异常 | 隔离电源不足 | 确保隔离侧电源满足收发器峰值电流 |
| TJA1050 发热 | 总线短路 | 检查 CAN_H/CAN_L 是否短路 |
| SN65HVD230 不工作 | RS 引脚悬空 | RS 接 GND 进入高速模式 |
| 远程帧无响应 | 目标节点未处理 RTR | 目标节点需检测 RTR 并返回数据 |
| 扩展帧无法通信 | 帧类型不匹配 | 确认发送端和接收端 IDE 位一致 |

## 相关文档

- `skill://mcu-driver-core/templates/driver-template-spi.c` — SPI 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
- `skill://mcu-driver-core/guides/pitfalls.md` — 跨类别常见问题与避坑指南
