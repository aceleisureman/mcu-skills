---
name: mcu-displays
description: MCU 显示设备开发 Skill，覆盖 OLED、字符/图形 LCD、TFT、电子墨水屏、LED 点阵与数码管的选型、接口电路、显存组织、刷新策略、图形绘制、性能优化和故障排查。当用户提到 SSD1306、LCD1602、ILI9341、ST7789、电子纸、MAX7219、TM1637 或 WS2812 显示时使用。
---

# MCU 显示设备

## 使用流程

1. 先确认分辨率、颜色、刷新率、视角、功耗、接口带宽和显存预算。
2. 从路由表加载对应显示规范，确定控制器、初始化序列和刷新方式。
3. I2C/SPI/GPIO 通用骨架与调试方法读取 `skill://mcu-driver-core/SKILL.md`。
4. 输出说明缓冲策略、脏区刷新、字库/图片存储和线程安全。

## 意图路由

> 下表由 `python3 tools/skill_registry.py --write` 从 skill.json 生成，请勿手工编辑。

<!-- GENERATED:ROUTE_TABLE:START -->
| 意图 | 关键词或型号 | 读取文件 |
|------|------------|----------|
| OLED 显示 | OLED、SSD1306、SH1106 | `references/oled.md` |
| 字符与图形 LCD | LCD1602、PCF8574、ST7920、字符 LCD | `references/lcd.md` |
| 彩色 TFT | TFT、ILI9341、ST7789 | `references/tft.md` |
| 电子纸 | 电子纸、电子墨水屏、SSD1680、UC8176 | `references/e-paper.md` |
| LED 点阵、数码管与灯带 | LED 点阵、数码管、MAX7219、TM1637、WS2812 | `references/led-matrix.md` |
| OLED 刷新示例 | OLED 示例、OLED 脏页刷新、OLED 批量写入 | `examples/oled/` |
<!-- GENERATED:ROUTE_TABLE:END -->

## 输出要求

- 初始化表必须关联具体控制器和面板参数，避免混用相似型号。
- 估算全屏刷新耗时、RAM 占用和总线占用率。
- 低功耗产品说明休眠、唤醒和残影处理。
