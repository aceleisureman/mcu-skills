# 多节点气瓶柜安全监控系统

> 来源项目：多节点气瓶柜安全监控 (STM32CubeMX + HAL)  
> 架构：主从双 MCU + ZigBee 无线 + ESP8266 WiFi + OneNET MQTT 上云

---

## 1. 系统概述

工业气瓶柜安全监控系统，采用主从双节点架构：

- **主机（协调器节点）**：STM32F103C8T6，负责环境监测（温湿度/可燃气体/CO）、联动控制（风扇/电磁阀）、ZigBee 协调、ESP8266 WiFi + OneNET MQTT 上云。
- **从机（门禁节点）**：STM32F103C8T6，负责指纹识别开锁、门磁检测、声光告警、OLED 显示、ZigBee 子节点通信。
- **上位机**：QT (PyQt6) 桌面应用，通过 OneNET API 远程监控。
- **通信链路**：ZigBee 透传（主机↔从机）、WiFi MQTT（主机→OneNET 云）。

```
┌──────────────┐     ZigBee 9600      ┌──────────────┐
│   从机 (门禁)  │◄──────────────────►│   主机 (协调器) │
│  STM32F103C8  │   帧协议 AA55...     │  STM32F103C8  │
│              │                      │              │
│ AS608 指纹    │                      │ DHT11 温湿度   │
│ 门磁开关      │                      │ MQ-2 可燃气体  │
│ 继电器(电磁锁) │                      │ MQ-7 一氧化碳  │
│ 蜂鸣器+RGB LED│                      │ 继电器(风扇)   │
│ OLED SSD1306  │                      │ 继电器(电磁阀) │
│              │                      │              │
│              │                      │ ESP8266 WiFi  │
└──────────────┘                      └──────┬───────┘
                                             │ MQTT
                                      ┌──────▼───────┐
                                      │  OneNET 云平台 │
                                      │  (物模型属性)   │
                                      └──────┬───────┘
                                             │ HTTP API
                                      ┌──────▼───────┐
                                      │  QT 上位机    │
                                      │  (PyQt6)     │
                                      └──────────────┘
```

---

## 2. 技术栈

### 2.1 MCU 固件

| 项目 | 主机 | 从机 |
|------|------|------|
| MCU | STM32F103C8T6 | STM32F103C8T6 |
| 库 | STM32 HAL 库 | STM32 HAL 库 |
| 时钟 | HSE 8MHz × PLL9 = 72MHz | 同左 |
| 编译 | arm-none-eabi-gcc (Makefile) | 同左 |
| IDE | STM32CubeMX 生成 + IAR 兼容 | 同左 |
| 调试 | USART1 printf + 命令行 | 同左 |

### 2.2 传感器与外设

| 器件 | 型号 | 接口 | 节点 | 用途 |
|------|------|------|------|------|
| 温湿度 | DHT11 | 单总线 | 主机 | 环境温湿度监测 |
| 可燃气体 | MQ-2 | ADC + GPIO | 主机 | 可燃气体浓度检测 |
| 一氧化碳 | MQ-7 | ADC + GPIO | 主机 | CO 浓度检测 |
| 指纹模块 | AS608 | UART (57600) | 从机 | 指纹识别开锁 |
| 门磁开关 | 干簧管 | GPIO | 从机 | 检测柜门开关 |
| OLED | SSD1306 | 软件 I2C | 从机 | 中文状态显示 |
| 继电器 | — | GPIO | 主机×2/从机×1 | 电磁阀/风扇/电磁锁 |
| 蜂鸣器 | 有源 | GPIO | 从机 | 声光告警 |
| RGB LED | — | GPIO ×3 | 从机 | 状态指示 |
| WiFi | ESP8266 (ESP-01S) | UART (AT) | 主机 | OneNET MQTT 上云 |
| ZigBee | 透传模块 | UART (9600) | 双端 | 主从无线通信 |

### 2.3 云平台与上位机

| 组件 | 技术 | 说明 |
|------|------|------|
| 云平台 | OneNET MQTT 物联网套件 | 物模型属性上报/下发 |
| 上位机 | PyQt6 + QML | 桌面 GUI，通过 OneNET API 远程监控 |
| 协议 | MQTT 1883 (AT+MQTT 命令) | ESP-AT v2.x 固件 |

---

## 3. 引脚分配

### 3.1 主机引脚 (STM32F103C8T6)

| 引脚 | 外设 | 器件 | 方向 | 说明 |
|------|------|------|------|------|
| PA0 | GPIO | MQ-2 DO | IN | 低电平=超标 |
| PA1 | ADC1_IN1 | MQ-2 AO | IN | 模拟量 0~4095 |
| PA2 | USART2_TX | ESP8266 RX | AF | WiFi AT 115200 |
| PA3 | USART2_RX | ESP8266 TX | AF | WiFi AT 115200 |
| PA5 | GPIO | DHT11 | I/O | 单总线 |
| PA6 | ADC1_IN6 | MQ-7 AO | IN | 模拟量 |
| PA7 | GPIO | MQ-7 DO | IN | 低电平=超标 |
| PA9 | USART1_TX | 调试串口 | AF | printf 115200 |
| PA10 | USART1_RX | 调试串口 | AF | 命令行 |
| PB10 | USART3_TX | ZigBee RX | AF | 透传 9600 |
| PB11 | USART3_RX | ZigBee TX | AF | 透传 9600 |
| PB12 | GPIO | 继电器-电磁阀 | OUT | 低电平吸合=断气 |
| PB13 | GPIO | 继电器-风扇 | OUT | 低电平吸合=排风 |
| PC13 | GPIO | LED | OUT | 低电平点亮，告警快闪 |

### 3.2 从机引脚 (STM32F103C8T6)

| 引脚 | 外设 | 器件 | 方向 | 说明 |
|------|------|------|------|------|
| PA2 | USART2_TX | AS608 RX | AF | 指纹 57600 |
| PA3 | USART2_RX | AS608 TX | AF | 指纹 57600 |
| PA9 | USART1_TX | 调试串口 | AF | printf 115200 |
| PA10 | USART1_RX | 调试串口 | AF | 命令行 |
| PB6 | GPIO | OLED SCL | OUT | 软件 I2C |
| PB7 | GPIO | OLED SDA | I/O | 软件 I2C |
| PB8 | GPIO | 蜂鸣器 | OUT | 高电平响 |
| PB9 | GPIO | 门磁开关 | IN | 上拉, 闭合=低 |
| PB10 | USART3_TX | ZigBee RX | AF | 透传 9600 |
| PB11 | USART3_RX | ZigBee TX | AF | 透传 9600 |
| PB12 | GPIO | 继电器-电磁锁 | OUT | 低电平吸合=开锁 |
| PB13 | GPIO | RGB LED 蓝 | OUT | 高电平亮 |
| PB14 | GPIO | RGB LED 红 | OUT | 高电平亮 |
| PB15 | GPIO | RGB LED 绿 | OUT | 高电平亮 |
| PC13 | GPIO | LED | OUT | 低电平点亮 |

---

## 4. ZigBee 通信协议

### 4.1 帧格式

```
| 0xAA | 0x55 | LEN | CMD | PAYLOAD... | SUM |
```

- `LEN`：CMD + PAYLOAD 的字节数
- `SUM`：LEN + CMD + PAYLOAD 的 8 位累加和（取低 8 位）

### 4.2 主机 → 从机（下行命令）

| CMD | 功能 | PAYLOAD | 说明 |
|-----|------|---------|------|
| 0x01 | 远程开锁 | 无 | 继电器吸合 5 秒后自动上锁 |
| 0x02 | 远程上锁 | 无 | 立即上锁 |
| 0x03 | 录入指纹 | 1 字节 id (1~299) | OLED 提示"请按手指" |
| 0x04 | 删除指纹 | 1 字节 id | 删除指定指纹 |
| 0x05 | 查询状态 | 无 | 从机立即上报状态 |
| 0x06 | 告警控制 | 1 字节 (1=触发/0=解除) | 声光告警 |

### 4.3 从机 → 主机（上行上报）

| CMD | 功能 | PAYLOAD | 触发时机 |
|-----|------|---------|----------|
| 0x81 | 状态上报 | 3 字节: door, lock, alarm | 状态变化/被查询 |
| 0x82 | 指纹事件 | 2 字节: result(0=通过/1=拒绝), id | 每次刷指纹 |
| 0x83 | 心跳 | 无 | 每 3~10 秒 |
| 0x84 | 命令应答 | 2 字节: cmd(回显), result(0=成功) | 收到命令处理完 |

### 4.4 可靠性机制

- 命令确认与重发：发命令后 1 秒内未收到 0x84 应答则重发，最多 3 次
- 从机离线判断：超过 30 秒未收到 0x83 心跳判定离线
- 主动探询：无数据时每 10 秒发 0x05 查询

---

## 5. 业务逻辑

### 5.1 主机业务

1. **传感器采集**（每 2 秒）：DHT11 温湿度、MQ-2/MQ-7 ADC 值和 DO 状态
2. **气体超标联动**：
   - DO 触发或 ADC ≥ 2000 时：开风扇排风 → 关电磁阀断气 → ZigBee 下发 0x06(1) 通知从机声光告警
   - 连续 3 个采集周期恢复正常后自动解除
3. **从机管理**：接收从机状态/指纹事件/心跳，30 秒超时判离线
4. **OneNET 上云**（每 15 秒）：MQTT 上报温湿度/气体/风扇/阀/门/锁/告警/从机在线
5. **云端下行**：接收 OneNET 下发的录指纹/解除告警命令，转发给从机
6. **MQTT 重连**：断开时每 60 秒自动重连

### 5.2 从机业务

1. **指纹识别**（上锁态轮询，200ms 周期）：
   - 识别通过 → 开锁 5 秒 + 短鸣 + ZigBee 上报
   - 拒绝 → 双短鸣 + 上报
   - 告警态刷指纹可解除告警
2. **门磁检测**：上锁状态下门被打开 → 声光告警 + 上报
3. **OLED 显示**：中文界面显示标题/门锁状态/系统状态/指纹在线
4. **ZigBee 通信**：每 3~10 秒心跳，状态变化即时上报
5. **RGB LED 状态**：正常=绿灯、开锁中=蓝灯、告警=红灯快闪+蜂鸣器

---

## 6. OneNET 物模型

| 属性标识符 | 名称 | 类型 | 读写 | 说明 |
|-----------|------|------|------|------|
| temp | 温度 | int32 (0~100 ℃) | R | DHT11 温度 |
| humi | 湿度 | int32 (0~100 %RH) | R | DHT11 湿度 |
| mq2 | 可燃气体 | int32 (0~4095) | R | MQ-2 ADC 原始值 |
| mq7 | 一氧化碳 | int32 (0~4095) | R | MQ-7 ADC 原始值 |
| alarm | 告警状态 | bool | R | 气体超标或从机告警 |
| fan | 风扇 | bool | R | 排风风扇状态 |
| valve | 电磁阀 | bool | R | 1=已关闭断气 |
| door | 柜门 | bool | R | 从机门磁状态 |
| lock | 电磁锁 | bool | R | 从机锁状态 |
| slave | 从机在线 | bool | R | ZigBee 从机在线状态 |
| enroll_id | 录入指纹ID | int32 (0~255) | RW | 下发后从机录入该 ID 指纹 |
| alarm_clear | 解除告警 | bool | RW | 下发 true 解除从机告警 |

---

## 7. 源码结构

### 7.1 主机

```
主机/
├── Core/
│   ├── Inc/
│   │   ├── main.h            # 引脚定义、系统配置
│   │   ├── board_io.h         # 继电器/LED/MQ-2/MQ-7 操作接口
│   │   ├── debug_uart.h       # USART1 调试串口 (printf + 命令行)
│   │   ├── dht11.h            # DHT11 温湿度驱动
│   │   ├── esp8266.h          # ESP8266 WiFi + OneNET MQTT 驱动
│   │   └── zigbee.h           # ZigBee 帧协议收发
│   └── Src/
│       ├── main.c             # 主循环：采集/联动/ZigBee/上云/命令行
│       ├── board_io.c         # GPIO/ADC 初始化与操作
│       ├── debug_uart.c       # 串口收发 + 命令解析
│       ├── dht11.c            # DHT11 单总线时序
│       ├── esp8266.c          # AT 指令 + MQTT 连接/发布/订阅
│       └── zigbee.c           # 帧封装/解析/校验
├── onenet_thing_model.json    # OneNET 物模型定义
└── Makefile                   # arm-none-eabi-gcc 构建
```

### 7.2 从机

```
从机/
├── Core/
│   ├── Inc/
│   │   ├── main.h             # 引脚定义
│   │   ├── board_io.h         # 锁/蜂鸣器/RGB/门磁/LED 接口
│   │   ├── debug_uart.h       # USART1 调试串口
│   │   ├── finger.h           # AS608 指纹驱动
│   │   ├── oled.h             # SSD1306 OLED 驱动
│   │   ├── oledfont.h         # 中文字模数据
│   │   └── zigbee.h           # ZigBee 帧协议
│   └── Src/
│       ├── main.c             # 主循环：指纹/门磁/ZigBee/OLED/命令行
│       ├── board_io.c         # GPIO 初始化与操作
│       ├── debug_uart.c       # 串口收发 + 命令解析
│       ├── finger.c           # AS608 协议 (GenImg/Search/Enroll/Delete)
│       ├── oled.c             # SSD1306 软件 I2C + 中文字符显示
│       └── zigbee.c           # 帧封装/解析/校验
└── Makefile
```

---

## 8. 关键设计决策

### 8.1 为何使用双 MCU 而非单 MCU

- **物理隔离**：气瓶柜门禁节点与环境监测节点物理分离，线缆短、可靠性高
- **ZigBee 无线**：避免在金属柜体上走线，部署灵活
- **独立故障域**：从机断电不影响主机监测告警，主机断网从机仍可指纹开锁

### 8.2 ZigBee 透传 vs 协议栈

- 采用串口透传模块（非 ZigBee 协议栈），简化开发
- 自定义帧协议 `AA 55 | LEN | CMD | PAYLOAD | SUM` 轻量可靠
- 多从机扩展时在 CMD 前加 1 字节地址，或利用模块短地址点对点

### 8.3 OneNET MQTT vs HTTP

- 选择 MQTT：长连接、低延迟、支持云端下发命令（property/set）
- ESP8266 使用 ESP-AT v2.x 固件（内置 AT+MQTT 命令），无需自行实现 MQTT 协议栈
- 断线自动重连（60 秒间隔）

### 8.4 OLED 中文字模

- 从机 OLED 使用 16×16 中文字模，内置 `oledfont.h`
- 支持"气瓶柜监控"、"门"、"锁"、"开"、"关"、"状态"、"正常"、"告警"等汉字
- 字模按行存储，SSD1306 页列转换显示

---

## 9. 涉及的 Skill 知识点

| 领域 | Skill | 对应内容 |
|------|-------|----------|
| 传感器 | `mcu-sensors` | DHT11 单总线时序、MQ-2/MQ-7 ADC 读取与阈值判断 |
| 执行器 | `mcu-actuators` | 继电器驱动（电磁阀/风扇/电磁锁）、有源蜂鸣器 |
| 显示 | `mcu-displays` | SSD1306 软件 I2C、16×16 中文字模与页列转换 |
| 通信 | `mcu-communication` | ESP8266 AT 指令、MQTT 连接 OneNET、ZigBee 透传 |
| 输入 | `mcu-input` | AS608 指纹识别（GenImg/Search/Enroll/Delete） |
| 驱动核心 | `mcu-driver-core` | HAL 抽象层、GPIO/ADC/UART 模板、调试串口命令行 |
| 系统设计 | `mcu-system-design` | 主从架构、联动控制、心跳/重发/离线判断、云端下行 |
