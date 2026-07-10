# 电磁阀/电磁铁驱动开发规范

## 1. 概述与选型指南

### 简介

电磁阀（Solenoid Valve）和电磁铁（Electromagnet）都是通过电磁线圈产生磁力驱动机械运动的执行器。电磁阀控制流体（气体/液体）的通断，电磁铁产生推/拉机械力。

与继电器类似，都是电感负载，但有关键区别：
- **工作电流更大**：持续通电电流可达数百 mA ~ 数 A
- **需要保持力**：动作后需持续维持状态（阀门开启/铁芯吸合）
- **节能需求**：长时间保持需降低电流（保持电流 < 启动电流）
- **机械行程**：电磁铁有可见的机械运动行程

### 常见类型对比

| 类型 | 工作电压 | 启动电流 | 保持电流 | 行程/通径 | 响应时间 | 适用场景 | 价格 |
|------|----------|----------|----------|-----------|----------|----------|------|
| 推拉电磁铁(小型) | 5~12V | 0.3~1A | 0.2~0.5A | 5~20mm | 10~50ms | 锁具/门禁 | ¥5~20 |
| 推拉电磁铁(大型) | 12~24V | 1~3A | 0.5~1.5A | 10~30mm | 20~100ms | 工业自动化 | ¥20~80 |
| 微型电磁阀(水) | 5~12V | 0.3~0.7A | 0.2~0.5A | DN2~DN5 | 20~50ms | 饮水机/灌溉 | ¥5~15 |
| 常闭电磁阀(水) | 12~24V | 0.5~1.5A | 0.3~0.8A | DN8~DN15 | 30~100ms | 水控制 | ¥10~50 |
| 常开电磁阀(气) | 12~24V | 0.3~1A | 0.2~0.5A | DN5~DN10 | 20~50ms | 气体控制 | ¥10~40 |
| 脉冲电磁阀 | 12~24V | 1~3A(脉冲) | 0(无需保持) | DN15~DN50 | 10~30ms | 除尘/脉冲清洗 | ¥20~80 |

### 常开 vs 常闭电磁阀

```
  常闭型(NC - Normally Closed):
    断电 = 阀门关闭(流体截止)
    通电 = 阀门打开(流体通过)
    应用: 需要断电安全关闭的场景(如水阀)

  常开型(NO - Normally Open):
    断电 = 阀门打开(流体通过)
    通电 = 阀门关闭(流体截止)
    应用: 需要断电安全打开的场景(如排气阀)

  选择原则: 断电状态应是安全状态!
    水阀 → 常闭(断电停水)
    排气阀 → 常开(断电排气)
```

### 选型决策树

```
控制流体？ → 是(电磁阀) →
  流体为水？ → 是 → 常闭型(断电关闭) → 通径选择 DN
  流体为气体？ → 是 → 按断电安全状态选常开/常闭
  需要长时间开启？ → 是 → 选脉冲型(无需保持电流)
  否 →
  控制机械运动？ → 是(电磁铁) →
    行程 ≤ 10mm？ → 是 → 小型推拉电磁铁(5~12V)
    行程 > 10mm？ → 是 → 大型推拉电磁铁(12~24V)
    需要大力量？ → 是 → 选 24V 大功率电磁铁
  需要长时间保持？ → 是 → 必须实现 PWM 保持电流(节能)
```

### 适用场景

| 场景 | 推荐类型 | 理由 |
|------|----------|------|
| 饮水机水阀 | 5V/12V 常闭电磁阀 | 小电流、断电关水 |
| 自动灌溉 | 12V 常闭电磁阀 | 低功耗、安全 |
| 门禁电控锁 | 12V 推拉电磁铁 | 行程大、力量够 |
| 自动门电磁锁 | 24V 电磁铁 | 大吸力、安全断电 |
| 气动控制 | 24V 常开电磁阀 | 快速响应 |
| 工业清洗 | 脉冲电磁阀 | 脉冲驱动、无需保持 |

## 2. 硬件设计规范

### 基础驱动电路（三极管驱动）

```
  VCC(12V/24V) ──┬── 电磁阀/电磁铁线圈 ──┬── 集电极
                 │                        │
                 │                    NPN三极管
                 │                   (TIP120 达林顿, Ic=5A)
                 │                        │
                 │                    基极 ── 1kΩ ── MCU GPIO
                 │                        │
                 │                    发射极
                 │                        │
                GND ──────────────────────┘

  续流二极管(1N4007 或 SS54):
    阴极 ── VCC
    阳极 ── 集电极

  电路:

       VCC(12V)
        │
        ├──┤<── 续流二极管 (1N4007, 阴极接VCC)
        │  │
        │  └── 线圈 ──┬── C(TIP120)
        │              │
        │             B ── 1kΩ ── MCU GPIO
        │              │
        │             E
        │              │
       GND ────────────┘
```

**三极管选型**：
| 线圈电流 | 推荐三极管 | 类型 | Ic | 备注 |
|----------|-----------|------|-----|------|
| < 0.5A | S8050 | NPN | 0.5A | 小型电磁铁 |
| < 1A | 2N2222 | NPN | 0.8A | 中型电磁铁 |
| < 3A | TIP120 | NPN达林顿 | 5A | 大型电磁阀 |
| < 5A | TIP122 | NPN达林顿 | 5A | 大功率 |
| 任意 | AO3400(MOS) | N-MOS | 5A | 效率高、压降低 |

> **推荐使用 MOS 管**：压降低（< 0.1V）、发热小、驱动简单

### MOS 管驱动电路（推荐）

```
       VCC(12V/24V)
        │
        ├──┤<── 续流二极管 (SS54 肖特基)
        │  │
        │  └── 线圈 ──┬── D(AO3400 / IRLZ44N)
        │              │
        │             G ── 100Ω ── MCU GPIO
        │              │
        │             S
        │              │
       GND ────────────┘

  G-S 间加 10kΩ 下拉电阻(确保上电时 MOS 管关断)
  G 串联 100Ω 电阻(抑制栅极振荡)
```

### PWM 保持电流电路（节能）

```
  启动阶段: 全功率导通(100%占空比), 持续 100~500ms
  保持阶段: PWM 降功率(20~40%占空比), 持续维持

       VCC
        │
        ├──┤<── 续流二极管
        │  │
        │  └── 线圈 ──┬── D(MOS管)
        │              │
        │             G ── 100Ω ──┬── MCU PWM 引脚
        │              │           │
        │             S       10kΩ下拉
        │              │           │
       GND ────────────┘───────────┘

  PWM 参数:
    频率: 1~20kHz (超出可闻范围)
    启动占空比: 100% (全压启动)
    保持占空比: 25~40% (维持吸合)
    启动持续时间: 100~500ms (根据机械响应调整)

  节能效果:
    12V 电磁铁, 线圈电阻 24Ω
    全功率: I = 12/24 = 0.5A, P = 6W
    30%保持: I_eff ≈ 0.15A, P ≈ 1.8W (节能70%)
```

### 续流二极管选型

```
  电磁阀线圈断电时反向电动势:

    V_spike = -L × (di/dt)

  可达 100~500V (取决于线圈电感和关断速度)

  续流二极管选型:
    反向耐压 Vr ≥ VCC × 3 (12V系统 → 50V+)
    正向电流 If ≥ 线圈电流
    速度: 快恢复或肖特基

  推荐型号:
    1N4007: 1000V/1A, 通用(便宜但慢)
    SS34:   40V/3A, 肖特基(低压系统推荐)
    SS54:   40V/5A, 肖特基(大电流)
    FR107:  1000V/1A, 快恢复(高压系统)

  续流二极管会延长释放时间:
    无二极管: 释放快(反电动势大, 但危险)
    有二极管: 释放慢(续流延长电流衰减)
    加稳压管: 释放快且安全(二极管+稳压管串联)
```

### 限流电路

```
  大功率电磁阀限流方案:

  方案1: 串联限流电阻
    VCC ── 线圈 ── R_limit ── MOS管
    R_limit = (VCC - V_coil) / I_rated
    缺点: 电阻发热, 效率低

  方案2: PWM 限流(推荐)
    启动后切换到 PWM 模式, 通过占空比控制平均电流
    优点: 无额外功耗, 精确控制

  方案3: 恒流源驱动
    使用专用 LED 驱动IC(如 PT4115)做恒流源
    优点: 电流精确, 适合多路驱动
```

### 硬件设计要点

- **续流二极管必接**：线圈断电瞬间反电动势可达数百伏，必接续流二极管
- **MOS 管栅极下拉**：10kΩ 下拉确保上电时 MOS 管关断，防止误动作
- **电源滤波**：线圈旁加大电解电容（470µF+），吸收启动电流冲击
- **散热设计**：大功率电磁阀的驱动 MOS 管需加散热片
- **机械安装**：电磁铁行程末端需加缓冲（橡胶垫），防止撞击损坏

### PCB 设计要点

- 功率走线（VCC→线圈→MOS→GND）宽度 ≥ 2mm（1A）或 ≥ 3mm（3A+）
- MOS 管 GND 走线尽量短而粗，减小寄生电感
- 续流二极管紧贴线圈引脚（< 10mm）
- 大电解电容紧贴线圈电源引脚
- 多路电磁阀需独立保险丝（1A/2A 自恢复保险丝）

## 3. 驱动开发规范

### 统一接口定义

```c
typedef enum {
    SOLENOID_OK = 0,
    SOLENOID_ERR_INIT,
    SOLENOID_ERR_INVALID_PARAM,
} solenoid_status_t;

typedef struct {
    uint8_t control_pin;       /* GPIO 或 PWM 通道 */
    bool use_pwm;              /* 是否使用 PWM 保持电流 */
    uint16_t startup_ms;       /* 全功率启动时间(ms) */
    uint8_t  hold_duty;        /* 保持占空比(0~100) */
    uint16_t pwm_freq_hz;      /* PWM 频率 */
} solenoid_config_t;

typedef struct {
    solenoid_config_t cfg;
    bool initialized;
    bool active;
    uint32_t activate_time;    /* 激活时间戳 */
} solenoid_handle_t;

solenoid_status_t solenoid_init(solenoid_handle_t *h, const solenoid_config_t *cfg);
solenoid_status_t solenoid_activate(solenoid_handle_t *h);
solenoid_status_t solenoid_deactivate(solenoid_handle_t *h);
void solenoid_update(solenoid_handle_t *h);  /* 周期调用, 管理启动→保持切换 */
```

### 驱动代码（含 PWM 保持电流）

```c
/* solenoid.c - 电磁阀/电磁铁驱动(含PWM保持电流) */

static void gpio_set(uint8_t pin, bool level) { /* TODO */ }
static void pwm_set_duty(uint8_t ch, uint8_t duty) { /* TODO */ }
static void pwm_start(uint8_t ch) { /* TODO */ }
static void pwm_stop(uint8_t ch) { /* TODO */ }
static uint32_t millis(void) { /* TODO */ return 0; }

solenoid_status_t solenoid_init(solenoid_handle_t *h, const solenoid_config_t *cfg) {
    if (!h || !cfg) return SOLENOID_ERR_INVALID_PARAM;

    h->cfg = *cfg;
    h->active = false;
    h->activate_time = 0;

    if (cfg->use_pwm) {
        /* PWM 模式: 初始占空比 0 */
        pwm_set_duty(cfg->control_pin, 0);
        pwm_start(cfg->control_pin);
    } else {
        /* GPIO 模式: 初始低电平 */
        gpio_set(cfg->control_pin, 0);
    }

    h->initialized = true;
    return SOLENOID_OK;
}

solenoid_status_t solenoid_activate(solenoid_handle_t *h) {
    if (!h || !h->initialized) return SOLENOID_ERR_INIT;

    if (h->cfg.use_pwm) {
        /* 启动阶段: 100% 占空比(全功率) */
        pwm_set_duty(h->cfg.control_pin, 100);
    } else {
        gpio_set(h->cfg.control_pin, 1);
    }

    h->active = true;
    h->activate_time = millis();
    return SOLENOID_OK;
}

solenoid_status_t solenoid_deactivate(solenoid_handle_t *h) {
    if (!h || !h->initialized) return SOLENOID_ERR_INIT;

    if (h->cfg.use_pwm) {
        pwm_set_duty(h->cfg.control_pin, 0);
    } else {
        gpio_set(h->cfg.control_pin, 0);
    }

    h->active = false;
    return SOLENOID_OK;
}

/*
 * 周期调用: 管理启动→保持切换
 * 放在主循环中, 每 10~50ms 调用一次
 *
 * 时序:
 *   activate() → 100% 全功率 → startup_ms 后 → 切换到 hold_duty% 保持
 *   deactivate() → 0% 关闭
 */
void solenoid_update(solenoid_handle_t *h) {
    if (!h || !h->initialized || !h->active) return;
    if (!h->cfg.use_pwm) return;  /* GPIO 模式无需切换 */

    uint32_t elapsed = millis() - h->activate_time;

    if (elapsed >= h->cfg.startup_ms) {
        /* 启动时间结束, 切换到保持电流 */
        pwm_set_duty(h->cfg.control_pin, h->cfg.hold_duty);
    }
    /* 启动阶段: 保持 100% 占空比, 无需操作 */
}
```

### 使用示例

```c
/* 示例: 电磁水阀控制(12V, 启动500ms后PWM保持) */

solenoid_handle_t water_valve;

void valve_init(void) {
    solenoid_config_t cfg = {
        .control_pin  = 0x03,     /* PA3 PWM 通道 */
        .use_pwm      = true,     /* 使用 PWM 保持 */
        .startup_ms   = 300,      /* 全功率启动 300ms */
        .hold_duty    = 30,       /* 30% 占空比保持 */
        .pwm_freq_hz  = 10000,    /* 10kHz PWM */
    };
    solenoid_init(&water_valve, &cfg);
}

void valve_open(void) {
    solenoid_activate(&water_valve);
    /* 主循环中调用 solenoid_update() 自动切换启动→保持 */
}

void valve_close(void) {
    solenoid_deactivate(&water_valve);
}

/* 主循环 */
void main_loop(void) {
    solenoid_update(&water_valve);  /* 每 10~50ms 调用 */
}
```

### 纯 GPIO 驱动示例（无 PWM 保持）

```c
/* 示例: 简单电磁铁(无节能需求, 直接GPIO控制) */

solenoid_handle_t lock_solenoid;

void lock_init(void) {
    solenoid_config_t cfg = {
        .control_pin  = 0x05,     /* PB5 */
        .use_pwm      = false,    /* 纯 GPIO */
        .startup_ms   = 0,
        .hold_duty    = 100,
        .pwm_freq_hz  = 0,
    };
    solenoid_init(&lock_solenoid, &cfg);
}

void lock_open(void) {
    solenoid_activate(&lock_solenoid);
    /* 5秒后自动关闭 */
    timer_start(5000, lock_close);
}

void lock_close(void) {
    solenoid_deactivate(&lock_solenoid);
}
```

## 4. 调试与测试规范

### 硬件验证清单

- [ ] 万用表确认线圈电阻与数据手册一致
- [ ] 确认线圈电压规格（5V/12V/24V）
- [ ] 续流二极管方向正确（阴极接 VCC）
- [ ] MOS 管栅极 10kΩ 下拉电阻已贴装
- [ ] 大电解电容已贴装在线圈电源旁
- [ ] 示波器检查 PWM 波形（频率、占空比）

### 功能验证

- **启动测试**：全功率通电，确认电磁铁吸合/电磁阀开启
- **释放测试**：断电后确认电磁铁释放/电磁阀关闭
- **行程测试**：测量电磁铁实际行程是否符合规格
- **保持测试**：PWM 保持模式下确认机械状态稳定（无释放）
- **流体测试**（电磁阀）：通水/通气验证密封性，无泄漏

### PWM 保持电流验证

```
  示波器测量线圈电流:

  启动阶段(100%):         保持阶段(30% PWM):
  ┌────────────────┐      ┌┐ ┌┐ ┌┐ ┌┐ ┌┐
  │                │      └┘ └┘ └┘ └┘ └┘ └...
  └────────────────┘
  ←── startup_ms ──→     ← 保持阶段 →

  电流:
  ↑
  │  I_start (满电流)
  │ ┌────────────────┐
  │ │                │ ┌┐┌┐┌┐┌┐ I_hold_avg
  │ │                │ └┘└┘└┘└┘
  └──────────────────────────────────→ 时间

  验证项:
  ✓ 启动电流足够吸合
  ✓ 保持电流足够维持
  ✓ PWM 频率在可闻范围外(> 20kHz 或 < 100Hz)
  ✓ 无机械振动/嗡嗡声
```

### 性能指标

| 指标 | 测试方法 | 合格标准 |
|------|----------|----------|
| 启动电流 | 全功率通电瞬间测电流 | 与标称值 ±15% |
| 保持电流 | PWM 保持模式下测平均电流 | ≤ 启动电流 × 50% |
| 响应时间 | 通电到机械动作完成 | < 标称响应时间 |
| 释放时间 | 断电到机械复位 | < 50ms |
| 温升 | 持续工作30min测线圈温度 | < 65°C |
| 寿命 | 连续开关 10000 次 | 功能正常 |

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| MOS 管击穿 | 未接续流二极管 | 必接续流二极管（SS34/1N4007） |
| 电磁铁吸合后嗡嗡响 | PWM 频率在可闻范围 | 提高到 > 20kHz |
| 电磁铁吸不住 | 保持电流太小 | 增大保持占空比；延长启动时间 |
| 电磁阀漏水/漏气 | 密封件损坏/压力过高 | 检查密封圈；确认工作压力范围 |
| 线圈过热 | 长时间全功率通电 | 实现 PWM 保持电流降功耗 |
| 释放慢 | 续流二极管延长电流衰减 | 串联稳压管加速释放 |
| 启动时MCU复位 | 电流冲击导致电压跌落 | 加大电源滤波；独立电源供电 |
| 电磁铁力量不足 | 电压不足/行程过大 | 提高电压；减小实际行程；选大功率型号 |
| 长时间通电烧毁 | 无过流保护/超时检测 | 加软件超时断电；加温度传感器 |
| 多路同时启动电压跌落 | 电源功率不足 | 错峰启动；加大电源容量 |

### 设计注意事项

1. **PWM 保持电流必做**：长时间通电必须降功率，否则线圈过热烧毁
2. **启动时间要够**：机械运动需要时间，启动阶段必须全功率保证完全吸合
3. **续流二极管选肖特基**：肖特基二极管正向压降低（0.3V），续流效果优于普通整流管
4. **断电安全**：常闭电磁阀断电关闭，常开电磁阀断电打开，按安全需求选型
5. **机械缓冲**：电磁铁行程末端加橡胶垫缓冲，防止撞击损坏和噪音
6. **过热保护**：长时间保持的电磁铁需加温度检测，超温自动断电

## 相关文档

- `../../templates/driver-template-pwm.c` — PWM 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `../../guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `../../guides/debugging-testing.md` — 调试与测试规范
- `../../guides/pitfalls.md` — 跨类别常见问题与避坑指南
