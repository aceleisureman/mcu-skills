# 湿度传感器开发规范

## 1. 概述与选型指南

### 常见型号对比

| 型号 | 接口 | 范围 | 精度 | 供电 | 响应时间 | 特点 | 价格 |
|------|------|------|------|------|----------|------|------|
| DHT11 | 单总线 | 20~90%RH | ±5%RH | 3.3~5.5V | 慢(>2s) | 廉价、精度低 | ¥3~5 |
| DHT22 | 单总线 | 0~100%RH | ±2%RH | 3.3~6V | ~2s | 精度较高 | ¥10~20 |
| SHT30 | I2C | 0~100%RH | ±2%RH | 2.15~5.5V | ~8s | 高精度、快响应 | ¥8~20 |
| SHT40 | I2C | 0~100%RH | ±1.8%RH | 1.08~3.6V | ~6s | 低功耗、第四代 | ¥10~25 |
| BME280 | I2C/SPI | 0~100%RH | ±3%RH | 1.71~3.6V | 1s | 温湿度气压三合一 | ¥8~15 |
| HIH-4030 | 模拟 | 20~95%RH | ±3.5%RH | 4~5.8V | 慢 | 模拟输出 | ¥15~30 |

### 选型建议
- **低成本原型** → DHT11（教学/演示）
- **一般应用** → DHT22（性价比好）
- **高精度** → SHT30/SHT40（I2C，推荐）
- **需气压** → BME280（三合一）
- **模拟接口** → HIH-4030（无 I2C 的场景）

## 2. 硬件设计规范

### DHT22 电路

```
VCC(3.3~5V) ── 10kΩ ──┬── VCC(1)
                       │
MCU GPIO ──────────────┴── DATA(2)
                          NC(3) - 悬空
GND ────────────────────── GND(4)
```

**要点**：
- DATA 线 4.7kΩ~10kΩ 上拉电阻（模块通常已内置）
- VCC 旁放 100nF 去耦电容
- 数据线长度 < 20cm，使用屏蔽线可延长
- 采样间隔 ≥ 2 秒

### SHT30 电路

参考 `temperature.md` 中 SHT30 电路部分（SHT30 为温湿度一体传感器）。

### 通用注意事项
- 湿度传感器需暴露在空气中，不可完全密封
- 避免凝结水直接接触传感器（除防水型外）
- 远离热源，温度变化会影响湿度读数
- 焊接时温度 < 260°C，时间 < 3 秒

## 3. 驱动开发规范

### DHT22 驱动核心

```c
/* DHT22 单总线时序协议:
 * 主机起始: 拉低 ≥1ms → 拉高 20~40μs
 * 器件响应: 拉低 80μs → 拉高 80μs
 * 数据位:   低电平 50μs + 高电平(26~28μs=0, 70μs=1)
 * 数据格式: 16bit湿度 + 16bit温度 + 8bit校验和 (MSB first)
 */

temp_status_t dht22_read(dht22_handle_t *h, float *temp, float *rh) {
    uint8_t data[5] = {0};

    /* 主机发起起始信号 */
    gpio_set_output(h->gpio, h->pin);
    gpio_write_low(h->gpio, h->pin);
    delay_ms(2);                    /* 拉低 ≥1ms */
    gpio_write_high(h->gpio, h->pin);
    delay_us(30);                   /* 拉高 20~40μs */
    gpio_set_input(h->gpio, h->pin);

    /* 等待器件响应 */
    uint32_t timeout = 0;
    while (gpio_read(h->gpio, h->pin)) { if (++timeout > 100) return TEMP_ERR_TIMEOUT; delay_us(1); }
    timeout = 0;
    while (!gpio_read(h->gpio, h->pin)) { if (++timeout > 100) return TEMP_ERR_TIMEOUT; delay_us(1); }
    timeout = 0;
    while (gpio_read(h->gpio, h->pin)) { if (++timeout > 100) return TEMP_ERR_TIMEOUT; delay_us(1); }

    /* 读取 40 bit 数据 */
    for (int i = 0; i < 40; i++) {
        /* 等待低电平结束 (50μs) */
        timeout = 0;
        while (!gpio_read(h->gpio, h->pin)) { if (++timeout > 70) return TEMP_ERR_TIMEOUT; delay_us(1); }
        /* 测量高电平持续时间 */
        uint32_t hi_us = 0;
        while (gpio_read(h->gpio, h->pin)) { if (++hi_us > 100) return TEMP_ERR_TIMEOUT; delay_us(1); }
        /* 26~28μs = 0, 70μs = 1 */
        if (hi_us > 40) data[i / 8] |= (1 << (7 - (i % 8)));
    }

    /* 校验和验证 */
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4]) return TEMP_ERR_CRC;

    /* 数据转换 */
    uint16_t raw_rh = (data[0] << 8) | data[1];
    uint16_t raw_temp = (data[2] << 8) | data[3];
    *rh = raw_rh * 0.1f;
    /* 温度最高位为符号位 */
    if (raw_temp & 0x8000) *temp = -(raw_temp & 0x7FFF) * 0.1f;
    else *temp = raw_temp * 0.1f;

    return TEMP_OK;
}
```

### SHT30 湿度读取

参考 `temperature.md` 中 SHT30 驱动（SHT30 同时输出温度和湿度）。

## 4. 调试与测试规范

### 验证方法
- **饱和盐溶液法**：使用 NaCl 饱和溶液（75%RH）或 LiCl 饱和溶液（11%RH）校准
- **对比法**：与已知精度更高的湿度计对比
- **温湿度交叉验证**：温度变化时湿度读数应有合理变化

### 注意事项
- 湿度传感器响应较慢（6~8秒），不要期望瞬时变化
- 传感器上电后需等待稳定时间（通常 5~10 分钟）
- 长期使用需定期校准（每 1~2 年）

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| DHT22 读数全 99.9% | 采样间隔太短 | 确保 ≥ 2 秒间隔 |
| DHT22 读数为 NaN | 校验和失败 | 检查时序、线长、上拉电阻 |
| 湿度读数偏高 | 传感器表面有水汽 | 待传感器完全干燥后使用 |
| 湿度读数偏低 | 长期未校准/老化 | 定期校准或更换 |
| DHT22 时序不稳定 | 中断打断 | 关键时序段关中断 |
| SHT30 湿度偏差大 | 温度补偿未做 | SHT30 内部已补偿，检查是否读取错误 |

## 相关文档

- `skill://mcu-driver-core/templates/driver-template-i2c.c` — I2C 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-spi.c` — SPI 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
- `skill://mcu-driver-core/guides/pitfalls.md` — 跨类别常见问题与避坑指南
