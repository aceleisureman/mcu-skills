# 温度传感器开发规范

## 1. 概述与选型指南

### 常见型号对比

| 型号 | 接口 | 范围 | 精度 | 供电 | 特点 | 价格 |
|------|------|------|------|------|------|------|
| DS18B20 | 1-Wire | -55~+125°C | ±0.5°C | 3.0~5.5V | 单总线、多节点、防水 | ¥3~8 |
| DHT11 | 单总线 | 0~+50°C | ±2°C | 3.3~5.5V | 温湿度一体、廉价 | ¥3~5 |
| DHT22 | 单总线 | -40~+80°C | ±0.5°C | 3.3~6V | 温湿度一体、精度较高 | ¥10~20 |
| LM35 | 模拟 | -55~+150°C | ±0.75°C | 4~30V | 线性输出 10mV/°C | ¥3~6 |
| NTC | 模拟(ADC) | -40~+125°C | ±1°C | 任意 | 廉价、需校准 | ¥0.1~2 |
| PT100 | SPI(MAX31865) | -200~+650°C | ±0.1°C | 依电路 | 工业级高精度 | ¥10~100+ |
| SHT30 | I2C | -40~+125°C | ±0.2°C | 2.15~5.5V | 温湿度一体、高精度 | ¥8~20 |
| BME280 | I2C/SPI | -40~+85°C | ±1°C | 1.71~3.6V | 温湿度气压三合一 | ¥8~15 |

### 选型决策树

```
精度 ≤ ±0.1°C？ → 是 → 工业级？ → 是 → PT100 + MAX31865
                                   否 → SHT31 (±0.2°C)
需同时测湿度？ → 是 → 高精度？ → 是 → SHT30/BME280
                              否 → DHT11/DHT22
需防水/多点？ → 是 → DS18B20 (1-Wire 可并联多节点)
预算极低？ → 是 → NTC (需ADC) 或 LM35
通用推荐 → SHT30 (I2C, 综合性价比最优)
```

## 2. 硬件设计规范

### DS18B20 电路

```
VCC ── 4.7kΩ ──┬── VDD(3)
               │
MCU GPIO ──────┴── DQ(2)
                  
GND ───────────── GND(1)
```

**要点**：
- 必须接 4.7kΩ 上拉电阻（短线可 2.2kΩ，长线增至 10kΩ）
- VDD 旁放 100nF 去耦电容
- 数据线暴露场景加 ESD 保护（PESD3V3L5UY）
- 总线长度建议 < 30cm
- 寄生供电模式：VDD 接 GND，需强上拉电路

### SHT30 电路（I2C）

```
VCC ── 4.7kΩ ── SCL ── SHT30 SCL(1)
VCC ── 4.7kΩ ── SDA ── SHT30 SDA(2)
GND ──────────────── SHT30 ADDR(3) → 地址 0x44
VCC ──────────────── SHT30 VDD(5)
GND ──────────────── SHT30 VSS(6)
```

**要点**：
- SCL/SDA 各接 4.7kΩ~10kΩ 上拉
- PCB 上远离发热器件（MCU、DC-DC）
- ADDR 必须接 VDD 或 VSS，不可悬空
- 外壳留通风孔

### NTC 分压电路

```
VCC ── R_fixed(10kΩ ±1%) ──┬── ADC 输入
                             │
                           NTC(10kΩ@25°C, B=3950)
                             │
GND ────────────────────────┘
```

**要点**：
- 参考电阻用 ±1% 精密电阻
- ADC 输入加 100nF 抗混叠滤波
- 流过 NTC 电流 < 0.1mA 避免自热
- 使用 Steinhart-Hart 公式转换：`T = 1 / (1/T0 + (1/B)*ln(R/R0))`

## 3. 驱动开发规范

### 统一接口定义

```c
typedef enum {
    TEMP_SENSOR_DS18B20, TEMP_SENSOR_DHT11, TEMP_SENSOR_DHT22,
    TEMP_SENSOR_SHT30, TEMP_SENSOR_BME280, TEMP_SENSOR_LM35,
    TEMP_SENSOR_NTC, TEMP_SENSOR_PT100,
} temp_sensor_type_t;

typedef enum {
    TEMP_OK = 0, TEMP_ERR_NOT_FOUND, TEMP_ERR_CRC,
    TEMP_ERR_TIMEOUT, TEMP_ERR_INIT, TEMP_ERR_INVALID_PARAM,
} temp_status_t;

typedef struct {
    float temperature;
    float humidity;          /* 仅温湿度传感器有效 */
    int64_t timestamp_ms;
    bool data_valid;
} temp_data_t;

temp_status_t temp_sensor_init(temp_sensor_handle_t *handle, void *hw_config);
temp_status_t temp_sensor_read(temp_sensor_handle_t *handle, temp_data_t *data);
void temp_sensor_deinit(temp_sensor_handle_t *handle);
```

### DS18B20 驱动核心

```c
/* 1-Wire 时序 - 必须精确! */
static bool onewire_reset(const onewire_config_t *cfg) {
    bool presence;
    onewire_enter_critical();       /* 关中断保证时序 */
    onewire_set_output(cfg);
    onewire_write_low(cfg);
    onewire_delay_us(500);          /* 复位脉冲 ≥480μs */
    onewire_set_input(cfg);
    onewire_delay_us(65);
    presence = !onewire_read(cfg);  /* 器件存在脉冲 */
    onewire_exit_critical();
    onewire_delay_us(420);
    return presence;
}

static void onewire_write_bit(const onewire_config_t *cfg, bool bit) {
    onewire_enter_critical();
    onewire_set_output(cfg);
    onewire_write_low(cfg);
    if (bit) {
        onewire_delay_us(6);        /* 写1: 短拉低 */
        onewire_set_input(cfg);
        onewire_delay_us(64);
    } else {
        onewire_delay_us(60);       /* 写0: 长拉低 */
        onewire_set_input(cfg);
        onewire_delay_us(10);
    }
    onewire_exit_critical();
}

/* CRC8 校验 (Dallas/Maxim 多项式) */
static uint8_t onewire_crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0;
    while (len--) {
        uint8_t inbyte = *data++;
        for (int i = 0; i < 8; i++) {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            inbyte >>= 1;
        }
    }
    return crc;
}

/* 读取温度 (阻塞) */
temp_status_t ds18b20_read_blocking(ds18b20_handle_t *h, float *temp) {
    if (!onewire_reset(&h->ow_cfg)) return TEMP_ERR_NOT_FOUND;
    onewire_write_byte(&h->ow_cfg, 0xCC);  /* SKIP_ROM */
    onewire_write_byte(&h->ow_cfg, 0x44);  /* CONVERT_T */
    onewire_delay_ms(750);                 /* 12bit: 750ms */

    if (!onewire_reset(&h->ow_cfg)) return TEMP_ERR_NOT_FOUND;
    onewire_write_byte(&h->ow_cfg, 0xCC);  /* SKIP_ROM */
    onewire_write_byte(&h->ow_cfg, 0xBE);  /* READ_SCRATCHPAD */

    uint8_t buf[9];
    for (int i = 0; i < 9; i++) buf[i] = onewire_read_byte(&h->ow_cfg);

    if (onewire_crc8(buf, 8) != buf[8]) return TEMP_ERR_CRC;

    int16_t raw = (buf[1] << 8) | buf[0];
    *temp = raw * 0.0625f;  /* 12bit: 0.0625°C/LSB */
    return TEMP_OK;
}
```

### SHT30 驱动核心

```c
/* CRC-8 (多项式 0x31, 初始值 0xFF) */
static uint8_t sht30_crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0xFF;
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++)
            crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
    }
    return crc;
}

temp_status_t sht30_read(sht30_handle_t *h, float *temp, float *rh) {
    uint8_t cmd[2] = {0x2C, 0x06};  /* 单次高重复性测量 */
    if (i2c_write(h->i2c, h->addr, cmd, 2)) return TEMP_ERR_TIMEOUT;
    i2c_delay_ms(20);

    uint8_t data[6];
    if (i2c_read(h->i2c, h->addr, data, 6)) return TEMP_ERR_TIMEOUT;

    if (sht30_crc8(&data[0], 2) != data[2]) return TEMP_ERR_CRC;
    if (sht30_crc8(&data[3], 2) != data[5]) return TEMP_ERR_CRC;

    uint16_t raw_t = (data[0] << 8) | data[1];
    *temp = -45.0f + 175.0f * raw_t / 65535.0f;

    uint16_t raw_rh = (data[3] << 8) | data[4];
    *rh = 100.0f * raw_rh / 65535.0f;
    if (*rh > 100.0f) *rh = 100.0f;
    if (*rh < 0.0f) *rh = 0.0f;
    return TEMP_OK;
}
```

### NTC 驱动核心

```c
temp_status_t ntc_read_temp(ntc_handle_t *h, float *temp) {
    /* 16次采样取平均降噪 */
    uint32_t sum = 0;
    for (int i = 0; i < 16; i++) {
        uint16_t val;
        if (adc_read(h->adc, &val)) return TEMP_ERR_TIMEOUT;
        sum += val;
    }
    float adc_avg = sum / 16.0f;
    float v_ntc = adc_avg * h->vref / h->adc_max;

    /* 计算阻值: R_ntc = R_ref * V_ntc / (VCC - V_ntc) */
    float r_ntc = h->r_ref * v_ntc / (h->vref - v_ntc);

    /* B值法转换: 1/T = 1/T0 + (1/B)*ln(R/R0) */
    float t_kelvin = 1.0f / (1.0f/h->t0_kelvin +
                             (1.0f/h->b_value) * logf(r_ntc / h->r_nominal));
    *temp = t_kelvin - 273.15f;
    return TEMP_OK;
}
```

## 4. 调试与测试规范

### 硬件验证清单
- [ ] 用万用表确认 VDD/GND 电压正确
- [ ] 确认上拉电阻阻值和连接
- [ ] 示波器检查数据线波形（电平幅值、上升沿时间）
- [ ] 确认去耦电容已贴装

### 通信验证
- **DS18B20**：示波器抓复位脉冲，确认器件存在脉冲宽度 60~240μs
- **SHT30**：I2C 扫描确认地址 0x44/0x45 响应
- **NTC**：万用表测 NTC 常温阻值是否接近标称值

### 数据校验
- 对比已知温度源（如冰水 0°C、体温 ~37°C）
- 连续读取 100 次检查数据稳定性
- CRC 校验必须通过，不通过时丢弃并重试

### 性能指标
| 指标 | DS18B20 | SHT30 | NTC |
|------|---------|-------|-----|
| 单次读取耗时 | ~750ms | ~20ms | <1ms |
| 最小采样间隔 | 750ms | 1s(单次) | 10ms |
| 功耗 | 1.5mA(工作) | 1.2mA(测量) | <0.1mA |

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| DS18B20 读到 85°C | 转换未完成就读取 | 确保等待 750ms 或轮询 DQ 线变高 |
| DS18B20 读到 -127°C | 数据线未接上拉电阻 | 检查 4.7kΩ 上拉电阻 |
| 1-Wire 时序异常 | 中断打断时序 | 关键时序段关中断 |
| DHT22 读数全0/全1 | 采样间隔 < 2s | 确保两次读取间隔 ≥ 2s |
| SHT30 I2C 无响应 | ADDR 悬空 | ADDR 必须接 VDD 或 VSS |
| SHT30 CRC 错误 | I2C 线路干扰 | 检查上拉电阻、缩短线长、加滤波 |
| NTC 温度偏差大 | B值/阻值不匹配 | 核对数据手册参数，多点校准 |
| NTC 高温读数跳变 | ADC 分辨率不足 | 增大 ADC 采样位数或换 Pt100 |
| 多个 DS18B20 地址冲突 | 未使用 MATCH_ROM | 使用 READ_ROM 读取 ROM 序列号后逐个寻址 |

## 相关文档

- `../../templates/driver-template-i2c.c` — I2C 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-spi.c` — SPI 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-adc.c` — ADC 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `../../guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `../../guides/debugging-testing.md` — 调试与测试规范
- `../../guides/pitfalls.md` — 跨类别常见问题与避坑指南
