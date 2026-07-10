# 步进电机驱动开发规范

## 1. 概述与选型指南

### 简介

步进电机通过脉冲信号控制精确转动角度，每个脉冲对应固定步距角，无需编码器即可实现开环位置控制。广泛应用于 3D 打印、CNC、机器人、精密定位等场景。

步进电机按结构分为：
- **PM型（永磁式）**：转子为永磁体，步距角较大（如 28BYJ-48）
- **HB型（混合式）**：永磁+齿结构，步距角小、扭矩大（如 NEMA17/42）
- **VR型（可变磁阻式）**：无永磁体，已较少使用

### 常见型号对比

#### 电机对比

| 型号 | 类型 | 步距角 | 额定电压 | 额定电流 | 保持扭矩 | 减速比 | 适用场景 | 价格 |
|------|------|--------|----------|----------|----------|--------|----------|------|
| 28BYJ-48 | PM减速 | 5.625° | 5V | 200mA | 34mN·m | 1:64 | 玩具/低精度 | ¥3~8 |
| NEMA17(42) | HB | 1.8° | 2.8V | 1.5~1.7A | 0.4~0.5N·m | 直驱 | 3D打印/CNC | ¥20~80 |
| NEMA23(57) | HB | 1.8° | 2.5V | 2.0~2.8A | 1.0~1.5N·m | 直驱 | 中型CNC | ¥50~200 |
| NEMA11 | HB | 0.9° | 3.2V | 0.67A | 0.12N·m | 直驱 | 小型设备 | ¥30~60 |

#### 驱动芯片对比

| 型号 | 最大电压 | 最大电流 | 细分 | 接口 | 特点 | 价格 |
|------|----------|----------|------|------|------|------|
| ULN2003 | 50V | 500mA/路 | 无(整步/半步) | 4路GPIO | 达林顿阵列, 仅适合小电机 | ¥1~2 |
| A4988 | 35V | 2A | 16细分 | Step/Dir | 最常见, 模块化 | ¥3~8 |
| DRV8825 | 45V | 2.5A | 32细分 | Step/Dir | 高压大电流 | ¥5~12 |
| TMC2209 | 36V | 2A | 256细分 | Step/Dir / UART | 超静音(SpreadCycle), 电流自适应 | ¥15~30 |
| TMC5160 | 60V | 5A(外置检测) | 256细分 | Step/Dir / SPI | 高性能, 大功率 | ¥30~60 |
| LV8729 | 32V | 1.5A | 128细分 | Step/Dir | 低振动, 低成本 | ¥8~15 |

### 选型决策树

```
电机是 28BYJ-48(5V减速)？ → 是 → ULN2003 驱动板 (简单廉价)
                            否 →
  电流 ≤ 2A？ → 是 →
    需要静音？ → 是 → TMC2209 (256细分, SpreadCycle)
               否 → 预算低？ → A4988 (最常见, 16细分)
                            否 → DRV8825 (32细分, 45V)
  电流 > 2A？ → 是 → TMC5160 (5A, 外置MOS)
  电压 > 36V？ → 是 → DRV8825 (45V) 或 TMC5160 (60V)
  需要位置反馈？ → 是 → 考虑闭环步进(AS5600编码器)或换伺服电机
```

### 适用场景

| 场景 | 推荐组合 | 理由 |
|------|----------|------|
| 3D 打印机 | NEMA17 + TMC2209 | 超静音、高精度、256细分 |
| 桌面 CNC | NEMA17/NEMA23 + DRV8825 | 高压大电流、32细分 |
| 教学实验 | 28BYJ-48 + ULN2003 | 廉价、易接线、5V供电 |
| 机械臂 | NEMA17 + A4988 | 性价比好、社区支持多 |
| 纺织机械 | NEMA23 + TMC5160 | 大扭矩、高可靠性 |
| 智能窗帘 | 28BYJ-48 + ULN2003 | 低成本、低噪音 |

## 2. 硬件设计规范

### ULN2003 + 28BYJ-48 电路

```
  MCU GPIO ──┬── IN1(ULN2003) ── O1 ──── Motor 蓝
             ├── IN2            ── O2 ──── Motor 粉
             ├── IN3            ── O3 ──── Motor 黄
             ├── IN4            ── O4 ──── Motor 橙
             │
  5V ────────┴── COM(续流公共端)
  5V ────────── Motor 红(公共端)

  ULN2003 内部为达林顿管阵列, 集电极开路输出:
  - 输出拉低时对应线圈导通
  - COM 接电源提供续流回路
  - 内部已集成续流二极管
```

**28BYJ-48 线序**：

| 线色 | 接口 | ULN2003输出 |
|------|------|-------------|
| 红 | 公共端 | 5V |
| 橙 | A相 | O4 (IN4) |
| 黄 | B相 | O3 (IN3) |
| 粉 | A'相 | O2 (IN2) |
| 蓝 | B'相 | O1 (IN1) |

**ULN2003 电路要点**：
- COM 引脚必须接 5V，提供续流回路
- 28BYJ-48 减速比 1:64，输出轴每圈 = 4096 步（半步模式）
- 不适合高速旋转，推荐 ≤ 15 RPM

### A4988 + NEMA17 电路

```
  VMotor (≤35V) ── 100µF电解 ── 100nF ── GND
       │
       ├── A4988 VMOT
  MCU STEP ────── A4988 STEP
  MCU DIR  ────── A4988 DIR
  MCU EN   ────── A4988 ENABLE(低有效)
  MCU MS1  ────── A4988 MS1 ┐
  MCU MS2  ────── A4988 MS2 ├── 细分设置
  MCU MS3  ────── A4988 MS3 ┘
       ├── A4988 1A,1B ──── Motor A相
       ├── A4988 2A,2B ──── Motor B相
  VREF(电位器) ── A4988 REF ── 限流设置
  3.3V ────────── A4988 VDD(逻辑电源)
```

**A4988 细分设置真值表**：

| MS3 | MS2 | MS1 | 细分 | 步距角(NEMA17) | 每圈脉冲数 |
|-----|-----|-----|------|----------------|------------|
| 0 | 0 | 0 | 整步 | 1.8° | 200 |
| 0 | 0 | 1 | 半步 | 0.9° | 400 |
| 0 | 1 | 0 | 1/4步 | 0.45° | 800 |
| 0 | 1 | 1 | 1/8步 | 0.225° | 1600 |
| 1 | 0 | 0 | 1/16步 | 0.1125° | 3200 |
| 1 | 1 | 1 | 1/16步 | 0.1125° | 3200 |

**A4988 限流计算**：
```
  Imax = VREF / (8 × Rs)
  A4988 模块 Rs = 0.05Ω:
  Imax = VREF / 0.4
  目标 1A → VREF = 0.4V
  目标 1.5A → VREF = 0.6V
  目标 2A → VREF = 0.8V
```

**A4988 电路要点**：
- VMOT 旁必须放 100µF 电解电容（吸收反电动势）
- ENABLE 低有效，不使用时拉高关闭输出
- VREF 限流设置至关重要，过大烧芯片/电机，过小扭矩不足
- 散热：满载需要贴散热片，过热自动降流导致丢步

### TMC2209 + NEMA17 电路

```
  VMotor (≤36V) ── 100µF ── 100nF ── GND
       ├── TMC2209 VM
  MCU STEP ────── TMC2209 STEP
  MCU DIR  ────── TMC2209 DIR
  MCU EN   ────── TMC2209 EN(低有效)
  MCU UART TX ───┐
                 ├── 单线UART (PDN/UART 引脚)
  MCU UART RX ───┘
       ├── TMC2209 OA1,OA2 ──── Motor A相
       ├── TMC2209 OB1,OB2 ──── Motor B相
  MCU ADC ─────── TMC2209 DIAG(诊断引脚)
```

**TMC2209 电路要点**：
- 支持 Step/Dir 和 UART 配置模式（可同时使用）
- 内置 256 细分，通过 UART 配置微步数
- StealthChop2 模式实现超静音（< 10dB 电机噪音）
- SpreadCycle 模式适合高速大扭矩
- 电流自适应：根据负载自动调节电流

### DRV8825 电路

**DRV8825 细分真值表**：

| M2 | M1 | M0 | 细分 | 每圈脉冲数 |
|----|-----|-----|------|------------|
| 0 | 0 | 0 | 整步 | 200 |
| 0 | 0 | 1 | 半步 | 400 |
| 0 | 1 | 0 | 1/4步 | 800 |
| 0 | 1 | 1 | 1/8步 | 1600 |
| 1 | 0 | 0 | 1/16步 | 3200 |
| 1 | 0 | 1 | 1/32步 | 6400 |
| 1 | 1 | x | 1/32步 | 6400 |

**DRV8825 限流计算**（与 A4988 不同）：
```
  Imax = VREF / (2 × Rs)
  DRV8825 模块 Rs = 0.1Ω:
  Imax = VREF / 0.2
  目标 1A → VREF = 0.2V
  目标 1.5A → VREF = 0.3V
  目标 2.5A → VREF = 0.5V
```

### 通用硬件设计要点

#### 限流电阻配置

```
  ┌──────────────────────────────────────────┐
  │  警告: VREF 过大将烧毁驱动IC和电机!      │
  │  上电前必须用万用表测量 VREF!             │
  └──────────────────────────────────────────┘

  设置步骤:
  1. 断开电机
  2. 上电(仅逻辑电源)
  3. 万用表测 VREF 引脚电压
  4. 调节电位器到目标值
  5. 断电, 接电机
  6. 测试运行
```

#### 使能/方向/步进信号时序

```
  ENABLE(低有效): 拉低=电机励磁(有保持力), 拉高=释放(无保持力)
  DIR:            高=正转(CW), 低=反转(CCW)
  STEP:           上升沿触发一步, 脉冲宽度 ≥ 1µs

           ┌─────≥1µs─────┐
  STEP ────┘               └───
  DIR  ──────────────────────── (DIR 需在 STEP 上升沿前 ≥200ns 稳定)
  EN   ──────────────────────── (EN 需在 STEP 前 ≥1µs 稳定)
```

#### 散热设计

| 芯片 | 无散热片 | 有散热片 | 建议 |
|------|----------|----------|------|
| A4988 | ≤ 1A | ≤ 1.5A | >1A 必加散热片 |
| DRV8825 | ≤ 1.2A | ≤ 2A | >1.2A 必加散热片 |
| TMC2209 | ≤ 1A | ≤ 2A | 自适应降流, 散热要求略低 |

### PCB 设计要点

- VMOT 到驱动 IC 的走线短而粗（≥1.5mm），减小寄生电感
- 电机输出走线远离逻辑信号，避免串扰
- VMOT 旁的大电解电容紧贴 IC VMOT 引脚（< 5mm）
- GND 大面积铺铜，功率地与信号地单点连接
- 多片驱动并排时间距 ≥ 5mm，便于贴散热片

## 3. 驱动开发规范

### 统一接口定义

```c
typedef enum {
    STEPPER_OK = 0,
    STEPPER_ERR_INIT,
    STEPPER_ERR_INVALID_PARAM,
    STEPPER_ERR_FAULT,        /* 过温/过流 */
    STEPPER_ERR_STALLED,      /* 堵转检测 */
} stepper_status_t;

typedef struct {
    uint8_t step_pin;
    uint8_t dir_pin;
    uint8_t enable_pin;       /* 低有效 */
    uint16_t steps_per_rev;   /* 每圈脉冲数(含细分) */
    uint16_t max_speed_rpm;
    uint16_t accel_rpm_per_s;
} stepper_config_t;

typedef struct {
    stepper_config_t cfg;
    bool initialized;
    bool enabled;
    int32_t current_position; /* 当前位置(步) */
    int32_t target_position;
    int32_t current_speed;    /* 当前速度(步/秒) */
} stepper_handle_t;

stepper_status_t stepper_init(stepper_handle_t *h, const stepper_config_t *cfg);
stepper_status_t stepper_enable(stepper_handle_t *h);
stepper_status_t stepper_disable(stepper_handle_t *h);
stepper_status_t stepper_move_steps(stepper_handle_t *h, int32_t steps);
stepper_status_t stepper_move_to(stepper_handle_t *h, int32_t position);
stepper_status_t stepper_rotate(stepper_handle_t *h, int16_t rpm);
stepper_status_t stepper_stop(stepper_handle_t *h);
void stepper_set_zero(stepper_handle_t *h);
```

### ULN2003 半步/全步驱动代码（28BYJ-48）

```c
/* stepper_28byj48.c - 28BYJ-48 步进电机驱动 */

/* 半步驱动序列 (8步, 励磁1-2相交替) */
static const uint8_t half_step_seq[8] = {
    0b0001, 0b0011, 0b0010, 0b0110,
    0b0100, 0b1100, 0b1000, 0b1001,
};

/* 全步驱动序列 (4步, 励磁1相) */
static const uint8_t full_step_seq[4] = {
    0b0001, 0b0010, 0b0100, 0b1000,
};

#define HALF_STEPS_PER_REV  4096   /* 半步: 4096步/圈(含减速比1:64) */
#define FULL_STEPS_PER_REV  2048   /* 全步: 2048步/圈 */

typedef struct {
    uint8_t in_pins[4];      /* IN1~IN4 GPIO */
    uint8_t step_index;
    bool use_half_step;
    int32_t current_position;
} stepper_28byj_t;

void stepper_28byj_init(stepper_28byj_t *h, uint8_t in1, uint8_t in2,
                         uint8_t in3, uint8_t in4, bool half_step) {
    h->in_pins[0] = in1;
    h->in_pins[1] = in2;
    h->in_pins[2] = in3;
    h->in_pins[3] = in4;
    h->step_index = 0;
    h->use_half_step = half_step;
    h->current_position = 0;
    for (int i = 0; i < 4; i++)
        gpio_set(h->in_pins[i], 0);
}

static void stepper_28byj_output(stepper_28byj_t *h, uint8_t pattern) {
    for (int i = 0; i < 4; i++)
        gpio_set(h->in_pins[i], (pattern >> i) & 0x01);
}

/* 走一步: dir=1正转, dir=-1反转 */
void stepper_28byj_step(stepper_28byj_t *h, int8_t dir) {
    const uint8_t *seq = h->use_half_step ? half_step_seq : full_step_seq;
    uint8_t seq_len = h->use_half_step ? 8 : 4;

    if (dir > 0)
        h->step_index = (h->step_index + 1) % seq_len;
    else
        h->step_index = (h->step_index + seq_len - 1) % seq_len;

    stepper_28byj_output(h, seq[h->step_index]);

    if (dir > 0) h->current_position++;
    else         h->current_position--;
}

/* 走指定步数(阻塞) */
void stepper_28byj_move(stepper_28byj_t *h, int32_t steps, uint16_t step_delay_ms) {
    int8_t dir = (steps >= 0) ? 1 : -1;
    uint32_t abs_steps = (steps >= 0) ? steps : -steps;
    for (uint32_t i = 0; i < abs_steps; i++) {
        stepper_28byj_step(h, dir);
        delay_ms(step_delay_ms);
    }
}

/* 转指定圈数 */
void stepper_28byj_rotate(stepper_28byj_t *h, float rev, uint16_t step_delay_ms) {
    uint16_t spr = h->use_half_step ? HALF_STEPS_PER_REV : FULL_STEPS_PER_REV;
    stepper_28byj_move(h, (int32_t)(rev * spr), step_delay_ms);
}

/* 释放(断电) */
void stepper_28byj_release(stepper_28byj_t *h) {
    for (int i = 0; i < 4; i++)
        gpio_set(h->in_pins[i], 0);
}
```

### A4988 Step/Dir 驱动代码（含梯形加减速）

```c
/* stepper_a4988.c - A4988 Step/Dir 步进电机驱动(梯形加减速) */

typedef struct {
    uint8_t step_pin;
    uint8_t dir_pin;
    uint8_t enable_pin;
    uint16_t steps_per_rev;

    int32_t current_position;
    uint32_t step_interval_us;

    /* 加减速参数 */
    uint32_t min_step_interval; /* 最大速度步间隔(µs) */
    uint32_t max_step_interval; /* 启动速度步间隔(µs) */

    /* 运动状态 */
    bool is_moving;
    uint32_t last_step_time;
    int8_t direction;
    uint32_t steps_remaining;
    uint32_t steps_accel;
    uint32_t steps_decel;
    uint32_t steps_constant;
    uint32_t step_count;
} stepper_a4988_t;

void stepper_a4988_init(stepper_a4988_t *h, uint8_t step, uint8_t dir,
                         uint8_t enable, uint16_t steps_per_rev) {
    h->step_pin = step;
    h->dir_pin = dir;
    h->enable_pin = enable;
    h->steps_per_rev = steps_per_rev;
    h->current_position = 0;
    h->is_moving = false;

    /* 默认: 最大600rpm, 启动50rpm */
    h->min_step_interval = 1000000 / (steps_per_rev * 600 / 60);
    h->max_step_interval = 1000000 / (steps_per_rev * 50 / 60);

    gpio_set(h->enable_pin, 1);  /* 禁用 */
    gpio_set(h->step_pin, 0);
    gpio_set(h->dir_pin, 0);
}

void stepper_a4988_enable(stepper_a4988_t *h, bool en) {
    gpio_set(h->enable_pin, en ? 0 : 1);  /* 低有效 */
}

/*
 * 梯形加减速规划:
 *   速度
 *    ↑        ┌──────┐
 *    │       ╱      ╲
 *    │      ╱        ╲
 *    └───╱──────────────╲───→ 时间
 *      加速   匀速    减速
 */
static void planner_calc_profile(stepper_a4988_t *h, uint32_t total_steps) {
    /* 加速到最大速度需要的步数: s = (v²-v0²)/(2a) */
    /* 简化: 用步间隔近似 */
    float v_max = 1000000.0f / h->min_step_interval;
    float v_start = 1000000.0f / h->max_step_interval;
    /* 加速度: 1000步/s² */
    float accel = 1000.0f;
    float s_accel = (v_max * v_max - v_start * v_start) / (2.0f * accel);

    h->steps_accel = (uint32_t)s_accel;
    h->steps_decel = h->steps_accel;
    uint32_t half = total_steps / 2;

    if (h->steps_accel > half) {
        /* 距离不够: 三角形曲线 */
        h->steps_accel = half;
        h->steps_decel = half;
        h->steps_constant = 0;
    } else {
        h->steps_constant = total_steps - 2 * h->steps_accel;
    }

    h->steps_remaining = total_steps;
    h->step_count = 0;
    h->step_interval_us = h->max_step_interval;
}

void stepper_a4988_move_to(stepper_a4988_t *h, int32_t target_pos) {
    int32_t diff = target_pos - h->current_position;
    if (diff == 0) return;

    h->direction = (diff > 0) ? 1 : -1;
    uint32_t total = (diff > 0) ? diff : -diff;

    planner_calc_profile(h, total);
    gpio_set(h->dir_pin, h->direction > 0 ? 1 : 0);
    stepper_a4988_enable(h, true);
    h->is_moving = true;
    h->last_step_time = micros();
}

/* 步进引擎(主循环或定时器中断调用) */
void stepper_a4988_run(stepper_a4988_t *h) {
    if (!h->is_moving) return;

    uint32_t now = micros();
    if (now - h->last_step_time < h->step_interval_us) return;

    /* 产生步进脉冲(≥1µs) */
    gpio_set(h->step_pin, 1);
    delay_us(2);
    gpio_set(h->step_pin, 0);

    h->last_step_time = now;
    h->step_count++;
    h->steps_remaining--;
    h->current_position += h->direction;

    /* 更新步间隔(加减速) */
    if (h->step_count < h->steps_accel) {
        /* 加速段: 线性减小步间隔 */
        float p = (float)h->step_count / h->steps_accel;
        h->step_interval_us = h->max_step_interval -
            (uint32_t)((h->max_step_interval - h->min_step_interval) * p);
    } else if (h->step_count < h->steps_accel + h->steps_constant) {
        /* 匀速段 */
        h->step_interval_us = h->min_step_interval;
    } else {
        /* 减速段: 线性增大步间隔 */
        uint32_t ds = h->step_count - h->steps_accel - h->steps_constant;
        float p = (float)ds / h->steps_decel;
        h->step_interval_us = h->min_step_interval +
            (uint32_t)((h->max_step_interval - h->min_step_interval) * p);
    }

    if (h->steps_remaining == 0) {
        h->is_moving = false;
    }
}

void stepper_a4988_set_speed(stepper_a4988_t *h, uint16_t max_rpm,
                              uint16_t start_rpm) {
    h->min_step_interval = 1000000 / (h->steps_per_rev * max_rpm / 60);
    h->max_step_interval = 1000000 / (h->steps_per_rev * start_rpm / 60);
}

bool stepper_a4988_is_moving(stepper_a4988_t *h) { return h->is_moving; }

void stepper_a4988_stop(stepper_a4988_t *h) {
    h->is_moving = false;
    h->steps_remaining = 0;
}

void stepper_a4988_set_zero(stepper_a4988_t *h) {
    h->current_position = 0;
}
```

### 使用示例

```c
/* 示例: A4988 驱动 NEMA17, 1/8细分(1600步/圈) */
stepper_a4988_t motor;

void setup(void) {
    /* STEP=PA0, DIR=PA1, EN=PA2, 1600步/圈 */
    stepper_a4988_init(&motor, 0x00, 0x01, 0x02, 1600);
    /* 最大300rpm, 启动20rpm */
    stepper_a4988_set_speed(&motor, 300, 20);
}

void loop(void) {
    if (!stepper_a4988_is_moving(&motor)) {
        /* 运动到 100mm 位置(导程8mm/圈 → 200步/mm) */
        stepper_a4988_move_to(&motor, 100 * 200);
    }
    stepper_a4988_run(&motor);  /* 每次循环调用 */
}
```

## 4. 调试与测试规范

### 硬件验证清单

- [ ] 万用表确认 VMOT 电压正确且极性无误
- [ ] 上电前测量 VREF 电压，确认限流值合理
- [ ] 确认 VMOT 旁大电解电容已贴装
- [ ] 示波器检查 STEP 脉冲波形（脉宽 ≥ 1µs、电平幅值）
- [ ] 确认 DIR/ENABLE 信号逻辑正确
- [ ] 电机接线与相序正确（A/A' 和 B/B' 不能接反）

### 步进电机验证

- **单步测试**：发送单个 STEP 脉冲，观察电机是否走一步
- **方向测试**：DIR=高/低，确认正反转方向
- **细分测试**：设置不同细分，走固定步数验证每圈步数
- **电流测试**：万用表串联在电机回路中，确认工作电流
- **堵转测试**：手动卡住电机轴，确认驱动 IC 不会过热烧毁

### 加减速验证

```
  示波器测量 STEP 脉冲间隔:

  加速段(间隔逐渐变短):     匀速段(间隔恒定):      减速段(间隔逐渐变长):
  ┌┐ ┌┐ ┌┐ ┌┐ ┌┐          ┌┐┌┐┌┐┌┐              ┌┐  ┌┐  ┌┐  ┌┐
  └┘ └┘ └┘ └┘ └┘          └┘└┘└┘└┘              └┘  └┘  └┘  └┘
  ←宽→  ←窄→               ←恒定→                 ←窄→  ←宽→
```

### 性能指标

| 指标 | 测试方法 | 合格标准 |
|------|----------|----------|
| 最高转速 | 逐步增大 STEP 频率 | ≥ 标称最大转速的 80% |
| 丢步检测 | 走 N 圈后比较实际位置 | 偏差 = 0（无丢步） |
| 低速振动 | 10rpm 观察轴抖动 | 细分 ≥ 1/8 时无明显振动 |
| 噪音 | 1米处分贝计测量 | TMC2209 < 40dB, A4988 < 55dB |
| 温升 | 满载运行 30min | 驱动 IC < 70°C, 电机 < 60°C |
| 定位精度 | 走 360° 回到起点 | 误差 < 1 步 |

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 电机不转只振动 | 某相接线断开或接反 | 检查4根线是否全部接通，确认A/A'和B/B'相序 |
| 电机丢步 | 加速度过大/负载过重/电压不足 | 降低加速度；提高 VMOT；检查 VREF 限流 |
| 驱动 IC 发热严重 | 电流设置过大/散热不足 | 降低 VREF；加散热片；检查电机额定电流 |
| 电机啸叫 | PWM 频率在可闻范围 | 使用细分模式；换 TMC2209 StealthChop |
| 高速扭矩骤降 | 步进电机高速时扭矩随速度平方下降 | 提高电压；使用升扭矩驱动(TMC5160) |
| A4988 烧毁 | VREF 过大/电源反接/无续流电容 | 仔细设置 VREF；VMOT 加大电解电容 |
| 启动直接丢步 | 启动速度过高 | 使用加减速曲线，启动速度 < 牵入频率 |
| 28BYJ-48 转方向反 | 线序接错 | 交换任意两根相邻相线 |
| DRV8825 细分不对 | M0/M1/M2 悬空 | 细分引脚必须接 MCU 或拉固定电平 |
| TMC2209 无 UART 响应 | 波特率不匹配/接线错误 | 默认 115200，单线 UART 需串联电阻 |
| 断电后电机无保持力 | ENABLE 拉高(禁用) | 需保持力时 ENABLE 保持低电平 |

### 设计注意事项

1. **加减速必做**：直接高速启动必然丢步，必须实现梯形或 S 型加减速曲线
2. **限流必调**：A4988/DRV8825 上电前必须调 VREF，默认值可能过大
3. **续流电容必加**：VMOT 旁大电解电容不可省略，否则反电动势可能烧毁驱动 IC
4. **散热必考虑**：>1A 持续运行必须加散热片，过热会自动降流导致丢步
5. **细分选择**：1/8 或 1/16 细分是精度与性能的最佳折中，过高细分增加 CPU 负载
6. **信号完整性**：STEP/DIR 线长 > 30cm 时需考虑信号反射，加 100Ω 端接电阻

## 相关文档

- `skill://mcu-driver-core/templates/driver-template-spi.c` — SPI 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-uart.c` — UART 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-adc.c` — ADC 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-pwm.c` — PWM 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
- `skill://mcu-driver-core/guides/pitfalls.md` — 跨类别常见问题与避坑指南
