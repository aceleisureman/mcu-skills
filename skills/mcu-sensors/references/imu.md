# 惯性测量单元(IMU)开发规范

## 1. 概述与选型指南

### 常见型号对比

| 型号 | 轴数 | 接口 | 陀螺仪范围 | 加速度范围 | 供电 | 特点 | 价格 |
|------|------|------|-----------|-----------|------|------|------|
| MPU6050 | 6轴 | I2C | ±250~±2000°/s | ±2~±16g | 2.375~3.46V | 经典廉价、DMP内置 | ¥3~8 |
| BMI270 | 6轴 | I2C/SPI | ±125~±2000°/s | ±2~±16g | 1.71~3.6V | 超低功耗、内置手势 | ¥10~20 |
| ICM-42688 | 6轴 | SPI/I2C | ±15.625~±2000°/s | ±2~±16g | 1.71~3.6V | 高性能、低噪声 | ¥15~30 |
| QMC5883L | 3轴磁力计 | I2C | ±8 Gauss | - | 1.65~3.6V | 廉价替代HMC5883L | ¥2~5 |
| HMC5883L | 3轴磁力计 | I2C | ±8 Gauss | - | 2.16~3.6V | 经典电子罗盘(已停产) | ¥5~10 |

### 选型决策树

```
需要电子罗盘(航向)？ → 是 → QMC5883L (HMC5883L已停产, QMC为替代)
                          (需9轴融合时配IMU使用)
6轴IMU选型:
  预算极低？ → 是 → MPU6050 (经典, 有DMP)
  超低功耗(穿戴)？ → 是 → BMI270 (低至0.01mA)
  高精度/低噪声？ → 是 → ICM-42688 (SPI, 高性能)
  需要DMP硬件解算？ → 是 → MPU6050 (内置DMP)
通用推荐 → MPU6050 (I2C, 6轴, 廉价, 生态丰富)
```

## 2. 硬件设计规范

### MPU6050 电路 (I2C)

```
VCC(3.3V) ── 4.7kΩ ── SCL ── MPU6050 SCL
VCC(3.3V) ── 4.7kΩ ── SDA ── MPU6050 SDA
VCC(3.3V) ────────────── MPU6050 VDD
VCC(3.3V) ────────────── MPU6050 VLOGIC
                          MPU6050 AD0 → GND (地址0x68)
                          MPU6050 INT → MCU GPIO (可选中断)
GND ──────────────────── MPU6050 GND
```

**要点**：
- SCL/SDA 各接 4.7kΩ 上拉电阻
- AD0 接 GND 地址为 0x68；接 VCC 地址为 0x69
- VDD 和 VLOGIC 旁各放 100nF 去耦电容
- INT 引脚可配置数据就绪中断，避免轮询
- 传感器需刚性安装，避免震动传导变形
- PCB 上远离电机/开关电源等干扰源

### ICM-42688 电路 (SPI)

```
MCU                    ICM-42688
─── SPI_SCK ─────────── SCLK
─── SPI_MOSI ────────── SDI
─── SPI_MISO ────────── SDO
─── GPIO_CS ─────────── CSB
─── GPIO ────────────── INT1 (数据就绪中断)
─── GPIO ────────────── INT2 (FIFO/运动中断)
VCC(3.3V) ───────────── VDDIO
1.8V/3.3V ───────────── VDD
GND ─────────────────── GND
```

**要点**：
- SPI 模式最高 24MHz，比 I2C(400kHz) 快 60 倍
- CSB 默认拉高，下降沿启动通信
- VDD 1.71~3.6V, VDDIO 1.71~3.6V（可不同电压域）
- SPI 走线等长，远离高速时钟

### 9轴组合电路 (MPU6050 + QMC5883L)

```
VCC(3.3V) ── 4.7kΩ ── SCL ──┬── MPU6050 SCL
                              └── QMC5883L SCL
VCC(3.3V) ── 4.7kΩ ── SDA ──┬── MPU6050 SDA
                              └── QMC5883L SDA
VCC(3.3V) ────────────────── MPU6050 VDD + QMC5883L VDD
GND ──────────────────────── MPU6050 GND + QMC5883L GND
MPU6050 AD0 → GND (0x68)
QMC5883L    地址固定 0x0D
```

**要点**：
- 两个传感器共用 I2C 总线，地址不冲突
- 安装方向需统一坐标系，注意 PCB 丝印标注 X/Y/Z 方向
- 两传感器尽量靠近，保证测量点一致

## 3. 驱动开发规范

### 统一接口定义

```c
typedef enum {
    IMU_SENSOR_MPU6050, IMU_SENSOR_BMI270, IMU_SENSOR_ICM42688,
    IMU_SENSOR_QMC5883L, IMU_SENSOR_HMC5883L,
} imu_sensor_type_t;

typedef enum {
    IMU_OK = 0, IMU_ERR_NOT_FOUND, IMU_ERR_CRC,
    IMU_ERR_TIMEOUT, IMU_ERR_INIT, IMU_ERR_INVALID_PARAM,
} imu_status_t;

typedef struct {
    float accel_x, accel_y, accel_z;   /* 加速度 m/s² */
    float gyro_x, gyro_y, gyro_z;      /* 陀螺仪 rad/s */
    float mag_x, mag_y, mag_z;          /* 磁力计 μT */
    float roll, pitch, yaw;             /* 欧拉角 度 */
    float temperature;                  /* 温度 °C */
    int64_t timestamp_ms;
    bool data_valid;
} imu_data_t;

imu_status_t imu_sensor_init(imu_sensor_handle_t *handle, void *hw_config);
imu_status_t imu_sensor_read(imu_sensor_handle_t *handle, imu_data_t *data);
void imu_sensor_deinit(imu_sensor_handle_t *handle);
```

### MPU6050 I2C 驱动代码

```c
/* MPU6050 寄存器 */
#define MPU6050_ADDR_AD0_LOW  0x68
#define MPU6050_ADDR_AD0_HIGH 0x69
#define MPU6050_REG_SMPLRT_DIV   0x19
#define MPU6050_REG_CONFIG       0x1A
#define MPU6050_REG_GYRO_CONFIG  0x1B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_TEMP_OUT_H   0x41
#define MPU6050_REG_GYRO_XOUT_H  0x43
#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_WHO_AM_I     0x75
#define MPU6050_WHO_AM_I_VAL     0x68

/* 量程配置 */
typedef enum {
    MPU6050_ACCEL_2G  = 0x00,  /* 16384 LSB/g */
    MPU6050_ACCEL_4G  = 0x08,  /* 8192  LSB/g */
    MPU6050_ACCEL_8G  = 0x10,  /* 4096  LSB/g */
    MPU6050_ACCEL_16G = 0x18,  /* 2048  LSB/g */
} mpu6050_accel_range_t;

typedef enum {
    MPU6050_GYRO_250DPS  = 0x00,  /* 131   LSB/(°/s) */
    MPU6050_GYRO_500DPS  = 0x08,  /* 65.5  LSB/(°/s) */
    MPU6050_GYRO_1000DPS = 0x10,  /* 32.8  LSB/(°/s) */
    MPU6050_GYRO_2000DPS = 0x18,  /* 16.4  LSB/(°/s) */
} mpu6050_gyro_range_t;

/* 初始化 */
imu_status_t mpu6050_init(mpu6050_handle_t *h, i2c_bus_t *i2c, uint8_t addr) {
    h->i2c = i2c;
    h->addr = addr;

    /* 验证芯片ID */
    uint8_t whoami;
    if (i2c_read_reg(i2c, addr, MPU6050_REG_WHO_AM_I, &whoami, 1))
        return IMU_ERR_NOT_FOUND;
    if (whoami != MPU6050_WHO_AM_I_VAL) return IMU_ERR_NOT_FOUND;

    /* 唤醒: 写PWR_MGMT_1 = 0x00 (解除休眠, 时钟=内部8MHz) */
    uint8_t val = 0x00;
    if (i2c_write_reg(i2c, addr, MPU6050_REG_PWR_MGMT_1, &val, 1))
        return IMU_ERR_INIT;
    delay_ms(100);  /* 等待稳定 */

    /* 采样率分频器: 采样率 = 1kHz / (1 + SMPLRT_DIV) */
    val = 0x07;  /* 125Hz */
    i2c_write_reg(i2c, addr, MPU6050_REG_SMPLRT_DIV, &val, 1);

    /* 配置: DLPF=3 (42Hz低通), 延迟4.9ms */
    val = 0x03;
    i2c_write_reg(i2c, addr, MPU6050_REG_CONFIG, &val, 1);

    /* 陀螺仪量程: ±2000°/s */
    val = MPU6050_GYRO_2000DPS;
    i2c_write_reg(i2c, addr, MPU6050_REG_GYRO_CONFIG, &val, 1);
    h->gyro_lsb = 16.4f;  /* LSB/(°/s) */

    /* 加速度计量程: ±4g */
    val = MPU6050_ACCEL_4G;
    i2c_write_reg(i2c, addr, MPU6050_REG_ACCEL_CONFIG, &val, 1);
    h->accel_lsb = 8192.0f;  /* LSB/g */

    h->initialized = true;
    return IMU_OK;
}

/* 读取原始数据 */
imu_status_t mpu6050_read_raw(mpu6050_handle_t *h,
        int16_t *ax, int16_t *ay, int16_t *az,
        int16_t *gx, int16_t *gy, int16_t *gz,
        float *temp) {
    uint8_t data[14];
    /* 从0x3B连续读取14字节: accel(6) + temp(2) + gyro(6) */
    if (i2c_read_reg(h->i2c, h->addr, MPU6050_REG_ACCEL_XOUT_H, data, 14))
        return IMU_ERR_TIMEOUT;

    *ax = (data[0] << 8) | data[1];
    *ay = (data[2] << 8) | data[3];
    *az = (data[4] << 8) | data[5];
    int16_t raw_temp = (data[6] << 8) | data[7];
    *gx = (data[8] << 8) | data[9];
    *gy = (data[10] << 8) | data[10];
    *gz = (data[12] << 8) | data[13];

    /* 温度转换: T(°C) = raw / 340 + 36.53 */
    *temp = raw_temp / 340.0f + 36.53f;
    return IMU_OK;
}

/* 读取物理量 */
imu_status_t mpu6050_read(mpu6050_handle_t *h, imu_data_t *data) {
    int16_t ax, ay, az, gx, gy, gz;
    imu_status_t st = mpu6050_read_raw(h, &ax, &ay, &az, &gx, &gy, &gz, &data->temperature);
    if (st != IMU_OK) return st;

    /* 转换为物理量 */
    data->accel_x = ax / h->accel_lsb * 9.80665f;  /* g → m/s² */
    data->accel_y = ay / h->accel_lsb * 9.80665f;
    data->accel_z = az / h->accel_lsb * 9.80665f;

    data->gyro_x = gx / h->gyro_lsb * (3.14159265f / 180.0f);  /* °/s → rad/s */
    data->gyro_y = gy / h->gyro_lsb * (3.14159265f / 180.0f);
    data->gyro_z = gz / h->gyro_lsb * (3.14159265f / 180.0f);

    data->data_valid = true;
    return IMU_OK;
}

/* 校准: 静置时读取偏移量 */
imu_status_t mpu6050_calibrate(mpu6050_handle_t *h, uint16_t samples) {
    int32_t ax_sum = 0, ay_sum = 0, az_sum = 0;
    int32_t gx_sum = 0, gy_sum = 0, gz_sum = 0;

    for (uint16_t i = 0; i < samples; i++) {
        int16_t ax, ay, az, gx, gy, gz;
        float temp;
        if (mpu6050_read_raw(h, &ax, &ay, &az, &gx, &gy, &gz, &temp) != IMU_OK)
            return IMU_ERR_TIMEOUT;
        ax_sum += ax; ay_sum += ay; az_sum += az;
        gx_sum += gx; gy_sum += gy; gz_sum += gz;
        delay_ms(10);  /* 100Hz采样 */
    }

    h->accel_offset_x = ax_sum / samples;
    h->accel_offset_y = ay_sum / samples;
    h->accel_offset_z = az_sum / samples - (int16_t)h->accel_lsb;  /* Z轴含1g重力 */
    h->gyro_offset_x  = gx_sum / samples;
    h->gyro_offset_y  = gy_sum / samples;
    h->gyro_offset_z  = gz_sum / samples;
    h->calibrated = true;
    return IMU_OK;
}
```

### 姿态解算 - 互补滤波代码

```c
/* 互补滤波器(Complementary Filter)姿态解算
 * 原理: 加速度计低频可靠(静态), 陀螺仪高频可靠(动态)
 *       融合: angle = α * (angle + gyro*dt) + (1-α) * accel_angle
 *       α 典型值 0.96~0.98
 */

typedef struct {
    float alpha;          /* 互补滤波系数 (0.96~0.98) */
    float roll, pitch;    /* 当前姿态角(度) */
    uint32_t last_tick;   /* 上次时间戳 */
    bool first_run;       /* 首次运行标志 */
} complementary_filter_t;

void complementary_filter_init(complementary_filter_t *f, float alpha) {
    f->alpha = alpha;
    f->roll = 0;
    f->pitch = 0;
    f->first_run = true;
}

/* 更新姿态 (输入: 加速度m/s², 陀螺仪°/s) */
void complementary_filter_update(complementary_filter_t *f,
        float ax, float ay, float az,
        float gx, float gy, float gz,
        uint32_t now_tick) {
    /* 计算时间步长 dt(秒) */
    float dt;
    if (f->first_run) {
        dt = 0.01f;  /* 默认10ms */
        f->last_tick = now_tick;
        f->first_run = false;
    } else {
        dt = (now_tick - f->last_tick) / 1000.0f;  /* ms → s */
        f->last_tick = now_tick;
        if (dt > 0.5f) dt = 0.01f;  /* 超时保护 */
    }

    /* 加速度计计算角度 (静态参考) */
    float accel_roll  = atan2f(ay, az) * 180.0f / 3.14159265f;
    float accel_pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / 3.14159265f;

    /* 陀螺仪积分 (动态更新) */
    float gyro_roll  = f->roll  + gx * dt;
    float gyro_pitch = f->pitch + gy * dt;

    /* 互补滤波融合 */
    f->roll  = f->alpha * gyro_roll  + (1.0f - f->alpha) * accel_roll;
    f->pitch = f->alpha * gyro_pitch + (1.0f - f->alpha) * accel_pitch;

    /* Yaw角无法从加速度计获取, 需磁力计辅助 */
    /* 简化处理: 仅陀螺仪积分 (会漂移) */
    /* 完整方案: 9轴Mahony/Madgwick滤波 */
}

/* 带磁力计的9轴航向角计算 */
float calculate_yaw_from_mag(float mx, float my, float mz,
                             float roll, float pitch,
                             float declination) {
    /* 磁力计倾斜补偿 */
    float cos_roll = cosf(roll * 3.14159265f / 180.0f);
    float sin_roll = sinf(roll * 3.14159265f / 180.0f);
    float cos_pitch = cosf(pitch * 3.14159265f / 180.0f);
    float sin_pitch = sinf(pitch * 3.14159265f / 180.0f);

    /* 倾斜补偿后的水平磁场分量 */
    float mx_h = mx * cos_pitch + mz * sin_pitch;
    float my_h = mx * sin_roll * sin_pitch + my * cos_roll - mz * sin_roll * cos_pitch;

    /* 航向角 (0°=北, 顺时针) */
    float heading = atan2f(my_h, mx_h) * 180.0f / 3.14159265f;

    /* 加磁偏角修正 */
    heading += declination;

    /* 归一化到 0~360° */
    if (heading < 0) heading += 360.0f;
    if (heading >= 360.0f) heading -= 360.0f;
    return heading;
}
```

### QMC5883L 磁力计驱动

```c
#define QMC5883L_ADDR      0x0D
#define QMC5883L_REG_XOUT_L 0x00
#define QMC5883L_REG_CONFIG 0x09

/* 初始化 */
imu_status_t qmc5883l_init(i2c_bus_t *i2c) {
    /* 配置: 连续测量, 200Hz, ±8Gauss, 512OSR */
    uint8_t config = 0x01 | (0x0C << 2) | (0x10 << 4) | (0x00 << 6);
    /* ODR=200Hz(0x0C<<2), RNG=±8G(0x10<<4), OSR=512(0x00<<6), MODE=连续(0x01) */
    if (i2c_write_reg(i2c, QMC5883L_ADDR, QMC5883L_REG_CONFIG, &config, 1))
        return IMU_ERR_INIT;

    /* 设置复位/定义周期寄存器 (必须写, 否则可能无数据) */
    uint8_t period = 0x01;
    i2c_write_reg(i2c, QMC5883L_ADDR, 0x0B, &period, 1);
    return IMU_OK;
}

/* 读取磁力计数据 (μT) */
imu_status_t qmc5883l_read(i2c_bus_t *i2c, float *mx, float *my, float *mz) {
    uint8_t data[6];
    if (i2c_read_reg(i2c, QMC5883L_ADDR, QMC5883L_REG_XOUT_L, data, 6))
        return IMU_ERR_TIMEOUT;

    int16_t raw_x = (data[1] << 8) | data[0];
    int16_t raw_y = (data[3] << 8) | data[2];
    int16_t raw_z = (data[5] << 8) | data[4];

    /* ±8Gauss范围, 3000 LSB/Gauss, 1 Gauss = 100μT */
    *mx = raw_x / 3000.0f * 100.0f;  /* μT */
    *my = raw_y / 3000.0f * 100.0f;
    *mz = raw_z / 3000.0f * 100.0f;
    return IMU_OK;
}
```

## 4. 调试与测试规范

### 硬件验证清单
- [ ] 万用表确认 VDD/GND 电压正确（MPU6050 为 3.3V）
- [ ] I2C 总线：示波器检查 SCL/SDA 波形
- [ ] 确认 AD0 引脚连接，对应地址 0x68 或 0x69
- [ ] 传感器刚性固定，PCB 无明显变形
- [ ] 确认去耦电容已贴装

### 通信验证
- **MPU6050**：读取 WHO_AM_I(0x75) 应为 0x68
- **ICM-42688**：读取 WHO_AM_I 应为 0x47
- **QMC5883L**：读取状态寄存器确认 DRDY 位

### 校准流程
1. **陀螺仪校准**：静置 2 秒，采集 200 次取平均作为零偏
2. **加速度计校准**：六个面分别水平放置，采集各轴 0g 和 1g 值
3. **磁力计校准**：水平旋转画"8"字，采集最大最小值做椭圆修正

### 数据校验
- 静置时加速度计 Z 轴应约 9.8m/s²，XY 轴近 0
- 静置时陀螺仪三轴应近 0（< 0.5°/s），否则需校准
- 磁力计水平旋转 360°，X/Y 应呈正弦变化

### 性能指标

| 指标 | MPU6050 | BMI270 | ICM-42688 | QMC5883L |
|------|---------|--------|-----------|----------|
| 采样率 | 1kHz | 6.25kHz | 32kHz | 200Hz |
| 功耗 | 3.9mA | 0.01mA | 0.69mA | 0.11mA |
| 噪声密度 | 400μg/√Hz | 120μg/√Hz | 70μg/√Hz | 1.5mG/√Hz |
| 接口 | I2C(400kHz) | I2C/SPI | SPI(24MHz) | I2C(400kHz) |

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| MPU6050 无响应 | AD0 悬空或地址错误 | AD0 接 GND(0x68) 或 VCC(0x69) |
| MPU6050 读取全0 | 芯片未唤醒 | 写 PWR_MGMT_1=0x00 解除休眠 |
| 陀螺仪零漂大 | 未校准 | 静置校准，存储零偏并减去 |
| 加速度计Z轴不为9.8 | 未校准/安装倾斜 | 六面校准，确保安装水平 |
| 姿态角漂移 | 纯陀螺仪积分无修正 | 使用互补滤波或Mahony/Madgwick算法 |
| Yaw角无法参考 | 无磁力计 | 增加 QMC5883L 做9轴融合 |
| 磁力计受干扰 | 靠近铁磁物体/电机 | 远离金属，做硬磁/软磁补偿 |
| QMC5883L 无数据 | 未写 SET/RESET 周期寄存器 | 写寄存器 0x0B = 0x01 |
| I2C 读取偶发错误 | 总线干扰/上拉不足 | 加上拉至 2.2kΩ，缩短线长 |
| 姿态角震荡 | 互补滤波系数不当 | α 取 0.96~0.98，增大 DLPF 截止频率 |
| 高速运动丢数据 | 采样率不足 | 提高采样率至 200Hz+，使用中断驱动 |
| ICM-42688 SPI通信失败 | SPI模式/极性不对 | CPOL=1, CPHA=1 (Mode 3) |
| 温度读数偏差 | 未做温度补偿 | 参考数据手册温度系数做补偿 |

## 相关文档

- `skill://mcu-driver-core/templates/driver-template-i2c.c` — I2C 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-spi.c` — SPI 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
- `skill://mcu-driver-core/guides/pitfalls.md` — 跨类别常见问题与避坑指南
