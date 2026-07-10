# 压力传感器开发规范

## 1. 概述与选型指南

### 常见型号对比

| 型号 | 类型 | 接口 | 量程 | 精度 | 供电 | 特点 | 价格 |
|------|------|------|------|------|------|------|------|
| BMP280 | 气压 | I2C/SPI | 300~1100hPa | ±1hPa | 1.71~3.6V | 低功耗、海拔测量 | ¥5~12 |
| BME280 | 气压+温湿度 | I2C/SPI | 300~1100hPa | ±1hPa | 1.71~3.6V | 三合一、广泛使用 | ¥8~15 |
| MPX5700AP | 表压 | 模拟(ADC) | 15~115kPa | ±1.5% | 4.75~5.25V | 绝对压力、汽车级 | ¥15~30 |
| MPX5010DP | 差压 | 模拟(ADC) | 0~10kPa | ±5% | 4.75~5.25V | 差压测量、流量计 | ¥20~40 |
| HX711 | 24bit ADC | GPIO(串行) | 依传感器 | 24bit | 2.6~5.5V | 称重专用ADC、高精度 | ¥3~8 |
| MPS20N0040D-D | 表压 | 模拟(ADC) | 0~40kPa | ±2.5% | 5V | 廉价表压、需AMP | ¥2~5 |

### 选型决策树

```
测量大气压/海拔？ → 是 → 需温湿度？ → 是 → BME280
                                  否 → BMP280
差压测量？ → 是 → MPX5010DP (差压专用)
称重/测力？ → 是 → HX711 + 称重传感器 (24bit高精度)
低预算表压？ → 是 → MPS20N0040D-D (需配运放电路)
绝对压力(工业)？ → 是 → MPX5700AP
通用推荐 → BME280 (气压+温湿度，I2C接口，性价比优)
```

## 2. 硬件设计规范

### BMP280/BME280 电路 (I2C)

```
VCC(3.3V) ── 4.7kΩ ── SCL ── BMP280 SCL
VCC(3.3V) ── 4.7kΩ ── SDA ── BMP280 SDA
VCC(3.3V) ────────────── BMP280 VDD
                          BMP280 CSB → VCC (选I2C模式)
                          BMP280 SDO → GND (地址0x76)
GND ──────────────────── BMP280 GND
```

**要点**：
- SCL/SDA 各接 4.7kΩ~10kΩ 上拉电阻
- VDD 旁放 100nF 去耦电容
- CSB 接 VCC 选择 I2C 模式；接 GND 选择 SPI 模式
- SDO 接 GND 地址为 0x76；接 VCC 地址为 0x77
- BME280 需暴露在气流中，不可密封

### HX711 称重电路

```
                    ┌── E+ ── 称重传感器(红)
VCC(5V) ────────────┤
                    ├── E- ── GND(黑)
                    │
HX711               ├── A+ ── 传感器A(白)
                    ├── A- ── 传感器A(绿)
                    │
MCU GPIO_SCK ───────┤ SCK
MCU GPIO_DOUT ──────┤ DOUT
                    │
VCC(5V) ────────────┤ VCC
                    ├── VFB → GND (增益128)
GND ────────────────┤ GND
                    └── AVDD ── 100nF ── GND
```

**要点**：
- HX711 供电 5V，MCU GPIO 需电平转换（若 MCU 为 3.3V）
- 传感器线缆使用屏蔽线，屏蔽层接 GND
- 传感器供电源 E+/E- 电压需稳定，纹波 < 10mV
- PCB 上 HX711 靠近传感器，模拟走线短且远离数字信号
- AVDD 旁加 100nF + 10μF 滤波电容
- 增益选择：通道A增益128（满量程±20mV），通道B增益64（满量程±40mV）

### MPS20N0040D-D 表压电路

```
VCC(5V) ── 1kΩ ──┬── 传感器 VCC
                  │
                  100nF
                  │
GND ──────────────┴── 传感器 GND

传感器 OUT ──┬── AMP(LM358) ── ADC输入
             │
          100nF(滤波)
             │
GND ────────┘

运放电路(同相放大):
 VCC ── R1(1kΩ) ──┬── LM358 OUT ── ADC
                   │
              Rf(10kΩ)
                   │
传感器OUT ── R2(1kΩ) ── LM358 IN+
                   │
 GND ── Rg(1kΩ) ──┘
```

**要点**：
- MPS20N0040D-D 输出信号微弱（mV级），必须配运放放大
- 运放选低噪声轨到轨型（LM358/MCP6002）
- 供电 5V，需 LDO 稳压
- 传感器与运放之间走线最短化

## 3. 驱动开发规范

### 统一接口定义

```c
typedef enum {
    PRESS_SENSOR_BMP280, PRESS_SENSOR_BME280, PRESS_SENSOR_MPX5700,
    PRESS_SENSOR_MPX5010, PRESS_SENSOR_HX711, PRESS_SENSOR_MPS20N0040D,
} press_sensor_type_t;

typedef enum {
    PRESS_OK = 0, PRESS_ERR_NOT_FOUND, PRESS_ERR_CRC,
    PRESS_ERR_TIMEOUT, PRESS_ERR_INIT, PRESS_ERR_INVALID_PARAM,
    PRESS_ERR_CALIBRATION,
} press_status_t;

typedef struct {
    float pressure_hpa;     /* 气压 hPa */
    float pressure_kpa;     /* 压力 kPa */
    float altitude_m;       /* 海拔 m (气压传感器) */
    float weight_g;         /* 重量 g (称重传感器) */
    int64_t timestamp_ms;
    bool data_valid;
} press_data_t;

press_status_t press_sensor_init(press_sensor_handle_t *handle, void *hw_config);
press_status_t press_sensor_read(press_sensor_handle_t *handle, press_data_t *data);
void press_sensor_deinit(press_sensor_handle_t *handle);
```

### BMP280 I2C 驱动代码

```c
/* BMP280 寄存器定义 */
#define BMP280_REG_ID            0xD0
#define BMP280_REG_RESET         0xE0
#define BMP280_REG_CTRL_MEAS     0xF4
#define BMP280_REG_CONFIG        0xF5
#define BMP280_REG_PRESS_MSB     0xF7
#define BMP280_REG_TEMP_MSB      0xFA
#define BMP280_REG_CALIB00       0x88  /* 温度校准参数起始 */
#define BMP280_REG_CALIB26       0xE1  /* 压力校准参数起始 */
#define BMP280_CHIP_ID           0x58
#define BMP280_RESET_VAL         0xB6

/* 校准参数结构体 */
typedef struct {
    uint16_t dig_T1; int16_t dig_T2, dig_T3;
    uint16_t dig_P1; int16_t dig_P2, dig_P3, dig_P4, dig_P5;
    int16_t dig_P6, dig_P7, dig_P8, dig_P9;
} bmp280_calib_t;

/* 读取校准参数 */
static press_status_t bmp280_read_calib(bmp280_handle_t *h) {
    uint8_t buf[24];
    if (i2c_read_reg(h->i2c, h->addr, BMP280_REG_CALIB00, buf, 24))
        return PRESS_ERR_TIMEOUT;

    h->calib.dig_T1 = (buf[1] << 8) | buf[0];
    h->calib.dig_T2 = (buf[3] << 8) | buf[2];
    h->calib.dig_T3 = (buf[5] << 8) | buf[4];
    h->calib.dig_P1 = (buf[7] << 8) | buf[6];
    h->calib.dig_P2 = (buf[9] << 8) | buf[8];
    h->calib.dig_P3 = (buf[11] << 8) | buf[10];
    h->calib.dig_P4 = (buf[13] << 8) | buf[12];
    h->calib.dig_P5 = (buf[15] << 8) | buf[14];
    h->calib.dig_P6 = (buf[17] << 8) | buf[16];
    h->calib.dig_P7 = (buf[19] << 8) | buf[18];
    h->calib.dig_P8 = (buf[21] << 8) | buf[20];
    h->calib.dig_P9 = (buf[23] << 8) | buf[22];
    return PRESS_OK;
}

/* 初始化 */
press_status_t bmp280_init(bmp280_handle_t *h, i2c_bus_t *i2c, uint8_t addr) {
    h->i2c = i2c;
    h->addr = addr;
    h->t_fine = 0;

    /* 验证芯片ID */
    uint8_t id;
    if (i2c_read_reg(i2c, addr, BMP280_REG_ID, &id, 1)) return PRESS_ERR_NOT_FOUND;
    if (id != BMP280_CHIP_ID) return PRESS_ERR_NOT_FOUND;

    /* 读取校准参数 */
    press_status_t st = bmp280_read_calib(h);
    if (st != PRESS_OK) return st;

    /* 配置: 温度x1, 压力x1, 正常模式 */
    uint8_t ctrl = 0x27;  /* osrs_t=001, osrs_p=001, mode=11(正常) */
    uint8_t cfg  = 0xA0;  /* t_sb=1000ms(101), filter=off(000), spi3w=0 */
    if (i2c_write_reg(i2c, addr, BMP280_REG_CTRL_MEAS, &ctrl, 1)) return PRESS_ERR_INIT;
    if (i2c_write_reg(i2c, addr, BMP280_REG_CONFIG, &cfg, 1)) return PRESS_ERR_INIT;

    h->initialized = true;
    return PRESS_OK;
}

/* 温度补偿计算 (返回int32, 更新t_fine) */
static int32_t bmp280_compensate_temp(bmp280_handle_t *h, int32_t adc_T) {
    int32_t var1, var2;
    var1 = ((((adc_T >> 3) - ((int32_t)h->calib.dig_T1 << 1)))
            * ((int32_t)h->calib.dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)h->calib.dig_T1))
              * ((adc_T >> 4) - ((int32_t)h->calib.dig_T1))) >> 12)
            * ((int32_t)h->calib.dig_T3)) >> 14;
    h->t_fine = var1 + var2;
    return (h->t_fine * 5 + 128) >> 8;  /* 0.01°C */
}

/* 压力补偿计算 (返回Pa) */
static uint32_t bmp280_compensate_press(bmp280_handle_t *h, int32_t adc_P) {
    int32_t var1, var2;
    uint32_t p;
    var1 = ((int32_t)h->t_fine >> 1) - 64000;
    var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * ((int32_t)h->calib.dig_P6);
    var2 = var2 + ((var1 * ((int32_t)h->calib.dig_P5)) << 1);
    var2 = (var2 >> 2) + (((int32_t)h->calib.dig_P4) << 16);
    var1 = (((h->calib.dig_P3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3)
            + ((((int32_t)h->calib.dig_P2) * var1) >> 1)) >> 18;
    var1 = ((((32768 + var1)) * ((int32_t)h->calib.dig_P1)) >> 15);
    if (var1 == 0) return 0;  /* 避免除零 */
    p = (((uint32_t)((1048576 - adc_P) - (var2 >> 12))) * 3125) / var1;
    var1 = (((int32_t)h->calib.dig_P9) * ((int32_t)(((p >> 3) * (p >> 3)) >> 13))) >> 12;
    var2 = (((int32_t)(p >> 2)) * ((int32_t)h->calib.dig_P8)) >> 13;
    p = (uint32_t)((int32_t)p + ((var1 + var2 + h->calib.dig_P7) >> 4));
    return p;  /* Pa */
}

/* 读取气压 (hPa) */
press_status_t bmp280_read(bmp280_handle_t *h, float *pressure, float *temperature) {
    uint8_t data[6];
    if (i2c_read_reg(h->i2c, h->addr, BMP280_REG_PRESS_MSB, data, 6))
        return PRESS_ERR_TIMEOUT;

    int32_t adc_P = ((int32_t)data[0] << 12) | ((int32_t)data[1] << 4)
                    | (data[2] >> 4);
    int32_t adc_T = ((int32_t)data[3] << 12) | ((int32_t)data[4] << 4)
                    | (data[5] >> 4);

    /* 先算温度更新t_fine, 再算压力 */
    int32_t temp_cx100 = bmp280_compensate_temp(h, adc_T);
    uint32_t press_pa  = bmp280_compensate_press(h, adc_P);

    *temperature = temp_cx100 / 100.0f;
    *pressure    = press_pa / 100.0f;  /* Pa → hPa */
    return PRESS_OK;
}

/* 海拔计算 (基于标准大气压) */
float bmp280_calc_altitude(float pressure_hpa, float sea_level_hpa) {
    /* 国际标准大气模型: h = 44330 * (1 - (P/P0)^(1/5.255)) */
    return 44330.0f * (1.0f - powf(pressure_hpa / sea_level_hpa, 0.1903f));
}
```

### HX711 24bit ADC 读取代码

```c
/* HX711 增益与通道选择 */
typedef enum {
    HX711_GAIN_A_128 = 1,  /* 通道A, 增益128, 下次读数后25个时钟 */
    HX711_GAIN_B_32  = 2,  /* 通道B, 增益32,  下次读数后26个时钟 */
    HX711_GAIN_A_64  = 3,  /* 通道A, 增益64,  下次读数后27个时钟 */
} hx711_gain_t;

/* 等待数据就绪 (DOUT=0) */
static bool hx711_wait_ready(hx711_handle_t *h, uint32_t timeout_ms) {
    uint32_t elapsed = 0;
    while (gpio_read(h->dout_port, h->dout_pin)) {
        delay_ms(1);
        if (++elapsed > timeout_ms) return false;
    }
    return true;
}

/* 读取24bit原始数据 */
static int32_t hx711_read_raw(hx711_handle_t *h) {
    if (!hx711_wait_ready(h, 1000)) return 0;

    int32_t value = 0;
    /* 读取24bit数据, MSB first */
    for (int i = 0; i < 24; i++) {
        gpio_write_high(h->sck_port, h->sck_pin);
        delay_us(1);
        value = (value << 1) | gpio_read(h->dout_port, h->dout_pin);
        gpio_write_low(h->sck_port, h->sck_pin);
        delay_us(1);
    }

    /* 设置增益/通道: 发送额外脉冲 */
    uint8_t extra_pulses = (uint8_t)h->gain;
    for (int i = 0; i < extra_pulses; i++) {
        gpio_write_high(h->sck_port, h->sck_pin);
        delay_us(1);
        gpio_write_low(h->sck_port, h->sck_pin);
        delay_us(1);
    }

    /* 24bit 有符号数扩展为32bit */
    if (value & 0x800000) value |= 0xFF000000;
    return value;
}

/* 初始化 */
press_status_t hx711_init(hx711_handle_t *h, gpio_port_t *sck_port, uint8_t sck_pin,
                          gpio_port_t *dout_port, uint8_t dout_pin, hx711_gain_t gain) {
    h->sck_port = sck_port; h->sck_pin = sck_pin;
    h->dout_port = dout_port; h->dout_pin = dout_pin;
    h->gain = gain;
    h->offset = 0;
    h->scale = 1.0f;
    h->zero_set = false;

    gpio_set_output(sck_port, sck_pin);
    gpio_write_low(sck_port, sck_pin);
    gpio_set_input(dout_port, dout_pin);

    /* 上电后等待芯片就稳 */
    delay_ms(100);

    /* 丢弃前几次读数 */
    for (int i = 0; i < 3; i++) {
        hx711_read_raw(h);
        delay_ms(10);
    }
    h->initialized = true;
    return PRESS_OK;
}

/* 去皮(零点校准) */
press_status_t hx711_tare(hx711_handle_t *h, uint8_t samples) {
    int64_t sum = 0;
    for (uint8_t i = 0; i < samples; i++) {
        int32_t raw = hx711_read_raw(h);
        sum += raw;
    }
    h->offset = sum / samples;
    h->zero_set = true;
    return PRESS_OK;
}

/* 读取重量(g) */
press_status_t hx711_read_weight(hx711_handle_t *h, float *weight_g) {
    /* 多次采样取平均 */
    int64_t sum = 0;
    uint8_t samples = 5;
    for (uint8_t i = 0; i < samples; i++) {
        int32_t raw = hx711_read_raw(h);
        sum += raw;
    }
    int32_t avg = sum / samples;

    /* 减去偏移, 乘以比例系数 */
    *weight_g = (avg - h->offset) / h->scale;
    return PRESS_OK;
}

/* 设置标定系数 (用已知重量校准) */
void hx711_set_scale(hx711_handle_t *h, float scale) {
    h->scale = scale;
    /* scale = (原始值 - offset) / 已知重量g */
}

/* 完整校准流程示例 */
press_status_t hx711_calibrate(hx711_handle_t *h, float known_weight_g) {
    /* 步骤1: 空载去皮 */
    hx711_tare(h, 10);
    delay_ms(500);

    /* 步骤2: 放上已知重量, 读取原始值 */
    int64_t sum = 0;
    for (int i = 0; i < 10; i++) {
        int32_t raw = hx711_read_raw(h);
        sum += raw;
    }
    int32_t loaded = sum / 10;

    /* 步骤3: 计算标定系数 */
    int32_t diff = loaded - h->offset;
    if (diff == 0) return PRESS_ERR_CALIBRATION;
    h->scale = (float)diff / known_weight_g;
    return PRESS_OK;
}
```

### MPX 系列模拟读取

```c
/* MPX5700AP 气压读取 (绝对压力) */
press_status_t mpx5700_read(mpx5700_handle_t *h, float *pressure_kpa) {
    uint32_t sum = 0;
    for (int i = 0; i < 16; i++) {
        uint16_t val;
        if (adc_read(h->adc, h->channel, &val)) return PRESS_ERR_TIMEOUT;
        sum += val;
    }
    float adc_avg = sum / 16.0f;
    float v_out = adc_avg * h->vref / h->adc_max;

    /* MPX5700AP: V_out = V_s × (0.009 × P - 0.0428)
     * P(kPa) = (V_out / V_s + 0.0428) / 0.009
     * Vs=5V: P = (V_out/5 + 0.0428) / 0.009 */
    *pressure_kpa = (v_out / h->vref + 0.0428f) / 0.009f;
    return PRESS_OK;
}
```

## 4. 调试与测试规范

### 硬件验证清单
- [ ] 万用表确认 VDD/GND 电压正确（BMP280 为 3.3V，HX711/MPX 为 5V）
- [ ] I2C 总线：示波器检查 SCL/SDA 波形幅值和上升沿
- [ ] HX711：确认 SCK/DOUT 连线正确，DOUT 初始为高电平
- [ ] 称重传感器：万用表测量各线阻值（激励±约 350Ω，信号±约 350Ω）
- [ ] 确认去耦电容已贴装

### 通信验证
- **BMP280**：I2C 扫描确认地址 0x76/0x77 响应；读取 ID 寄存器应为 0x58
- **BME280**：同 BMP280，ID 寄存器应为 0x60
- **HX711**：上电后 DOUT 应为高电平，SCK 拉低后 DOUT 应在数据就绪时变低
- **MPX**：万用表测模拟输出电压，对比数据手册公式计算值

### 数据校验
- BMP280 气压标准海平面约 1013.25hPa，误差 ±1hPa
- 称重传感器：用已知重量校准，误差 < 0.1%
- 连续读取 100 次检查数据稳定性，标准差 < 满量程 0.1%

### 性能指标

| 指标 | BMP280 | HX711 | MPX5700 |
|------|--------|-------|---------|
| 单次读取耗时 | ~8ms | ~100ms | <1ms |
| 最小采样间隔 | 10ms | 100ms | 10ms |
| 功耗 | 2.7μA(1Hz) | 1.6mA | 7mA |
| 分辨率 | 0.0016hPa | 24bit | 12bit ADC |

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| BMP280 I2C 无响应 | SDO/CSB 悬空 | CSB 接 VCC 选 I2C；SDO 接 GND/VCC 定地址 |
| BMP280 气压偏差大 | 未读取校准参数 | 必须先读取 0x88/0xE1 处校准数据 |
| BMP280 读数全0 | 转换未完成 | 配置为正常模式或读前等待转换完成 |
| BME280 湿度读数异常 | 温度补偿未做 | 先算温度更新 t_fine，再算气压/湿度 |
| HX711 读数跳变 | SCK 频率过高 | SCK 高电平时间 0.2~50μs，不要过快 |
| HX711 读数不稳 | 电源噪声/线干扰 | 加 LC 滤波，屏蔽线，远离开关电源 |
| HX711 一直等待就绪 | DOUT 接线错误/芯片损坏 | 检查 DOUT 连线，确认上电后 DOUT=高 |
| 称重重复性差 | 传感器安装应力 | 确保传感器受力面平整，避免侧向力 |
| 称重漂移 | 温度变化/蠕变 | 预热 15 分钟，定期去皮 |
| MPX 读数不对 | 运放增益错误 | 核对运放增益与传感器输出范围匹配 |
| MPS20N0040D 信号太弱 | 未配运放 | 必须加运放电路放大 mV 级信号 |
| 海拔计算偏差大 | 海平面气压未校准 | 用当地实际海平面气压代替 1013.25hPa |
| HX711 增益切换无效 | 脉冲数不对 | 增益128=1脉冲, 64=3脉冲, 32=2脉冲 |

## 相关文档

- `skill://mcu-driver-core/templates/driver-template-i2c.c` — I2C 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-spi.c` — SPI 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-adc.c` — ADC 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
- `skill://mcu-driver-core/guides/pitfalls.md` — 跨类别常见问题与避坑指南
