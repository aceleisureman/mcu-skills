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

> 下表由 `python3 tools/skill_registry.py --write` 从 skill.json 生成，请勿手工编辑。

<!-- GENERATED:ROUTE_TABLE:START -->
| 意图 | 关键词或型号 | 读取文件 |
|------|------------|----------|
| WiFi、AT 与 MQTT | WiFi、ESP8266、ESP32、MQTT、WiFi AT | `references/wifi.md` |
| 经典蓝牙与 BLE | 蓝牙、BLE、HC-05、GATT、nRF52 | `references/bluetooth.md` |
| LoRa 点对点通信 | LoRa、SX1278、SX1276 | `references/lora.md` |
| ZigBee 透传通信 | ZigBee、CC2530、透传模块、DRF1609、无线组网 | `references/zigbee.md` |
| NB-IoT 与蜂窝通信 | NB-IoT、BC95、蜂窝通信 | `references/nb-iot.md` |
| CAN 总线 | CAN 总线、MCP2515、bxCAN、FDCAN | `references/can.md` |
| RS485 与 Modbus RTU | RS485、Modbus RTU、485 总线 | `references/rs485.md` |
| NFC 与 RFID | NFC、RFID、RC522、PN532、Mifare | `references/nfc.md` |
| 以太网 | 以太网、W5500、LAN8720、lwIP | `references/ethernet.md` |
| ESP8266 AT 与 MQTT 示例 | ESP8266 示例、MQTT 示例 | `examples/esp8266-wifi/` |
<!-- GENERATED:ROUTE_TABLE:END -->

## 输出要求

- 明确帧格式、字节序、最大长度、校验方式和状态机。
- 网络协议说明断线重连、心跳、幂等和离线缓存。
- 涉及凭证时仅描述安全存储和配置接口，不在代码中硬编码密钥。
