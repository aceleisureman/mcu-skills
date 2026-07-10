# 直流电机驱动开发规范

## 1. 概述与选型指南

### 简介

直流有刷电机（DC Motor）是嵌入式系统中最常用的旋转执行器，通过 H 桥电路实现正反转控制，通过 PWM 实现调速。电机驱动 IC 内部集成了 H 桥功率 MOS/BJT，简化了硬件设计。

### 常见驱动芯片对比

| 型号 | 桥数 | 电压范围 | 单路电流(峰值) | 内部压降 | 细分 | 接口 | 保护功能 | 价格 |
|------|------|----------|----------------|----------|------|------|----------|------|
| L298N | 双H桥 | 5~46V | 2A (3A) | ~1.8V(BJT) | 无 | IN/EN | 过热 | ¥5~10 |
| TB6612FNG | 双H桥 | 2.5~13.5V | 1.2A (3.2A) | ~0.5V(MOS) | 无 | IN/IN/PWM | 过热 | ¥3~6 |
| DRV8833 | 双H桥 | 2.7~10.8V | 2A (2A峰值) | ~0.36V | 无 | IN/IN | 过热/过流 | ¥3~8 |
| DRV8871 | 单H桥 | 6~45V | 3.6A (3.6A峰值) | ~0.3V | 无 | IN/IN | 过热/过流/欠压 | ¥8~15 |
| MX1508 | 双H桥 | 2~10V | 1.5A | ~0.6V | 无 | IN/IN | 无 | ¥1~3 |
| DRV8701 | 单H桥(预驱) | 5.9~45V | 外置MOS | 取决MOS | 有 | IN/IN/PWM | 过流/过热/欠压 | ¥5~10 |

> **压降说明**：L298N 采用 BJT 功率管，内部压降约 1.5~1.8V，发热严重效率低；其余芯片采用 MOS 管，压降 <0.5V，效率高。

### 选型决策树

```
电机电压 > 24V？ → 是 → L298N (46V) 或 DRV8871 (45V)
                 否 →
  电流 > 2A？ → 是 → DRV8871 (3.6A) 或 DRV8871 双片并联
              否 →
    电压 ≤ 11V？ → 是 → DRV8833 (低压高效)
                 否 → TB6612FNG (13.5V, 综合最优)
  需要静音/高效率？ → 是 → TB6612FNG / DRV8833 (MOS管)
  预算极低？ → 是 → MX1508 (无保护, 玩具级)
  工业级/大功率？ → 是 → DRV8701 (外置MOS, 可扩展)
```

### 适用场景

| 场景 | 推荐芯片 | 理由 |
|------|----------|------|
| 智能小车(6V/12V) | TB6612FNG | 体积小、效率高、双桥驱动双轮 |
| 玩具遥控(3.7V锂电) | DRV8833 | 低压性能好、内置保护 |
| 工业控制(24V) | DRV8871 | 高压大电流、集成保护 |
| 教学实验 | L298N | 模块化、易接线、但效率低 |
| 大功率机器人 | DRV8701+外置MOS | 灵活扩展、功率无上限 |

## 2. 硬件设计规范

### L298N 典型电路

```
                    Vmotor (≤46V)
                        │
              ┌─────────┴─────────┐
              │   L298N           │
  MCU IN1 ────┤IN1   OUT1├───────┤──→ Motor A+
  MCU IN2 ────┤IN2   OUT2├───────┤──→ Motor A-
  MCU ENA ────┤ENA        │      │
              │           │      │
  MCU IN3 ────┤IN3   OUT3├───────┤──→ Motor B+
  MCU IN4 ────┤IN4   OUT4├───────┤──→ Motor B-
  MCU ENB ────┤ENB        │      │
              └───────────┘      │
                        │         │
                   8×续流二极管    │
                   (1N4007)       │
                        │         │
                       GND ───────┘

  VCC(5V逻辑) ── 100nF ── GND
  Vmotor ── 470µF电解 + 100nF瓷片 ── GND
```

**L298N 电路要点**：
- **必须外接续流二极管**（8颗 1N4007 或肖特基 SS34），L298N 内部无续流二极管
- Vmotor 与逻辑 VCC 分开供电，共用 GND
- ENA/ENB 接 PWM 实现调速，IN1/IN2 控制方向
- 散热片必须安装，满载时功耗 P = I² × R_on ≈ 2² × 0.9 = 3.6W/桥
- 大电解电容（470µF）紧贴电源引脚，吸收电机反电动势冲击

### TB6612FNG 典型电路

```
                    VMotor (≤13.5V)
                        │
              ┌─────────┴─────────┐
              │  TB6612FNG        │
  MCU AIN1 ──┤AIN1  AO1├──────────┤──→ Motor A+
  MCU AIN2 ──┤AIN2  AO2├──────────┤──→ Motor A-
  MCU PWMA ──┤PWMA     │          │
              │         │          │
  MCU BIN1 ──┤BIN1  BO1├──────────┤──→ Motor B+
  MCU BIN2 ──┤BIN2  BO2├──────────┤──→ Motor B-
  MCU PWMB ──┤PWMB     │          │
              │         │          │
  MCU STBY ──┤STBY     │          │
              └─────────┘         │
                                   │
  VCC(3.3V) ── 100nF ── GND       │
  VMotor ── 10µF + 100nF ── GND ──┘
```

**TB6612FNG 电路要点**：
- 内部集成续流二极管，无需外接
- STBY 引脚接 MCU GPIO，低功耗时拉低
- PWM 频率推荐 < 100kHz（数据手册标称）
- VMotor 引脚旁必须放 10µF + 100nF 去耦
- 封装为 SSOP24，PCB 焊接需注意散热焊盘

### DRV8833 典型电路

```
                    VMotor (≤10.8V)
                        │
              ┌─────────┴─────────┐
              │  DRV8833          │
  MCU IN1 ───┤IN1   OUT1├─────────┤──→ Motor A+
  MCU IN2 ───┤IN2   OUT2├─────────┤──→ Motor A-
  MCU IN3 ───┤IN3   OUT3├─────────┤──→ Motor B+
  MCU IN4 ───┤IN4   OUT4├─────────┤──→ Motor B-
              │                   │
     MCU SLP ─┤nSLEEP    nFAULT├──┤──→ MCU GPIO(上拉)
              │                   │
              └───────────────────┘

  VMotor ── 47µF + 100nF ── GND
  VCC(3.3V内部稳压, 无需外接逻辑电源)
```

**DRV8833 电路要点**：
- 内部集成续流二极管和过流保护
- nFAULT 引脚接 MCU GPIO（上拉），故障时拉低
- nSLEEP 引脚控制休眠模式
- 支持 IN/IN 模式：两个引脚组合控制方向和制动
- 适合 3.7V 锂电池供电场景

### DRV8871 典型电路

```
                    VMotor (≤45V)
                        │
              ┌─────────┴─────────┐
              │  DRV8871          │
  MCU IN1 ───┤IN1   OUT1├─────────┤──→ Motor+
  MCU IN2 ───┤IN2   OUT2├─────────┤──→ Motor-
              │                   │
              │   nFAULT ─────────┤──→ MCU GPIO(上拉)
              │   IPROPI ─────────┤──→ R_IPROPI(2kΩ) → GND
              │                   │   (电流采样引脚)
              │   MODE ───────────┤──→ GND (IN/IN模式)
              └───────────────────┘

  VMotor ── 1µF + 100nF ── GND
  VM(高压) ── 0.1µF + 1µF ── GND
```

**DRV8871 电路要点**：
- 内置过流保护（OCP）、过热关断（TSD）、欠压锁定（UVLO）
- IPROPI 引脚输出电流镜像信号，接电阻后用 ADC 读取实时电流
- MODE 引脚选择控制模式（IN/IN 或 PH/EN）
- 高压应用需注意 PCB 电气间距（≥0.5mm@45V）

### 通用硬件设计要点

#### 续流二极管

```
电机换向时产生反向电动势(Back-EMF):

    Motor ──┤<── VCC     续流路径1
    Motor ──┤<── GND     续流路径2

  L298N: 必须外接8颗续流二极管 (1N4007 或 SS34)
  TB6612/DRV8833/DRV8871: 内部集成, 无需外接
```

#### PWM 频率选择

| 频率范围 | 特点 | 适用场景 |
|----------|------|----------|
| 100Hz~1kHz | 可闻噪音、电流纹波大 | 低端玩具 |
| 10kHz~20kHz | 超出可闻范围、效率好 | **通用推荐** |
| 20kHz~50kHz | 开关损耗增加 | 高响应要求 |
| >50kHz | 不推荐 | 损耗大、EMI严重 |

#### 死区时间

```
H桥换向时必须插入死区, 防止上下管直通短路:

  上管PWM:  ──────┐     ┌──────
                   │     │
                   └─────┘
  下管PWM:  ┌──────┘     └──────
                   ←死区→
                   ~500ns~1µs

  软件死区: 在切换方向前先 PWM=0 停止 1ms
  硬件死区: 使用高级定时器(STM32 TIM1/TIM8)的BDTR寄存器
```

#### 电源滤波

```
  VMotor ──┬── 470µF/35V 电解 ──┬── 100nF 瓷片 ──┬── 驱动IC VM
           │                     │                 │
          大电容吸收浪涌         高频去耦         IC旁路
```

### PCB 设计要点

- 功率走线（VM→IC→Motor）宽度 ≥ 2mm（2A）或 ≥ 3mm（3A+）
- 大面积铺铜做散热，驱动 IC 底部加散热过孔阵列
- 逻辑信号线远离功率走线，避免串扰
- 电机输出走线尽量短而粗，减小寄生电感
- 多路电机驱动需独立电源走线，避免共阻抗耦合

## 3. 驱动开发规范

### 统一接口定义

```c
typedef enum {
    DC_MOTOR_DIR_STOP = 0,
    DC_MOTOR_DIR_FORWARD,
    DC_MOTOR_DIR_BACKWARD,
    DC_MOTOR_DIR_BRAKE,       /* 制动（短接电机端子） */
} dc_motor_dir_t;

typedef enum {
    DC_MOTOR_OK = 0,
    DC_MOTOR_ERR_INIT,
    DC_MOTOR_ERR_INVALID_PARAM,
    DC_MOTOR_ERR_FAULT,       /* 驱动IC报错(过流/过热) */
} dc_motor_status_t;

typedef struct {
    uint8_t in1_pin;          /* 方向控制引脚1 */
    uint8_t in2_pin;          /* 方向控制引脚2 */
    uint8_t pwm_channel;      /* PWM通道号 */
    uint16_t pwm_freq_hz;     /* PWM频率, 推荐 10000~20000 */
    uint16_t pwm_resolution;  /* PWM分辨率, 如 1000 */
    uint8_t fault_pin;        /* 故障反馈引脚(可选, 0xFF=无) */
} dc_motor_config_t;

typedef struct {
    dc_motor_config_t cfg;
    bool initialized;
    dc_motor_dir_t current_dir;
    int16_t current_speed;    /* -1000~+1000, 正负=方向 */
} dc_motor_handle_t;

dc_motor_status_t dc_motor_init(dc_motor_handle_t *h, const dc_motor_config_t *cfg);
dc_motor_status_t dc_motor_set_speed(dc_motor_handle_t *h, int16_t speed);
dc_motor_status_t dc_motor_set_dir(dc_motor_handle_t *h, dc_motor_dir_t dir);
dc_motor_status_t dc_motor_stop(dc_motor_handle_t *h);
dc_motor_status_t dc_motor_brake(dc_motor_handle_t *h);
bool dc_motor_check_fault(dc_motor_handle_t *h);
void dc_motor_deinit(dc_motor_handle_t *h);
```

### L298N 驱动代码（PWM调速 + 方向控制）

```c
/* dc_motor_l298n.c - L298N 双H桥直流电机驱动 */

#include "dc_motor.h"

/* 平台适配层 - 需根据具体MCU实现 */
static void gpio_set(uint8_t pin, bool level) {
    /* TODO: 实现 GPIO 输出 */
}
static void pwm_set_duty(uint8_t ch, uint16_t duty) {
    /* TODO: 实现 PWM 占空比设置 */
}
static void pwm_start(uint8_t ch) { /* TODO */ }
static void pwm_stop(uint8_t ch)  { /* TODO */ }
static bool gpio_read(uint8_t pin) { /* TODO: return pin level */ return true; }

/* L298N 真值表:
 * IN1  IN2  ENA(PWM)  →  电机状态
 *  0    0    PWM       →  停止(滑行)
 *  1    0    PWM       →  正转, 速度=PWM占空比
 *  0    1    PWM       →  反转, 速度=PWM占空比
 *  1    1    X         →  制动(短接)
 */

dc_motor_status_t dc_motor_init(dc_motor_handle_t *h, const dc_motor_config_t *cfg) {
    if (!h || !cfg) return DC_MOTOR_ERR_INVALID_PARAM;
    if (cfg->pwm_freq_hz < 100 || cfg->pwm_freq_hz > 50000)
        return DC_MOTOR_ERR_INVALID_PARAM;

    h->cfg = *cfg;
    h->current_dir = DC_MOTOR_DIR_STOP;
    h->current_speed = 0;

    /* 初始化方向引脚为输出, 默认低电平 */
    gpio_set(cfg->in1_pin, 0);
    gpio_set(cfg->in2_pin, 0);

    /* 初始化PWM, 占空比=0 */
    pwm_set_duty(cfg->pwm_channel, 0);
    pwm_start(cfg->pwm_channel);

    h->initialized = true;
    return DC_MOTOR_OK;
}

dc_motor_status_t dc_motor_set_speed(dc_motor_handle_t *h, int16_t speed) {
    if (!h || !h->initialized) return DC_MOTOR_ERR_INIT;

    /* 限幅 */
    int16_t max = h->cfg.pwm_resolution;
    if (speed > max)  speed = max;
    if (speed < -max) speed = -max;

    if (speed == 0) {
        /* 速度为0: 停止 */
        gpio_set(h->cfg.in1_pin, 0);
        gpio_set(h->cfg.in2_pin, 0);
        pwm_set_duty(h->cfg.pwm_channel, 0);
        h->current_dir = DC_MOTOR_DIR_STOP;
    } else if (speed > 0) {
        /* 正转 */
        gpio_set(h->cfg.in1_pin, 1);
        gpio_set(h->cfg.in2_pin, 0);
        pwm_set_duty(h->cfg.pwm_channel, (uint16_t)speed);
        h->current_dir = DC_MOTOR_DIR_FORWARD;
    } else {
        /* 反转 */
        gpio_set(h->cfg.in1_pin, 0);
        gpio_set(h->cfg.in2_pin, 1);
        pwm_set_duty(h->cfg.pwm_channel, (uint16_t)(-speed));
        h->current_dir = DC_MOTOR_DIR_BACKWARD;
    }

    h->current_speed = speed;
    return DC_MOTOR_OK;
}

dc_motor_status_t dc_motor_stop(dc_motor_handle_t *h) {
    return dc_motor_set_speed(h, 0);
}

dc_motor_status_t dc_motor_brake(dc_motor_handle_t *h) {
    if (!h || !h->initialized) return DC_MOTOR_ERR_INIT;

    /* 制动: IN1=IN2=1, 短接电机端子 */
    gpio_set(h->cfg.in1_pin, 1);
    gpio_set(h->cfg.in2_pin, 1);
    pwm_set_duty(h->cfg.pwm_channel, 0);
    h->current_dir = DC_MOTOR_DIR_BRAKE;
    h->current_speed = 0;
    return DC_MOTOR_OK;
}

bool dc_motor_check_fault(dc_motor_handle_t *h) {
    if (!h || h->cfg.fault_pin == 0xFF) return false;
    /* L298N 无故障引脚, 此接口对L298N无效 */
    return false;
}
```

### TB6612FNG 驱动代码

```c
/* dc_motor_tb6612.c - TB6612FNG 双H桥直流电机驱动 */

/* TB6612FNG 真值表:
 * AIN1  AIN2  PWMA  →  电机状态
 *  H     H     X    →  制动(短接)
 *  L     H     PWM  →  反转(CCW)
 *  H     L     PWM  →  正转(CW)
 *  L     L     X    →  停止(滑行)
 *  STBY=L: 休眠模式
 */

typedef struct {
    dc_motor_config_t motor_a;
    dc_motor_config_t motor_b;
    uint8_t standby_pin;       /* STBY 引脚 */
} tb6612_config_t;

typedef struct {
    tb6612_config_t cfg;
    dc_motor_handle_t motor_a;
    dc_motor_handle_t motor_b;
    bool initialized;
} tb6612_handle_t;

tb6612_status_t tb6612_init(tb6612_handle_t *h, const tb6612_config_t *cfg) {
    if (!h || !cfg) return DC_MOTOR_ERR_INVALID_PARAM;

    h->cfg = *cfg;

    /* 唤醒: STBY = HIGH */
    gpio_set(cfg->standby_pin, 1);

    /* 初始化两路电机 */
    if (dc_motor_init(&h->motor_a, &cfg->motor_a) != DC_MOTOR_OK)
        return DC_MOTOR_ERR_INIT;
    if (dc_motor_init(&h->motor_b, &cfg->motor_b) != DC_MOTOR_OK)
        return DC_MOTOR_ERR_INIT;

    h->initialized = true;
    return DC_MOTOR_OK;
}

/* 设置电机A速度 (-1000~+1000) */
tb6612_status_t tb6612_set_motor_a(tb6612_handle_t *h, int16_t speed) {
    if (!h || !h->initialized) return DC_MOTOR_ERR_INIT;
    return dc_motor_set_speed(&h->motor_a, speed);
}

/* 设置电机B速度 */
tb6612_status_t tb6612_set_motor_b(tb6612_handle_t *h, int16_t speed) {
    if (!h || !h->initialized) return DC_MOTOR_ERR_INIT;
    return dc_motor_set_speed(&h->motor_b, speed);
}

/* 差速控制: left/right 为左右轮速度 */
tb6612_status_t tb6612_set_diff_speed(tb6612_handle_t *h,
                                       int16_t left, int16_t right) {
    if (!h || !h->initialized) return DC_MOTOR_ERR_INIT;

    int16_t max = h->motor_a.cfg.pwm_resolution;
    if (left > max)   left = max;
    if (left < -max)  left = -max;
    if (right > max)  right = max;
    if (right < -max) right = -max;

    dc_motor_set_speed(&h->motor_a, left);
    dc_motor_set_speed(&h->motor_b, right);
    return DC_MOTOR_OK;
}

/* 进入休眠模式 */
void tb6612_sleep(tb6612_handle_t *h) {
    if (!h || !h->initialized) return;
    dc_motor_stop(&h->motor_a);
    dc_motor_stop(&h->motor_b);
    gpio_set(h->cfg.standby_pin, 0);  /* STBY=0 进入休眠 */
}

/* 唤醒 */
void tb6612_wake(tb6612_handle_t *h) {
    if (!h || !h->initialized) return;
    gpio_set(h->cfg.standby_pin, 1);  /* STBY=1 唤醒 */
}
```

### 平台适配示例（STM32 HAL）

```c
/* platform_stm32.c - STM32 HAL 适配层 */

/* PWM配置: TIM3 CH1, 10kHz, 分辨率1000 */
void pwm_init_stm32(void) {
    /* 假设 TIM3 已由 CubeMX 配置 */
    extern TIM_HandleTypeDef htim3;
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    /* ARR=8399, PSC=0 → 84MHz/8400 = 10kHz */
    /* 修改 ARR 实现分辨率调整: ARR=999, PSC=83 → 84MHz/84/1000 = 1kHz? */
    /* 正确: PSC=0, ARR=8399 → 10kHz, 分辨率8400 */
}

static void pwm_set_duty_stm32(uint8_t ch, uint16_t duty) {
    extern TIM_HandleTypeDef htim3;
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim3);
    uint32_t compare = (uint32_t)duty * arr / 1000;
    __HAL_TIM_SET_COMPARE(&htim3, ch == 0 ? TIM_CHANNEL_1 : TIM_CHANNEL_2, compare);
}

static void gpio_set_stm32(uint8_t pin, bool level) {
    /* pin 编码: 高4位=端口(A=E, B=F...), 低4位=引脚号 */
    GPIO_TypeDef *port;
    uint16_t gpio_pin;
    switch (pin >> 4) {
        case 0xE: port = GPIOA; break;
        case 0xF: port = GPIOB; break;
        default:  port = GPIOA; break;
    }
    gpio_pin = 1 << (pin & 0x0F);
    HAL_GPIO_WritePin(port, gpio_pin, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
```

## 4. 调试与测试规范

### 硬件验证清单

- [ ] 万用表确认 VMotor 和 VCC 电压在规格范围内
- [ ] 确认电机两端电阻（万用表测量），排除短路
- [ ] L298N：确认8颗续流二极管方向正确（阴极接VCC/阳极接输出，或反向并联）
- [ ] 示波器检查 PWM 输出波形（频率、占空比、电平幅值）
- [ ] 确认去耦电容已贴装且容值正确
- [ ] 上电前测量 VM-GND 和 VCC-GND 是否短路

### 通信验证方法

- **方向控制验证**：IN1=1,IN2=0 观察电机正转；IN1=0,IN2=1 观察反转
- **PWM验证**：示波器测量 ENA/ENB 引脚 PWM 波形，确认频率 10~20kHz
- **死区验证**：快速切换方向时示波器观察 OUT1/OUT2，确认无直通
- **FAULT验证**（DRV8833/DRV8871）：人为堵转电机，观察 nFAULT 是否拉低

### 性能测试指标

| 测试项 | 方法 | 合格标准 |
|--------|------|----------|
| 空载电流 | 电机空转时万用表测 VM 电流 | < 标称空载电流 |
| 堵转电流 | 卡住电机轴, 瞬间测电流 | < 驱动IC峰值电流 |
| PWM线性度 | 设置不同占空比, 测电机转速 | 线性误差 < 10% |
| 温升 | 满载运行10min, 红外测温IC表面 | < 70°C |
| 换向响应 | 正转→反转切换, 示波器测响应时间 | < 5ms（含死区） |
| 效率 | P_out/P_in × 100% | L298N > 60%, MOS类 > 85% |

### 示波器测量要点

```
PWM 信号测量:
  ┌──────── Probe A: PWM 引脚 (EN/ENA)
  │         Probe B: 电机输出端 (OUT1)
  │
  │  正常波形:
  │  PWM:  ┌──┐  ┌──┐  ┌──┐
  │        ┘  └──┘  └──┘  └──
  │  OUT:  ──┐  ┌──┐  ┌──┐
  │           ┘  └──┘  └──┘
  │
  │  异常(直通)波形:
  │  PWM:  ┌────┐ ← 无死区, 上下管同时导通
  │        ┘    └
  │  电流:  ↑↑↑ 大尖峰
```

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| L298N 发热严重 | BJT 压降大(1.8V), 效率低 | 换 TB6612FNG/DRV8833 或加散热片+风扇 |
| L298N 烧毁 | 未接续流二极管 | 必须接8颗续流二极管（SS34 肖特基最佳） |
| 电机转速达不到预期 | 驱动IC压降导致VM降低 | 选低压降MOS驱动；提高VM补偿 |
| PWM频率太低电机有啸叫 | 频率在人耳可闻范围(20Hz~20kHz) | 提高到 20kHz 以上 |
| 换向时MCU复位 | 电机反电动势冲击电源 | 加大电源滤波电容(470µF+)；电源隔离 |
| TB6612 不工作 | STBY 引脚悬空或为低 | STBY 必须接高电平才能工作 |
| 电机抖动不转 | PWM占空比太低/死区太大 | 增大最低有效占空比(>15%) |
| 堵转烧驱动IC | 无过流保护/超时检测 | 加堵转检测(电流采样+超时停机) |
| 多电机同时启动电压跌落 | 电源功率不足 | 选大功率电源；软件错峰启动 |
| DRV8833 nFAULT 拉低 | 过流/过热触发保护 | 检查电机是否堵转；降低PWM占空比 |
| DRV8871 电流采样不准 | IPROPI 电阻精度不够 | 使用 1% 精密电阻；校准 ADC |

### 设计注意事项

1. **堵转保护必做**：电机堵转电流可达额定电流的5~10倍，必须软件检测并限时断电
2. **电源隔离**：电机电源与 MCU 电源独立，共地但不共 VCC，避免电机噪声干扰 MCU
3. **PWM分辨率与频率权衡**：高分辨率需要低频率，10kHz@10bit 是较好的折中
4. **线材选择**：电机电流 > 1A 时使用硅胶线（AWG22 以上），杜邦线仅适合小电流测试
5. **上电顺序**：先上逻辑电源 VCC，再上电机电源 VMotor；断电反过来，保护驱动 IC

## 相关文档

- `../../templates/driver-template-adc.c` — ADC 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-pwm.c` — PWM 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `../../guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `../../guides/debugging-testing.md` — 调试与测试规范
- `../../guides/pitfalls.md` — 跨类别常见问题与避坑指南
