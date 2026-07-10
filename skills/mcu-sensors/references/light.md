# 光照/图像传感器开发规范

## 1. 概述与选型指南

### 常见型号对比

| 型号 | 类型 | 接口 | 量程 | 精度 | 供电 | 特点 | 价格 |
|------|------|------|------|------|------|------|------|
| BH1750 | 光照度 | I2C | 1~65535 lux | ±20% | 2.4~3.6V | 数字输出、接近人眼响应 | ¥2~6 |
| TSL2561 | 光照度 | I2C | 0.1~40000 lux | ±10% | 2.7~3.6V | 红外+可见光双通道 | ¥8~15 |
| LDR(GL5528) | 光敏电阻 | 模拟(ADC) | 依电路 | ±30% | 任意 | 廉价、需分压电路 | ¥0.5~2 |
| OV2640 | 摄像头 | DVP/并行 | 200万像素 | - | 1.7~3.0V | JPEG压缩、内置DSP | ¥10~25 |
| OV5640 | 摄像头 | DVP/并行 | 500万像素 | - | 1.5~3.0V | 自动对焦、高分辨率 | ¥20~40 |

### 选型决策树

```
只需环境光强度？ → 是 → 预算低？ → 是 → LDR (光敏电阻分压)
                          否 → 精度要求高？ → 是 → TSL2561 (双通道)
                                                    否 → BH1750 (推荐)
需要图像采集？ → 是 → 需要高分辨率？ → 是 → OV5640 (500万, 支持AF)
                                    否 → OV2640 (200万, 内置JPEG)
需要色温测量？ → 是 → TSL2561 (红外+可见光双通道可推算色温)
通用推荐 → BH1750 (I2C数字输出, 接近人眼响应, 性价比优)
```

## 2. 硬件设计规范

### BH1750 电路 (I2C)

```
VCC(3.3V) ── 4.7kΩ ── SCL ── BH1750 SCL
VCC(3.3V) ── 4.7kΩ ── SDA ── BH1750 SDA
VCC(3.3V) ────────────── BH1750 VDD
                          BH1750 ADDR → GND (地址0x23)
GND ──────────────────── BH1750 GND
```

**要点**：
- SCL/SDA 各接 4.7kΩ~10kΩ 上拉电阻
- ADDR 引脚接 GND 地址为 0x23；接 VCC 地址为 0x5C
- VDD 旁放 100nF 去耦电容
- 传感器窗口朝向被测光源方向
- I2C 地址无寄存器子地址，直接发送指令码

### TSL2561 电路 (I2C)

```
VCC(3.3V) ── 4.7kΩ ── SCL ── TSL2561 SCL
VCC(3.3V) ── 4.7kΩ ── SDA ── TSL2561 SDA
VCC(3.3V) ────────────── TSL2561 VDD
                          TSL2561 INT → 悬空或接MCU GPIO
GND ──────────────────── TSL2561 GND
```

**要点**：
- 地址通过 ADDR 引脚选择：GND=0x29, VDD=0x49, 悬空=0x39
- INT 引脚可配置阈值中断
- 内含通道0(可见+红外)和通道1(红外)，数字输出

### LDR 光敏电阻分压电路

```
VCC(3.3V) ── LDR(GL5528) ──┬── ADC输入
                             │
                         10kΩ(固定电阻)
                             │
GND ────────────────────────┘

(亮阻≈10kΩ@10lux, 暗阻≈1MΩ@黑暗)
```

**要点**：
- LDR 和固定电阻组成分压器
- 固定电阻阻值选 LDR 中间量程阻值（GL5528 约为 10kΩ）
- ADC 输入端加 100nF 滤波电容
- LDR 响应非线性，需查表或对数拟合
- 精度要求不高时可用，否则选 BH1750

### OV2640 摄像头接口电路 (DVP并行)

```
MCU                    OV2640
─── GPIO ────────────── PWDN
─── GPIO ────────────── RESET
─── I2C_SDA ─────────── SIOC(ADDR)
─── I2C_SCL ─────────── SIOD(ADDR)
─── GPIO ────────────── XCLK (外部时钟 24MHz)
─── GPIO ────────────── PCLK (像素时钟输出)
─── GPIO ────────────── VSYNC (帧同步)
─── GPIO ────────────── HREF  (行同步)
─── D0~D7 ───────────── D0~D7 (8bit并行数据)
3.3V ────────────────── DOVDD (数字供电)
1.8V ────────────────── DVDD  (内核供电)
2.5V ────────────────── AVDD  (模拟供电)
GND  ────────────────── GND
```

**要点**：
- 三路供电（AVDD 2.5V, DVDD 1.8V, DOVDD 1.7~3.0V），需 LDO 独立供电
- XCLK 由 MCU 提供，典型 24MHz
- D0~D7 并行数据线需等长走线
- PCLK/VSYNC/HREF 时序需 MCU DMA 配合
- 使用 SCCB（类I2C）协议配置寄存器
- PCB 走线远离高速数字信号，模拟供电加铁氧体磁珠隔离

## 3. 驱动开发规范

### 统一接口定义

```c
typedef enum {
    LIGHT_SENSOR_BH1750, LIGHT_SENSOR_TSL2561, LIGHT_SENSOR_LDR,
    LIGHT_SENSOR_OV2640, LIGHT_SENSOR_OV5640,
} light_sensor_type_t;

typedef enum {
    LIGHT_OK = 0, LIGHT_ERR_NOT_FOUND, LIGHT_ERR_CRC,
    LIGHT_ERR_TIMEOUT, LIGHT_ERR_INIT, LIGHT_ERR_INVALID_PARAM,
} light_status_t;

typedef struct {
    float lux;              /* 光照度 lux */
    float ir_lux;           /* 红外光照度 (TSL2561) */
    float visible_lux;      /* 可见光光照度 (TSL2561) */
    int64_t timestamp_ms;
    bool data_valid;
} light_data_t;

light_status_t light_sensor_init(light_sensor_handle_t *handle, void *hw_config);
light_status_t light_sensor_read(light_sensor_handle_t *handle, light_data_t *data);
void light_sensor_deinit(light_sensor_handle_t *handle);
```

### BH1750 I2C 驱动代码

```c
/* BH1750 指令码 */
#define BH1750_ADDR_LOW   0x23  /* ADDR=GND */
#define BH1750_ADDR_HIGH  0x5C  /* ADDR=VCC */
#define BH1750_POWER_DOWN 0x00
#define BH1750_POWER_ON   0x01
#define BH1750_RESET      0x07
#define BH1750_CONT_HRES  0x10  /* 连续高分辨率(1lux, 120ms) */
#define BH1750_CONT_HRES2 0x11  /* 连续高分辨率(0.5lux, 120ms) */
#define BH1750_CONT_LRES  0x13  /* 连续低分辨率(4lux, 16ms) */
#define BH1750_ONCE_HRES  0x20  /* 单次高分辨率(自动关机) */
#define BH1750_ONCE_HRES2 0x21
#define BH1750_ONCE_LRES  0x23

/* 初始化 */
light_status_t bh1750_init(bh1750_handle_t *h, i2c_bus_t *i2c, uint8_t addr) {
    h->i2c = i2c;
    h->addr = addr;

    /* 上电 */
    if (i2c_write_cmd(i2c, addr, BH1750_POWER_ON)) return LIGHT_ERR_NOT_FOUND;
    /* 复位 */
    i2c_write_cmd(i2c, addr, BH1750_RESET);
    /* 设置连续高分辨率模式 */
    if (i2c_write_cmd(i2c, addr, BH1750_CONT_HRES)) return LIGHT_ERR_INIT;

    h->mode = BH1750_CONT_HRES;
    h->initialized = true;
    return LIGHT_OK;
}

/* 读取光照度 */
light_status_t bh1750_read(bh1750_handle_t *h, float *lux) {
    /* 等待测量完成 (高分辨率模式需120ms) */
    delay_ms(120);

    /* 读取2字节数据 (无寄存器地址, 直接读) */
    uint8_t data[2];
    if (i2c_read_raw(h->i2c, h->addr, data, 2)) return LIGHT_ERR_TIMEOUT;

    uint16_t raw = (data[0] << 8) | data[1];
    /* 公式: lux = raw / 1.2 (高分辨率模式) */
    *lux = raw / 1.2f;
    return LIGHT_OK;
}

/* 单次测量模式(自动关机, 省电) */
light_status_t bh1750_read_once(bh1750_handle_t *h, float *lux) {
    /* 发送单次高分辨率指令 */
    if (i2c_write_cmd(h->i2c, h->addr, BH1750_ONCE_HRES)) return LIGHT_ERR_TIMEOUT;
    /* 等待测量完成 */
    delay_ms(120);
    /* 读取结果 */
    uint8_t data[2];
    if (i2c_read_raw(h->i2c, h->addr, data, 2)) return LIGHT_ERR_TIMEOUT;

    uint16_t raw = (data[0] << 8) | data[1];
    *lux = raw / 1.2f;
    return LIGHT_OK;
}

/* 设置测量灵敏度 (MTreg: 31~254, 默认69) */
light_status_t bh1750_set_sensitivity(bh1750_handle_t *h, uint8_t mtreg) {
    if (mtreg < 31 || mtreg > 254) return LIGHT_ERR_INVALID_PARAM;
    /* 高3位 */
    uint8_t hi = 0x40 | ((mtreg >> 5) & 0x07);
    /* 低5位 */
    uint8_t lo = 0x60 | (mtreg & 0x1F);
    if (i2c_write_cmd(h->i2c, h->addr, hi)) return LIGHT_ERR_TIMEOUT;
    if (i2c_write_cmd(h->i2c, h->addr, lo)) return LIGHT_ERR_TIMEOUT;
    return LIGHT_OK;
}
```

### LDR ADC 读取代码

```c
/* LDR 分压电路参数 */
typedef struct {
    adc_bus_t *adc;
    uint8_t channel;
    float vref;        /* ADC 参考电压 */
    uint16_t adc_max;  /* ADC 满量程值 (如4095) */
    float r_fixed;     /* 分压电阻阻值(Ω) */
    float lux_scale;   /* 标定系数 */
    float lux_offset;  /* 标定偏移 */
} ldr_handle_t;

/* 读取ADC并转换为lux */
light_status_t ldr_read_lux(ldr_handle_t *h, float *lux) {
    /* 多次采样取平均 */
    uint32_t sum = 0;
    const uint8_t samples = 16;
    for (uint8_t i = 0; i < samples; i++) {
        uint16_t val;
        if (adc_read(h->adc, h->channel, &val)) return LIGHT_ERR_TIMEOUT;
        sum += val;
    }
    float adc_avg = sum / (float)samples;

    /* 转换为电压 */
    float v_out = adc_avg * h->vref / h->adc_max;

    /* 计算LDR阻值: R_ldr = R_fixed * V_out / (Vref - V_out) */
    float r_ldr = h->r_fixed * v_out / (h->vref - v_out);

    /* GL5528 近似公式: lux = (R_ldr / 10000)^(-1.43) * 10
     * (经验公式, 不同LDR参数不同, 建议标定) */
    if (r_ldr < 1.0f) {
        *lux = 65535.0f;  /* 饱和 */
        return LIGHT_OK;
    }

    /* 对数拟合: log(lux) = A * log(R) + B */
    float log_r = log10f(r_ldr);
    float log_lux = h->lux_scale * log_r + h->lux_offset;
    *lux = powf(10.0f, log_lux);

    /* 限幅 */
    if (*lux < 0.1f) *lux = 0.1f;
    if (*lux > 65535.0f) *lux = 65535.0f;
    return LIGHT_OK;
}

/* LDR 标定流程: 在已知lux环境下校准 */
light_status_t ldr_calibrate(ldr_handle_t *h, float known_lux_1, float known_lux_2) {
    /* 在两种已知光照下读取LDR阻值, 计算标定系数 */
    /* 实际使用中需用标准光源逐步标定 */
    /* 此处为接口示例, 实际需配合校准光源 */
    (void)h; (void)known_lux_1; (void)known_lux_2;
    return LIGHT_OK;
}
```

### TSL2561 驱动核心

```c
/* TSL2561 寄存器 */
#define TSL2561_REG_CONTROL   0x80
#define TSL2561_REG_TIMING    0x81
#define TSL2561_REG_DATA0LOW  0x8C
#define TSL2561_REG_DATA0HIGH 0x8D
#define TSL2561_REG_DATA1LOW  0x8E
#define TSL2561_REG_DATA1HIGH 0x8F

/* 读取双通道数据并计算lux */
light_status_t tsl2561_read(tsl2561_handle_t *h, float *lux) {
    uint8_t data[4];
    if (i2c_read_reg(h->i2c, h->addr, TSL2561_REG_DATA0LOW, data, 4))
        return LIGHT_ERR_TIMEOUT;

    uint16_t ch0 = (data[1] << 8) | data[0];  /* 可见+红外 */
    uint16_t ch1 = (data[3] << 8) | data[2];  /* 红外 */

    if (ch0 == 0) {
        *lux = 0;
        return LIGHT_OK;
    }

    /* 比值法计算lux */
    float ratio = (float)ch1 / ch0;
    float lux_val;

    if (ratio <= 0.5f)        lux_val = 0.0304f * ch0 - 0.062f * ch0 * powf(ratio, 1.4f);
    else if (ratio <= 0.61f)  lux_val = 0.0224f * ch0 - 0.031f * ch1;
    else if (ratio <= 0.80f)  lux_val = 0.0128f * ch0 - 0.0153f * ch1;
    else if (ratio <= 1.3f)   lux_val = 0.00146f * ch0 - 0.00112f * ch1;
    else                       lux_val = 0;

    /* 积分时间/增益补偿 */
    if (h->integration == TSL2561_INT_13MS)  lux_val *= 0.034f;
    if (h->integration == TSL2561_INT_101MS) lux_val *= 0.492f;
    if (!h->gain) lux_val *= 16.0f;

    *lux = lux_val;
    return LIGHT_OK;
}
```

## 4. 调试与测试规范

### 硬件验证清单
- [ ] 万用表确认 VDD/GND 电压正确
- [ ] I2C 总线：示波器检查 SCL/SDA 波形幅值
- [ ] BH1750：确认 ADDR 引脚连接，对应地址 0x23 或 0x5C
- [ ] LDR：万用表测 LDR 亮暗阻值（亮阻约几kΩ，暗阻约1MΩ）
- [ ] OV2640：示波器检查 XCLK 时钟（24MHz），PCLK/VSYNC/HREF 信号

### 通信验证
- **BH1750**：I2C 扫描确认地址 0x23/0x5C 响应
- **TSL2561**：I2C 扫描确认地址 0x29/0x39/0x49 响应
- **OV2640**：SCCB 读取芯片ID（寄存器0x0A/0x0B），应为 0x77/0x2641

### 数据校验
- BH1750：室内日光灯约 200~500 lux，室外晴天 > 10000 lux
- LDR：与 BH1750 对比，误差在数量级范围内即可
- 摄像头：检查帧同步时序，确认图像无撕裂/偏色

### 性能指标

| 指标 | BH1750 | TSL2561 | LDR | OV2640 |
|------|--------|---------|-----|--------|
| 单次读取耗时 | 120ms | 14~414ms | <1ms | 33ms(30fps) |
| 最小采样间隔 | 120ms | 14ms | 1ms | 33ms |
| 功耗 | 120μA | 0.6mA | <0.1mA | 60mA |
| 分辨率 | 1 lux | 0.1 lux | 依ADC | 200万像素 |

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| BH1750 I2C 无响应 | ADDR 悬空 | ADDR 接 GND(0x23) 或 VCC(0x5C) |
| BH1750 读数全0 | 未等待测量完成 | 高分辨率模式需等待 120ms |
| BH1750 读数偏高 | 灵敏度寄存器被修改 | 恢复默认 MTreg=69，或重新标定 |
| TSL2561 lux为负 | 红外通道 > 全光通道 | 检查积分时间/增益配置，极端红外环境 |
| LDR 读数不稳 | ADC噪声/环境光闪烁 | 16次平均 + 100nF 滤波电容 |
| LDR lux误差大 | 未标定 | 用标准光源多点标定，拟合对数曲线 |
| LDR 高光读数饱和 | 分压电阻选错 | 固定电阻阻值应接近 LDR 工作点阻值 |
| OV2640 无图像 | XCLK 频率不对 | 确认 XCLK 为 24MHz，检查 GPIO 配置 |
| OV2640 图像偏色 | 白平衡未配置 | SCCB 配置 AWB 寄存器使能自动白平衡 |
| OV2640 图像撕裂 | DMA/PCLK同步问题 | 确认 PCLK 上升沿采样，DMA 缓冲区大小正确 |
| OV2640 图像暗 | 曝光时间太短 | 配置 AEC 寄存器或手动设置曝光 |
| OV5640 无法对焦 | AF未初始化 | 发送 AF 自动对焦指令序列 |
| I2C 地址找不到 | 模块上拉缺失 | 确认 SCL/SDA 各接 4.7kΩ 上拉 |
| BH1750 低分辨率模式不准 | 分辨率不够 | 使用 CONT_HRES(0x10) 高分辨率模式 |

## 相关文档

- `skill://mcu-driver-core/templates/driver-template-i2c.c` — I2C 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-adc.c` — ADC 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
- `skill://mcu-driver-core/guides/pitfalls.md` — 跨类别常见问题与避坑指南
