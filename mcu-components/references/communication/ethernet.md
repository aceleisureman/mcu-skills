# 以太网模块开发规范

## 1. 概述与选型指南

### 常见型号对比

| 型号 | 类型 | 接口 | 速率 | 协议栈 | 特点 | 价格 |
|------|------|------|------|--------|------|------|
| W5500 | MAC+PHY+硬件协议栈 | SPI | 10/100M | 芯片内置 TCP/IP | 8 路 socket、开发最简单 | ¥8~15 |
| W5100S | MAC+PHY+硬件协议栈 | SPI | 10/100M | 内置 | 4 socket、低价版 | ¥6~10 |
| ENC28J60 | MAC+PHY | SPI | 10M | 需软件栈(lwIP/uIP) | 便宜但费 RAM/CPU | ¥5~8 |
| LAN8720 | 仅 PHY | RMII | 10/100M | MCU MAC + lwIP | 配 STM32F4/F7/H7、ESP32 | ¥3~6 |
| CH395 | 硬件协议栈 | SPI/UART | 10M | 内置 | 国产、UART 可用 | ¥8~12 |

### 选型决策树

```
小资源 MCU（F103 等）？ → W5500（硬件协议栈，MCU 只发命令）
MCU 带 MAC（F407/ESP32）？ → LAN8720 + lwIP（成本最低、性能最好）
只是替换串口透传？ → CH395/USR 模块（UART 直接透传）
不推荐新设计使用 ENC28J60（软件栈占用大、只有 10M）
```

## 2. 硬件设计规范

### W5500 典型电路

```
SPI: SCS/SCLK/MISO/MOSI ── MCU SPI（≤ 80MHz，常用 ≤ 20MHz）
RSTn ── MCU GPIO（低电平复位 ≥ 500μs）
INTn ── MCU GPIO EXTI（可选，中断方式收发）
25MHz 晶振 ± 30ppm
RJ45: 必须用带网络变压器的 HR911105A 或外置变压器
```

**要点**：
- TXP/TXN、RXP/RXN 差分对等长走线，阻抗 100Ω，远离晶振
- 网络变压器中心抽头按芯片手册接（W5500 TX 中心抽头接 3.3V 经磁珠）
- 3.3V 供电电流峰值 ~130mA，LDO 留裕量
- LAN8720 RMII 的 50MHz REF_CLK 走线最短，必要时源端串 22Ω

## 3. 驱动开发规范

### 统一接口定义

```c
typedef enum { NET_OK = 0, NET_ERR_LINK, NET_ERR_TIMEOUT, NET_ERR_SOCKET } net_status_t;

net_status_t eth_init(eth_handle_t *h, const uint8_t mac[6]);
net_status_t eth_dhcp(eth_handle_t *h, uint32_t timeout_ms);     /* 或静态 IP */
net_status_t tcp_connect(eth_handle_t *h, uint8_t sn, const uint8_t ip[4], uint16_t port);
int32_t      tcp_send(eth_handle_t *h, uint8_t sn, const uint8_t *buf, uint16_t len);
int32_t      tcp_recv(eth_handle_t *h, uint8_t sn, uint8_t *buf, uint16_t maxlen);
```

### W5500 驱动核心（SPI 帧：2 字节地址 + 1 字节控制 + 数据）

```c
static void w5500_write(w5500_t *h, uint16_t addr, uint8_t bsb,
                        const uint8_t *data, uint16_t len) {
    uint8_t hdr[3] = { addr >> 8, addr & 0xFF, (uint8_t)((bsb << 3) | 0x04) };
    spi_cs_low(h);
    spi_tx(h, hdr, 3);
    spi_tx(h, data, len);
    spi_cs_high(h);
}

/* socket 打开并连接 TCP 服务器 */
net_status_t w5500_tcp_connect(w5500_t *h, uint8_t sn,
                               const uint8_t ip[4], uint16_t port) {
    w5500_sock_cmd(h, sn, SOCK_CMD_CLOSE);
    w5500_write_sn8(h, sn, Sn_MR, Sn_MR_TCP);
    w5500_write_sn16(h, sn, Sn_PORT, local_port++);
    w5500_sock_cmd(h, sn, SOCK_CMD_OPEN);
    if (w5500_wait_status(h, sn, SOCK_INIT, 100)) return NET_ERR_SOCKET;

    w5500_write_sn(h, sn, Sn_DIPR, ip, 4);
    w5500_write_sn16(h, sn, Sn_DPORT, port);
    w5500_sock_cmd(h, sn, SOCK_CMD_CONNECT);
    return w5500_wait_status(h, sn, SOCK_ESTABLISHED, 5000)
           ? NET_ERR_TIMEOUT : NET_OK;
}

/* 发送: 写 TX buffer → 更新 TX_WR 指针 → SEND 命令 */
int32_t w5500_send(w5500_t *h, uint8_t sn, const uint8_t *buf, uint16_t len) {
    uint16_t free = w5500_read_sn16(h, sn, Sn_TX_FSR);
    if (len > free) return -1;
    uint16_t wr = w5500_read_sn16(h, sn, Sn_TX_WR);
    w5500_write(h, wr, BSB_TXBUF(sn), buf, len);
    w5500_write_sn16(h, sn, Sn_TX_WR, wr + len);
    w5500_sock_cmd(h, sn, SOCK_CMD_SEND);
    return len;
}
```

### LAN8720 + lwIP（STM32）要点

- CubeMX 使能 ETH（RMII）+ LWIP，PHY 地址按硬件（常为 0）
- lwIP 内存：`MEM_SIZE ≥ 10KB`、`PBUF_POOL_SIZE ≥ 8`，RAM 紧张时收发描述符放 SRAM2
- 必须周期调用 `MX_LWIP_Process()`（裸机）或使用带 RTOS 的 netconn/socket API

## 4. 调试与测试规范

### 硬件验证清单
- [ ] RJ45 灯：LINK 常亮 / ACT 闪烁
- [ ] W5500 读 VERSIONR 寄存器 == 0x04（SPI 通信正常）
- [ ] PHY 寄存器可读（LAN8720 读 ID = 0x0007C0Fx）

### 功能测试
- PC `ping` 模块 IP，丢包率 0
- TCP 回环测试：1KB × 1000 次收发数据完全一致
- 拔插网线各 10 次，链路状态检测与自动重连正常
- iperf/长连接 24h 稳定性测试

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| VERSIONR 读不到 0x04 | SPI 时序/接线错 | 降 SPI 速率至 1MHz 排查，检查模式 0/3 |
| LINK 灯不亮 | 变压器接错/差分线断 | 核对 RJ45 内置变压器型号与手册电路 |
| ping 通但 TCP 连不上 | 网关/掩码错、防火墙 | 核对同网段配置，先测局域网直连 |
| 大流量丢包 | SPI 速率低于线速 | SPI 提到 ≥ 20MHz，使能 INTn 中断收发 |
| DHCP 拿不到 IP | 未实现 DHCP 流程 | W5500 需软件 DHCP 客户端（官方 ioLibrary） |
| 断线后不恢复 | 未检测 PHY 状态 | 周期读 PHYCFGR/BMSR，断链后重新初始化 socket |
| lwIP 跑几天死机 | 内存池耗尽 | 增大 PBUF 池、检查 pbuf_free 泄漏 |

## 相关文档

- `../../templates/driver-template-spi.c` — SPI 驱动模板（HAL 抽象层骨架）
- `../../guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `../../guides/debugging-testing.md` — 调试与测试规范
- `../../guides/pitfalls.md` — 跨类别常见问题与避坑指南
