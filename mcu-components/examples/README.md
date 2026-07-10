# 示例索引

## 概述

本目录是 MCU 元器件开发规范体系的示例索引。各规范文档和代码模板中包含的代码片段可作为开发参考，此处汇总索引以便快速查找。

## 规范体系结构

```
mcu-components/
├── SKILL.md                 -- 主入口与使用说明
├── references/              -- 元器件规范文档 (按类别)
│   ├── sensors/             -- 传感器类
│   ├── actuators/           -- 执行器类
│   ├── display/             -- 显示类
│   ├── communication/       -- 通信模块
│   ├── storage/             -- 存储类
│   └── power/               -- 电源管理类
├── templates/               -- 跨平台代码模板
├── guides/                  -- 通用指南
└── examples/                -- 本文件 (示例索引)
```

## 示例索引

### 传感器类 (references/sensors/)

| 文档 | 包含代码示例 | 接口 |
|------|-------------|------|
| `temperature.md` | DS18B20 驱动 (1-Wire 时序 + CRC8)、SHT30 驱动 (I2C + CRC8)、NTC 驱动 (ADC + B值法) | 1-Wire / I2C / ADC |
| `humidity.md` | DHT22 驱动 (单总线时序 + 校验和) | 单总线 |
| `pressure.md` | BMP280 I2C 驱动代码、HX711 24bit ADC 读取代码、MPX 系列模拟读取 | I2C / SPI / ADC |
| `light.md` | BH1750 I2C 驱动代码、LDR ADC 读取代码、TSL2561 驱动核心 | I2C / ADC |
| `imu.md` | MPU6050 I2C 驱动代码、姿态解算 - 互补滤波代码、QMC5883L 磁力计驱动 | I2C / SPI |
| `gas.md` | MQ 系列 ADC 读取代码、SGP30 I2C 驱动代码 | I2C / ADC |
| `distance.md` | HC-SR04 GPIO 驱动代码 (定时器测量回波)、VL53L0X I2C 驱动代码、TFmini UART 驱动 | GPIO / I2C / UART |
| `magnetic.md` | 霍尔传感器 GPIO 读取代码、HMC5883L I2C 驱动和罗盘航向计算代码、QMC5883L 驱动 (HMC5883L 替代) | GPIO / I2C |

### 执行器类 (references/actuators/)

| 文档 | 包含代码示例 | 接口 |
|------|-------------|------|
| `dc-motor.md` | L298N 驱动代码（PWM调速 + 方向控制）、TB6612FNG 驱动代码、平台适配示例（STM32 HAL） | PWM / GPIO |
| `stepper-motor.md` | ULN2003 半步/全步驱动代码（28BYJ-48）、A4988 Step/Dir 驱动代码（含梯形加减速） | GPIO / PWM |
| `servo.md` | 标准 PWM 舵机驱动代码、多舵机定时器复用方案、平台适配示例（STM32 HAL）、平台适配示例（ESP32 Arduino）、连续旋转舵机驱动、总线舵机驱动 | PWM / UART |
| `relay.md` | GPIO 驱动继电器代码、多继电器互锁控制（电机正反转） | GPIO |
| `solenoid.md` | 驱动代码（含 PWM 保持电流）、纯 GPIO 驱动示例（无 PWM 保持） | GPIO / PWM |
| `buzzer.md` | 有源蜂鸣器 GPIO 驱动、无源蜂鸣器 PWM 驱动（播放音符/旋律）、旋律播放示例、非阻塞旋律播放（状态机方式） | GPIO / PWM |

### 显示类 (references/display/)

| 文档 | 包含代码示例 | 接口 |
|------|-------------|------|
| `oled.md` | 显存组织方式、SSD1306 I2C 初始化序列、显存刷新（水平地址模式）、像素操作、字符显示（6x8 字体）、图形绘制、SPI 模式驱动适配 | I2C / SPI |
| `lcd.md` | LCD1602 并行 4bit 驱动、PCF8574 I2C 转接驱动、ST7920 图形显示驱动 (SPI) | GPIO / I2C / SPI |
| `tft.md` | 平台适配层 (SPI)、ILI9341 初始化序列、地址窗口与颜色填充、图形绘制、帧缓冲管理、屏幕旋转 | SPI |
| `e-paper.md` | 波形 LUT 说明、平台适配层、SSD1680 驱动、UC8176 驱动差异 | SPI |
| `led-matrix.md` | MAX7219 驱动 (SPI)、TM1637 驱动 (2线协议)、WS2812 驱动 (单线 NZP, SPI 编码法) | SPI / GPIO |

### 通信模块 (references/communication/)

| 文档 | 包含代码示例 | 接口 |
|------|-------------|------|
| `wifi.md` | 通信协议说明、AT 指令驱动核心、MQTT 驱动（AT 指令扩展）、WiFi 连接完整流程、ESP32 ESP-IDF 原生开发 | UART (AT) / SPI |
| `bluetooth.md` | HC-05 AT 指令配置、ESP32 BLE GATT 服务定义、nRF52832 BLE 广播与连接 (SoftDevice) | UART (AT) / BLE |
| `lora.md` | 通信协议说明、SX1278 SPI 寄存器驱动、LoRa 收发完整示例 | SPI |
| `nb-iot.md` | 通信协议说明、BC95 AT 指令驱动、NB-IoT 完整通信流程 | UART (AT) |
| `can.md` | CAN 帧结构说明、MCP2515 SPI 驱动、STM32 bxCAN 配置示例 | SPI / CAN |
| `rs485.md` | RS485 方向控制驱动、Modbus RTU 帧驱动 | UART |
| `nfc.md` | Mifare 卡通信流程、RC522 SPI 寄存器驱动、完整读写流程 | SPI / I2C |

### 存储类 (references/storage/)

| 文档 | 包含代码示例 | 接口 |
|------|-------------|------|
| `eeprom.md` | I2C EEPROM 驱动接口定义、I2C EEPROM 驱动核心代码、写循环时间管理、平台适配层 | I2C |
| `flash.md` | Flash 命令表、驱动接口定义、W25Q64 SPI 驱动核心代码、磨损均衡策略 (简单版) | SPI |
| `sd-card.md` | SD 卡 SPI 命令表、R1 响应标志位、驱动接口定义、SD 卡 SPI 模式初始化代码、扇区读写代码 | SPI |
| `fram.md` | FRAM SPI 命令表、FRAM 与 Flash 驱动的关键差异、驱动接口定义、SPI FRAM 驱动核心代码、I2C FRAM 驱动 (MB85RC256) | SPI / I2C |
| `rtc.md` | DS3231 寄存器表、DS3231 I2C 驱动核心代码、PCF8563 驱动差异 | I2C |

### 电源管理类 (references/power/)

| 文档 | 包含代码示例 | 接口 |
|------|-------------|------|
| `ldo.md` | 电源监测代码 | 硬件设计 + ADC 监测 |
| `dc-dc.md` | 硬件设计参考（选型/电路/保护为主） | 硬件设计参考 |
| `battery-charger.md` | TP4056 充电状态读取代码、充电电流计算与电池监测 | GPIO / ADC |
| `battery-monitor.md` | 分压 ADC 驱动、INA219 驱动、MAX17043 驱动 | I2C / ADC |
| `protection.md` | 硬件设计参考（选型/电路/保护为主） | 硬件设计参考 |

### 代码模板 (templates/)

| 模板文件 | 内容 | 关键功能 |
|----------|------|----------|
| `driver-template-i2c.c` | I2C 设备驱动模板 | HAL 抽象层、寄存器读写、CRC-8 校验、总线扫描、带重试读取 |
| `driver-template-spi.c` | SPI 设备驱动模板 | HAL 抽象层、4 种时钟模式、CS 控制、DMA 异步传输 |
| `driver-template-uart.c` | UART 驱动模板 | 环形缓冲区、AT 指令状态机、超时处理、URC 等待 |
| `driver-template-adc.c` | ADC 驱动模板 | 单次/多次平均/中值滤波、DMA 连续采集、电压转换、NTC 温度 |
| `driver-template-pwm.c` | PWM 驱动模板 | 频率/占空比设置、多通道同步、呼吸灯、舵机控制 |
| `driver-template-gpio.c` | GPIO 驱动模板 | 中断回调管理、按键消抖、软件 I2C、软件 SPI |

### 通用指南 (guides/)

| 指南文件 | 内容 |
|----------|------|
| `hardware-design.md` | 电源设计、去耦电容、PCB 布局、走线规范、接口防护、EMC 设计、电平匹配 | - |
| `debugging-testing.md` | 硬件调试流程、工具使用、协议调试、测试用例规范、压力/边界/低功耗/环境测试、代码审查清单 | - |
| `pitfalls.md` | 电源/时钟/通信/GPIO/ADC/中断/存储/EMC/低功耗 九大类常见问题与解决方案 | - |

## 如何使用本规范体系

### 场景 1: 开发新传感器驱动

1. 查阅 `references/sensors/` 下对应传感器文档，了解选型、硬件设计和通信协议
2. 参考 `templates/driver-template-i2c.c` (或 SPI/ADC 模板) 搭建驱动框架
3. 按 `guides/hardware-design.md` 设计硬件电路
4. 按 `guides/debugging-testing.md` 中的测试用例模板编写测试
5. 遇到问题时查阅 `guides/pitfalls.md` 对应类别

### 场景 2: 调试通信故障

1. 按 `guides/debugging-testing.md` 第 3 节"通信协议调试方法"逐步排查
2. 查阅 `guides/pitfalls.md` 第 3 节"通信类问题"
3. 参考对应通信模板 (`driver-template-i2c.c` / `spi.c` / `uart.c`) 中的注意事项

### 场景 3: 设计新 PCB

1. 遵循 `guides/hardware-design.md` 中的设计规范
2. 使用文末的"设计审查清单"进行自检
3. 查阅 `guides/pitfalls.md` 中"EMC 类问题"避免常见设计缺陷

### 场景 4: 降低系统功耗

1. 按 `guides/debugging-testing.md` 第 6 节"低功耗测试方法"测量基线
2. 对照功耗优化检查清单逐项排查
3. 查阅 `guides/pitfalls.md` 第 9 节"低功耗类问题"解决异常耗电

## 快速查找指南

| 需求 | 查找位置 |
|------|----------|
| 元器件选型 | `references/<类别>/<器件>.md` → 第 1 节 |
| 硬件电路设计 | `references/<类别>/<器件>.md` → 第 2 节 + `guides/hardware-design.md` |
| 驱动代码框架 | `templates/driver-template-<接口>.c` |
| 驱动接口定义 | `references/<类别>/<器件>.md` → 第 3 节 |
| 调试方法 | `guides/debugging-testing.md` |
| 故障排查 | `guides/pitfalls.md` |
| PCB 布局规范 | `guides/hardware-design.md` → 第 3~4 节 |
| EMC 设计 | `guides/hardware-design.md` → 第 6 节 |
| 测试用例编写 | `guides/debugging-testing.md` → 第 4 节 |
| 电平匹配方案 | `guides/hardware-design.md` → 第 7 节 |
