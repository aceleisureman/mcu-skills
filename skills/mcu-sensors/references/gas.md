# 气体与粉尘传感器开发规范

## 1. 概述与选型指南

### 常见型号对比

| 型号 | 检测对象 | 接口 | 量程 | 供电 | 加热 | 特点 | 价格 |
|------|---------|------|------|------|------|------|------|
| MQ-2 | 可燃气体/烟雾 | 模拟(ADC) | 300~10000ppm | 5V | 5V加热 | 廉价、通用 | ¥3~6 |
| MQ-135 | 空气质量(NH3/CO2) | 模拟(ADC) | 10~1000ppm | 5V | 5V加热 | 多气体、空气监测 | ¥4~8 |
| MQ-7 | 一氧化碳(CO) | 模拟(ADC) | 20~2000ppm | 5V | 5V/1.4V循环 | 需加热循环 | ¥4~8 |
| CCS811 | eCO2/TVOC | I2C | 400~8192ppm / 0~1187ppb | 3.3V | 内置 | 数字输出、基线算法 | ¥8~20 |
| SGP30 | eCO2/TVOC | I2C | 400~60000ppm / 0~60000ppb | 1.62~1.98V | 内置 | 内置基线校准、多像素 | ¥15~30 |
| GP2Y1014AU | PM2.5 粉尘 | 模拟(ADC)+GPIO | 0~500μg/m³ | 5V | 内部LED脉冲 | 低成本、单粒子散射 | ¥8~15 |

### 选型决策树

```
检测可燃气体/烟雾？ → 是 → MQ-2 (通用可燃气体)
检测CO(一氧化碳)？ → 是 → MQ-7 (需加热循环, 精度较高)
检测空气质量？ → 是 → 预算低？ → 是 → MQ-135 (模拟输出)
                          否 → 数字接口？ → 是 → SGP30 (I2C, 内置基线)
                                              否 → CCS811 (I2C, eCO2/TVOC)
检测PM2.5/粉尘？ → 是 → GP2Y1014AU (模拟输出, 内部LED脉冲采样)
需要精确CO2测量？ → 是 → MQ系列不适合 → 使用NDIR传感器(如MH-Z19)
通用推荐 → SGP30 (I2C数字输出, 内置基线校准, TVOC+eCO2)
```

## 2. 硬件设计规范

### MQ 系列传感器电路 (模拟输出)

```
VCC(5V) ────────────── MQ模块 VCC(A/H引脚, 加热)
GND ─────────────────── MQ模块 GND
                        MQ模块 AOUT ──┬── ADC输入
                                        │
                                    100nF(滤波)
                                        │
GND ────────────────────────────────────┘

(MQ传感器内部电路):
VCC(5V) ── RH(加热电阻) ── GND     (加热回路)
VCC(5V) ── RL(负载电阻) ──┬── AOUT  (传感回路)
                           │
                        Rs(传感电阻, 随气体浓度变化)
                           │
GND ─────────────────────┘

AOUT电压: V_out = VCC * RL / (RL + Rs)
```

**要点**：
- 加热电压必须稳定 5V±0.1V，纹波影响灵敏度
- 加热功耗大（MQ-2 约 0.8W），MCU 供电需充足
- 上电后需预热 24~48 小时首次使用，日常预热 ≥ 2 分钟
- ADC 输入端加 100nF 滤波电容
- 传感器有方向性，金属网面朝向被测气体方向
- 不可用于密闭环境，需保持空气流通

### MQ 系列双通道检测电路（AO + DO）

实际安全监控场景常需同时获取模拟量和数字告警，MQ 模块通常同时引出 AO（模拟输出）和 DO（数字阈值输出）：

```
MQ-2 / MQ-7 模块

VCC(5V) ────────── 模块 VCC
GND ────────────── 模块 GND

模块 AO ──┬── 100nF ── GND    (模拟滤波)
           │
           └── MCU ADC 引脚 (如 PA1, ADC1_IN1)

模块 DO ──┬── 10kΩ 上拉 ── 3.3V   (数字量, 低电平=超标)
           │
           ├── MCU GPIO 输入 (如 PA0)
           │
           └── 模块电位器调节阈值
```

**双通道优势**：
- **AO（模拟量）**：通过 ADC 读取 0~4095 原始值，可实时跟踪浓度变化趋势，设置软件阈值
- **DO（数字量）**：模块板载比较器输出，低电平表示超标，可用于中断触发或快速判断
- **双重判断**：`DO 触发 || ADC ≥ 阈值` 即判定超标，提高可靠性

**接线要点**：
- MQ 模块供电 5V，AO 输出范围 0~5V，3.3V MCU 的 ADC 不可直连（需分压或选 3.3V 兼容模块）
- DO 通常为开漏输出或 5V 电平，3.3V MCU 需电平转换或选 3.3V 兼容模块
- 实际项目中常用带分压电阻的转接板，AO 经分压后接入 3.3V ADC

**双通道检测示例（MQ-2 + MQ-7 气瓶柜场景）**：

| 传感器 | AO 引脚 | DO 引脚 | 用途 | ADC 阈值 |
|--------|---------|---------|------|----------|
| MQ-2 可燃气体 | PA1 (ADC1_IN1) | PA0 | 检测 LPG/甲烷/烟雾 | 2000 |
| MQ-7 一氧化碳 | PA6 (ADC1_IN6) | PA7 | 检测 CO 浓度 | 2000 |

```c
/* 双通道气体检测：AO(ADC) + DO(GPIO) 联合判断 */
uint8_t gas_check(uint16_t mq2_adc, uint16_t mq7_adc) {
    uint8_t alarm = 0;
    if (mq2_do_alarm() || mq2_adc >= 2000)  alarm |= 1;  /* 可燃气体超标 */
    if (mq7_do_alarm() || mq7_adc >= 2000)  alarm |= 2;  /* CO 超标 */
    return alarm;
}
```

### MQ-7 加热循环电路

```
VCC(5V) ────────────────── MQ-7 VH(加热A)
                           MQ-7 VH(加热B)
GND ────────────────────── MQ-7 GND

(加热循环控制 - 需MOSFET切换电压):
MCU PWM ── MOSFET栅极 ── 控制 VH 电压

加热周期: 5V(60秒) → 1.4V(90秒) → 循环
  5V高温: 清除传感器表面吸附气体(热清洁)
  1.4V低温: 在此期间读取ADC(实际测量阶段)
```

**要点**：
- MQ-7 不能持续 5V 加热，必须高低交替循环
- 1.4V 加热阶段才能读取有效数据
- 使用 MOSFET 或 LDO 切换加热电压
- 循环周期：60 秒 5V + 90 秒 1.4V = 150 秒

### SGP30 电路 (I2C)

```
VCC(1.8V) ── 4.7kΩ ── SCL ── SGP30 SCL
VCC(1.8V) ── 4.7kΩ ── SDA ── SGP30 SDA
VCC(1.8V) ────────────── SGP30 VDD
                          SGP30 ADDR → GND (地址0x58)
GND ──────────────────── SGP30 GND
```

**要点**：
- SGP30 供电 1.62~1.98V（不可接 3.3V，需 LDO 降压）
- 若 MCU 为 3.3V，I2C 需电平转换或使用 3.3V 兼容模块
- SCL/SDA 上拉电阻接 1.8V 域
- 传感器需暴露在空气中，不可密封
- 内置加热器，功耗约 8mA@1.8V

### CCS811 电路 (I2C)

```
VCC(3.3V) ── 4.7kΩ ── SCL ── CCS811 SCL
VCC(3.3V) ── 4.7kΩ ── SDA ── CCS811 SDA
VCC(3.3V) ────────────── CCS811 VDD
VCC(3.3V) ────────────── CCS811 nWAKE → 接GND或MCU GPIO
                          CCS811 nINT → MCU GPIO (可选)
                          CCS810 nRST → MCU GPIO (可选)
GND ──────────────────── CCS811 GND
```

**要点**：
- 供电 3.3V，I2C 地址 0x5A（ADDR 接 GND）或 0x5B
- nWAKE 拉低才能通信，通信时拉高
- 首次使用需等待 20 分钟基线稳定
- 可配合温湿度传感器做补偿（SHT30 等）

### GP2Y1014AU 粉尘传感器电路 (ADC + GPIO)

```
                        5V
                        │
                    150Ω(限流)
                        │
GP2Y1014AU ── V-LED ────┘    (LED 正极, 内部LED驱动)
GP2Y1014AU ── LED-GND ── GND  (LED 负极)

MCU GPIO (ILED) ── 150Ω ── GP2Y1014AU V-LED  (脉冲驱动内部LED)

GP2Y1014AU ── VOUT ──┬── 分压电阻 (转接板: 1kΩ + 10kΩ)
                      │
                      ├── ADC 输入引脚 (如 PA0)
                      │
                    10kΩ (下拉)
                      │
GND ─────────────────┘

GP2Y1014AU ── VCC ─── 5V
GP2Y1014AU ── GND ─── GND
```

**接线说明（参考实际模块）**：

| GP2Y1014AU 引脚 | STM32 引脚 | 说明 |
|-----------------|-----------|------|
| VCC | 5V | 传感器供电 |
| GND | GND | 公共地 |
| VOUT (AO) | PA0 (ADC1_CH0) | 模拟输出，经分压电阻接入 |
| V-LED (ILED) | PA2 (GPIO 推挽) | 内部 LED 脉冲控制 |

**要点**：
- 传感器供电 5V，VOUT 输出最高可达 3.5V 以上，3.3V MCU 的 ADC 不能直接接，需分压
- 转接板通常使用 1kΩ + 10kΩ 分压（分压比 1/11），读取后电压值需乘以 11 还原
- ILED 控制引脚用于触发内部 LED 脉冲，采样时序严格：高电平 0.28ms 后采样，总脉宽 0.32ms
- 采样周期 10ms（LED 脉冲 0.32ms + 恢复 9.68ms），不可连续采样
- 传感器内部有放大电路，VOUT 与粉尘浓度成近似线性关系
- 安装时传感器开口朝下或水平，避免灰尘沉积在光路上
- 不可在凝露环境中使用

## 3. 驱动开发规范

### 统一接口定义

```c
typedef enum {
    GAS_SENSOR_MQ2, GAS_SENSOR_MQ135, GAS_SENSOR_MQ7,
    GAS_SENSOR_CCS811, GAS_SENSOR_SGP30,
} gas_sensor_type_t;

typedef enum {
    GAS_OK = 0, GAS_ERR_NOT_FOUND, GAS_ERR_CRC,
    GAS_ERR_TIMEOUT, GAS_ERR_INIT, GAS_ERR_INVALID_PARAM,
    GAS_ERR_WARMUP, GAS_ERR_CALIBRATION,
} gas_status_t;

typedef struct {
    float co2_ppm;         /* eCO2 浓度 ppm */
    float tvoc_ppb;        /* TVOC 浓度 ppb */
    float gas_ppm;         /* 气体浓度 ppm (MQ系列) */
    float rs_ratio;        /* Rs/R0 比值 (MQ系列) */
    int64_t timestamp_ms;
    bool data_valid;
} gas_data_t;

gas_status_t gas_sensor_init(gas_sensor_handle_t *handle, void *hw_config);
gas_status_t gas_sensor_read(gas_sensor_handle_t *handle, gas_data_t *data);
void gas_sensor_deinit(gas_sensor_handle_t *handle);
```

### MQ 系列 ADC 读取代码

```c
/* MQ传感器配置 */
typedef struct {
    adc_bus_t *adc;
    uint8_t channel;
    float vref;        /* ADC参考电压(通常5V) */
    uint16_t adc_max;  /* ADC满量程(如4095) */
    float rl;          /* 负载电阻阻值(Ω), 通常10kΩ */
    float r0;          /* 干净空气中Rs值(校准获得) */
    float cal_a;       /* 对数拟合系数a: log(ppm) = a * log(Rs/R0) + b */
    float cal_b;       /* 对数拟合系数b */
} mq_handle_t;

/* 读取传感电阻Rs */
static float mq_read_rs(mq_handle_t *h) {
    /* 32次采样取平均降噪 */
    uint32_t sum = 0;
    for (int i = 0; i < 32; i++) {
        uint16_t val;
        if (adc_read(h->adc, h->channel, &val)) return -1;
        sum += val;
    }
    float adc_avg = sum / 32.0f;
    float v_out = adc_avg * h->vref / h->adc_max;

    /* V_out = VCC * RL / (RL + Rs)
     * Rs = RL * (VCC - V_out) / V_out */
    if (v_out < 0.01f) return 1e6f;  /* 防止除零 */
    float rs = h->rl * (h->vref - v_out) / v_out;
    return rs;
}

/* 读取气体浓度(ppm) */
gas_status_t mq_read_gas(mq_handle_t *h, float *ppm) {
    float rs = mq_read_rs(h);
    if (rs < 0) return GAS_ERR_TIMEOUT;

    /* 计算Rs/R0比值 */
    float ratio = rs / h->r0;
    if (ratio < 0.01f) ratio = 0.01f;

    /* 对数拟合: log(ppm) = a * log(Rs/R0) + b */
    float log_ppm = h->cal_a * log10f(ratio) + h->cal_b;
    *ppm = powf(10.0f, log_ppm);

    /* 限幅 */
    if (*ppm < 0) *ppm = 0;
    h->last_rs_ratio = ratio;
    return GAS_OK;
}

/* R0校准: 在干净空气中采集Rs作为R0 */
gas_status_t mq_calibrate_r0(mq_handle_t *h, uint16_t samples) {
    /* 警告: 必须在干净空气中预热24小时后进行 */
    float rs_sum = 0;
    for (uint16_t i = 0; i < samples; i++) {
        float rs = mq_read_rs(h);
        if (rs < 0) return GAS_ERR_TIMEOUT;
        rs_sum += rs;
        delay_ms(100);  /* 10Hz采样 */
    }
    h->r0 = rs_sum / samples;
    h->calibrated = true;
    return GAS_OK;
}

/* MQ-2 校准系数 (参考数据手册曲线) */
/* log(ppm) = -0.43 * log(Rs/R0) + 1.93 (丙烷) */
void mq2_set_calibration(mq_handle_t *h) {
    h->cal_a = -0.43f;
    h->cal_b = 1.93f;
}

/* MQ-7 CO校准系数 */
/* log(ppm) = -1.41 * log(Rs/R0) + 2.82 */
void mq7_set_calibration(mq_handle_t *h) {
    h->cal_a = -1.41f;
    h->cal_b = 2.82f;
}

/* MQ-135 空气质量校准系数 (CO2近似) */
/* log(ppm) = -0.42 * log(Rs/R0) + 2.95 */
void mq135_set_calibration(mq_handle_t *h) {
    h->cal_a = -0.42f;
    h->cal_b = 2.95f;
}
```

### GP2Y1014AU 粉尘传感器驱动代码

```c
/* ====== 配置参数（需根据实际硬件校准） ====== */
#define GP2Y_NO_DUST_VOLTAGE   400     /* 遮住传感器时的零点电压(mV)，通常 400~900 */
#define GP2Y_COV_RATIO         0.20f   /* 灵敏度转换系数 (μg/m³ per mV)，通常 0.17~0.20 */
#define GP2Y_DIVIDER_RATIO     11      /* 分压比：1kΩ+10kΩ → 1/11，还原时乘 11 */
#define GP2Y_VREF_MV           3300    /* ADC 参考电压 (mV) */
#define GP2Y_ADC_MAX           4096    /* 12 位 ADC 满量程 */
#define GP2Y_LED_PULSE_US      280     /* LED 点亮后等待采样时间 (μs) */
#define GP2Y_LED_TOTAL_US      320     /* LED 总脉宽 (μs)，采样后补足到 0.32ms */
#define GP2Y_RECOVERY_US       9680    /* 采样后恢复时间 (μs)，总周期 10ms */
#define GP2Y_FILTER_SIZE       10      /* 滑动平均滤波窗口 */

/* ====== 驱动结构 ====== */
typedef struct {
    adc_bus_t   *adc;           /* ADC 总线句柄 */
    uint8_t      channel;       /* ADC 通道 */
    gpio_pin_t   iled_pin;      /* LED 控制引脚 */
    uint16_t     filter_buf[GP2Y_FILTER_SIZE];
    uint16_t     filter_sum;
    uint8_t      filter_idx;
    bool         filter_init;
    uint16_t     last_raw;      /* 最近一次原始 ADC 值 */
    uint16_t     last_voltage;  /* 最近一次电压 (mV) */
    float        last_density;  /* 最近一次浓度 (μg/m³) */
} gp2y_handle_t;

/* ====== 滑动平均滤波 ====== */
static uint16_t gp2y_filter(gp2y_handle_t *h, uint16_t new_val) {
    if (!h->filter_init) {
        for (int i = 0; i < GP2Y_FILTER_SIZE; i++)
            h->filter_buf[i] = new_val;
        h->filter_sum = (uint32_t)new_val * GP2Y_FILTER_SIZE;
        h->filter_init = true;
        return new_val;
    }
    h->filter_sum -= h->filter_buf[h->filter_idx];
    h->filter_buf[h->filter_idx] = new_val;
    h->filter_sum += new_val;
    h->filter_idx = (h->filter_idx + 1) % GP2Y_FILTER_SIZE;
    return h->filter_sum / GP2Y_FILTER_SIZE;
}

/* ====== 核心采样（严格遵循 datasheet 时序） ====== */
gas_status_t gp2y_read(gp2y_handle_t *h, float *density_ugm3) {
    /* 1. 开启内部 LED */
    gpio_write(h->iled_pin, 1);

    /* 2. 等待 280μs（datasheet 规定的采样窗口） */
    delay_us(GP2Y_LED_PULSE_US);

    /* 3. 立即采集 ADC */
    uint16_t raw;
    if (adc_read(h->adc, h->channel, &raw)) return GAS_ERR_TIMEOUT;
    h->last_raw = raw;

    /* 4. 补足脉宽至 0.32ms 后关闭 LED */
    delay_us(GP2Y_LED_TOTAL_US - GP2Y_LED_PULSE_US);
    gpio_write(h->iled_pin, 0);

    /* 5. 恢复等待（总周期 10ms） */
    delay_us(GP2Y_RECOVERY_US);

    /* 6. 滑动平均滤波 */
    uint16_t filtered = gp2y_filter(h, raw);

    /* 7. 电压换算：ADC → mV，再乘分压比还原 */
    h->last_voltage = (uint16_t)(
        ((uint32_t)filtered * GP2Y_VREF_MV) / GP2Y_ADC_MAX
        * GP2Y_DIVIDER_RATIO
    );

    /* 8. 浓度计算：减去零点电压 × 灵敏度系数 */
    if (h->last_voltage > GP2Y_NO_DUST_VOLTAGE) {
        h->last_density = (float)(h->last_voltage - GP2Y_NO_DUST_VOLTAGE)
                          * GP2Y_COV_RATIO;
    } else {
        h->last_density = 0;
    }

    if (density_ugm3) *density_ugm3 = h->last_density;
    return GAS_OK;
}

/* ====== 零点校准（在干净空气中执行） ====== */
gas_status_t gp2y_calibrate_zero(gp2y_handle_t *h, uint16_t samples) {
    /* 警告: 必须在无尘环境中执行，建议遮住传感器开口 */
    uint32_t voltage_sum = 0;
    for (uint16_t i = 0; i < samples; i++) {
        gp2y_read(h, NULL);
        voltage_sum += h->last_voltage;
        delay_ms(100);
    }
    /* 校准后的零点电压取平均值 */
    uint16_t zero_voltage = voltage_sum / samples;
    /* 注意：校准结果需写入 Flash/EEPROM 持久化 */
    printf("GP2Y zero-point: %d mV (update NO_DUST_VOLTAGE)\n", zero_voltage);
    return GAS_OK;
}
```

**采样时序图**：

```
ILED  ___              ┌──────────────────┐                    ___
       ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾                  ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
                        │←─ 280μs ─→│←40μs→│
                        ↑           ↑
                       LED开       ADC采样点
                                             │←──── 9680μs 恢复 ────→│
                                             │                        │
                                        总周期 = 10ms
```

**浓度换算公式**：

```
V_actual(mV) = (ADC_raw × V_ref / ADC_max) × Divider_Ratio
Density(μg/m³) = max(0, (V_actual - V_zero) × COV_RATIO)

典型参数：
  V_ref     = 3300 mV  (3.3V MCU)
  ADC_max   = 4096     (12-bit)
  Divider   = 11       (1kΩ + 10kΩ 分压板)
  V_zero    = 400 mV   (无粉尘时零点，需实测校准)
  COV_RATIO = 0.20     (μg/m³ per mV)
```

### SGP30 I2C 驱动代码

```c
/* SGP30 命令定义 */
#define SGP30_ADDR               0x58
#define SGP30_CMD_INIT_AIR       0x2003  /* 初始化空气质量测量 */
#define SGP30_CMD_MEASURE_AIR    0x2008  /* 测量空气质量 */
#define SGP30_CMD_GET_BASELINE   0x2015  /* 读取基线 */
#define SGP30_CMD_SET_BASELINE   0x201E  /* 设置基线 */
#define SGP30_CMD_GET_SERIAL     0x3682  /* 读取序列号 */
#define SGP30_CMD_GET_FEATURE    0x202F  /* 读取特性版本 */

/* CRC-8 (多项式0x31, 初始值0xFF) */
static uint8_t sgp30_crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0xFF;
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++)
            crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
    }
    return crc;
}

/* 初始化 */
gas_status_t sgp30_init(sgp30_handle_t *h, i2c_bus_t *i2c) {
    h->i2c = i2c;
    h->addr = SGP30_ADDR;
    h->baseline_valid = false;
    h->measure_count = 0;

    /* 发送初始化空气质量命令 */
    uint8_t cmd[2] = {0x20, 0x03};
    if (i2c_write_raw(i2c, h->addr, cmd, 2)) return GAS_ERR_NOT_FOUND;
    delay_ms(10);  /* 最大10ms */

    /* 恢复基线 (如有存储的基线值) */
    if (h->saved_baseline_co2 != 0 && h->saved_baseline_tvoc != 0) {
        sgp30_set_baseline(h, h->saved_baseline_co2, h->saved_baseline_tvoc);
    }

    h->initialized = true;
    return GAS_OK;
}

/* 读取空气质量 (必须每秒调用一次) */
gas_status_t sgp30_measure_air(sgp30_handle_t *h, uint16_t *co2, uint16_t *tvoc) {
    /* 发送测量命令 */
    uint8_t cmd[2] = {0x20, 0x08};
    if (i2c_write_raw(h->i2c, h->addr, cmd, 2)) return GAS_ERR_TIMEOUT;

    /* 等待测量完成 (最大12ms) */
    delay_ms(12);

    /* 读取6字节: co2(2+CRC) + tvoc(2+CRC) */
    uint8_t data[6];
    if (i2c_read_raw(h->i2c, h->addr, data, 6)) return GAS_ERR_TIMEOUT;

    /* CRC校验 */
    if (sgp30_crc8(&data[0], 2) != data[2]) return GAS_ERR_CRC;
    if (sgp30_crc8(&data[3], 2) != data[5]) return GAS_ERR_CRC;

    *co2  = (data[0] << 8) | data[1];
    *tvoc = (data[3] << 8) | data[4];

    /* 前15秒输出固定值(CO2=450, TVOC=0), 属正常 */
    h->measure_count++;

    /* 每小时自动保存基线 (3600秒) */
    if (h->measure_count % 3600 == 0 && h->measure_count >= 3600) {
        uint16_t bl_co2, bl_tvoc;
        if (sgp30_get_baseline(h, &bl_co2, &bl_tvoc) == GAS_OK) {
            h->saved_baseline_co2 = bl_co2;
            h->saved_baseline_tvoc = bl_tvoc;
            h->baseline_valid = true;
            /* 此处应将基线值存入Flash/EEPROM */
        }
    }
    return GAS_OK;
}

/* 读取基线值 */
gas_status_t sgp30_get_baseline(sgp30_handle_t *h, uint16_t *co2, uint16_t *tvoc) {
    uint8_t cmd[2] = {0x20, 0x15};
    if (i2c_write_raw(h->i2c, h->addr, cmd, 2)) return GAS_ERR_TIMEOUT;
    delay_ms(10);

    uint8_t data[6];
    if (i2c_read_raw(h->i2c, h->addr, data, 6)) return GAS_ERR_TIMEOUT;

    if (sgp30_crc8(&data[0], 2) != data[2]) return GAS_ERR_CRC;
    if (sgp30_crc8(&data[3], 2) != data[5]) return GAS_ERR_CRC;

    *co2  = (data[0] << 8) | data[1];
    *tvoc = (data[3] << 8) | data[4];
    return GAS_OK;
}

/* 设置基线值 (从Flash恢复) */
gas_status_t sgp30_set_baseline(sgp30_handle_t *h, uint16_t co2, uint16_t tvoc) {
    uint8_t cmd[8];
    cmd[0] = 0x20; cmd[1] = 0x1E;
    cmd[2] = (co2 >> 8) & 0xFF;
    cmd[3] = co2 & 0xFF;
    cmd[4] = sgp30_crc8(&cmd[2], 2);
    cmd[5] = (tvoc >> 8) & 0xFF;
    cmd[6] = tvoc & 0xFF;
    cmd[7] = sgp30_crc8(&cmd[5], 2);

    if (i2c_write_raw(h->i2c, h->addr, cmd, 8)) return GAS_ERR_TIMEOUT;
    delay_ms(10);
    return GAS_OK;
}
```

## 4. 调试与测试规范

### 硬件验证清单
- [ ] 万用表确认加热电压稳定 5V（MQ系列）
- [ ] SGP30 供电 1.8V，不可接 3.3V（除非使用 3.3V 兼容模块）
- [ ] I2C 总线：示波器检查 SCL/SDA 波形
- [ ] 传感器暴露在空气中，不可密封
- [ ] 确认去耦电容已贴装
- [ ] GP2Y1014AU：确认 ILED 引脚 GPIO 推挽输出、ADC 引脚模拟输入
- [ ] GP2Y1014AU：确认分压电阻焊接正确（1kΩ+10kΩ），ADC 输入电压不超过 VREF

### 通信验证
- **MQ系列**：万用表测 AOUT 电压，预热后应在 0.5~4V 范围
- **SGP30**：I2C 扫描确认地址 0x58 响应；读取序列号验证
- **CCS811**：I2C 扫描确认地址 0x5A/0x5B；读取硬件ID应为 0x81
- **GP2Y1014AU**：遮住传感器开口，VOUT 应低于 NO_DUST_VOLTAGE；对准烟雾源，VOUT 应明显上升

### 校准流程
1. **MQ系列**：在干净空气中预热 24 小时，采集 Rs 值作为 R0
2. **SGP30**：首次使用需运行 12 小时建立基线，之后每小时保存基线到 Flash
3. **CCS811**：首次使用需等待 20 分钟基线稳定，可配合温湿度做补偿
4. **GP2Y1014AU**：遮住传感器开口采集多组 VOUT 取均值作为 NO_DUST_VOLTAGE，写入 Flash

### 数据校验
- MQ系列：在已知浓度气体环境中验证 Rs/R0 比值是否符合数据手册曲线
- SGP30：干净空气中 eCO2 约 400~450ppm，TVOC 接近 0
- CCS811：干净空气中 eCO2 约 400ppm，TVOC 接近 0
- GP2Y1014AU：干净空气中浓度应接近 0 μg/m³；对着烟雾浓度应快速上升至 100+ μg/m³

### 性能指标

| 指标 | MQ-2 | MQ-7 | SGP30 | CCS811 | GP2Y1014AU |
|------|------|------|-------|--------|------------|
| 预热时间 | 2min(日常)/24h(首次) | 2min(日常)/48h(首次) | 15s | 20min | 即时(无需预热) |
| 采样间隔 | 1s | 150s(加热循环) | 1s(必须) | 1s | 10ms(最快)，实际建议 1s |
| 功耗 | ~800mW | ~150mW(平均) | ~14mW | ~60mW | ~20mW(LED脉冲平均) |
| 寿命 | ~5年 | ~5年 | ~10年 | ~5年 | ~5年 |

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| MQ传感器读数异常高 | 未充分预热 | 首次使用预热24h，日常预热≥2min |
| MQ读数持续偏高 | R0未校准 | 在干净空气中校准R0 |
| MQ读数为0 | 加热电压不足 | 确认5V加热电压稳定 |
| MQ对特定气体不灵敏 | 校准系数不对 | 核对数据手册曲线，调整拟合系数 |
| MQ-7读数跳变 | 在5V加热阶段读取 | 仅在1.4V阶段读取ADC |
| MQ-7加热循环不准 | PWM占空比不对 | 5V持续60s + 1.4V持续90s，严格定时 |
| SGP30 CO2始终450 | 基线未建立 | 连续运行12h+建立基线，保存到Flash |
| SGP30 I2C无响应 | 供电电压不对 | SGP30需1.8V供电，3.3V需电平转换 |
| SGP30读数不准 | 基线丢失/未恢复 | 上电后立即从Flash恢复基线值 |
| CCS811读数异常 | 未等待基线稳定 | 首次使用等待20min |
| CCS811温漂大 | 未做温湿度补偿 | 读取SHT30温湿度写入CCS811补偿寄存器 |
| 所有传感器漂移 | 长期使用老化 | MQ系列5年更换，定期校准 |
| 传感器响应慢 | 气体扩散不足 | 保持空气流通，避免密闭安装 |
| SGP30必须每秒调用 | 协议要求 | 使用定时器中断每1s调用measure_air |
| MQ传感器交叉敏感 | 对多种气体响应 | 使用多传感器阵列+算法区分 |
| GP2Y1014AU 读数为0或负值 | NO_DUST_VOLTAGE 设置过高 | 遮住传感器实测零点电压，调低该参数 |
| GP2Y1014AU 读数偏低 | 分压比未乘或乘错 | 确认转接板分压电阻，1kΩ+10kΩ 需乘 11 |
| GP2Y1014AU 读数跳变大 | 滤波窗口太小或采样过频 | 使用 10 次滑动平均，采样间隔 ≥ 1s |
| GP2Y1014AU 时序不准 | delay_us 不精确 | 使用定时器或 DWT 周期计数器实现微秒延时 |
| GP2Y1014AU 浓度不线性 | COV_RATIO 系数不准 | 用已知浓度粉尘源校准，调整 0.17~0.20 范围 |
| GP2Y1014AU LED 不亮 | ILED 引脚接错或驱动能力不足 | 确认 GPIO 推挽输出，串联 150Ω 限流电阻 |

## 相关文档

- `skill://mcu-driver-core/templates/driver-template-i2c.c` — I2C 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-adc.c` — ADC 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-pwm.c` — PWM 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
- `skill://mcu-driver-core/guides/pitfalls.md` — 跨类别常见问题与避坑指南
