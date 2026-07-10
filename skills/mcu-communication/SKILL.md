---
name: mcu-communication
description: MCU 有线与无线通信 Skill，覆盖 WiFi、蓝牙/BLE、LoRa、NB-IoT、CAN、RS485/Modbus、NFC/RFID 和以太网的模块选型、接口电路、协议栈、AT 指令驱动、收发状态机、重连、调试和安全设计。当用户提到 ESP8266、ESP32、HC-05、SX1278、BC95、MCP2515、Modbus、RC522、W5500 等通信模块时使用。
---

# MCU 通信

## 使用流程

1. 先确认距离、速率、功耗、拓扑、实时性、网络覆盖和认证要求。
2. 从路由表读取对应协议规范，区分物理层、链路层和应用协议。
3. UART/SPI/I2C/GPIO 驱动骨架与调试方法读取 `skill://mcu-driver-core/SKILL.md`。
4. 输出必须包含超时、重试、重连、缓冲区边界、帧校验和异常恢复。

## 意图路由

| 需求或型号 | 读取文件 |
|------------|----------|
| ESP8266/ESP32、AT、MQTT | `references/wifi.md` |
| HC-05、BLE GATT、nRF52 | `references/bluetooth.md` |
| SX127x、LoRa 点对点通信 | `references/lora.md` |
| BC95、蜂窝、NB-IoT | `references/nb-iot.md` |
| CAN、MCP2515、bxCAN | `references/can.md` |
| RS485、Modbus RTU | `references/rs485.md` |
| RC522、PN532、Mifare | `references/nfc.md` |
| W5500、LAN8720、lwIP | `references/ethernet.md` |

## 示例

- ESP8266 AT、MQTT 与 ESP-IDF 实现：`examples/esp8266-wifi/`

## 输出要求

- 明确帧格式、字节序、最大长度、校验方式和状态机。
- 网络协议说明断线重连、心跳、幂等和离线缓存。
- 涉及凭证时仅描述安全存储和配置接口，不在代码中硬编码密钥。
