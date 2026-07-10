# 舵机驱动开发规范

## 1. 概述与选型指南

### 简介

舵机（Servo）是一种带有位置反馈的伺服机构，通过 PWM 信号控制输出轴转到指定角度。标准舵机旋转范围 0~180°，内部由直流电机、减速齿轮组、电位器反馈和控制电路组成。

舵机分类：
- **标准 PWM 舵机**：50Hz PWM 信号控制角度（0.5~2.5ms 脉宽 → 0~180°）
- **连续旋转舵机**：PWM 信号控制旋转方向和速度（不可定位）
- **总线舵机**：串口/半双工通信，支持位置反馈、扭矩控制、ID 编址

### 常见型号对比

| 型号 | 类型 | 扭矩 | 旋转范围 | 电压 | 速度 | 齿轮材质 | 价格 |
|------|------|------|----------|------|------|----------|------|
| SG90 | 标准 | 1.8 kg·cm | 0~180° | 4.8~6V | 0.1s/60° | 塑料 | ¥5~10 |
| MG90S | 标准 | 2.2 kg·cm | 0~180° | 4.8~6V | 0.1s/60° | 金属 | ¥10~20 |
| MG996R | 标准 | 11 kg·cm | 0~180° | 4.8~7.2V | 0.17s/60° | 金属 | ¥20~40 |
| DS3218 | 标准 | 20 kg·cm | 0~270° | 6.8~7.4V | 0.16s/60° | 钢/铝 | ¥30~60 |
| SR430 | 连续旋转 | - | 360° | 4.8~6V | 0~80rpm | 塑料 | ¥15~25 |
| LX-16A | 总线 | 12 kg·cm | 0~240° | 6~8.4V | 0.2s/60° | 金属 | ¥30~50 |
| DS3225 | 总线 | 25 kg·cm | 0~300° | 6.8~7.4V | 0.13s/60° | 钢 | ¥50~80 |

### 选型决策树

```
需要位置反馈/多舵机串联？ → 是 → 总线舵机 (LX-16A / DS3225)
                                否 →
  需要 360° 连续旋转？ → 是 → 连续旋转舵机 (SR430) 或改装标准舵机
                         否 →
    扭矩 ≤ 2 kg·cm？ → 是 → SG90 (微型, 廉价)
    扭矩 2~5 kg·cm？ → 是 → MG90S (金属齿, 耐用)
    扭矩 > 5 kg·cm？ → 是 → MG996R (大扭力, 通用)
    扭矩 > 15 kg·cm？ → 是 → DS3218 (超大扭力)
  需要高精度？ → 是 → 总线舵机 (0.25°/步)
```

### 适用场景

| 场景 | 推荐型号 | 理由 |
|------|----------|------|
| 机械臂小型关节 | MG90S | 金属齿耐用、体积适中 |
| 机械臂大关节 | MG996R | 扭力大、性价比高 |
| 遥控车转向 | SG90 | 轻量、廉价 |
| 智能窗帘 | 连续旋转舵机 | 需连续旋转控制 |
| 机器人狗/四足 | DS3225/LX-16A | 大扭力+位置反馈 |
| 云台控制 | MG996R | 双轴联动、扭力够 |
| 多舵机机器人 | LX-16A 总线 | 串联布线、反馈位置 |

## 2. 硬件设计规范

### 标准 PWM 舵机电路

```
  ┌────────────────────────────────────────────────────┐
  │                    舵机(SG90/MG996R)               │
  │                                                    │
  │  红色(电源) ─── VServo (5V/6V, 独立电源!)          │
  │  棕色(地)   ─── GND (与 MCU 共地)                  │
  │  橙色(信号) ─── MCU PWM 输出 (50Hz TTL)            │
  └────────────────────────────────────────────────────┘

  重要: VServo 必须独立供电, 不可从 MCU 的 3.3V/5V 引脚取电!

  电源方案:
    方案1(小电流): LM2596 DC-DC 5V → VServo (≤2A)
    方案2(大电流): 7805 + 散热片 → VServo (≤1A)
    方案3(隔离):   独立电池组 → VServo

  信号隔离(可选):
    MCU PWM ── 1kΩ ── 光耦(PC817) ── 舵机信号
```

### PWM 信号规格

```
  标准 PWM 舵机信号:

  频率: 50Hz (周期 20ms)
  脉宽: 0.5ms ~ 2.5ms 对应 0° ~ 180°

       ←─────── 20ms 周期 ───────→
       │                          │
       ├──0.5ms──┐                │
       │         └────────────────┘
       │ 0° (最小角度)

       ├──2.5ms──┐
       │         └────────────────┘
       │ 180° (最大角度)

       ├──1.5ms──┐
       │         └────────────────┘
       │ 90° (中位)

  角度计算:
    脉宽(µs) = 500 + (angle / 180) × 2000
    angle = (脉宽 - 500) / 2000 × 180

  连续旋转舵机:
    1.5ms → 停止
    <1.5ms → 反转 (脉宽越小速度越快)
    >1.5ms → 正转 (脉宽越大速度越快)
```

### 总线舵机电路

```
  ┌──────────────────────────────────────────────┐
  │  总线舵机 (LX-16A 等)                        │
  │                                              │
  │  V+ (红)  ─── VServo (6~8.4V, 独立电源)     │
  │  GND (黑) ─── GND (与 MCU 共地)              │
  │  信号(黄) ─── 半双工串口 (TTL)               │
  └──────────────────────────────────────────────┘

  半双工串口接线:
    MCU TX ──┬── 1kΩ ──── 总线信号线
             │
    MCU RX ──┴────────── 总线信号线

  多舵机串联:
    MCU ────┬───────┬───────┬─────── 总线
            │       │       │
          舵机1   舵机2   舵机3
          ID=1   ID=2   ID=3

  通信参数: 115200 8N1, 半双工
  协议: 帧头(0x55 0x55) + ID + 长度 + 命令 + 数据 + 校验
```

### 多舵机供电方案

```
  方案1: 独立 DC-DC (推荐)

  电池(7.4V) ── LM2596 ── 6V ──┬── 舵机1 V+
                                ├── 舵机2 V+
                                ├── 舵机3 V+
                                └── ...
  电池(7.4V) ── AMS1117 ── 3.3V ── MCU

  方案2: 独立电源完全隔离

  舵机电池 ── 6V ── 舵机 V+
  MCU 电池 ── 3.3V ── MCU
  共地连接

  电源容量计算:
    MG996R 堵转电流 ≈ 2.5A
    6 个舵机同时动作: 6 × 2.5A = 15A (峰值)
    电池需 ≥ 15A 放电能力 (如 3S 2200mAh 20C = 44A)
```

### 硬件设计要点

#### 独立供电

```
  ┌──────────────────────────────────────────────────────┐
  │  警告: 绝对不要从 MCU 的引脚直接给舵机供电!          │
  │                                                      │
  │  SG90 空载电流 ~150mA, 堵转 ~600mA                  │
  │  MG996R 空载 ~200mA, 堵转 ~2500mA                   │
  │                                                      │
  │  MCU 引脚最大输出通常仅 20~40mA                      │
  │  直接取电会导致: MCU复位/稳压器烧毁/电压跌落          │
  └──────────────────────────────────────────────────────┘
```

#### PWM 信号隔离

- 小型舵机（SG90）：信号线可直接接 MCU GPIO，串联 220Ω 电阻限流
- 大型舵机（MG996R）：建议通过光耦隔离，防止舵机干扰 MCU
- 总线舵机：半双工串口通过电阻合并 TX/RX

#### 电源滤波

```
  VServo ── 1000µF 电解 + 100nF 瓷片 ── GND

  大电容吸收舵机启动时的电流冲击
  小电容滤除高频开关噪声
```

### PCB 设计要点

- VServo 走线宽度 ≥ 2mm（多舵机 ≥ 3mm）
- 电源地与信号地单点连接，避免地环路
- 舵机接口旁预留大焊盘，方便外接电源线
- 总线舵机的信号线加 ESD 保护（TVS 管）

## 3. 驱动开发规范

### 统一接口定义

```c
typedef enum {
    SERVO_OK = 0,
    SERVO_ERR_INIT,
    SERVO_ERR_INVALID_PARAM,
    SERVO_ERR_TIMEOUT,
} servo_status_t;

typedef struct {
    uint8_t pwm_channel;       /* PWM 通道号 */
    uint16_t min_pulse_us;     /* 最小脉宽(0°), 默认 500 */
    uint16_t max_pulse_us;     /* 最大脉宽(180°), 默认 2500 */
    uint16_t min_angle;        /* 最小角度, 默认 0 */
    uint16_t max_angle;        /* 最大角度, 默认 180 */
} servo_config_t;

typedef struct {
    servo_config_t cfg;
    bool initialized;
    uint16_t current_angle;
} servo_handle_t;

servo_status_t servo_init(servo_handle_t *h, const servo_config_t *cfg);
servo_status_t servo_set_angle(servo_handle_t *h, uint16_t angle);
servo_status_t servo_set_pulse(servo_handle_t *h, uint16_t pulse_us);
servo_status_t servo_stop(servo_handle_t *h);
```

### 标准 PWM 舵机驱动代码

```c
/* servo_pwm.c - 标准 PWM 舵机驱动 (50Hz, 0.5~2.5ms) */

/* 平台适配层 */
static void pwm_set_pulse_us(uint8_t ch, uint16_t pulse_us) {
    /* TODO: 根据具体MCU实现
     * STM32: 使用定时器输出比较模式
     *   频率50Hz: PSC和ARR需满足 84MHz/(PSC+1)/(ARR+1) = 50Hz
     *   例: PSC=83(1MHz), ARR=19999(20ms周期)
     *   脉宽 = pulse_us × (1MHz) = pulse_us 个计数
     *   CCR = pulse_us
     */
}

/* 舵机角度转脉宽 */
static uint16_t angle_to_pulse(servo_handle_t *h, uint16_t angle) {
    if (angle < h->cfg.min_angle) angle = h->cfg.min_angle;
    if (angle > h->cfg.max_angle) angle = h->cfg.max_angle;

    uint32_t range = h->cfg.max_angle - h->cfg.min_angle;
    uint32_t pulse_range = h->cfg.max_pulse_us - h->cfg.min_pulse_us;

    return h->cfg.min_pulse_us +
           (uint16_t)((uint32_t)(angle - h->cfg.min_angle) * pulse_range / range);
}

servo_status_t servo_init(servo_handle_t *h, const servo_config_t *cfg) {
    if (!h || !cfg) return SERVO_ERR_INVALID_PARAM;
    if (cfg->pwm_channel == 0xFF) return SERVO_ERR_INVALID_PARAM;

    h->cfg = *cfg;
    h->current_angle = cfg->min_angle;

    /* 设置 PWM 频率为 50Hz (周期 20ms) */
    /* 平台相关: 配置定时器为 50Hz */
    /* STM32: PSC=83, ARR=19999 → 1MHz/20000 = 50Hz */
    pwm_config_50hz(cfg->pwm_channel);

    /* 初始位置为最小角度 */
    pwm_set_pulse_us(cfg->pwm_channel, angle_to_pulse(h, cfg->min_angle));
    pwm_start(cfg->pwm_channel);

    h->initialized = true;
    return SERVO_OK;
}

servo_status_t servo_set_angle(servo_handle_t *h, uint16_t angle) {
    if (!h || !h->initialized) return SERVO_ERR_INIT;

    /* 限幅 */
    if (angle < h->cfg.min_angle) angle = h->cfg.min_angle;
    if (angle > h->cfg.max_angle) angle = h->cfg.max_angle;

    uint16_t pulse = angle_to_pulse(h, angle);
    pwm_set_pulse_us(h->cfg.pwm_channel, pulse);
    h->current_angle = angle;

    return SERVO_OK;
}

servo_status_t servo_set_pulse(servo_handle_t *h, uint16_t pulse_us) {
    if (!h || !h->initialized) return SERVO_ERR_INIT;
    if (pulse_us < h->cfg.min_pulse_us) pulse_us = h->cfg.min_pulse_us;
    if (pulse_us > h->cfg.max_pulse_us) pulse_us = h->cfg.max_pulse_us;

    pwm_set_pulse_us(h->cfg.pwm_channel, pulse_us);
    return SERVO_OK;
}

servo_status_t servo_stop(servo_handle_t *h) {
    if (!h || !h->initialized) return SERVO_ERR_INIT;
    pwm_stop(h->cfg.pwm_channel);
    return SERVO_OK;
}
```

### 多舵机定时器复用方案

```c
/* servo_multi.c - 多舵机驱动(单定时器多通道) */

/*
 * STM32 单定时器驱动多个舵机:
 * - TIM2/TIM3/TIM4/TIM5 (16/32位) 支持 4 个 PWM 通道
 * - 配置为 50Hz, 4 个 CCR 独立设置脉宽
 * - 一个定时器可同时驱动 4 个舵机
 *
 * ESP32:
 * - LEDC PWM 控制器, 16 通道
 * - 频率 50Hz, 分辨率 16bit
 * - 可驱动 16 个舵机
 *
 * Arduino:
 * - Servo 库使用定时器, Uno 支持 12 个舵机
 * - Mega 支持 48 个舵机
 */

#define MAX_SERVOS 8

typedef struct {
    uint8_t channel;
    uint16_t pulse_us;
    bool active;
} servo_channel_t;

static servo_channel_t servo_channels[MAX_SERVOS];

/* 定时器中断方式生成多路 PWM (软件 PWM 方案) */
/* 适用场景: MCU 硬件 PWM 通道不足时 */
void servo_timer_isr(void) {
    static uint8_t current_ch = 0;
    static uint16_t time_counter = 0;  /* 0~20000 (µs) */

    /* 关闭所有通道输出 */
    for (int i = 0; i < MAX_SERVOS; i++) {
        if (!servo_channels[i].active) continue;

        if (time_counter == 0) {
            /* 周期开始: 拉高所有活跃通道 */
            gpio_set(servo_channels[i].channel, 1);
        } else if (time_counter == servo_channels[i].pulse_us) {
            /* 到达脉宽: 拉低该通道 */
            gpio_set(servo_channels[i].channel, 0);
        }
    }

    time_counter += 100;  /* 每 100µs 中断一次 */
    if (time_counter >= 20000) {
        time_counter = 0;  /* 20ms 周期重置 */
    }
}

void servo_multi_init(void) {
    /* 配置定时器: 100µs 中断一次 */
    /* STM32: SysTick 或基本定时器 */
    timer_init_100us(servo_timer_isr);

    for (int i = 0; i < MAX_SERVOS; i++) {
        servo_channels[i].active = false;
        servo_channels[i].pulse_us = 1500;  /* 默认中位 */
    }
}

int8_t servo_multi_attach(uint8_t gpio_pin) {
    for (int i = 0; i < MAX_SERVOS; i++) {
        if (!servo_channels[i].active) {
            servo_channels[i].channel = gpio_pin;
            servo_channels[i].pulse_us = 1500;
            servo_channels[i].active = true;
            gpio_config_output(gpio_pin);
            return i;
        }
    }
    return -1;  /* 无空闲通道 */
}

void servo_multi_write(int8_t id, uint16_t angle) {
    if (id < 0 || id >= MAX_SERVOS) return;
    if (!servo_channels[id].active) return;
    /* 角度转脉宽 */
    servo_channels[id].pulse_us = 500 + (uint32_t)angle * 2000 / 180;
}
```

### 平台适配示例（STM32 HAL）

```c
/* platform_stm32_servo.c */

/* STM32 配置 PWM 为 50Hz:
 * 定时器时钟 84MHz
 * PSC = 83 → 计数频率 = 84MHz/84 = 1MHz (1µs/tick)
 * ARR = 19999 → 周期 = 20000µs = 20ms = 50Hz
 * CCR = pulse_us → 直接用微秒值
 */
void pwm_config_50hz_stm32(uint8_t ch) {
    extern TIM_HandleTypeDef htim2;
    /* 假设 CubeMX 已配置 TIM2: PSC=83, ARR=19999 */
    /* 启动对应通道的 PWM */
    uint32_t channel;
    switch (ch) {
        case 0: channel = TIM_CHANNEL_1; break;
        case 1: channel = TIM_CHANNEL_2; break;
        case 2: channel = TIM_CHANNEL_3; break;
        case 3: channel = TIM_CHANNEL_4; break;
        default: return;
    }
    HAL_TIM_PWM_Start(&htim2, channel);
}

void pwm_set_pulse_stm32(uint8_t ch, uint16_t pulse_us) {
    extern TIM_HandleTypeDef htim2;
    uint32_t channel;
    switch (ch) {
        case 0: channel = TIM_CHANNEL_1; break;
        case 1: channel = TIM_CHANNEL_2; break;
        case 2: channel = TIM_CHANNEL_3; break;
        case 3: channel = TIM_CHANNEL_4; break;
        default: return;
    }
    /* CCR 直接等于 pulse_us (因为 1µs/tick) */
    __HAL_TIM_SET_COMPARE(&htim2, channel, pulse_us);
}
```

### 平台适配示例（ESP32 Arduino）

```c
/* platform_esp32_servo.c - ESP32 LEDC PWM */

#include "driver/ledc.h"

void pwm_config_50hz_esp32(uint8_t ch) {
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num  = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_16_BIT,  /* 16bit 分辨率 */
        .freq_hz = 50,                          /* 50Hz */
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_conf);
}

void pwm_set_pulse_esp32(uint8_t ch, uint16_t pulse_us) {
    /* 50Hz, 16bit → 分辨率 = 20000µs / 65536 ≈ 0.3µs */
    /* duty = pulse_us / 20000 × 65536 = pulse_us × 3.2768 */
    uint32_t duty = (uint32_t)pulse_us * 65536 / 20000;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
}
```

### 连续旋转舵机驱动

```c
/* servo_continuous.c - 连续旋转舵机驱动 */

/*
 * 连续旋转舵机 PWM 信号:
 *   1500µs → 停止
 *   500µs  → 最大反转速度
 *   2500µs → 最大正转速度
 *   线性映射: speed(-100~+100) → pulse(500~2500)
 */

servo_status_t servo_continuous_set_speed(servo_handle_t *h, int8_t speed) {
    if (!h || !h->initialized) return SERVO_ERR_INIT;
    if (speed < -100) speed = -100;
    if (speed > 100)  speed = 100;

    /* speed: -100(最大反转) ~ 0(停止) ~ +100(最大正转) */
    /* pulse: 500µs(反转) ~ 1500µs(停止) ~ 2500µs(正转) */
    uint16_t pulse;
    if (speed >= 0) {
        pulse = 1500 + (uint16_t)((int32_t)speed * 1000 / 100);
    } else {
        pulse = 1500 - (uint16_t)((int32_t)(-speed) * 1000 / 100);
    }

    return servo_set_pulse(h, pulse);
}
```

### 总线舵机驱动

```c
/* servo_bus.c - 总线舵机驱动 (LX-16A 协议) */

/* LX-16A 串口协议:
 * 帧头: 0x55 0x55
 * ID: 0xFE(广播) 或 0x00~0xFC
 * 长度: 数据长度+3
 * 命令: 功能码
 * 数据: 参数
 * 校验: ~sum & 0xFF (ID+长度+命令+数据)
 */

typedef struct {
    uint8_t id;                /* 舵机 ID */
    uart_port_t uart;          /* 串口端口 */
    uint8_t tx_pin;            /* 发送引脚 */
    uint8_t rx_pin;            /* 接收引脚 */
} bus_servo_config_t;

/* 计算校验和 */
static uint8_t bus_servo_checksum(const uint8_t *data, uint8_t len) {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < len; i++) sum += data[i];
    return (uint8_t)(~sum & 0xFF);
}

/* 发送指令 */
static void bus_servo_send(bus_servo_config_t *cfg, uint8_t cmd,
                            const uint8_t *data, uint8_t data_len) {
    uint8_t frame[16];
    uint8_t pos = 0;

    frame[pos++] = 0x55;  /* 帧头 */
    frame[pos++] = 0x55;
    frame[pos++] = cfg->id;
    frame[pos++] = data_len + 3;  /* 长度 = 数据+3 */
    frame[pos++] = cmd;
    for (uint8_t i = 0; i < data_len; i++)
        frame[pos++] = data[i];
    frame[pos++] = bus_servo_checksum(&frame[2], pos - 2);

    uart_write(cfg->uart, frame, pos);
}

/* 设置角度 (0~1000 对应 0~240°) */
servo_status_t bus_servo_set_angle(bus_servo_config_t *cfg,
                                    uint16_t angle, uint16_t time_ms) {
    if (angle > 1000) angle = 1000;

    uint8_t data[4];
    data[0] = angle & 0xFF;          /* 角度低字节 */
    data[1] = (angle >> 8) & 0xFF;   /* 角度高字节 */
    data[2] = time_ms & 0xFF;        /* 运行时间低字节 */
    data[3] = (time_ms >> 8) & 0xFF; /* 运行时间高字节 */

    /* 命令 0x01: 设置位置 */
    bus_servo_send(cfg, 0x01, data, 4);
    return SERVO_OK;
}

/* 读取位置反馈 */
int16_t bus_servo_read_angle(bus_servo_config_t *cfg) {
    /* 命令 0x02: 读取位置 */
    bus_servo_send(cfg, 0x02, NULL, 0);

    /* 等待回复 */
    uint8_t rx[10];
    int len = uart_read(cfg->uart, rx, 10, 100);  /* 100ms 超时 */
    if (len < 8) return -1;

    /* 验证帧头和校验 */
    if (rx[0] != 0x55 || rx[1] != 0x55) return -1;
    if (bus_servo_checksum(&rx[2], len - 3) != rx[len - 1]) return -1;

    uint16_t angle = rx[5] | (rx[6] << 8);
    return angle;
}

/* 设置舵机 ID */
servo_status_t bus_servo_set_id(bus_servo_config_t *cfg, uint8_t new_id) {
    uint8_t data[1] = {new_id};
    /* 命令 0x0D: 设置 ID (需用广播 ID 0xFE) */
    cfg->id = 0xFE;
    bus_servo_send(cfg, 0x0D, data, 1);
    cfg->id = new_id;
    return SERVO_OK;
}
```

## 4. 调试与测试规范

### 硬件验证清单

- [ ] 万用表确认 VServo 电压在规格范围内（SG90: 4.8~6V, MG996R: 4.8~7.2V）
- [ ] 确认 VServo 独立供电，非从 MCU 取电
- [ ] 确认 VServo 与 MCU 共地（GND 相连）
- [ ] 示波器检查 PWM 信号：50Hz、0.5~2.5ms 脉宽、TTL 电平
- [ ] 确认舵机线序：红=V+, 棕=GND, 橙=信号
- [ ] 大电解电容（1000µF）已并联在 VServo 旁

### 通信验证

- **PWM 舵机**：示波器测量信号引脚，确认 50Hz 频率和脉宽范围
- **总线舵机**：串口抓包确认帧格式正确（0x55 0x55 帧头）
- **角度验证**：设置 0°/90°/180°，目视确认舵机转到对应位置
- **行程校准**：某些舵机实际行程可能不完全等于 0~180°，需软件校准

### 性能指标

| 指标 | 测试方法 | 合格标准 |
|------|----------|----------|
| 角度精度 | 设置不同角度, 量角器对比 | 误差 < 2° |
| 响应时间 | 设置 0→180°, 计时 | ≤ 标称值 +10% |
| 堵转保护 | 堵住舵机, 测电流和温度 | 不烧毁, 无持续堵转 |
| 抖动 | 设定角度后观察 | 静止时无明显抖动 |
| 多舵机同步 | 同时设定多舵机同角度 | 角度差 < 3° |
| 通讯可靠性(总线) | 连续发送 1000 条指令 | 无丢失/错误 |

### 示波器测量

```
  PWM 信号验证:

  ┌──500µs──┐                    0°
  └─────────┘────────────────────┘ ← 20ms 周期

  ┌──1500µs─────────┐              90°
  └─────────────────┘──────────────┘

  ┌──2500µs──────────────────┐     180°
  └──────────────────────────┘─────┘

  检查项:
  ✓ 频率 = 50Hz (±1Hz)
  ✓ 脉宽范围 500~2500µs
  ✓ 高电平幅值 ≥ 3.0V
  ✓ 信号边沿陡峭 (< 1µs)
```

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 舵机不转 | 电源不足/接线错误 | 确认 VServo 独立供电且电压正确 |
| MCU 复位 | 从 MCU 取电, 电流冲击 | VServo 必须独立供电，不可从 MCU 取 |
| 舵机抖动 | PWM 信号不稳定/电源纹波 | 检查 PWM 频率(50Hz)；加大电源滤波 |
| 角度不准确 | 脉宽范围不匹配 | 校准 min/max pulse_us 值 |
| 舵机过热 | 长时间堵转/持续大角度 | 加堵转检测；运动完成后停止 PWM |
| 到不了 180° | 舵机机械限位或脉宽范围窄 | 增大 max_pulse_us (至 2500µs) |
| MG996R 噪音大 | 齿轮间隙/正常现象 | 金属齿舵机有正常工作噪音 |
| 多舵机同时动时电压跌落 | 电源功率不足 | 换大功率电源；错峰启动 |
| 总线舵机无响应 | ID 不匹配/波特率错误 | 确认 ID 和波特率(115200) |
| 总线舵机串口通信失败 | 半双工接线错误 | TX/RX 通过电阻合并到信号线 |

### 设计注意事项

1. **独立供电是第一要务**：舵机电流远超 MCU 引脚能力，必须独立供电
2. **共地必须连接**：VServo 电源地与 MCU 地必须相连，否则信号无参考
3. **避免堵转**：舵机堵转电流可达 2A+，长时间堵转烧毁驱动电路
4. **运动后断电**：不需要保持力时停止 PWM 输出，降低功耗和发热
5. **PWM 频率必须 50Hz**：频率偏差过大舵机可能工作异常或损坏
6. **总线舵机供电**：串联多舵机时线缆压降需考虑，末端电压可能不足
7. **校准每个舵机**：同型号舵机个体差异可能导致角度偏差，需软件校准

## 相关文档

- `../../templates/driver-template-pwm.c` — PWM 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `../../guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `../../guides/debugging-testing.md` — 调试与测试规范
- `../../guides/pitfalls.md` — 跨类别常见问题与避坑指南
