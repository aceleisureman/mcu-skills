---
name: mcu-components
description: 单片机（MCU）元器件开发规范知识库，覆盖传感器、执行器、显示、通信、存储、电源六大类 36 种常用元器件的选型对比、硬件设计、驱动开发、调试测试与避坑指南，并提供 I2C/SPI/UART/ADC/PWM/GPIO 六套驱动代码模板。适用于 STM32/ESP32/51/Arduino 等平台。当用户进行单片机/嵌入式开发、编写元器件驱动、做硬件选型或电路设计、排查传感器/模块不工作等问题时使用。
---

# MCU 元器件开发规范 (MCU Components Development Specification)

## 概述

本 Skill 是一套面向单片机（MCU）元器件开发的综合规范体系，支持多平台兼容（STM32 / ESP32 / 51 单片机 / Arduino 等），兼顾 AI Agent 知识库与团队开发规范文档双重用途。

适用场景：
- AI Agent 辅助编写单片机驱动代码时作为知识参考
- 团队开发时作为元器件选型、硬件设计、驱动开发、调试测试的规范标准
- 新成员快速了解各类元器件的开发要点与避坑指南

## 使用流程（AI Agent 必读）

1. **按需加载**：本文件只是路由入口，不包含具体规范。根据用户意图从下方「意图 → 文档」映射表定位文件，再读取该文件，不要一次性加载全部文档。
2. **驱动开发**：先读对应元器件的 `references/` 文档，再以 `templates/` 中对应总线的模板为骨架编写代码。
3. **问题排查**：优先查对应元器件文档的「常见问题与避坑指南」章节，其次查 `guides/pitfalls.md`。

## 目录结构

```
mcu-components/
├── SKILL.md                          # 本文件 - 主入口与使用说明
├── references/                       # 规范参考文档
│   ├── sensors/                      # 传感器类
│   │   ├── temperature.md            # 温度传感器
│   │   ├── humidity.md               # 湿度传感器
│   │   ├── pressure.md               # 压力传感器
│   │   ├── light.md                  # 光照/图像传感器
│   │   ├── imu.md                    # 惯性测量单元
│   │   ├── gas.md                    # 气体传感器
│   │   ├── distance.md               # 距离传感器
│   │   └── magnetic.md               # 磁性传感器
│   ├── actuators/                    # 执行器类
│   │   ├── dc-motor.md               # 直流电机
│   │   ├── stepper-motor.md          # 步进电机
│   │   ├── servo.md                  # 舵机
│   │   ├── relay.md                  # 继电器
│   │   ├── solenoid.md               # 电磁阀/电磁铁
│   │   └── buzzer.md                 # 蜂鸣器
│   ├── display/                      # 显示类
│   │   ├── oled.md                   # OLED 显示屏
│   │   ├── lcd.md                    # 字符/图形 LCD
│   │   ├── tft.md                    # TFT 彩屏
│   │   ├── e-paper.md                # 电子墨水屏
│   │   └── led-matrix.md            # LED 点阵/数码管
│   ├── communication/                # 通信模块
│   │   ├── wifi.md                   # WiFi
│   │   ├── bluetooth.md              # 蓝牙
│   │   ├── lora.md                   # LoRa
│   │   ├── nb-iot.md                 # NB-IoT
│   │   ├── can.md                    # CAN 总线
│   │   ├── rs485.md                  # RS485
│   │   └── nfc.md                    # NFC/RFID
│   ├── storage/                      # 存储类
│   │   ├── eeprom.md                 # EEPROM
│   │   ├── flash.md                  # Flash
│   │   ├── sd-card.md                # SD 卡
│   │   ├── fram.md                   # FRAM
│   │   └── rtc.md                    # RTC 时钟
│   └── power/                        # 电源管理类
│       ├── ldo.md                    # LDO 线性稳压
│       ├── dc-dc.md                  # DC-DC 开关电源
│       ├── battery-charger.md        # 电池充电管理
│       ├── battery-monitor.md        # 电池监控
│       └── protection.md             # 电源保护
├── templates/                        # 代码模板库
│   ├── driver-template-i2c.c         # I2C 驱动模板
│   ├── driver-template-spi.c         # SPI 驱动模板
│   ├── driver-template-uart.c        # UART 驱动模板
│   ├── driver-template-adc.c         # ADC 驱动模板
│   ├── driver-template-pwm.c         # PWM 驱动模板
│   └── driver-template-gpio.c        # GPIO 驱动模板
├── guides/                           # 通用指南
│   ├── hardware-design.md            # 通用硬件设计规范
│   ├── debugging-testing.md          # 调试与测试规范
│   └── pitfalls.md                   # 常见问题与避坑指南
└── examples/                         # 完整示例
    └── README.md                     # 示例索引
```

## 意图 → 文档 映射

| 用户意图关键词 | 加载文档 |
|---|---|
| 温度/湿度/压力/光照/加速度/气体/距离/磁场 传感器 | `references/sensors/` 下对应文件 |
| 电机/舵机/继电器/电磁阀/蜂鸣器 | `references/actuators/` 下对应文件 |
| OLED/LCD/TFT/电子纸/LED点阵 | `references/display/` 下对应文件 |
| WiFi/蓝牙/LoRa/NB-IoT/CAN/RS485/NFC | `references/communication/` 下对应文件 |
| EEPROM/Flash/SD卡/FRAM/RTC | `references/storage/` 下对应文件 |
| LDO/DC-DC/充电/电池监控/电源保护 | `references/power/` 下对应文件 |
| I2C/SPI/UART/ADC/PWM/GPIO 驱动模板 | `templates/` 下对应文件 |
| 硬件设计/PCB/原理图 | `guides/hardware-design.md` |
| 调试/测试/验证 | `guides/debugging-testing.md` |
| 常见问题/报错/不工作/异常 | `guides/pitfalls.md` |

## 芯片/器件型号快速索引

按具体型号查找文档时使用（型号 → `references/` 下路径）：

| 文档 | 覆盖型号/器件 |
|---|---|
| `actuators/buzzer.md` | 有源蜂鸣器, 无源蜂鸣器, 压电式无源, 电磁式无源, 蜂鸣片 |
| `actuators/dc-motor.md` | L298N, TB6612FNG, DRV8833, DRV8871, MX1508, DRV8701 |
| `actuators/relay.md` | 5V 电磁继电器模块, 12V 电磁继电器, 24V 电磁继电器, 固态继电器, 磁保持继电器 |
| `actuators/servo.md` | SG90, MG90S, MG996R, DS3218, SR430, LX-16A, DS3225 |
| `actuators/solenoid.md` | 推拉电磁铁, 微型电磁阀, 常闭电磁阀, 常开电磁阀, 脉冲电磁阀 |
| `actuators/stepper-motor.md` | 28BYJ-48, NEMA17, NEMA23, NEMA11 |
| `communication/bluetooth.md` | HC-05, HC-06, JDY-31, ESP32, nRF52832, nRF52840, CH582 |
| `communication/can.md` | MCP2515, TJA1050, SN65HVD230, ISO1042, ADM3053, STM32 bxCAN, STM32 FDCAN, SJA1000 |
| `communication/lora.md` | SX1278, SX1276, SX1262, SX1268, E22-400T22D, E22-900T22D, E32-433T20D, RA-01, SX1280 |
| `communication/nb-iot.md` | BC95-G, BC26, ME3616, SIM800C, SIM7600CE, SIM7600C, Air724UG, EC600S |
| `communication/nfc.md` | RC522, MFRC522, PN532, PN5180, ST25R3911B |
| `communication/rs485.md` | MAX485, MAX3485, SP3485, SP485R, MAX13442, ADM2582E |
| `communication/wifi.md` | ESP-01S, ESP8266 MOD, ESP32-WROOM-32, ESP32-C3, ESP32-S3, ESP8285 |
| `display/e-paper.md` | SSD1680, UC8176, SSD1675, IL3829, SSD1681, UC8176C |
| `display/lcd.md` | LCD1602, LCD2004, LCD12864, LCD1602+PCF8574, LCD2004+PCF8574 |
| `display/led-matrix.md` | MAX7219, TM1637, HT16K33, WS2812, TM1640 |
| `display/oled.md` | SSD1306, SH1106, SSD1309, SH1107 |
| `display/tft.md` | ST7735, ST7789, ILI9341, ILI9488 |
| `power/battery-charger.md` | TP4056, MCP73831, BQ24075, CN3791, TP4057, LTC4054, MAX1555 |
| `power/battery-monitor.md` | MAX17043, MAX17048, INA219, INA226, BQ27441, 分压ADC法 |
| `power/dc-dc.md` | MP1584, LM2596, TPS5430, MP2315, SY8120, MT3608, XL6009, MP2307 |
| `power/ldo.md` | AMS1117-3.3, HT7333, ME6211, RT9013, MIC5217, LP2985, AP2112 |
| `power/protection.md` | TVS 二极管, 自恢复保险丝 PTC, 压敏电阻 MOV, 肖特基二极管, MOSFET 反接保护, 气体放电管 GDT, ESD 保护阵列 |
| `sensors/distance.md` | HC-SR04, HC-SR04P, VL53L0X, VL53L1X, TFmini, GP2Y0A21 |
| `sensors/gas.md` | MQ-2, MQ-135, MQ-7, CCS811, SGP30 |
| `sensors/humidity.md` | DHT11, DHT22, SHT30, SHT40, BME280, HIH-4030 |
| `sensors/imu.md` | MPU6050, BMI270, ICM-42688, QMC5883L, HMC5883L |
| `sensors/light.md` | BH1750, TSL2561, LDR, OV2640, OV5640 |
| `sensors/magnetic.md` | A3144, 44E, A1302, HMC5883L, QMC5883L, MLX90393 |
| `sensors/pressure.md` | BMP280, BME280, MPX5700AP, MPX5010DP, HX711, MPS20N0040D-D |
| `sensors/temperature.md` | DS18B20, DHT11, DHT22, LM35, NTC, PT100, SHT30, BME280 |
| `storage/eeprom.md` | AT24C02, AT24C04, AT24C16, 24LC256, AT25HP512, AT25M01 |
| `storage/flash.md` | W25Q64, W25Q128, W25Q256, W25Q80, W25Q16, GD25Q16 |
| `storage/fram.md` | FM25V10, FM25V20, FM25W256, MB85RS256, MB85RC256, MB85RS1M, FM25V05 |
| `storage/rtc.md` | DS3231, PCF8563, DS1307, DS1302, DS3234, RV-3028 |
| `storage/sd-card.md` | SD / SDHC / SDXC, SPI 模式, SDIO 模式 |

## 开发者使用

1. **选型阶段**：查阅各元器件文档的「概述与选型指南」章节
2. **硬件设计**：查阅「硬件设计规范」章节 + `guides/hardware-design.md`
3. **驱动开发**：查阅「驱动开发规范」章节 + `templates/` 代码模板
4. **调试测试**：查阅「调试与测试规范」章节 + `guides/debugging-testing.md`
5. **问题排查**：查阅「常见问题与避坑指南」章节 + `guides/pitfalls.md`

## 每个元器件规范文档的标准结构

所有元器件规范文档遵循统一结构：

```
## 1. 元器件概述与选型指南
   - 器件简介
   - 常见型号对比表
   - 选型决策树
   - 适用场景

## 2. 硬件设计规范
   - 引脚定义与功能
   - 典型应用电路
   - PCB 设计要点
   - 去耦/滤波/保护电路
   - 电气参数限制

## 3. 驱动开发规范
   - 通信协议说明
   - 驱动接口定义 (统一 API 风格)
   - 初始化流程
   - 数据读取/控制流程
   - 平台适配层 (STM32/ESP32/Arduino)
   - 代码示例

## 4. 调试与测试规范
   - 硬件验证清单
   - 通信验证方法
   - 数据校验方法
   - 性能测试指标

## 5. 常见问题与避坑指南
   - 典型故障现象与原因
   - 解决方案
   - 设计注意事项
```

## 规范原则

1. **平台无关性**：驱动接口抽象与平台适配分离，核心逻辑可跨平台复用
2. **防御性编程**：所有外部输入需校验，所有硬件操作需检查返回值
3. **模块化设计**：一个元器件一个驱动文件，接口定义与实现分离
4. **文档即代码**：规范文档与代码模板共存，保持同步更新
5. **可测试性**：驱动层与硬件抽象层分离，支持 mock 测试
6. **低功耗意识**：所有驱动提供休眠/唤醒接口设计
7. **线程安全**：多线程环境下提供互斥保护设计指导
