# MCU 元器件开发规范知识库

覆盖传感器、执行器、显示、通信、存储、电源六大类元器件的选型、硬件设计、驱动开发、调试与避坑。

## 内容

- `mcu-components/` — 主 Skill
  - `SKILL.md` — AI Agent 路由入口
  - `references/` — 36 个元器件规范文档（6 大类）
  - `templates/` — 6 套跨平台驱动模板（I2C/SPI/UART/ADC/PWM/GPIO）
  - `examples/` — 80 个从实际 STM32 项目提取的驱动源码（25 类元器件）
  - `guides/` — 硬件设计规范 / 调试测试规范 / 避坑指南 / STM32 项目实践模式

## 元器件覆盖

传感器: 温度 / 湿度 / 压力 / 光照 / IMU / 气体 / 距离 / 磁性 / 心率血氧
执行器: 直流电机 / 步进电机 / 舵机 / 继电器 / 电磁阀 / 蜂鸣器
显示: OLED / LCD / TFT / 电子纸 / LED 点阵
通信: WiFi / 蓝牙 / LoRa / NB-IoT / CAN / RS485 / NFC
存储: EEPROM / Flash / SD 卡 / FRAM / RTC
电源: LDO / DC-DC / 充电管理 / 电池监控 / 电源保护
其他: 指纹模块 / K210 人脸识别 / GPS / DFPlayer / ASRPro 语音 / 矩阵键盘
