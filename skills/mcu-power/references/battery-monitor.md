# 电池监控开发规范

## 1. 概述与选型指南

### 常见型号对比

| 型号 | 接口 | 功能 | 精度 | 供电 | 特点 | 价格 |
|------|------|------|------|------|------|------|
| MAX17043 | I2C | 锂电电量计 | ±1% | 3.3V | ModelGauge算法、简单 | ¥8~15 |
| MAX17048 | I2C | 锂电电量计 | ±1% | 3.3V | 升级版、低功耗 | ¥10~20 |
| INA219 | I2C | 电压/电流/功率 | 12bit | 3.3V | 差分检测、双向 | ¥5~12 |
| INA226 | I2C | 电压/电流/功率 | 16bit | 3.3V | 高精度版 | ¥8~15 |
| BQ27441 | I2C | 锂电电量计 | ±1% | 3.3V | TI、集成温度补偿 | ¥10~20 |
| 分压ADC法 | ADC | 仅电压 | 依ADC | - | 最简单、零成本 | ¥0 |

### 选型建议
- **只需电压监控** → 分压 ADC 法（零成本）
- **锂电池电量百分比** → MAX17043（简单可靠）
- **精确电流/功率监控** → INA219（差分检测）
- **高精度电流** → INA226（16bit ADC）

## 2. 硬件设计规范

### 分压 ADC 法电路

```
   VBAT (4.2V max)
     │
    R1 (2MΩ)    ← 高阻值减小分流电流
     │
     ├────→ ADC 输入
     │
    R2 (1MΩ)
     │
    GND

   ADC 电压: V_adc = VBAT × R2 / (R1 + R2)
   VBAT = V_adc × 3 (分压比 1:3)
   VBAT=4.2V → V_adc=1.4V (在3.3V ADC范围内)
   消耗电流: 4.2V / 3MΩ = 1.4μA
```

### INA219 电路

```
   负载(系统)
     ↑  I_load →
   ┌─┴──┐
   │系统 │
   │负载 │
   └─┬──┘
     ├── Vin- ──┐
     │          │  INA219
   Rs(0.1Ω)     │  差分检测
     │          │  V_shunt = I × Rs
     ├── Vin+ ──┘
     │
   电池正极

   Rs 选值: Rs = V_shunt_max / I_max
   例: I_max=3.2A → Rs = 0.32V / 3.2A = 0.1Ω
   功耗: P = I² × Rs = 3.2² × 0.1 = 1.024W → 选 ≥2W 电阻
```

### MAX17043 电路

```
   电池正极 ──┬── CELL ┌──────────┐
              │        │ MAX17043 │
              │        │          │
              │        │ SCL   SDA├── I2C → MCU
              │        │          │
              │        │ VDD      │── 3.3V + 100nF
              │        │ GND      │── GND
              │        └──────────┘
   电池负极 ──┴── GND
```

## 3. 驱动开发规范

### 分压 ADC 驱动

```c
typedef struct {
    adc_config_t adc;
    float vref;
    float divider_ratio;  /* VBAT / V_adc = (R1+R2)/R2 */
    uint8_t adc_resolution;
} battery_adc_handle_t;

bat_status_t battery_adc_read_voltage(battery_adc_handle_t *h, float *voltage) {
    uint32_t sum = 0;
    uint16_t samples = 32;
    uint16_t adc_max = (1 << h->adc_resolution) - 1;

    for (int i = 0; i < samples; i++) {
        uint16_t val;
        if (adc_read(&h->adc, &val) != 0) return BAT_ERR_TIMEOUT;
        sum += val;
    }
    float v_adc = (float)sum / samples * h->vref / adc_max;
    *voltage = v_adc * h->divider_ratio;
    return BAT_OK;
}

/* 锂电池电压→电量百分比 (近似) */
uint8_t battery_voltage_to_percent(float v) {
    if (v >= 4.15f) return 100;
    if (v >= 4.05f) return 90 + (v - 4.05f) * 100;
    if (v >= 3.95f) return 80 + (v - 3.95f) * 100;
    if (v >= 3.85f) return 70 + (v - 3.85f) * 100;
    if (v >= 3.75f) return 50 + (v - 3.75f) * 200;
    if (v >= 3.65f) return 30 + (v - 3.65f) * 200;
    if (v >= 3.50f) return 10 + (v - 3.50f) * 133;
    if (v >= 3.30f) return 5  + (v - 3.30f) * 25;
    if (v >= 3.00f) return (v - 3.00f) * 16.7f;
    return 0;
}
```

### INA219 驱动

```c
#define INA219_REG_CONFIG   0x00
#define INA219_REG_SHUNT    0x01
#define INA219_REG_BUS      0x02
#define INA219_REG_POWER    0x03
#define INA219_REG_CURRENT  0x04
#define INA219_REG_CALIB    0x05

bat_status_t ina219_init(ina219_handle_t *h, i2c_config_t *i2c,
                          uint8_t addr, float shunt_r) {
    h->i2c = *i2c;
    h->addr = addr;
    h->shunt_resistor = shunt_r;
    h->current_lsb = 0.00005f;  /* 50μA/bit */
    h->power_lsb = 20 * h->current_lsb;

    uint16_t cal = (uint16_t)(0.04096f / (h->current_lsb * shunt_r));
    uint16_t config = 0x399F;  /* 32V, ±320mV, 12bit, 连续 */

    uint8_t buf[3];
    buf[0] = INA219_REG_CONFIG;
    buf[1] = (config >> 8); buf[2] = config;
    if (i2c_write(&h->i2c, h->addr, buf, 3)) return BAT_ERR_COMM;

    buf[0] = INA219_REG_CALIB;
    buf[1] = (cal >> 8); buf[2] = cal;
    if (i2c_write(&h->i2c, h->addr, buf, 3)) return BAT_ERR_COMM;
    return BAT_OK;
}

bat_status_t ina219_read_bus_voltage(ina219_handle_t *h, float *voltage) {
    uint8_t reg = INA219_REG_BUS;
    uint8_t data[2];
    if (i2c_write_read(&h->i2c, h->addr, &reg, 1, data, 2)) return BAT_ERR_COMM;
    int16_t raw = ((data[0] << 8) | data[1]) >> 3;
    *voltage = raw * 0.004f;  /* 4mV/bit */
    return BAT_OK;
}

bat_status_t ina219_read_current(ina219_handle_t *h, float *current) {
    uint8_t reg = INA219_REG_CURRENT;
    uint8_t data[2];
    if (i2c_write_read(&h->i2c, h->addr, &reg, 1, data, 2)) return BAT_ERR_COMM;
    int16_t raw = (data[0] << 8) | data[1];
    *current = raw * h->current_lsb;  /* 正=放电, 负=充电 */
    return BAT_OK;
}
```

### MAX17043 驱动

```c
#define MAX17043_ADDR        0x36
#define MAX17043_REG_VCELL   0x02
#define MAX17043_REG_SOC     0x04
#define MAX17043_REG_MODE    0x06
#define MAX17043_REG_VERSION 0x08

bat_status_t max17043_init(i2c_config_t *i2c) {
    uint8_t reg = MAX17043_REG_VERSION;
    uint8_t data[2];
    if (i2c_write_read(i2c, MAX17043_ADDR, &reg, 1, data, 2)) return BAT_ERR_COMM;
    uint16_t version = (data[0] << 8) | data[1];
    if (version == 0 || version == 0xFFFF) return BAT_ERR_NOT_FOUND;

    /* 快速启动 */
    uint8_t cmd[3] = {MAX17043_REG_MODE, 0x40, 0x00};
    if (i2c_write(i2c, MAX17043_ADDR, cmd, 3)) return BAT_ERR_COMM;
    return BAT_OK;
}

bat_status_t max17043_read_voltage(i2c_config_t *i2c, float *voltage) {
    uint8_t reg = MAX17043_REG_VCELL;
    uint8_t data[2];
    if (i2c_write_read(i2c, MAX17043_ADDR, &reg, 1, data, 2)) return BAT_ERR_COMM;
    uint16_t raw = ((data[0] << 8) | data[1]) >> 4;
    *voltage = raw * 0.00125f;  /* 1.25mV/bit */
    return BAT_OK;
}

bat_status_t max17043_read_percent(i2c_config_t *i2c, float *percent) {
    uint8_t reg = MAX17043_REG_SOC;
    uint8_t data[2];
    if (i2c_write_read(i2c, MAX17043_ADDR, &reg, 1, data, 2)) return BAT_ERR_COMM;
    *percent = data[0] + data[1] / 256.0f;  /* 整数+小数部分 */
    return BAT_OK;
}
```

## 4. 调试与测试规范

### 验证清单
- [ ] 分压电阻精度 ≤ 1%
- [ ] INA219 检流电阻功率足够
- [ ] I2C 地址正确
- [ ] ADC 参考电压稳定
- [ ] MAX17043 已执行快速启动

### 测试方法
- **万用表对比**：对比万用表电压和读数
- **充放电曲线**：记录完整充放电过程电压/电流
- **电流验证**：已知负载电流对比 INA219 读数
- **电量验证**：MAX17043 需完整充放电循环校准

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 电压读数偏差 | 分压电阻精度差 | 用 1% 精密电阻，软件校准 |
| INA219 电流为0 | 未写校准寄存器 | 必须先写 CALIB 寄存器 |
| INA219 电流方向反 | Vin+/Vin- 接反 | 对调接线或取绝对值 |
| MAX17043 电量跳变 | 未快速启动/未校准 | 上电后发 Quick Start |
| MAX17043 电量不准 | 电池特性不匹配 | 执行完整充放电循环 |
| ADC 读数跳动 | 参考电压不稳 | 用内部参考或外部 LDO |
| 分压电阻功耗大 | 阻值太小 | 用 MΩ 级电阻 |
| 电池低电不报警 | 阈值设置不当 | 设 3.3V 预警, 3.0V 关机 |

## 相关文档

- `skill://mcu-driver-core/templates/driver-template-i2c.c` — I2C 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-adc.c` — ADC 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
- `skill://mcu-driver-core/guides/pitfalls.md` — 跨类别常见问题与避坑指南
