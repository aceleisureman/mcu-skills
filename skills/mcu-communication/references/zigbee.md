# ZigBee 透传通信开发规范

## 1. 概述与选型指南

### 简介

ZigBee 是基于 IEEE 802.15.4 的低功耗无线通信协议，工作在 2.4GHz 频段，支持 mesh/星型/树型拓扑。MCU 项目中常用**串口透传模块**（而非完整 ZigBee 协议栈），将 ZigBee 模块当作"无线串口线"使用，大幅简化开发。

### 透传模块 vs 协议栈

| 方案 | 开发难度 | 灵活性 | 适用场景 |
|------|----------|--------|----------|
| 串口透传模块 | 低（当无线串口用） | 低（不能自定义网络层） | 一主多从、点对点、数据量小 |
| ZigBee 协议栈 (Z-Stack) | 高（需学协议栈 API） | 高（自定义端点/簇/路由） | 复杂 mesh、标准设备类型 |
| ZigBee 3.0 统一网关 | 中（需认证） | 中（标准化互操作） | 智能家居生态接入 |

> 本规范聚焦**串口透传模块**方案，适合快速搭建主从无线系统。

### 常见透传模块对比

| 模块 | 芯片 | 接口 | 波特率 | 距离(开阔) | 功耗(接收) | 组网 | 价格 |
|------|------|------|--------|-----------|-----------|------|------|
| DRF1609 | CC2530 | UART | 9600~115200 | 100m | ~25mA | 自动组网 | ¥15~25 |
| ZB2530 | CC2530 | UART | 9600~115200 | 100m | ~25mA | 自动组网 | ¥10~20 |
| E18-MS1 | CC2530 | UART | 9600~115200 | 160m | ~25mA | 自动组网 | ¥15~30 |
| XL-RS485-ZB | CC2530 | UART | 9600 | 300m(+PA) | ~35mA | 自动组网 | ¥25~40 |
| Ebyte E180 | CC2652R | UART | 9600~115200 | 200m | ~6mA | ZigBee 3.0 | ¥20~35 |

### 选型决策树

```
需要标准 ZigBee 3.0 互操作？ → 是 → 选 ZigBee 3.0 认证模块 (E180)
只需简单无线串口？ → 是 → 透传模块 (DRF1609/ZB2530)
  通信距离？
    < 100m → DRF1609 / ZB2530
    100~300m → E18-MS1 / XL-RS485-ZB(+PA)
  节点数量？
    ≤ 6 个从机 → 普通透传模块够用
    > 6 个 → 选支持 mesh 路由的模块
低功耗要求？ → 是 → CC2652R 方案 (~6mA 接收)
```

## 2. 硬件设计规范

### 串口透传模块接线

```
ZigBee 模块                 STM32 MCU

VCC ─────────────────── 3.3V (模块供电)
GND ─────────────────── GND
TXD ─────────────────── MCU RX (如 PB11 / USART3_RX)
RXD ─────────────────── MCU TX (如 PB10 / USART3_TX)

(可选):
EN/RESET ────────────── MCU GPIO (复位控制)
LINK/STAT ───────────── MCU GPIO 输入 (网络状态指示)
```

**要点**：
- 模块供电 3.3V，电流需 ≥ 50mA（发射时峰值）
- TXD/RXD 交叉连接：模块 TXD → MCU RX，模块 RXD → MCU TX
- 波特率默认 9600，透传模式下无需 AT 指令
- 模块天线朝上或朝外，避免金属遮挡
- 多模块同区域使用时，需配置不同 PAN ID（网络 ID）避免干扰

### 透传模块组网配置

透传模块通常通过 USB 配置工具设置参数：

| 参数 | 说明 | 典型值 |
|------|------|--------|
| PAN ID | 网络 ID，同网络的模块必须相同 | 0x1234 |
| 短地址 | 模块在网络中的地址，协调器=0x0000 | 0x0001~0xFFFF |
| 波特率 | 串口通信速率 | 9600 |
| 信道 | 11~26（2.4GHz），同网络必须相同 | 15 |
| 模式 | 协调器/路由/终端 | 主机=协调器，从机=终端 |

## 3. 透传帧协议设计

透传模块只提供"无线串口"通道，应用层需自定义帧协议保证可靠性。

### 帧格式

```
| 帧头(2B) | 长度(1B) | 命令(1B) | 载荷(NB) | 校验(1B) |
| 0xAA 0x55 | LEN       | CMD      | PAYLOAD  | SUM       |
```

| 字段 | 长度 | 说明 |
|------|------|------|
| 帧头 | 2 字节 | 固定 0xAA 0x55，用于帧同步 |
| LEN | 1 字节 | CMD + PAYLOAD 的字节数 |
| CMD | 1 字节 | 命令码，区分不同功能 |
| PAYLOAD | 0~N 字节 | 命令参数或数据 |
| SUM | 1 字节 | LEN + CMD + PAYLOAD 的 8 位累加和（取低 8 位） |

### 可靠性机制

| 机制 | 说明 |
|------|------|
| 帧头同步 | 接收时先找 0xAA 0x55，丢弃前面的杂散字节 |
| 长度校验 | LEN 字段用于预判帧长，防止缓冲区溢出 |
| 累加和校验 | SUM 校验数据完整性，错误则丢弃整帧 |
| 命令应答 | 接收方处理完命令后回发应答帧（ACK） |
| 超时重发 | 发送方 1 秒内未收到应答则重发，最多 3 次 |
| 心跳检测 | 从机定时发心跳，主机超时判定离线 |

### 多从机扩展

一主一从无需地址字段。多从机时在 CMD 前加 1 字节地址：

```
| 0xAA | 0x55 | LEN | ADDR | CMD | PAYLOAD... | SUM |
```

或利用模块的短地址做点对点透传，帧格式不变。

## 4. 驱动开发规范

### 帧收发代码

```c
/* ZigBee 透传帧协议驱动 */

#define ZB_FRAME_HEAD0    0xAA
#define ZB_FRAME_HEAD1    0x55
#define ZB_MAX_PAYLOAD    32
#define ZB_RX_BUF_SIZE    64

/* 命令码定义 */
#define ZB_CMD_UNLOCK     0x01   /* 远程开锁 */
#define ZB_CMD_LOCK       0x02   /* 远程上锁 */
#define ZB_CMD_QUERY      0x05   /* 查询状态 */
#define ZB_CMD_ALARM      0x06   /* 告警控制 */
#define ZB_RPT_STATUS     0x81   /* 状态上报 */
#define ZB_RPT_HEARTBEAT  0x83   /* 心跳 */
#define ZB_RPT_ACK        0x84   /* 命令应答 */

typedef struct {
    uint8_t cmd;
    uint8_t len;
    uint8_t payload[ZB_MAX_PAYLOAD];
} ZigbeeFrame_t;

static uint8_t s_rx_buf[ZB_RX_BUF_SIZE];
static uint8_t s_rx_len = 0;

/* 发送一帧 */
void Zigbee_Send(uint8_t cmd, const uint8_t *payload, uint8_t len) {
    uint8_t buf[ZB_MAX_PAYLOAD + 5];
    uint8_t i, sum = 0;

    buf[0] = ZB_FRAME_HEAD0;
    buf[1] = ZB_FRAME_HEAD1;
    buf[2] = len + 1;            /* LEN = CMD + PAYLOAD */
    buf[3] = cmd;
    for (i = 0; i < len; i++)
        buf[4 + i] = payload[i];

    /* SUM = LEN + CMD + PAYLOAD 的累加和低 8 位 */
    for (i = 2; i < 4 + len; i++)
        sum += buf[i];
    buf[4 + len] = sum;

    Uart3_SendBytes(buf, 4 + len + 1);
}

/* 轮询接收（在主循环中调用），返回 1=收到完整帧 */
uint8_t Zigbee_Poll(ZigbeeFrame_t *frame) {
    while (Uart3_GetByte(&s_rx_buf[s_rx_len]) != 0) {
        s_rx_len++;
        if (s_rx_len >= ZB_RX_BUF_SIZE) s_rx_len = 0;  /* 溢出保护 */

        /* 專找帧头 0xAA 0x55 */
        if (s_rx_len >= 2 &&
            s_rx_buf[s_rx_len - 2] != ZB_FRAME_HEAD0 ||
            s_rx_buf[s_rx_len - 1] != ZB_FRAME_HEAD1) {
            /* 不是帧头，滑动窗口 */
            if (s_rx_buf[s_rx_len - 1] == ZB_FRAME_HEAD0)
                s_rx_buf[0] = ZB_FRAME_HEAD0, s_rx_len = 1;
            else
                s_rx_len = 0;
            continue;
        }

        /* 帧头找到，检查长度 */
        if (s_rx_len < 4) continue;  /* 至少需要: AA 55 LEN CMD */
        uint8_t expected = s_rx_buf[2] + 4;  /* HEAD(2) + LEN(1) + CMD+PAYLOAD + SUM(1) */
        if (s_rx_len < expected) continue;   /* 帧未接收完 */

        /* 校验 SUM */
        uint8_t sum = 0;
        for (uint8_t i = 2; i < expected - 1; i++)
            sum += s_rx_buf[i];
        if (sum != s_rx_buf[expected - 1]) {
            s_rx_len = 0;  /* 校验失败，丢弃 */
            continue;
        }

        /* 解析帧 */
        frame->cmd = s_rx_buf[3];
        frame->len = s_rx_buf[2] - 1;
        for (uint8_t i = 0; i < frame->len; i++)
            frame->payload[i] = s_rx_buf[4 + i];

        s_rx_len = 0;  /* 复位接收缓冲 */
        return 1;
    }
    return 0;
}
```

## 5. 调试与测试规范

### 硬件验证清单

- [ ] 确认模块供电 3.3V，电流 ≥ 50mA
- [ ] TXD/RXD 交叉连接（模块 TXD → MCU RX）
- [ ] 模块 LINK/STAT 指示灯状态正常（组网后常亮或慢闪）
- [ ] 确认协调器和终端模块的 PAN ID 和信道一致
- [ ] 天线无金属遮挡，间距 > 5cm

### 通信验证

- **串口回环**：USB 配置工具发送测试数据，确认模块 TX/RX 正常
- **组网验证**：协调器上电后，终端模块 LINK 灯应在 10 秒内变亮
- **透传测试**：协调器侧串口发送数据，终端侧串口应收到相同数据
- **距离测试**：逐步拉开距离，确认通信不丢包

### 帧协议验证

```
发送远程开锁命令 (0x01, 无载荷):
  AA 55 01 01 02

预期应答:
  AA 55 03 84 01 00 88    (命令应答: cmd=0x01, result=0x00 成功)
  AA 55 04 81 00 01 00 86 (状态上报: door=0, lock=1, alarm=0)
```

### 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 模块不组网 | PAN ID/信道不一致 | 确认所有模块 PAN ID 和信道相同 |
| 组网后无数据 | TXD/RXD 接反 | 交叉连接：模块 TX→MCU RX |
| 数据偶尔丢失 | 无 ACK 机制 | 实现命令应答 + 超时重发 |
| 通信距离短 | 天线被遮挡/金属环境 | 天线朝外，远离金属；选 +PA 模块 |
| 多从机数据混淆 | 无地址区分 | 帧中加 ADDR 字段或用模块短地址 |
| 帧解析错乱 | 无帧头同步/校验 | 使用 0xAA 0x55 帧头 + 累加和校验 |
| 从机离线误判 | 心跳间隔太短 | 心跳 3~10 秒，超时 30 秒判离线 |
| 模块功耗大 | 终端未进入休眠 | 选支持休眠的终端模式或低功耗模块 |

## 相关文档

- `skill://mcu-driver-core/templates/driver-template-uart.c` — UART 驱动模板
- `skill://mcu-communication/references/lora.md` — LoRa 无线通信（对比参考）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
