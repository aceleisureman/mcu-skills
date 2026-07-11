# 内容索引

> 此文件由 `python3 tools/skill_registry.py --write` 生成，请勿手工编辑。

日常使用应从对应 `SKILL.md` 的意图路由进入，只加载当前任务需要的资源。

## MCU 驱动开发核心

提供可移植驱动模板、硬件设计、调试测试、常见陷阱和代码规范。

- Skill：[`mcu-driver-core`](../skills/mcu-driver-core/SKILL.md)
- 版本：`v2.1.0`
- 层级：`foundation`
- 依赖：无

### 路由

| 意图 | 关键词 | 目标 |
|------|--------|------|
| 统一驱动 API、状态码和配置结构 | 驱动 API、状态码、配置结构 | [`driver-template.h`](../skills/mcu-driver-core/templates/driver-template.h) |
| I2C 驱动模板 | I2C 驱动、I2C 模板 | [`driver-template-i2c.c`](../skills/mcu-driver-core/templates/driver-template-i2c.c) |
| SPI 驱动模板 | SPI 驱动、SPI 模板 | [`driver-template-spi.c`](../skills/mcu-driver-core/templates/driver-template-spi.c) |
| UART 与 AT 指令驱动模板 | UART 驱动、AT 指令驱动 | [`driver-template-uart.c`](../skills/mcu-driver-core/templates/driver-template-uart.c) |
| ADC 采样模板 | ADC 采样、ADC 模板 | [`driver-template-adc.c`](../skills/mcu-driver-core/templates/driver-template-adc.c) |
| PWM 输出模板 | PWM 输出、PWM 模板 | [`driver-template-pwm.c`](../skills/mcu-driver-core/templates/driver-template-pwm.c) |
| GPIO 与外部中断模板 | GPIO 驱动、外部中断 | [`driver-template-gpio.c`](../skills/mcu-driver-core/templates/driver-template-gpio.c) |
| FreeRTOS 任务、互斥和 ISR 通知模板 | FreeRTOS 驱动、总线互斥、ISR 通知 | [`driver-template-rtos.c`](../skills/mcu-driver-core/templates/driver-template-rtos.c) |
| 原理图、PCB、EMC 与电平匹配 | 硬件设计审查、PCB EMC、电平匹配 | [`hardware-design.md`](../skills/mcu-driver-core/guides/hardware-design.md) |
| 调试流程、协议分析和测试用例 | 协议分析、逻辑分析仪调试、驱动测试 | [`debugging-testing.md`](../skills/mcu-driver-core/guides/debugging-testing.md) |
| 常见硬件与固件陷阱 | 嵌入式避坑、硬件陷阱、固件陷阱 | [`pitfalls.md`](../skills/mcu-driver-core/guides/pitfalls.md) |
| 命名、格式、版本和评审规范 | 嵌入式代码规范、驱动代码评审、C 代码风格 | [`coding-style.md`](../skills/mcu-driver-core/guides/coding-style.md) |
| BSP 分层示例 | BSP 示例、板级接口示例 | [`bsp/`](../skills/mcu-driver-core/examples/bsp/) |
| GPIO 模拟 I2C 示例 | 软件 I2C、模拟 I2C | [`soft-i2c/`](../skills/mcu-driver-core/examples/soft-i2c/) |

### 资源

- templates（8）：[`driver-template-adc.c`](../skills/mcu-driver-core/templates/driver-template-adc.c)、[`driver-template-gpio.c`](../skills/mcu-driver-core/templates/driver-template-gpio.c)、[`driver-template-i2c.c`](../skills/mcu-driver-core/templates/driver-template-i2c.c)、[`driver-template-pwm.c`](../skills/mcu-driver-core/templates/driver-template-pwm.c)、[`driver-template-rtos.c`](../skills/mcu-driver-core/templates/driver-template-rtos.c)、[`driver-template-spi.c`](../skills/mcu-driver-core/templates/driver-template-spi.c)、[`driver-template-uart.c`](../skills/mcu-driver-core/templates/driver-template-uart.c)、[`driver-template.h`](../skills/mcu-driver-core/templates/driver-template.h)
- guides（4）：[`coding-style.md`](../skills/mcu-driver-core/guides/coding-style.md)、[`debugging-testing.md`](../skills/mcu-driver-core/guides/debugging-testing.md)、[`hardware-design.md`](../skills/mcu-driver-core/guides/hardware-design.md)、[`pitfalls.md`](../skills/mcu-driver-core/guides/pitfalls.md)
- examples（2）：[`bsp/`](../skills/mcu-driver-core/examples/bsp/)、[`soft-i2c/`](../skills/mcu-driver-core/examples/soft-i2c/)

## MCU 执行器

覆盖电机、舵机、继电器、电磁负载、蜂鸣器和音频模块的驱动与保护。

- Skill：[`mcu-actuators`](../skills/mcu-actuators/SKILL.md)
- 版本：`v2.1.0`
- 层级：`domain`
- 依赖：`mcu-driver-core`

### 路由

| 意图 | 关键词 | 目标 |
|------|--------|------|
| 直流电机与 H 桥 | 直流电机、L298N、TB6612、H 桥 | [`dc-motor.md`](../skills/mcu-actuators/references/dc-motor.md) |
| 步进电机 | 步进电机、28BYJ-48、ULN2003、A4988、TMC2209 | [`stepper-motor.md`](../skills/mcu-actuators/references/stepper-motor.md) |
| PWM 与总线舵机 | 舵机、PWM 舵机、总线舵机、连续旋转舵机 | [`servo.md`](../skills/mcu-actuators/references/servo.md) |
| 机械与固态继电器 | 继电器、固态继电器、继电器互锁 | [`relay.md`](../skills/mcu-actuators/references/relay.md) |
| 电磁阀与电磁铁 | 电磁阀、电磁铁、保持电流 | [`solenoid.md`](../skills/mcu-actuators/references/solenoid.md) |
| 有源与无源蜂鸣器 | 蜂鸣器、有源蜂鸣器、无源蜂鸣器、蜂鸣器旋律 | [`buzzer.md`](../skills/mcu-actuators/references/buzzer.md) |
| 音频播放模块 | DFPlayer、I2S 功放、音频播放模块 | [`audio.md`](../skills/mcu-actuators/references/audio.md) |
| 四轮直流电机示例 | 四轮电机示例 | [`dc-motor/`](../skills/mcu-actuators/examples/dc-motor/) |
| DFPlayer 示例 | DFPlayer 示例 | [`dfplayer/`](../skills/mcu-actuators/examples/dfplayer/) |
| PWM 舵机示例 | 舵机示例 | [`servo-pwm/`](../skills/mcu-actuators/examples/servo-pwm/) |

### 资源

- references（7）：[`audio.md`](../skills/mcu-actuators/references/audio.md)、[`buzzer.md`](../skills/mcu-actuators/references/buzzer.md)、[`dc-motor.md`](../skills/mcu-actuators/references/dc-motor.md)、[`relay.md`](../skills/mcu-actuators/references/relay.md)、[`servo.md`](../skills/mcu-actuators/references/servo.md)、[`solenoid.md`](../skills/mcu-actuators/references/solenoid.md)、[`stepper-motor.md`](../skills/mcu-actuators/references/stepper-motor.md)
- examples（3）：[`dc-motor/`](../skills/mcu-actuators/examples/dc-motor/)、[`dfplayer/`](../skills/mcu-actuators/examples/dfplayer/)、[`servo-pwm/`](../skills/mcu-actuators/examples/servo-pwm/)

## MCU 通信

覆盖 WiFi、蓝牙、LoRa、蜂窝、CAN、RS485、NFC 和以太网通信。

- Skill：[`mcu-communication`](../skills/mcu-communication/SKILL.md)
- 版本：`v2.2.0`
- 层级：`domain`
- 依赖：`mcu-driver-core`

### 路由

| 意图 | 关键词 | 目标 |
|------|--------|------|
| WiFi、AT 与 MQTT | WiFi、ESP8266、ESP32、MQTT、WiFi AT | [`wifi.md`](../skills/mcu-communication/references/wifi.md) |
| 经典蓝牙与 BLE | 蓝牙、BLE、HC-05、GATT、nRF52 | [`bluetooth.md`](../skills/mcu-communication/references/bluetooth.md) |
| LoRa 点对点通信 | LoRa、SX1278、SX1276 | [`lora.md`](../skills/mcu-communication/references/lora.md) |
| ZigBee 透传通信 | ZigBee、CC2530、透传模块、DRF1609、无线组网 | [`zigbee.md`](../skills/mcu-communication/references/zigbee.md) |
| NB-IoT 与蜂窝通信 | NB-IoT、BC95、蜂窝通信 | [`nb-iot.md`](../skills/mcu-communication/references/nb-iot.md) |
| CAN 总线 | CAN 总线、MCP2515、bxCAN、FDCAN | [`can.md`](../skills/mcu-communication/references/can.md) |
| RS485 与 Modbus RTU | RS485、Modbus RTU、485 总线 | [`rs485.md`](../skills/mcu-communication/references/rs485.md) |
| NFC 与 RFID | NFC、RFID、RC522、PN532、Mifare | [`nfc.md`](../skills/mcu-communication/references/nfc.md) |
| 以太网 | 以太网、W5500、LAN8720、lwIP | [`ethernet.md`](../skills/mcu-communication/references/ethernet.md) |
| ESP8266 AT 与 MQTT 示例 | ESP8266 示例、MQTT 示例 | [`esp8266-wifi/`](../skills/mcu-communication/examples/esp8266-wifi/) |

### 资源

- references（9）：[`bluetooth.md`](../skills/mcu-communication/references/bluetooth.md)、[`can.md`](../skills/mcu-communication/references/can.md)、[`ethernet.md`](../skills/mcu-communication/references/ethernet.md)、[`lora.md`](../skills/mcu-communication/references/lora.md)、[`nb-iot.md`](../skills/mcu-communication/references/nb-iot.md)、[`nfc.md`](../skills/mcu-communication/references/nfc.md)、[`rs485.md`](../skills/mcu-communication/references/rs485.md)、[`wifi.md`](../skills/mcu-communication/references/wifi.md)、[`zigbee.md`](../skills/mcu-communication/references/zigbee.md)
- examples（1）：[`esp8266-wifi/`](../skills/mcu-communication/examples/esp8266-wifi/)

## MCU 显示设备

覆盖 OLED、LCD、TFT、电子纸和 LED 显示的接口、显存与刷新优化。

- Skill：[`mcu-displays`](../skills/mcu-displays/SKILL.md)
- 版本：`v2.2.0`
- 层级：`domain`
- 依赖：`mcu-driver-core`

### 路由

| 意图 | 关键词 | 目标 |
|------|--------|------|
| OLED 显示 | OLED、SSD1306、SH1106 | [`oled.md`](../skills/mcu-displays/references/oled.md) |
| 字符与图形 LCD | LCD1602、PCF8574、ST7920、字符 LCD | [`lcd.md`](../skills/mcu-displays/references/lcd.md) |
| 彩色 TFT | TFT、ILI9341、ST7789 | [`tft.md`](../skills/mcu-displays/references/tft.md) |
| 电子纸 | 电子纸、电子墨水屏、SSD1680、UC8176 | [`e-paper.md`](../skills/mcu-displays/references/e-paper.md) |
| LED 点阵、数码管与灯带 | LED 点阵、数码管、MAX7219、TM1637、WS2812 | [`led-matrix.md`](../skills/mcu-displays/references/led-matrix.md) |
| OLED 刷新示例 | OLED 示例、OLED 脏页刷新、OLED 批量写入 | [`oled/`](../skills/mcu-displays/examples/oled/) |

### 资源

- references（5）：[`e-paper.md`](../skills/mcu-displays/references/e-paper.md)、[`lcd.md`](../skills/mcu-displays/references/lcd.md)、[`led-matrix.md`](../skills/mcu-displays/references/led-matrix.md)、[`oled.md`](../skills/mcu-displays/references/oled.md)、[`tft.md`](../skills/mcu-displays/references/tft.md)
- examples（1）：[`oled/`](../skills/mcu-displays/examples/oled/)

## MCU 输入与识别

覆盖编码器、键盘、触摸、指纹、语音和视觉识别输入。

- Skill：[`mcu-input`](../skills/mcu-input/SKILL.md)
- 版本：`v2.1.0`
- 层级：`domain`
- 依赖：`mcu-driver-core`

### 路由

| 意图 | 关键词 | 目标 |
|------|--------|------|
| 旋转编码器 | 旋转编码器、EC11、增量编码器、绝对编码器 | [`rotary-encoder.md`](../skills/mcu-input/references/rotary-encoder.md) |
| 独立按键与矩阵键盘 | 独立按键、矩阵键盘、ADC 键盘、按键去抖 | [`keypad.md`](../skills/mcu-input/references/keypad.md) |
| 电容触摸 | 电容触摸、TTP223、TTP229、MPR121 | [`touch.md`](../skills/mcu-input/references/touch.md) |
| AS608 指纹示例 | AS608、指纹识别 | [`fingerprint/`](../skills/mcu-input/examples/fingerprint/) |
| ASRPro 语音与 K210 视觉识别示例 | ASRPro、K210 视觉识别、离线语音识别 | [`asrpro-voice/`](../skills/mcu-input/examples/asrpro-voice/) |
| 按键与矩阵键盘示例 | 矩阵键盘示例、按键扫描示例 | [`keyboard/`](../skills/mcu-input/examples/keyboard/) |

### 资源

- references（3）：[`keypad.md`](../skills/mcu-input/references/keypad.md)、[`rotary-encoder.md`](../skills/mcu-input/references/rotary-encoder.md)、[`touch.md`](../skills/mcu-input/references/touch.md)
- examples（3）：[`asrpro-voice/`](../skills/mcu-input/examples/asrpro-voice/)、[`fingerprint/`](../skills/mcu-input/examples/fingerprint/)、[`keyboard/`](../skills/mcu-input/examples/keyboard/)

## MCU 电源

覆盖稳压、变换、充电、电量监控和保护电路的选型、热设计与 PCB。

- Skill：[`mcu-power`](../skills/mcu-power/SKILL.md)
- 版本：`v2.1.0`
- 层级：`domain`
- 依赖：`mcu-driver-core`

### 路由

| 意图 | 关键词 | 目标 |
|------|--------|------|
| LDO 线性稳压 | LDO、AMS1117、ME6211、HT7333 | [`ldo.md`](../skills/mcu-power/references/ldo.md) |
| DC-DC 变换 | DC-DC、MP1584、LM2596、TPS5430、升降压 | [`dc-dc.md`](../skills/mcu-power/references/dc-dc.md) |
| 电池充电 | 电池充电、TP4056、BQ24075、锂电充电 | [`battery-charger.md`](../skills/mcu-power/references/battery-charger.md) |
| 电池与电量监控 | 电量计、MAX17048、INA219、BQ27441 | [`battery-monitor.md`](../skills/mcu-power/references/battery-monitor.md) |
| 输入与接口保护 | TVS、反接保护、浪涌保护、过流保护、ESD 保护 | [`protection.md`](../skills/mcu-power/references/protection.md) |

### 资源

- references（5）：[`battery-charger.md`](../skills/mcu-power/references/battery-charger.md)、[`battery-monitor.md`](../skills/mcu-power/references/battery-monitor.md)、[`dc-dc.md`](../skills/mcu-power/references/dc-dc.md)、[`ldo.md`](../skills/mcu-power/references/ldo.md)、[`protection.md`](../skills/mcu-power/references/protection.md)

## MCU 传感器

覆盖环境、运动、气体、距离、磁性和定位传感器的选型、硬件与驱动。

- Skill：[`mcu-sensors`](../skills/mcu-sensors/SKILL.md)
- 版本：`v2.3.0`
- 层级：`domain`
- 依赖：`mcu-driver-core`

### 路由

| 意图 | 关键词 | 目标 |
|------|--------|------|
| 温度传感器 | 温度传感器、DS18B20、SHT30、NTC、PT100、LM35 | [`temperature.md`](../skills/mcu-sensors/references/temperature.md) |
| 湿度传感器 | 湿度传感器、DHT11、DHT22、BME280 | [`humidity.md`](../skills/mcu-sensors/references/humidity.md) |
| 压力与称重传感器 | 压力传感器、BMP280、HX711、称重传感器 | [`pressure.md`](../skills/mcu-sensors/references/pressure.md) |
| 光照与图像传感器 | 光照传感器、BH1750、TSL2561、OV2640、OV5640 | [`light.md`](../skills/mcu-sensors/references/light.md) |
| IMU 与姿态传感器 | IMU、MPU6050、BMI270、ICM42688、电子罗盘 | [`imu.md`](../skills/mcu-sensors/references/imu.md) |
| 气体与空气质量传感器 | 气体传感器、MQ135、CCS811、SGP30、空气质量 | [`gas.md`](../skills/mcu-sensors/references/gas.md) |
| 超声波、ToF 与红外测距 | 测距传感器、HC-SR04、VL53L0X、TFmini | [`distance.md`](../skills/mcu-sensors/references/distance.md) |
| 霍尔与磁场传感器 | 霍尔传感器、HMC5883L、QMC5883L、MLX90393 | [`magnetic.md`](../skills/mcu-sensors/references/magnetic.md) |
| GPS 与 GNSS | GPS 模块、GNSS、NEO-6M、ATGM336H、NMEA | [`gps.md`](../skills/mcu-sensors/references/gps.md) |
| ADC 水位与 MQ135 示例 | 水位传感器示例、MQ135 示例 | [`adc-water/`](../skills/mcu-sensors/examples/adc-water/) |
| GP2Y1014AU 粉尘传感器示例 | GP2Y1014AU、粉尘传感器示例、PM2.5 示例 | [`gp2y1014au/`](../skills/mcu-sensors/examples/gp2y1014au/) |
| BH1750 示例 | BH1750 示例 | [`bh1750-light/`](../skills/mcu-sensors/examples/bh1750-light/) |
| CCS811 与 SGP30 示例 | CCS811 示例、SGP30 示例 | [`ccs811-sgp30/`](../skills/mcu-sensors/examples/ccs811-sgp30/) |
| DHT11 与 DHT22 时序示例 | DHT11 示例、DHT22 示例 | [`dht11-dht22/`](../skills/mcu-sensors/examples/dht11-dht22/) |
| DS18B20 示例 | DS18B20 示例 | [`ds18b20/`](../skills/mcu-sensors/examples/ds18b20/) |
| GPS 解析示例 | GPS 解析示例、NMEA 解析示例 | [`gps/`](../skills/mcu-sensors/examples/gps/) |
| HC-SR04 示例 | HC-SR04 示例 | [`hc-sr04/`](../skills/mcu-sensors/examples/hc-sr04/) |
| HX711 示例 | HX711 示例 | [`hx711/`](../skills/mcu-sensors/examples/hx711/) |
| MAX30102 示例 | MAX30102、心率血氧 | [`max30102/`](../skills/mcu-sensors/examples/max30102/) |
| MPU6050 示例 | MPU6050 示例 | [`mpu6050/`](../skills/mcu-sensors/examples/mpu6050/) |

### 资源

- references（9）：[`distance.md`](../skills/mcu-sensors/references/distance.md)、[`gas.md`](../skills/mcu-sensors/references/gas.md)、[`gps.md`](../skills/mcu-sensors/references/gps.md)、[`humidity.md`](../skills/mcu-sensors/references/humidity.md)、[`imu.md`](../skills/mcu-sensors/references/imu.md)、[`light.md`](../skills/mcu-sensors/references/light.md)、[`magnetic.md`](../skills/mcu-sensors/references/magnetic.md)、[`pressure.md`](../skills/mcu-sensors/references/pressure.md)、[`temperature.md`](../skills/mcu-sensors/references/temperature.md)
- examples（11）：[`adc-water/`](../skills/mcu-sensors/examples/adc-water/)、[`bh1750-light/`](../skills/mcu-sensors/examples/bh1750-light/)、[`ccs811-sgp30/`](../skills/mcu-sensors/examples/ccs811-sgp30/)、[`dht11-dht22/`](../skills/mcu-sensors/examples/dht11-dht22/)、[`ds18b20/`](../skills/mcu-sensors/examples/ds18b20/)、[`gp2y1014au/`](../skills/mcu-sensors/examples/gp2y1014au/)、[`gps/`](../skills/mcu-sensors/examples/gps/)、[`hc-sr04/`](../skills/mcu-sensors/examples/hc-sr04/)、[`hx711/`](../skills/mcu-sensors/examples/hx711/)、[`max30102/`](../skills/mcu-sensors/examples/max30102/)、[`mpu6050/`](../skills/mcu-sensors/examples/mpu6050/)

## MCU 存储与 RTC

覆盖 EEPROM、Flash、SD 卡、FRAM 和 RTC 的驱动、寿命与掉电一致性。

- Skill：[`mcu-storage`](../skills/mcu-storage/SKILL.md)
- 版本：`v2.1.0`
- 层级：`domain`
- 依赖：`mcu-driver-core`

### 路由

| 意图 | 关键词 | 目标 |
|------|--------|------|
| I2C EEPROM | EEPROM、AT24C、24LC | [`eeprom.md`](../skills/mcu-storage/references/eeprom.md) |
| SPI NOR Flash | SPI Flash、W25Q、GD25Q、NOR Flash | [`flash.md`](../skills/mcu-storage/references/flash.md) |
| SD 卡与 FatFs | SD 卡、SDIO、FatFs、SDHC | [`sd-card.md`](../skills/mcu-storage/references/sd-card.md) |
| FRAM | FRAM、FM25、MB85 | [`fram.md`](../skills/mcu-storage/references/fram.md) |
| 实时时钟 | RTC、DS3231、PCF8563、DS1302 | [`rtc.md`](../skills/mcu-storage/references/rtc.md) |
| DS1302 RTC 示例 | DS1302 示例 | [`ds1302-rtc/`](../skills/mcu-storage/examples/ds1302-rtc/) |
| Flash 参数保存示例 | Flash 参数示例、参数保存示例 | [`flash-param/`](../skills/mcu-storage/examples/flash-param/) |

### 资源

- references（5）：[`eeprom.md`](../skills/mcu-storage/references/eeprom.md)、[`flash.md`](../skills/mcu-storage/references/flash.md)、[`fram.md`](../skills/mcu-storage/references/fram.md)、[`rtc.md`](../skills/mcu-storage/references/rtc.md)、[`sd-card.md`](../skills/mcu-storage/references/sd-card.md)
- examples（2）：[`ds1302-rtc/`](../skills/mcu-storage/examples/ds1302-rtc/)、[`flash-param/`](../skills/mcu-storage/examples/flash-param/)

## MCU 系统设计

编排多个硬件领域，覆盖系统架构、功耗、任务、OTA、故障降级和验证。

- Skill：[`mcu-system-design`](../skills/mcu-system-design/SKILL.md)
- 版本：`v2.2.0`
- 层级：`orchestrator`
- 依赖：`mcu-driver-core`、`mcu-sensors`、`mcu-actuators`、`mcu-displays`、`mcu-communication`、`mcu-storage`、`mcu-power`、`mcu-input`

### 路由

| 意图 | 关键词 | 目标 |
|------|--------|------|
| 加载共享驱动基础 | 系统驱动基础、系统硬件审查 | `skill://mcu-driver-core/SKILL.md` |
| 加载传感器领域 | 传感器子系统、采集子系统 | `skill://mcu-sensors/SKILL.md` |
| 加载执行器领域 | 执行器子系统、运动子系统 | `skill://mcu-actuators/SKILL.md` |
| 加载显示领域 | 显示子系统、人机界面子系统 | `skill://mcu-displays/SKILL.md` |
| 加载通信领域 | 通信子系统、联网子系统 | `skill://mcu-communication/SKILL.md` |
| 加载存储领域 | 存储子系统、日志子系统 | `skill://mcu-storage/SKILL.md` |
| 加载电源领域 | 电源子系统、供电子系统 | `skill://mcu-power/SKILL.md` |
| 加载输入与识别领域 | 输入子系统、识别子系统 | `skill://mcu-input/SKILL.md` |
| 系统级低功耗设计 | 低功耗设计、睡眠唤醒、功耗预算 | [`low-power.md`](../skills/mcu-system-design/guides/low-power.md) |
| Bootloader 与 OTA | Bootloader、双分区、OTA 回滚、固件签名 | [`ota-bootloader.md`](../skills/mcu-system-design/guides/ota-bootloader.md) |
| 多项目 STM32 工程模式 | STM32 工程模式、STM32 项目模式、跨项目模式 | [`stm32-project-patterns.md`](../skills/mcu-system-design/guides/stm32-project-patterns.md) |
| 环境监测站端到端示例 | 环境监测站、环境监测站示例、多传感器上云 | [`env-monitor-station.md`](../skills/mcu-system-design/examples/env-monitor-station.md) |
| 门控系统端到端示例 | 门控系统、PIR 继电器联动 | [`buzzer-pir-relay/`](../skills/mcu-system-design/examples/buzzer-pir-relay/) |
| 多节点气瓶柜安全监控端到端示例 | 气瓶柜、主从双 MCU、ZigBee 无线、OneNET MQTT 上云、指纹门禁、气体检测联动 | [`gas-cabinet-monitor.md`](../skills/mcu-system-design/examples/gas-cabinet-monitor.md) |

### 资源

- guides（3）：[`low-power.md`](../skills/mcu-system-design/guides/low-power.md)、[`ota-bootloader.md`](../skills/mcu-system-design/guides/ota-bootloader.md)、[`stm32-project-patterns.md`](../skills/mcu-system-design/guides/stm32-project-patterns.md)
- examples（3）：[`buzzer-pir-relay/`](../skills/mcu-system-design/examples/buzzer-pir-relay/)、[`env-monitor-station.md`](../skills/mcu-system-design/examples/env-monitor-station.md)、[`gas-cabinet-monitor.md`](../skills/mcu-system-design/examples/gas-cabinet-monitor.md)

## 项目整理

扫描和规范化项目，并生成嵌入式功能、硬件、引脚与使用说明文档。

- Skill：[`project-organizer`](../skills/project-organizer/SKILL.md)
- 版本：`v2.2.0`
- 层级：`utility`
- 依赖：无

### 路由

| 意图 | 关键词 | 目标 |
|------|--------|------|
| 目录、命名与清理规则 | 整理规则、命名规范、目录规范、垃圾文件清理 | [`organize-rules.md`](../skills/project-organizer/references/organize-rules.md) |
| 功能、硬件、引脚和说明文档模板 | 功能清单、硬件清单、引脚分配表、使用说明文档、/organize-manual-single | [`feature-hardware-inventory.md`](../skills/project-organizer/references/feature-hardware-inventory.md) |
| 只读项目扫描 | /organize-scan、扫描项目现状、失效链接扫描 | [`scan_project.py`](../skills/project-organizer/scripts/scan_project.py) |

### 资源

- references（2）：[`feature-hardware-inventory.md`](../skills/project-organizer/references/feature-hardware-inventory.md)、[`organize-rules.md`](../skills/project-organizer/references/organize-rules.md)
- scripts（1）：[`scan_project.py`](../skills/project-organizer/scripts/scan_project.py)
