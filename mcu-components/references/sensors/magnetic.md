# 磁性传感器开发规范

## 1. 概述与选型指南

### 常见型号对比

| 型号 | 类型 | 接口 | 量程 | 精度 | 供电 | 特点 | 价格 |
|------|------|------|------|------|------|------|------|
| A3144 | 霍尔开关(数字) | GPIO | 开关型 | - | 4.5~24V | 单极性、有磁检测 | ¥1~3 |
| 44E(3144兼容) | 霍尔开关(数字) | GPIO | 开关型 | - | 3.8~24V | A3144替代、宽压 | ¥1~2 |
| A1302 | 线性霍尔(模拟) | 模拟(ADC) | ±1×VCC/2 Gauss | 1.3mV/G | 4.5~6V | 线性输出、测磁场强度 | ¥3~6 |
| HMC5883L | 三轴磁力计 | I2C | ±8 Gauss | 1~2° | 2.16~3.6V | 经典电子罗盘(已停产) | ¥5~10 |
| QMC5883L | 三轴磁力计 | I2C | ±8 Gauss | 1~2° | 1.65~3.6V | HMC5883L替代品 | ¥2~5 |
| MLX90393 | 三轴磁力计 | I2C/SPI | ±5~±50mT | 0.2μT | 2.2~3.6V | 高精度、可编程 | ¥15~30 |

### 选型决策树

```
仅检测有无磁场？ → 是 → 数字霍尔(A3144/44E) GPIO读取, 简单廉价
需要测磁场强度？ → 是 → 线性霍尔(A1302) 模拟ADC输出
需要电子罗盘/航向？ → 是 → HMC5883L已停产 → QMC5883L (替代品, 廉价)
                            高精度需求 → MLX90393 (可编程, 高分辨率)
检测转速/计速？ → 是 → 数字霍尔 + 磁铁 (每转一个脉冲)
通用推荐 → QMC5883L (I2C, 三轴, 电子罗盘, 性价比优)
```

## 2. 硬件设计规范

### 霍尔开关电路 (A3144/44E)

```
VCC(5V) ── 10kΩ ──┬── A3144 VCC(1)
                   │
                   A31444 OUT(3) ── MCU GPIO (开漏输出, 需上拉)
                   │
GND ────────────── A3144 GND(2)

(3.3V MCU连接方案):
VCC(5V) ── 10kΩ ──┬── A3144 VCC(1)
                   │
                   A31444 OUT(3) ──┬── 1kΩ ──┬── MCU GPIO (3.3V)
                   │                │         │
GND ────────────── A3134 GND(2)  2kΩ      100nF
                                    │         │
GND ───────────────────────────────┴─────────┘
```

**要点**：
- A3144 为开漏输出，必须接上拉电阻（10kΩ）
- 供电 4.5~24V，OUT 输出电平随上拉电压而定
- 接 3.3V MCU 时，上拉接 3.3V 并加分压，或直接上拉到 3.3V（需确认霍尔模块支持）
- 检测面需面向磁铁方向，间距通常 1~10mm
- 磁铁靠近时输出低电平（有磁），远离时输出高电平（无磁）
- VCC 旁放 100nF 去耦电容

### 线性霍尔电路 (A1302)

```
VCC(5V) ────────────── A1302 VCC(1)
                        A1302 OUT(3) ──┬── 1kΩ ──┬── ADC输入
                        │               │         │
GND ────────────────── A1302 GND(2)  2kΩ       100nF(滤波)
                                        │         │
GND ───────────────────────────────────┴─────────┘
```

**要点**：
- 输出电压与磁场强度线性相关：V_out = VCC/2 + sensitivity × B
- A1302 灵敏度约 1.3mV/Gauss
- 零磁场时输出 VCC/2（2.5V @5V供电）
- ADC 输入端加 RC 滤波（1kΩ + 100nF）
- 检测方向需垂直于芯片平面

### QMC5883L 电路 (I2C)

```
VCC(3.3V) ── 4.7kΩ ── SCL ── QMC5883L SCL
VCC(3.3V) ── 4.7kΩ ── SDA ── QMC5883L SDA
VCC(3.3V) ────────────── QMC5883L VDD
                          QMC5883L DRDY → MCU GPIO (可选, 数据就绪)
GND ──────────────────── QMC5883L GND
```

**要点**：
- 供电 1.65~3.6V（通常 3.3V）
- I2C 地址固定 0x0D（与 HMC5883L 的 0x1E 不同，不可直接替换）
- SCL/SDA 各接 4.7kΩ 上拉电阻
- DRDY 引脚可配置数据就绪中断
- 必须写 SET/RESET 周期寄存器（0x0B=0x01），否则可能无数据输出
- 安装时远离铁磁物体（电机、电池、钢结构件）

### HMC5883L 电路 (I2C)

```
VCC(3.3V) ── 4.7kΩ ── SCL ── HMC5883L SCL
VCC(3.3V) ── 4.7kΩ ── SDA ── HMC5883L SDA
VCC(3.3V) ────────────── HMC5883L VCC
                          HMC5883L DRDY → MCU GPIO (可选)
GND ──────────────────── HMC5883L GND
```

**要点**：
- 供电 2.16~3.6V
- I2C 地址 0x1E（与 QMC5883L 的 0x0D 不同）
- HMC5883L 已停产，新设计建议用 QMC5883L
- 驱动代码与 QMC5883L 不兼容（寄存器映射不同）

## 3. 驱动开发规范

### 统一接口定义

```c
typedef enum {
    MAG_SENSOR_A3144, MAG_SENSOR_A1302, MAG_SENSOR_HMC5883L,
    MAG_SENSOR_QMC5883L, MAG_SENSOR_MLX90393,
} mag_sensor_type_t;

typedef enum {
    MAG_OK = 0, MAG_ERR_NOT_FOUND, MAG_ERR_CRC,
    MAG_ERR_TIMEOUT, MAG_ERR_INIT, MAG_ERR_INVALID_PARAM,
    MAG_ERR_CALIBRATION,
} mag_status_t;

typedef struct {
    float mag_x, mag_y, mag_z;   /* 磁场分量 μT */
    float field_strength;         /* 总磁场强度 μT */
    float heading_deg;            /* 航向角 度 (0=北, 顺时针) */
    bool field_detected;          /* 霍尔开关: 是否检测到磁场 */
    int64_t timestamp_ms;
    bool data_valid;
} mag_data_t;

mag_status_t mag_sensor_init(mag_sensor_handle_t *handle, void *hw_config);
mag_status_t mag_sensor_read(mag_sensor_handle_t *handle, mag_data_t *data);
void mag_sensor_deinit(mag_sensor_handle_t *handle);
```

### 霍尔传感器 GPIO 读取代码

```c
/* A3144/44E 霍尔开关配置 */
typedef struct {
    gpio_port_t *port;
    uint8_t pin;
    bool active_low;      /* A3144: 有磁=低电平(true) */
    uint32_t debounce_ms; /* 消抖时间 */
} hall_switch_handle_t;

/* 读取霍尔开关状态 (带消抖) */
mag_status_t hall_switch_read(hall_switch_handle_t *h, bool *detected) {
    /* 读取GPIO电平 */
    bool level = gpio_read(h->port, h->pin);

    /* 消抖: 连续读取确认 */
    uint8_t stable_count = 0;
    uint8_t target = 5;  /* 连续5次一致才算稳定 */
    bool last_level = level;
    for (uint8_t i = 0; i < 20; i++) {
        delay_ms(1);
        bool cur = gpio_read(h->port, h->pin);
        if (cur == last_level) {
            stable_count++;
            if (stable_count >= target) break;
        } else {
            stable_count = 0;
            last_level = cur;
        }
    }

    /* 判断是否检测到磁场 */
    if (h->active_low) {
        *detected = !last_level;  /* 低电平=有磁 */
    } else {
        *detected = last_level;   /* 高电平=有磁 */
    }
    return MAG_OK;
}

/* 转速测量 (配磁铁 + 中断) */
typedef struct {
    volatile uint32_t pulse_count;
    uint32_t last_count;
    uint32_t last_time_ms;
    float rpm;
} hall_rpm_handle_t;

void hall_rpm_isr(void *arg) {
    hall_rpm_handle_t *h = (hall_rpm_handle_t *)arg;
    h->pulse_count++;
}

/* 计算转速RPM (每秒调用) */
float hall_rpm_calc(hall_rpm_handle_t *h, uint32_t now_ms, uint8_t magnets) {
    uint32_t dt = now_ms - h->last_time_ms;
    if (dt < 100) return h->rpm;  /* 至少100ms间隔 */

    uint32_t pulses = h->pulse_count - h->last_count;
    /* RPM = 脉冲数 × 60000 / (dt_ms × 磁铁数) */
    h->rpm = (float)pulses * 60000.0f / ((float)dt * magnets);
    h->last_count = h->pulse_count;
    h->last_time_ms = now_ms;
    return h->rpm;
}
```

### HMC5883L I2C 驱动和罗盘航向计算代码

```c
/* HMC5883L 寄存器 */
#define HMC5883L_ADDR          0x1E
#define HMC5883L_REG_CONFIG_A  0x00
#define HMC5883L_REG_CONFIG_B  0x01
#define HMC5883L_REG_MODE      0x02
#define HMC5883L_REG_DATA_X_MSB 0x03
#define HMC5883L_REG_ID_A      0x0A

/* 量程设置 */
#define HMC5883L_GAIN_0_88  0x00  /* ±0.88 Ga, 1370 LSB/Ga */
#define HMC5883L_GAIN_1_3   0x20  /* ±1.3 Ga,  1090 LSB/Ga (默认) */
#define HMC5883L_GAIN_1_9   0x40  /* ±1.9 Ga,  820 LSB/Ga */
#define HMC5883L_GAIN_2_5   0x60  /* ±2.5 Ga,  660 LSB/Ga */
#define HMC5883L_GAIN_4_0   0x80  /* ±4.0 Ga,  440 LSB/Ga */
#define HMC5883L_GAIN_4_7   0xA0  /* ±4.7 Ga,  390 LSB/Ga */
#define HMC5883L_GAIN_5_6   0xC0  /* ±5.6 Ga,  330 LSB/Ga */
#define HMC5883L_GAIN_8_1   0xE0  /* ±8.1 Ga,  230 LSB/Ga */

/* 初始化 */
mag_status_t hmc5883l_init(hmc5883l_handle_t *h, i2c_bus_t *i2c) {
    h->i2c = i2c;
    h->addr = HMC5883L_ADDR;
    h->gain_lsb = 1090.0f;  /* 默认±1.3Ga */

    /* 验证芯片ID: "H43" (0x48 0x34 0x33) */
    uint8_t id[3];
    if (i2c_read_reg(i2c, h->addr, HMC5883L_REG_ID_A, id, 3))
        return MAG_ERR_NOT_FOUND;
    if (id[0] != 0x48 || id[1] != 0x34 || id[2] != 0x33)
        return MAG_ERR_NOT_FOUND;

    /* Config A: 8-average, 15Hz输出率, 正常测量模式 */
    uint8_t config_a = 0x70;  /* CRA=01110000 */
    i2c_write_reg(i2c, h->addr, HMC5883L_REG_CONFIG_A, &config_a, 1);

    /* Config B: 增益±1.3Ga (默认) */
    uint8_t config_b = HMC5883L_GAIN_1_3;
    i2c_write_reg(i2c, h->addr, HMC5883L_REG_CONFIG_B, &config_b, 1);

    /* Mode: 连续测量模式 */
    uint8_t mode = 0x00;
    i2c_write_reg(i2c, h->addr, HMC5883L_REG_MODE, &mode, 1);

    h->initialized = true;
    return MAG_OK;
}

/* 读取三轴磁力计数据 (μT) */
mag_status_t hmc5883l_read(hmc5883l_handle_t *h, float *mx, float *my, float *mz) {
    /* 从0x03连续读取6字节: X(2) + Z(2) + Y(2) (注意顺序!) */
    uint8_t data[6];
    if (i2c_read_reg(h->i2c, h->addr, HMC5883L_REG_DATA_X_MSB, data, 6))
        return MAG_ERR_TIMEOUT;

    /* 注意: HMC5883L 数据顺序为 X, Z, Y (不是X, Y, Z) */
    int16_t raw_x = (data[0] << 8) | data[1];
    int16_t raw_z = (data[2] << 8) | data[3];
    int16_t raw_y = (data[4] << 8) | data[5];

    /* 转换为μT: 1 Gauss = 100μT */
    *mx = raw_x / h->gain_lsb * 100.0f;
    *my = raw_y / h->gain_lsb * 100.0f;
    *mz = raw_z / h->gain_lsb * 100.0f;
    return MAG_OK;
}

/* 罗盘航向角计算 */
float hmc5883l_calc_heading(float mx, float my) {
    /* 航向角 = atan2(Y, X) × 180/π
     * 0°=北, 90°=东, 180°=南, 270°=西 */
    float heading = atan2f(my, mx) * 180.0f / 3.14159265f;

    /* 加磁偏角修正 (需查询当地磁偏角) */
    /* heading += declination; */

    /* 归一化到 0~360° */
    if (heading < 0) heading += 360.0f;
    if (heading >= 360.0f) heading -= 360.0f;
    return heading;
}

/* 带倾斜补偿的航向角计算 (配合IMU) */
float hmc5883l_heading_tilt_compensated(float mx, float my, float mz,
                                         float roll, float pitch) {
    /* roll, pitch 单位: 度 */
    float roll_rad  = roll  * 3.14159265f / 180.0f;
    float pitch_rad = pitch * 3.14159265f / 180.0f;

    float cos_roll  = cosf(roll_rad);
    float sin_roll  = sinf(roll_rad);
    float cos_pitch = cosf(pitch_rad);
    float sin_pitch = sinf(pitch_rad);

    /* 倾斜补偿: 将磁场向量投影到水平面 */
    float mx_h = mx * cos_pitch + mz * sin_pitch;
    float my_h = mx * sin_roll * sin_pitch + my * cos_roll - mz * sin_roll * cos_pitch;

    float heading = atan2f(my_h, mx_h) * 180.0f / 3.14159265f;

    if (heading < 0) heading += 360.0f;
    if (heading >= 360.0f) heading -= 360.0f;
    return heading;
}

/* 硬磁/软磁校准 (椭圆修正) */
typedef struct {
    float offset_x, offset_y, offset_z;  /* 硬磁偏移 */
    float scale_x, scale_y, scale_z;     /* 软磁缩放 */
} mag_calibration_t;

mag_status_t hmc5883l_calibrate(hmc5883l_handle_t *h, mag_calibration_t *cal,
                                 uint16_t samples) {
    /* 校准流程: 水平旋转传感器画"8"字 */
    float min_x = 1e6, max_x = -1e6;
    float min_y = 1e6, max_y = -1e6;
    float min_z = 1e6, max_z = -1e6;

    for (uint16_t i = 0; i < samples; i++) {
        float mx, my, mz;
        if (hmc5883l_read(h, &mx, &my, &mz) != MAG_OK) continue;

        if (mx < min_x) min_x = mx;  if (mx > max_x) max_x = mx;
        if (my < min_y) min_y = my;  if (my > max_y) max_y = my;
        if (mz < min_z) min_z = mz;  if (mz > max_z) max_z = mz;

        delay_ms(50);  /* 20Hz采样 */
    }

    /* 硬磁偏移 = (max + min) / 2 */
    cal->offset_x = (max_x + min_x) / 2.0f;
    cal->offset_y = (max_y + min_y) / 2.0f;
    cal->offset_z = (max_z + min_z) / 2.0f;

    /* 软磁缩放: 使椭圆变为正圆 */
    float avg_radius = ((max_x - min_x) + (max_y - min_y) + (max_z - min_z)) / 3.0f;
    if (avg_radius < 1.0f) return MAG_ERR_CALIBRATION;

    cal->scale_x = avg_radius / (max_x - min_x);
    cal->scale_y = avg_radius / (max_y - min_y);
    cal->scale_z = avg_radius / (max_z - min_z);
    return MAG_OK;
}

/* 应用校准数据 */
void hmc5883l_apply_calibration(float *mx, float *my, float *mz,
                                 const mag_calibration_t *cal) {
    *mx = (*mx - cal->offset_x) * cal->scale_x;
    *my = (*my - cal->offset_y) * cal->scale_y;
    *mz = (*mz - cal->offset_z) * cal->scale_z;
}
```

### QMC5883L 驱动 (HMC5883L 替代)

```c
/* QMC5883L 寄存器 (与HMC5883L完全不同!) */
#define QMC5883L_ADDR       0x0D  /* 注意: 地址不同! */
#define QMC5883L_REG_XOUT_L 0x00
#define QMC5883L_REG_XOUT_H 0x01
#define QMC5883L_REG_YOUT_L 0x02
#define QMC5883L_REG_YOUT_H 0x03
#define QMC5883L_REG_ZOUT_L 0x04
#define QMC5883L_REG_ZOUT_H 0x05
#define QMC5883L_REG_STATUS 0x06
#define QMC5883L_REG_TOUT_L 0x07
#define QMC5883L_REG_TOUT_H 0x08
#define QMC5883L_REG_CONFIG 0x09
#define QMC5883L_REG_RESET  0x0A
#define QMC5883L_REG_PERIOD 0x0B

/* 初始化 */
mag_status_t qmc5883l_init(i2c_bus_t *i2c) {
    /* 软复位 */
    uint8_t reset = 0x80;
    i2c_write_reg(i2c, QMC5883L_ADDR, QMC5883L_REG_RESET, &reset, 1);
    delay_ms(10);

    /* 必须写 SET/RESET 周期寄存器, 否则无数据! */
    uint8_t period = 0x01;
    i2c_write_reg(i2c, QMC5883L_ADDR, QMC5883L_REG_PERIOD, &period, 1);

    /* 配置: 连续测量, 200Hz, ±8Gauss, 512 OSR */
    /* Mode[1:0]=01(连续), ODR[3:2]=00(10Hz)/01(50Hz)/10(200Hz),
       RNG[5:4]=00(±2G)/01(±8G), ODR=200Hz, RNG=±8G */
    uint8_t config = 0x01       /* 连续模式 */
                   | (0x01 << 2) /* ODR=50Hz */
                   | (0x01 << 4) /* RNG=±8Gauss */
                   | (0x00 << 6);/* OSR=512 */
    /* 简化: 0x01 | 0x04 | 0x10 = 0x15 */
    config = 0x15;
    if (i2c_write_reg(i2c, QMC5883L_ADDR, QMC5883L_REG_CONFIG, &config, 1))
        return MAG_ERR_INIT;
    return MAG_OK;
}

/* 读取三轴数据 (μT) */
mag_status_t qmc5883l_read(i2c_bus_t *i2c, float *mx, float *my, float *mz) {
    uint8_t data[6];
    if (i2c_read_reg(i2c, QMC5883L_ADDR, QMC5883L_REG_XOUT_L, data, 6))
        return MAG_ERR_TIMEOUT;

    /* QMC5883L 数据顺序: X, Y, Z (与HMC5883L的X,Z,Y不同!) */
    int16_t raw_x = (data[1] << 8) | data[0];
    int16_t raw_y = (data[3] << 8) | data[2];
    int16_t raw_z = (data[5] << 8) | data[4];

    /* ±8Gauss, 3000 LSB/Gauss, 1 Gauss = 100μT */
    *mx = raw_x / 3000.0f * 100.0f;
    *my = raw_y / 3000.0f * 100.0f;
    *mz = raw_z / 3000.0f * 100.0f;
    return MAG_OK;
}
```

## 4. 调试与测试规范

### 硬件验证清单
- [ ] A3144：确认供电 5V，上拉电阻已接
- [ ] A1302：万用表测零磁场输出应约 VCC/2
- [ ] QMC5883L/HMC5883L：确认 3.3V 供电，I2C 上拉电阻
- [ ] 磁力计远离电机、电池、钢结构件（至少 5cm）
- [ ] 确认去耦电容已贴装

### 通信验证
- **A3144**：磁铁靠近时 GPIO 电平应翻转
- **HMC5883L**：I2C 扫描确认地址 0x1E；读取 ID 寄存器 "H43"(0x48 0x34 0x33)
- **QMC5883L**：I2C 扫描确认地址 0x0D；读取状态寄存器 DRDY 位

### 校准流程
1. **硬磁校准**：水平旋转画"8"字，采集 200+ 次最大最小值
2. **椭圆修正**：计算各轴偏移和缩放系数
3. **航向验证**：面朝北确认航向角约 0°，东约 90°，南约 180°，西约 270°
4. **磁偏角修正**：查询当地磁偏角并补偿

### 数据校验
- 地磁场约 25~65μT（中国地区约 50μT），总场强应在此范围
- 水平旋转 360°，X/Y 轴应呈正弦变化
- 航向角与实际方向偏差 < 3°（校准后）

### 性能指标

| 指标 | A3144 | A1302 | HMC5883L | QMC5883L |
|------|-------|-------|----------|----------|
| 响应时间 | <1μs | <3μs | 6.7ms(15Hz) | 5ms(200Hz) |
| 采样率 | 依GPIO | 依ADC | 75Hz(max) | 200Hz |
| 功耗 | 4.5mA | 10mA | 0.1mA | 0.11mA |
| 分辨率 | 1bit | 依ADC | 1~2mG | 0.33mG |

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| A3144 无反应 | 上拉电阻未接 | 开漏输出必须接10kΩ上拉电阻 |
| A3144 始终低电平 | 磁铁一直靠近 | 移开磁铁，确认无磁时输出高 |
| A3144 检测距离短 | 磁铁磁性弱/间距大 | 用钕铁硼强磁铁，间距<5mm |
| A1302 读数不对 | 零点偏移未修正 | 无磁场时应为VCC/2, 做零点校准 |
| HMC5883L I2C无响应 | 地址错误 | HMC5883L地址0x1E, 不是0x0D |
| QMC5883L无数据 | 未写PERIOD寄存器 | 必须写0x0B=0x01 |
| QMC5883L替换HMC5883L | 寄存器完全不同 | 不可直接替换, 需改驱动代码 |
| QMC5883L数据顺序错误 | 误以为X,Z,Y | QMC5883L是X,Y,Z(与HMC5883L不同) |
| 航向角偏差大 | 未做硬磁校准 | 画8字采集min/max做椭圆修正 |
| 航向角受倾斜影响 | 未做倾斜补偿 | 配合IMU获取roll/pitch做倾斜补偿 |
| 航向角偏移固定值 | 未做磁偏角修正 | 查询当地磁偏角并补偿 |
| 磁力计读数不稳 | 靠近电机/电池 | 远离铁磁物体至少5cm |
| 磁力计读数饱和 | 量程设置太小 | 配置寄存器B选择更大量程 |
| RPM测量不准 | 消抖不足/中断丢失 | 加硬件消抖, 用外部中断边沿触发 |
| 多个霍尔开关串扰 | 磁场叠加 | 增大间距, 用屏蔽材料隔离 |
| HMC5883L读取固定值 | 测量模式未配置 | 写MODE寄存器=0x00(连续模式) |

## 相关文档

- `../../templates/driver-template-i2c.c` — I2C 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-spi.c` — SPI 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-adc.c` — ADC 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `../../guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `../../guides/debugging-testing.md` — 调试与测试规范
- `../../guides/pitfalls.md` — 跨类别常见问题与避坑指南
