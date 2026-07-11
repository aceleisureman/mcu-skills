# 距离与接近传感器开发规范

## 1. 概述与选型指南

### 常见型号对比

| 型号 | 类型 | 接口 | 量程 | 精度 | 供电 | 特点 | 价格 |
|------|------|------|------|------|------|------|------|
| HC-SR04 | 超声波 | GPIO触发/回波 | 2~400cm | ±3mm | 5V | 廉价、通用 | ¥3~8 |
| HC-SR04P | 超声波(紧凑) | GPIO触发/回波 | 2~450cm | ±3mm | 3.3~5V | 低压兼容、小型化 | ¥5~10 |
| VL53L0X | 激光ToF | I2C | 30~2000mm | ±3% | 2.6~3.5V | 激光测距、不受颜色影响 | ¥8~20 |
| VL53L1X | 激光ToF(长距) | I2C | 40~4000mm | ±3% | 2.6~3.5V | 长距离、可编程ROI | ¥15~30 |
| TFmini | 激光雷达 | UART | 0.3~12m | ±1%(<6m) | 5V | 单点LiDAR、远距离 | ¥80~150 |
| GP2Y0A21 | 红外模拟 | 模拟(ADC) | 10~80cm | ±10% | 4.5~5.5V | 廉价、快速响应 | ¥8~15 |
| 干簧管门磁开关 | 磁性接近 | GPIO | 0~30mm | 二值 | 无需供电 | 无源、零功耗、门禁 | ¥1~3 |

### 选型决策树

```
只需检测开/关状态(门/窗/盖)？ → 是 → 干簧管门磁开关 (无源, 零功耗)
量程需求:
  < 80cm？  → 是 → 红外模拟？ → 是 → GP2Y0A21 (廉价, 近距)
                    否 → VL53L0X (激光ToF, 精度高)
  2~400cm？ → 是 → HC-SR04 (超声波, 廉价通用)
  > 400cm？ → 是 → < 4m？ → 是 → VL53L1X (长距ToF)
                       否 → TFmini (LiDAR, UART, 12m)
测透明物体？ → 是 → 超声波(HC-SR04) 红外/激光对透明物体无效
受颜色影响？ → 是 → 用激光ToF(VL53L0X) 超声波受表面材质影响小
通用推荐 → HC-SR04 (廉价通用) 或 VL53L0X (精度高, 不受颜色影响)
```

## 2. 硬件设计规范

### HC-SR04 电路 (GPIO)

```
VCC(5V) ────────────── HC-SR04 VCC
MCU GPIO_TRIG ──────── HC-SR04 TRIG (输入, ≥10μs高脉冲触发)
MCU GPIO_ECHO ──────── HC-SR04 ECHO (输出, 高电平时间=声波往返)
GND ─────────────────── HC-SR04 GND

(5V ECHO → 3.3V MCU需分压):
HC-SR04 ECHO ──┬── 1kΩ ──┬── MCU GPIO_ECHO (3.3V)
               │          │
            2kΩ         100nF(可选滤波)
               │          │
GND ───────────┴──────────┘
```

**要点**：
- 供电 5V，ECHO 输出为 5V 电平，接 3.3V MCU 需分压（1kΩ + 2kΩ）
- TRIG 触发脉冲 ≥ 10μs 高电平
- ECHO 高电平持续时间对应声波往返时间
- 测距周期建议 ≥ 60ms（避免发射波干扰）
- 超声波有扩散角（约 15°），近距离小物体可能漏检
- 对吸音材料（海绵、毛绒）测距不准

### VL53L0X 电路 (I2C)

```
VCC(3.3V) ── 4.7kΩ ── SCL ── VL53L0X SCL
VCC(3.3V) ── 4.7kΩ ── SDA ── VL53L0X SDA
VCC(3.3V) ────────────── VL53L0X VDD
                          VL53L0X XSHUT → MCU GPIO (可选, 地址切换/复位)
                          VL53L0X GPIO1 → MCU GPIO (可选, 中断)
GND ──────────────────── VL53L0X GND
```

**要点**：
- 供电 2.6~3.5V（通常 3.3V）
- SCL/SDA 各接 4.7kΩ~10kΩ 上拉
- XSHUT 拉低可复位芯片（多传感器时用 XSHUT 切换地址）
- GPIO1 可配置数据就绪中断
- 传感器窗口保持清洁，不可遮挡
- 发射的红外激光为 940nm，不可见、Class 1 安全等级

### TFmini 电路 (UART)

```
VCC(5V) ────────────── TFmini VCC
TFmini TX ── 1kΩ ──┬── MCU UART_RX
                    │
                 2kΩ
                    │
GND ────────────────┴── (3.3V电平转换)

TFmini RX ───────────── MCU UART_TX (3.3V直连5V tolerant)
GND ─────────────────── TFmini GND

UART配置: 115200 baud, 8N1
```

**要点**：
- 供电 5V，UART TX 输出 5V 电平，需分压接 3.3V MCU
- UART 配置 115200 baud, 8 数据位, 无校验, 1 停止位
- 数据帧固定 9 字节：帧头(0x59 0x59) + Dist_L + Dist_H + Strength_L + Strength_H + Mode + CRC
- 采样率可配置 100Hz/250Hz/500Hz（通过命令设置）
- 激光波长 850nm，室内外均可用

### GP2Y0A21 红外模拟电路

```
VCC(5V) ────────────── GP2Y0A21 VCC
GP2Y0A21 VO ──┬── 1kΩ ──┬── MCU ADC输入
              │          │
           2kΩ         10μF(滤波, 响应慢但稳定)
              │          │
GND ──────────┴──────────┘
GND ──────────────────── GP2Y0A21 GND
```

**要点**：
- 供电 5V，模拟输出 0.4~3.1V
- 输出与距离成反比非线性关系
- ADC 输入端加 RC 滤波（10μF + 1kΩ）降低噪声
- 响应时间约 38ms（含采样保持）
- 受目标颜色和反光率影响，深色物体测距偏短
- 测距范围有盲区（< 10cm 读数不可靠）

### 干簧管门磁开关电路 (GPIO)

干簧管（Reed Switch）是一种磁控开关，当磁铁靠近时触点吸合/断开。门磁开关通常由两部分组成：干簧管（安装在门框上）+ 磁铁（安装在门上）。

```
方案1: 内部上拉（推荐，最简）

3.3V ── MCU 内部上拉 ──┬── MCU GPIO 输入 (如 PB9)
                        │
                     干簧管
                        │
GND ────────────────────┘

  门关(磁铁靠近) → 干簧管吸合 → GPIO = 低电平
  门开(磁铁远离) → 干簧管断开 → GPIO = 高电平(上拉)


方案2: 外部上拉 + 滤波

3.3V ── 10kΩ ──┬── 100nF ── GND    (RC 滤波去抖)
                │
             干簧管
                │
GND ───────────┘
                │
               MCU GPIO 输入
```

**要点**：
- 干簧管为无源器件，无需供电，仅通过磁铁控制通断
- 使用 MCU 内部上拉电阻即可，无需外部器件（方案 1）
- 机械触点有抖动（bounce），软件去抖（10~50ms 延时读取）或硬件 RC 滤波
- 常开型（NO）：磁铁靠近时导通（门关=低电平）；常闭型（NC）：磁铁靠近时断开
- 门禁场景推荐常开型：门关=低电平，门开=高电平，断线时 GPIO 浮空/高电平=安全告警
- 干簧管寿命约 100 万次，不适合高频开关场景
- 安装时磁铁与干簧管间距 ≤ 15mm，对齐磁极方向

**门磁检测代码**：

```c
/* 门磁开关检测 (去抖) */
#define DOOR_PORT   GPIOB
#define DOOR_PIN    GPIO_PIN_9

uint8_t Door_IsOpen(void) {
    /* 门关 = 低电平(干簧管吸合), 门开 = 高电平 */
    return (HAL_GPIO_ReadPin(DOOR_PORT, DOOR_PIN) == GPIO_PIN_SET) ? 1 : 0;
}

/* 带去抖的状态变化检测 */
uint8_t Door_Poll(uint8_t *changed) {
    static uint8_t last = 0xFF;
    static uint32_t debounce_tick = 0;
    uint8_t cur = Door_IsOpen();

    if (cur != last) {
        if (HAL_GetTick() - debounce_tick > 50) {  /* 50ms 去抖 */
            *changed = 1;
            last = cur;
            return cur;
        }
    } else {
        debounce_tick = HAL_GetTick();
    }
    *changed = 0;
    return last;
}
```

## 3. 驱动开发规范

### 统一接口定义

```c
typedef enum {
    DIST_SENSOR_HCSR04, DIST_SENSOR_VL53L0X, DIST_SENSOR_VL53L1X,
    DIST_SENSOR_TFMINI, DIST_SENSOR_GP2Y0A21,
} dist_sensor_type_t;

typedef enum {
    DIST_OK = 0, DIST_ERR_NOT_FOUND, DIST_ERR_CRC,
    DIST_ERR_TIMEOUT, DIST_ERR_INIT, DIST_ERR_INVALID_PARAM,
    DIST_ERR_OUT_OF_RANGE,
} dist_status_t;

typedef struct {
    float distance_mm;    /* 距离 mm */
    float distance_cm;    /* 距离 cm */
    uint16_t signal;      /* 信号强度 (TFmini/VL53L0X) */
    int64_t timestamp_ms;
    bool data_valid;
} dist_data_t;

dist_status_t dist_sensor_init(dist_sensor_handle_t *handle, void *hw_config);
dist_status_t dist_sensor_read(dist_sensor_handle_t *handle, dist_data_t *data);
void dist_sensor_deinit(dist_sensor_handle_t *handle);
```

### HC-SR04 GPIO 驱动代码 (定时器测量回波)

```c
/* HC-SR04 配置 */
typedef struct {
    gpio_port_t *trig_port; uint8_t trig_pin;
    gpio_port_t *echo_port; uint8_t echo_pin;
    timer_t *timer;          /* 用于精确计时的定时器 */
    float temperature;       /* 环境温度(°C), 用于声速补偿 */
} hcsr04_handle_t;

/* 计算声速 (温度补偿) */
static float calc_sound_speed(float temp_c) {
    /* v = 331.3 + 0.606 × T (m/s) */
    return 331.3f + 0.606f * temp_c;
}

/* 触发测量 */
static void hcsr04_trigger(hcsr04_handle_t *h) {
    gpio_set_output(h->trig_port, h->trig_pin);
    gpio_write_low(h->trig_port, h->trig_pin);
    delay_us(2);
    gpio_write_high(h->trig_port, h->trig_pin);
    delay_us(10);                   /* ≥10μs 高脉冲 */
    gpio_write_low(h->trig_port, h->trig_pin);
}

/* 测量距离 (阻塞方式, 使用定时器精确测量回波时间) */
dist_status_t hcsr04_measure(hcsr04_handle_t *h, float *distance_cm) {
    /* 触发 */
    hcsr04_trigger(h);

    /* 设置Echo为输入 */
    gpio_set_input(h->echo_port, h->echo_pin);

    /* 等待Echo上升沿 (超时10ms) */
    uint32_t timeout = 0;
    while (!gpio_read(h->echo_port, h->echo_pin)) {
        if (++timeout > 10000) return DIST_ERR_TIMEOUT;  /* 10ms超时 */
        delay_us(1);
    }

    /* 启动定时器计数 */
    timer_reset(h->timer);
    timer_start(h->timer);

    /* 等待Echo下降沿 (超时30ms, 对应约5m) */
    timeout = 0;
    while (gpio_read(h->echo_port, h->echo_pin)) {
        if (++timeout > 30000) {
            timer_stop(h->timer);
            return DIST_ERR_TIMEOUT;
        }
        delay_us(1);
    }

    /* 读取定时器计数值 */
    timer_stop(h->timer);
    uint32_t count = timer_get_count(h->timer);
    float echo_us = count / (float)timer_get_freq(h->timer) * 1e6f;

    /* 计算距离: d = v × t / 2 (往返除2) */
    float sound_speed = calc_sound_speed(h->temperature);  /* m/s */
    float distance_m = sound_speed * echo_us / 2.0f / 1e6f;
    *distance_cm = distance_m * 100.0f;

    /* 范围检查 */
    if (*distance_cm < 2.0f || *distance_cm > 400.0f)
        return DIST_ERR_OUT_OF_RANGE;
    return DIST_OK;
}

/* 中断方式测量 (推荐, 非阻塞) */
volatile uint32_t echo_start_count = 0;
volatile bool echo_measured = false;
volatile uint32_t echo_pulse_count = 0;

void hcsr04_echo_isr(void *arg) {
    hcsr04_handle_t *h = (hcsr04_handle_t *)arg;
    if (gpio_read(h->echo_port, h->echo_pin)) {
        /* 上升沿: 记录起始时间 */
        echo_start_count = timer_get_count(h->timer);
    } else {
        /* 下降沿: 计算脉宽 */
        uint32_t end_count = timer_get_count(h->timer);
        echo_pulse_count = end_count - echo_start_count;
        echo_measured = true;
    }
}

/* 中断方式读取 */
dist_status_t hcsr04_measure_irq(hcsr04_handle_t *h, float *distance_cm) {
    echo_measured = false;

    /* 注册上升/下降沿中断 */
    gpio_attach_isr(h->echo_port, h->echo_pin, GPIO_EDGE_BOTH, hcsr04_echo_isr, h);

    /* 触发 */
    hcsr04_trigger(h);

    /* 等待测量完成 (超时50ms) */
    uint32_t timeout = 0;
    while (!echo_measured) {
        if (++timeout > 50) {
            gpio_detach_isr(h->echo_port, h->echo_pin);
            return DIST_ERR_TIMEOUT;
        }
        delay_ms(1);
    }

    gpio_detach_isr(h->echo_port, h->echo_pin);

    /* 计算距离 */
    float echo_us = echo_pulse_count / (float)timer_get_freq(h->timer) * 1e6f;
    float sound_speed = calc_sound_speed(h->temperature);
    *distance_cm = sound_speed * echo_us / 2.0f / 1e6f * 100.0f;

    if (*distance_cm < 2.0f || *distance_cm > 400.0f)
        return DIST_ERR_OUT_OF_RANGE;
    return DIST_OK;
}
```

### VL53L0X I2C 驱动代码

```c
/* VL53L0X 寄存器 */
#define VL53L0X_ADDR_DEFAULT  0x29
#define VL53L0X_REG_IDENTIFICATION_MODEL_ID  0xC0
#define VL53L0X_REG_SYSRANGE_START           0x00
#define VL53L0X_REG_RESULT_INTERRUPT_STATUS  0x13
#define VL53L0X_REG_RESULT_RANGE_STATUS      0x14
#define VL53L0X_REG_POWER_MANAGEMENT_GO1     0x80
#define VL53L0X_REG_SYSTEM_SEQUENCE_CONFIG   0x01
#define VL53L0X_REG_SYSTEM_INTERMEASUREMENT  0x04
#define VL53L0X_REG_SYSRANGE_PARTIAL_DATA    0x00
#define VL53L0X_MODEL_ID                     0xEE

/* 初始化 (简化版, 省略大量校准步骤) */
dist_status_t vl53l0x_init(vl53l0x_handle_t *h, i2c_bus_t *i2c, uint8_t addr) {
    h->i2c = i2c;
    h->addr = addr;

    /* 验证芯片ID */
    uint8_t id;
    if (i2c_read_reg(i2c, addr, VL53L0X_REG_IDENTIFICATION_MODEL_ID, &id, 1))
        return DIST_ERR_NOT_FOUND;
    if (id != VL53L0X_MODEL_ID) return DIST_ERR_NOT_FOUND;

    /* 数据手册初始化序列 (简化, 完整版需几十步) */
    uint8_t val;

    /* 禁用MSRC和信号率限制 */
    val = 0x00;
    i2c_write_reg(i2c, addr, 0xFF, &val, 1);  /* 预访问 */
    val = 0x01;
    i2c_write_reg(i2c, addr, 0xFF, &val, 1);
    val = 0x00;
    i2c_write_reg(i2c, addr, 0xFF, &val, 1);

    /* 使能REF/LDO设置 */
    val = 0x01;
    i2c_write_reg(i2c, addr, VL53L0X_REG_POWER_MANAGEMENT_GO1, &val, 1);

    /* 设置信号率限制为 6.3 MCPS (默认0.25) */
    uint16_t signal_rate_limit = 63;  /* 6.3 MCPS, 单位0.1 MCPS */
    uint8_t sr[2] = {(signal_rate_limit >> 8) & 0xFF, signal_rate_limit & 0xFF};
    i2c_write_reg(i2c, addr, 0x44, sr, 2);

    /* 配置默认测距序列 */
    val = 0xFF;
    i2c_write_reg(i2c, addr, VL53L0X_REG_SYSTEM_SEQUENCE_CONFIG, &val, 1);

    h->initialized = true;
    return DIST_OK;
}

/* 单次测距 */
dist_status_t vl53l0x_read_range(vl53l0x_handle_t *h, uint16_t *range_mm) {
    /* 启动单次测量 */
    uint8_t val = 0x01;
    if (i2c_write_reg(h->i2c, h->addr, VL53L0X_REG_SYSRANGE_START, &val, 1))
        return DIST_ERR_TIMEOUT;

    /* 等待测量完成 (轮询SYSRANGE_START回到0) */
    uint32_t timeout = 0;
    do {
        if (i2c_read_reg(h->i2c, h->addr, VL53L0X_REG_SYSRANGE_START, &val, 1))
            return DIST_ERR_TIMEOUT;
        if (++timeout > 100) return DIST_ERR_TIMEOUT;  /* 100ms超时 */
        delay_ms(1);
    } while (val & 0x01);

    /* 读取结果 */
    uint8_t data[12];
    if (i2c_read_reg(h->i2c, h->addr, VL53L0X_REG_RESULT_RANGE_STATUS, data, 12))
        return DIST_ERR_TIMEOUT;

    /* 检查状态 */
    uint8_t status = data[0] & 0x04;
    if (status) return DIST_ERR_OUT_OF_RANGE;

    /* 距离值: data[10] << 8 | data[11] */
    *range_mm = ((uint16_t)data[10] << 8) | data[11];

    /* 信号强度 */
    h->signal_rate = ((uint16_t)data[6] << 8) | data[7];
    h->ambient_rate = ((uint16_t)data[8] << 8) | data[9];

    if (*range_mm < 30 || *range_mm > 2000)
        return DIST_ERR_OUT_OF_RANGE;
    return DIST_OK;
}

/* 连续测量模式启动 */
dist_status_t vl53l0x_start_continuous(vl53l0x_handle_t *h, uint32_t period_ms) {
    if (period_ms == 0) {
        /* 连续后台测量 */
        uint8_t val = 0x02;
        i2c_write_reg(h->i2c, h->addr, VL53L0X_REG_SYSRANGE_START, &val, 1);
    } else {
        /* 定时连续测量 */
        uint32_t osc_freq = 11000000 / 10;  /* 1.1MHz */
        uint32_t period = period_ms * osc_freq / 1000;
        uint8_t buf[2] = {(period >> 8) & 0xFF, period & 0xFF};
        i2c_write_reg(h->i2c, h->addr, VL53L0X_REG_SYSTEM_INTERMEASUREMENT, buf, 2);
        uint8_t val = 0x04;  /* 定时连续模式 */
        i2c_write_reg(h->i2c, h->addr, VL53L0X_REG_SYSRANGE_START, &val, 1);
    }
    return DIST_OK;
}
```

### TFmini UART 驱动

```c
/* TFmini 数据帧解析 */
#define TFMINI_FRAME_HEADER0  0x59
#define TFMINI_FRAME_HEADER1  0x59
#define TFMINI_FRAME_LEN      9

typedef struct {
    uart_bus_t *uart;
    float last_distance_cm;
    uint16_t last_strength;
    uint8_t rx_buf[9];
    uint8_t rx_index;
} tfmini_handle_t;

/* UART接收中断回调, 逐字节解析 */
dist_status_t tfmini_parse_byte(tfmini_handle_t *h, uint8_t byte) {
    h->rx_buf[h->rx_index++] = byte;

    /* 帧头检测 */
    if (h->rx_index == 1 && byte != TFMINI_FRAME_HEADER0) {
        h->rx_index = 0;
        return DIST_ERR_INVALID_PARAM;
    }
    if (h->rx_index == 2 && byte != TFMINI_FRAME_HEADER1) {
        h->rx_index = 0;
        if (byte == TFMINI_FRAME_HEADER0) {
            h->rx_buf[0] = byte;
            h->rx_index = 1;
        }
        return DIST_ERR_INVALID_PARAM;
    }

    /* 完整帧接收 */
    if (h->rx_index >= TFMINI_FRAME_LEN) {
        h->rx_index = 0;

        /* CRC校验 */
        uint8_t checksum = 0;
        for (int i = 0; i < 8; i++) checksum += h->rx_buf[i];
        if (checksum != h->rx_buf[8]) return DIST_ERR_CRC;

        /* 解析数据 */
        uint16_t dist_mm = (h->rx_buf[3] << 8) | h->rx_buf[2];
        uint16_t strength = (h->rx_buf[5] << 8) | h->rx_buf[4];

        h->last_distance_cm = dist_mm / 10.0f;
        h->last_strength = strength;

        /* 信号强度有效性检查 */
        if (strength < 100 || strength > 65535)
            return DIST_ERR_OUT_OF_RANGE;

        return DIST_OK;
    }
    return DIST_ERR_INVALID_PARAM;  /* 帧未完成 */
}

/* 阻塞方式读取距离 */
dist_status_t tfmini_read(tfmini_handle_t *h, float *distance_cm, uint16_t *strength) {
    uint8_t buf[9];
    uint32_t timeout = 0;

    /* 等待帧头 */
    while (timeout < 100) {
        uint8_t byte;
        if (uart_read(h->uart, &byte, 1, 1)) {
            timeout++;
            continue;
        }
        if (byte == TFMINI_FRAME_HEADER0) {
            buf[0] = byte;
            /* 读取剩余8字节 */
            if (uart_read(h->uart, &buf[1], 8, 10) == 0) {
                if (buf[1] == TFMINI_FRAME_HEADER1) {
                    /* CRC校验 */
                    uint8_t checksum = 0;
                    for (int i = 0; i < 8; i++) checksum += buf[i];
                    if (checksum == buf[8]) {
                        uint16_t dist_mm = (buf[3] << 8) | buf[2];
                        *distance_cm = dist_mm / 10.0f;
                        *strength = (buf[5] << 8) | buf[4];
                        if (*distance_cm < 30.0f || *distance_cm > 1200.0f)
                            return DIST_ERR_OUT_OF_RANGE;
                        return DIST_OK;
                    }
                }
            }
        }
        timeout++;
    }
    return DIST_ERR_TIMEOUT;
}
```

## 4. 调试与测试规范

### 硬件验证清单
- [ ] HC-SR04：确认 5V 供电，ECHO 分压电路正确
- [ ] VL53L0X：确认 3.3V 供电，I2C 上拉电阻
- [ ] TFmini：确认 5V 供电，UART TX 分压电路
- [ ] GP2Y0A21：确认 5V 供电，ADC 分压电路
- [ ] 示波器检查 HC-SR04 TRIG 脉冲（≥10μs）和 ECHO 回波波形

### 通信验证
- **HC-SR04**：触发后 ECHO 应输出高电平脉冲，持续时间与距离成正比
- **VL53L0X**：I2C 扫描确认地址 0x29 响应；读取 Model ID(0xC0) 应为 0xEE
- **TFmini**：UART 接收帧头 0x59 0x59，CRC 校验通过
- **GP2Y0A21**：万用表测模拟电压，手遮挡时电压应变化

### 数据校验
- HC-SR04：用卷尺标定距离，30cm/50cm/100cm/200cm 各测 10 次取平均
- VL53L0X：白色目标板正面测量，精度 ±3%
- TFmini：室外开阔场地标定，注意信号强度 > 100
- GP2Y0A21：用白色纸板标定，记录 ADC 值与距离的对应关系

### 性能指标

| 指标 | HC-SR04 | VL53L0X | TFmini | GP2Y0A21 |
|------|---------|---------|--------|----------|
| 单次测量耗时 | 20~60ms | 30ms | 10ms | 39ms |
| 最小采样间隔 | 60ms | 30ms | 10ms | 39ms |
| 功耗 | 15mA | 40mA(工作) | 140mA | 40mA |
| 盲区 | 0~2cm | 0~3cm | 0~30cm | 0~10cm |
| 测量角度 | ~15° | ~25° | ~2.6° | ~5° |

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| HC-SR04 ECHO一直高 | 未接分压/3.3V不足 | 5V供电, ECHO分压到3.3V |
| HC-SR04 测距跳变 | 声速未补偿/温度变化 | 传入环境温度修正声速 |
| HC-SR04 近距离无回波 | 扩散角内无反射 | 最小测距2cm, 更近用ToF |
| HC-SR04 对软材质不准 | 吸音材料吸声波 | 换激光ToF传感器 |
| HC-SR04 多个互相干扰 | 串扰 | 错开测量时间, 间隔>100ms |
| ECHO脉宽测量不准 | delay_us不精确 | 使用硬件定时器或输入捕获 |
| VL53L0X I2C无响应 | XSHUT悬空 | XSHUT接VCC或MCU GPIO拉高 |
| VL53L0X 读数超范围 | 目标太近/太远/透明 | 范围30~2000mm, 透明物体无回波 |
| VL53L0X 读数不稳 | 环境光干扰/目标小 | 增加积分时间, 用更大目标 |
| 多个VL53L0X地址冲突 | 默认地址都是0x29 | 用XSHUT逐个使能并修改地址 |
| TFmini 读数全0 | 信号强度不足 | 检查目标反射率, 白色>黑色 |
| TFmini UART无数据 | 波特率不对/接反 | 确认115200 baud, TX↔RX交叉 |
| TFmini CRC错误 | 线路干扰 | 检查UART线长, 加屏蔽 |
| GP2Y0A21 读数非线性 | 输出与距离反比非线性 | 使用查表或拟合曲线转换 |
| GP2Y0A21 受颜色影响 | 深色反射率低 | 对不同颜色目标分别标定 |
| GP2Y0A21 近距离盲区 | <10cm无有效输出 | 限制最小测距10cm |
| 测距数据偶发异常 | 中断/延时精度不足 | 用硬件定时器输入捕获替代轮询 |

## 相关文档

- `skill://mcu-driver-core/templates/driver-template-i2c.c` — I2C 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-uart.c` — UART 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-adc.c` — ADC 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
- `skill://mcu-driver-core/guides/pitfalls.md` — 跨类别常见问题与避坑指南
