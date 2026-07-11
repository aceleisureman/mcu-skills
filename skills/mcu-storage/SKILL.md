---
name: mcu-storage
description: MCU 非易失存储与实时时钟 Skill，覆盖 EEPROM、SPI NOR Flash、SD 卡、FRAM 和 RTC 的器件选型、接口电路、读写驱动、磨损均衡、掉电一致性、文件系统、参数管理和故障排查。当用户提到 AT24Cxx、W25Qxx、SD/SDIO、FM25、DS3231、PCF8563、DS1302 等存储或时钟器件时使用。
---

# MCU 存储与 RTC

## 使用流程

1. 先确认容量、写入频率、保持年限、掉电风险、吞吐量和成本。
2. 从路由表读取对应规范，再设计数据布局和一致性策略。
3. I2C/SPI/GPIO 驱动骨架与调试方法读取 `skill://mcu-driver-core/SKILL.md`。
4. 输出说明边界检查、写周期、校验、版本迁移和恢复路径。

## 意图路由

> 下表由 `python3 tools/skill_registry.py --write` 从 skill.json 生成，请勿手工编辑。

<!-- GENERATED:ROUTE_TABLE:START -->
| 意图 | 关键词或型号 | 读取文件 |
|------|------------|----------|
| I2C EEPROM | EEPROM、AT24C、24LC | `references/eeprom.md` |
| SPI NOR Flash | SPI Flash、W25Q、GD25Q、NOR Flash | `references/flash.md` |
| SD 卡与 FatFs | SD 卡、SDIO、FatFs、SDHC | `references/sd-card.md` |
| FRAM | FRAM、FM25、MB85 | `references/fram.md` |
| 实时时钟 | RTC、DS3231、PCF8563、DS1302 | `references/rtc.md` |
| DS1302 RTC 示例 | DS1302 示例 | `examples/ds1302-rtc/` |
| Flash 参数保存示例 | Flash 参数示例、参数保存示例 | `examples/flash-param/` |
<!-- GENERATED:ROUTE_TABLE:END -->

## 输出要求

- 参数存储采用版本号、长度和校验值，避免直接持久化易变结构体布局。
- 高频写入必须评估擦写寿命、磨损均衡和掉电原子性。
- RTC 说明时区、闰年、备份电源和无效时间检测。
