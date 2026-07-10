# 继电器驱动开发规范

## 1. 概述与选型指南

### 简介

继电器是一种电控开关，通过小电流控制大电流负载的通断。MCU 输出逻辑信号驱动继电器线圈，继电器触点切换高电压/大电流负载。继电器实现了控制电路与负载电路的电气隔离。

继电器分类：
- **电磁继电器（EMR）**：电磁铁驱动机械触点，有物理触点磨损
- **固态继电器（SSR）**：半导体开关（晶闸管/MOSFET），无机械运动
- **光耦隔离继电器模块**：EMR + 光耦隔离，MCU 端与继电器端完全隔离

### 常见类型对比

| 类型 | 触点容量 | 驱动电压 | 隔离方式 | 寿命 | 开关速度 | 适用场景 | 价格 |
|------|----------|----------|----------|------|----------|----------|------|
| 5V 电磁继电器模块 | 10A/250VAC | 5V | 光耦(模块) | 10万次 | ~10ms | 通用控制 | ¥3~8 |
| 12V 电磁继电器 | 10A/250VAC | 12V | 光耦(模块) | 10万次 | ~10ms | 工业控制 | ¥5~10 |
| 24V 电磁继电器 | 16A/250VAC | 24V | 光耦(模块) | 10万次 | ~10ms | 工业自动化 | ¥8~15 |
| 固态继电器(SSR-40DA) | 40A/380VAC | 3~32VDC | 光电隔离 | 100万次+ | <1ms | 加热控制 | ¥10~30 |
| 固态继电器(SSR-25DD) | 25A/60VDC | 3~32VDC | 光电隔离 | 100万次+ | <1ms | 直流负载 | ¥15~40 |
| 磁保持继电器 | 30A/250VAC | 脉冲 | 光耦 | 100万次 | ~5ms | 节能控制 | ¥5~15 |

### 选型决策树

```
负载为直流？ → 是 →
  电流 > 10A？ → 是 → SSR-25DD (固态, 直流)
              否 → 5V/12V 电磁继电器模块
负载为交流？ → 是 →
  需要频繁开关(>1次/分钟)？ → 是 → SSR-40DA (固态, 无磨损)
                             否 →
    需要过零触发？ → 是 → SSR (自带过零)
                   否 → 电磁继电器模块(廉价)
  负载为感性(电机/水泵)？ → 是 → 选触点容量 ≥ 负载电流3倍的EMR, 或SSR+RC吸收
  需要低功耗持续保持？ → 是 → 磁保持继电器(脉冲驱动)
```

### 适用场景

| 场景 | 推荐类型 | 理由 |
|------|----------|------|
| 智能家居灯控 | 5V继电器模块 | 廉价、隔离好、开关频率低 |
| 工业加热器 | SSR-40DA | 频繁通断、无触点磨损 |
| 水泵控制 | 12V继电器(触点≥20A) | 感性负载需大余量 |
| 电池低功耗系统 | 磁保持继电器 | 脉冲驱动、无需持续电流 |
| 电动机正反转 | 双继电器互锁 | 防止同时吸合短路 |

## 2. 硬件设计规范

### 三极管驱动电磁继电器电路

```
  VCC(5V/12V) ──┬── 继电器线圈 ──┬── 集电极
                │                  │
                │              NPN三极管(S8050)
                │                  │
                │              基极 ── 1kΩ ── MCU GPIO
                │                  │
                │              发射极
                │                  │
               GND ────────────────┘

  续流二极管(1N4007):
    阴极 ── VCC(线圈上方)
    阳极 ── 集电极(线圈下方)

  完整电路:

       VCC
        │
        ├──┤<── 1N4007 (续流二极管, 阴极接VCC)
        │  │
        │  └── 继电器线圈 ──┬── C(S8050)
        │                    │
        │                   B ── 1kΩ ── MCU GPIO
        │                    │
        │                   E
        │                    │
       GND ──────────────────┘

  继电器触点:
    COM ──── 负载电源
    NO  ──── 负载(常开: GPIO=1 时导通)
    NC  ──── 负载(常闭: GPIO=0 时导通)
```

**电路要点**：
- 续流二极管必须反并联在线圈两端（阴极接 VCC，阳极接三极管集电极）
- 三极管选型：Ic ≥ 继电器线圈电流的 2 倍（S8050: 500mA, 2N2222: 800mA）
- 基极限流电阻：R = (V_GPIO - 0.7V) / I_B，I_B = I_C / hFE × 2（深度饱和）
- 5V继电器线圈电流70mA，hFE=100，I_B = 70/100×2 = 1.4mA，R = (3.3-0.7)/1.4mA ≈ 1.8kΩ，取 1kΩ

### 光耦隔离继电器模块电路

```
  MCU GPIO ── 1kΩ ── 光耦LED+(PC817 1脚)
  MCU GND ──────────── 光耦LED-(PC817 2脚)

  光耦输出(PC817 3脚) ── GND(继电器侧)
  光耦输出(PC817 4脚) ── 1kΩ ── 三极管基极

       VCC(继电器侧)
        │
        ├──┤<── 1N4007
        │  │
        │  └── 线圈 ──┬── C(S8050)
        │              │
        │             B ── 1kΩ ── 光耦4脚
        │              │
        │             E
        │              │
  GND(继电器侧) ───────┘  ← 光耦3脚

  MCU 侧与继电器侧电源完全隔离!
  MCU GND ≠ 继电器 GND
```

**光耦隔离要点**：
- MCU 侧与继电器侧使用独立电源，仅通过光信号耦合
- 光耦电流传输比（CTR）≥ 50%，PC817 典型值 80~120%
- 光耦 LED 侧限流电阻：R = (V_GPIO - 1.2V) / I_F，I_F 推荐 5~10mA
- 光耦隔离耐压 ≥ 2500Vrms（PC817）

### 固态继电器（SSR）电路

```
  MCU GPIO ── 1kΩ ── SSR+ (输入正)
  MCU GND ────────── SSR- (输入负)

  SSR 交流输出端:
    SSR 输出1 ──── 220VAC 火线
    SSR 输出2 ──── 负载
    负载另一端 ──── 220VAC 零线

  SSR-40DA 内部结构:
    输入+ ── LED ── 输入-
              │
           光耦合
              │
    过零检测 ── 触发电路
              │
    输出1 ── 双向晶闸管 ── 输出2

  过零触发: 在交流电过零点时导通, 减小浪涌电流和EMI
```

**SSR 电路要点**：
- 输入侧为 LED，只需 5~10mA 驱动电流
- 交流 SSR 只能控制交流负载，直流 SSR 只能控制直流负载
- SSR 导通有压降（交流 ~1.6V，直流 ~0.6V），大电流需配散热片
- 交流 SSR 内置过零触发电路，减少 EMI
- 输出端需加 RC 吸收回路或压敏电阻（MOV）保护

### 磁保持继电器电路

```
       VCC
        │
        ├──┤<── 1N4007 (续流二极管1)
        │  │
        │  └── 置位线圈 ──┬── C(Q1 NPN)
        │                  │
        │                 B ── 1kΩ ── MCU GPIO_SET
        │                  │
        │                 E ── GND
        │
        ├──┤<── 1N4007 (续流二极管2)
        │  │
        │  └── 复位线圈 ──┬── C(Q2 NPN)
        │                  │
        │                 B ── 1kΩ ── MCU GPIO_RESET
        │                  │
        │                 E ── GND

  SET 脉冲(50~100ms) → 触点闭合(保持)
  RESET 脉冲(50~100ms) → 触点断开(保持)
  无需持续电流 → 超低功耗
```

### 触点灭弧电路

```
  感性负载(电机/电磁阀)触点保护:

  1. RC 吸收回路(并联在触点两端):
    COM ── R(100Ω/2W) ── C(0.1µF/630V) ── NO

  2. 压敏电阻(MOV)并联在触点两端:
    MOV 电压 > 负载额定电压 × 1.5

  3. 直流负载加续流二极管(并联在负载两端):
    负载+ ──┤<── 负载- (二极管阴极接正端)
```

### 触点容量降额

| 负载类型 | 推荐降额系数 | 说明 |
|----------|-------------|------|
| 电阻负载 | 75% | 标称10A → 实际用7.5A |
| 感性负载 | 40% | 电机/电磁阀, 电弧严重 |
| 灯负载 | 20% | 冷启动浪涌电流10~15倍 |
| 电容负载 | 20% | 充电浪涌电流大 |

### PCB 设计要点

- 继电器触点走线宽度 ≥ 3mm（10A），大电流用铜排
- 触点回路与 MCU 控制回路物理隔离，间距 ≥ 6mm
- 高压侧与低压侧之间开槽（隔离线），防止爬电
- 续流二极管紧贴线圈引脚（< 5mm），走线短
- 光耦跨隔离区放置，一次侧和二次侧不共地

## 3. 驱动开发规范

### 统一接口定义

```c
typedef enum {
    RELAY_OK = 0,
    RELAY_ERR_INIT,
    RELAY_ERR_INVALID_PARAM,
} relay_status_t;

typedef enum {
    RELAY_TYPE_NORMAL,       /* 普通继电器(电平控制) */
    RELAY_TYPE_LATCHING,    /* 磁保持继电器(脉冲控制) */
} relay_type_t;

typedef struct {
    uint8_t control_pin;     /* 控制引脚(普通继电器) */
    uint8_t set_pin;         /* 置位引脚(磁保持) */
    uint8_t reset_pin;       /* 复位引脚(磁保持) */
    relay_type_t type;
    bool active_high;        /* true=高电平触发 */
    uint16_t pulse_ms;       /* 磁保持脉冲宽度(ms) */
} relay_config_t;

typedef struct {
    relay_config_t cfg;
    bool initialized;
    bool current_state;
} relay_handle_t;

relay_status_t relay_init(relay_handle_t *h, const relay_config_t *cfg);
relay_status_t relay_on(relay_handle_t *h);
relay_status_t relay_off(relay_handle_t *h);
relay_status_t relay_toggle(relay_handle_t *h);
bool relay_get_state(relay_handle_t *h);
```

### GPIO 驱动继电器代码

```c
/* relay.c - 继电器驱动(三极管/光耦驱动) */

static void gpio_set(uint8_t pin, bool level) { /* TODO */ }
static void gpio_config_output(uint8_t pin) { /* TODO */ }
static void delay_ms(uint32_t ms) { /* TODO */ }

relay_status_t relay_init(relay_handle_t *h, const relay_config_t *cfg) {
    if (!h || !cfg) return RELAY_ERR_INVALID_PARAM;
    h->cfg = *cfg;
    h->current_state = false;

    if (cfg->type == RELAY_TYPE_NORMAL) {
        if (cfg->control_pin == 0xFF) return RELAY_ERR_INVALID_PARAM;
        gpio_config_output(cfg->control_pin);
        gpio_set(cfg->control_pin, !cfg->active_high);
    } else {
        if (cfg->set_pin == 0xFF || cfg->reset_pin == 0xFF)
            return RELAY_ERR_INVALID_PARAM;
        gpio_config_output(cfg->set_pin);
        gpio_config_output(cfg->reset_pin);
        gpio_set(cfg->set_pin, 0);
        gpio_set(cfg->reset_pin, 0);
    }
    h->initialized = true;
    return RELAY_OK;
}

relay_status_t relay_on(relay_handle_t *h) {
    if (!h || !h->initialized) return RELAY_ERR_INIT;

    if (h->cfg.type == RELAY_TYPE_NORMAL) {
        gpio_set(h->cfg.control_pin, h->cfg.active_high);
        h->current_state = true;
    } else {
        if (h->current_state) return RELAY_OK;
        gpio_set(h->cfg.set_pin, h->cfg.active_high);
        delay_ms(h->cfg.pulse_ms > 0 ? h->cfg.pulse_ms : 50);
        gpio_set(h->cfg.set_pin, !h->cfg.active_high);
        h->current_state = true;
    }
    return RELAY_OK;
}

relay_status_t relay_off(relay_handle_t *h) {
    if (!h || !h->initialized) return RELAY_ERR_INIT;

    if (h->cfg.type == RELAY_TYPE_NORMAL) {
        gpio_set(h->cfg.control_pin, !h->cfg.active_high);
        h->current_state = false;
    } else {
        if (!h->current_state) return RELAY_OK;
        gpio_set(h->cfg.reset_pin, h->cfg.active_high);
        delay_ms(h->cfg.pulse_ms > 0 ? h->cfg.pulse_ms : 50);
        gpio_set(h->cfg.reset_pin, !h->cfg.active_high);
        h->current_state = false;
    }
    return RELAY_OK;
}

relay_status_t relay_toggle(relay_handle_t *h) {
    if (!h || !h->initialized) return RELAY_ERR_INIT;
    return h->current_state ? relay_off(h) : relay_on(h);
}

bool relay_get_state(relay_handle_t *h) {
    if (!h || !h->initialized) return false;
    return h->current_state;
}
```

### 多继电器互锁控制（电机正反转）

```c
/* relay_interlock.c - 双继电器互锁(电机正反转) */

/*
  互锁: 两个继电器不能同时吸合(否则短路!)
  切换方向时必须先断开当前方向, 延时后再吸合新方向
*/

typedef struct {
    relay_handle_t relay_fwd;
    relay_handle_t relay_rev;
    uint16_t switch_delay_ms;    /* 切换延时 */
} motor_relay_t;

typedef enum {
    MOTOR_STOP,
    MOTOR_FORWARD,
    MOTOR_REVERSE,
} motor_dir_t;

relay_status_t motor_relay_set_dir(motor_relay_t *m, motor_dir_t dir) {
    /* 先全部断开 */
    relay_off(&m->relay_fwd);
    relay_off(&m->relay_rev);

    /* 等待触点完全断开(机械延迟) */
    delay_ms(m->switch_delay_ms > 0 ? m->switch_delay_ms : 200);

    /* 再吸合目标方向 */
    switch (dir) {
        case MOTOR_FORWARD:
            return relay_on(&m->relay_fwd);
        case MOTOR_REVERSE:
            return relay_on(&m->relay_rev);
        default:
            return RELAY_OK;
    }
}
```

## 4. 调试与测试规范

### 硬件验证清单

- [ ] 万用表确认继电器线圈电压正确（5V/12V/24V）
- [ ] 测量线圈电阻，确认与数据手册一致（5V继电器约 70Ω）
- [ ] 确认续流二极管方向正确（阴极接 VCC）
- [ ] 三极管型号正确（NPN：S8050/2N2222）
- [ ] 光耦模块：确认 MCU 侧与继电器侧电源完全隔离
- [ ] 触点侧接线正确（COM/NO/NC 不可接错）
- [ ] 高压侧绝缘间距足够（≥ 6mm）

### 功能验证

- **吸合测试**：GPIO 输出高电平，听继电器"咔嗒"声
- **释放测试**：GPIO 输出低电平，听继电器释放声
- **触点测试**：万用表通断档测量 COM-NO（吸合时通）和 COM-NC（释放时通）
- **隔离测试**（光耦模块）：万用表测 MCU GND 与继电器 GND 之间电阻（应无穷大）

### 示波器验证

```
  续流二极管效果验证:

  无续流二极管(GPIO 下降沿断电时):
  VCE ────┐  ↑↑↑ 数十伏尖峰
          └── 0V

  有续流二极管:
  VCE ────┐  ↓ 微小过冲
          └── 0V

  测量点: 三极管集电极(C) 对 GND
```

### 性能指标

| 指标 | 测试方法 | 合格标准 |
|------|----------|----------|
| 吸合时间 | GPIO上升沿到触点导通 | < 15ms |
| 释放时间 | GPIO下降沿到触点断开 | < 10ms |
| 触点接触电阻 | 万用表毫欧档测 COM-NO | < 50mΩ |
| 线圈电流 | 串联万用表测 | 与标称值 ±10% |
| 绝缘电阻 | 500V兆欧表测触点-线圈 | > 100MΩ |
| 噪音 | 1米处分贝计 | < 50dB |

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 三极管击穿 | 未接续流二极管 | 必须接1N4007反并联在线圈两端 |
| 继电器不吸合 | 驱动电流不足 | 选Ic更大的三极管；减小基极电阻 |
| 继电器吸合后MCU复位 | 线圈电流冲击电源 | 独立电源供电；加大电源滤波电容 |
| 触点粘连 | 负载电流过大/电弧 | 选更大容量继电器；加RC吸收回路 |
| 触点烧黑 | 感性负载电弧 | 触点并联RC吸收；负载并联续流二极管 |
| 继电器发热 | 线圈长时间通电 | 正常现象(< 60°C)；用磁保持替代 |
| 光耦隔离无效 | 两侧共地 | MCU侧和继电器侧GND必须完全独立 |
| SSR 发热严重 | 电流大/散热不足 | 加散热片；选更大电流规格 |
| SSR 控制电机不转 | SSR类型不匹配 | 交流负载用AC SSR, 直流用DC SSR |
| 磁保持继电器状态不确定 | 上电初始状态未知 | 上电时先发一次RESET脉冲归位 |

### 设计注意事项

1. **续流二极管必接**：继电器线圈断电时反电动势可达数百伏，必接续流二极管保护三极管
2. **光耦隔离要彻底**：光耦模块的 MCU 侧与继电器侧不能共地，否则隔离无意义
3. **触点降额使用**：感性负载降至 40%，灯负载降至 20%，延长触点寿命
4. **互锁保护**：正反转控制必须软件+硬件双重互锁，防止同时吸合短路
5. **上电默认状态**：MCU 上电时 GPIO 默认输出低，确保继电器处于断开状态
6. **安全间距**：220V 市电走线间距 ≥ 6mm，PCB 开隔离槽

## 相关文档

- `../../templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `../../guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `../../guides/debugging-testing.md` — 调试与测试规范
- `../../guides/pitfalls.md` — 跨类别常见问题与避坑指南
