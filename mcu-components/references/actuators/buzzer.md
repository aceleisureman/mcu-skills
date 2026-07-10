# 蜂鸣器驱动开发规范

## 1. 概述与选型指南

### 简介

蜂鸣器是一种将电信号转换为声音的输出器件，用于提示音、报警音、旋律播放等。按驱动方式分为两类：
- **有源蜂鸣器**：内置振荡电路，只需 GPIO 通断即可发声，固定频率
- **无源蜂鸣器**：无振荡电路，需外部 PWM 驱动，频率可变，可播放旋律

> **"源"指振荡源**：有源 = 内部有振荡源；无源 = 需外部提供驱动信号

### 常见类型对比

| 类型 | 驱动方式 | 频率 | 电压 | 电流 | 音质 | 可变频率 | 价格 |
|------|----------|------|------|------|------|----------|------|
| 有源蜂鸣器(5V) | GPIO通断 | 固定~2.7kHz | 4~8V | ~30mA | 单音 | 否 | ¥0.5~2 |
| 有源蜂鸣器(3.3V) | GPIO通断 | 固定~2.3kHz | 2~5V | ~20mA | 单音 | 否 | ¥0.5~2 |
| 无源蜂鸣器(5V) | PWM | 1~5kHz可变 | 3~8V | ~35mA | 多音/旋律 | 是 | ¥0.5~2 |
| 压电式无源 | PWM | 1~10kHz | 3~20V | ~10mA | 清脆 | 是 | ¥1~3 |
| 电磁式无源 | PWM | 1~3kHz | 3~5V | ~50mA | 浑厚 | 是 | ¥0.5~2 |
| 蜂鸣片(裸片) | PWM | 4kHz谐振 | 3~20V | ~5mA | 较小 | 是 | ¥0.2~1 |

### 有源 vs 无源对比

| 特性 | 有源蜂鸣器 | 无源蜂鸣器 |
|------|-----------|-----------|
| 驱动方式 | GPIO 通断 | PWM 信号 |
| 频率 | 固定(出厂设定) | 可变(1~10kHz) |
| 音质 | 单一频率 | 可播放旋律 |
| 编程复杂度 | 极简(拉高/拉低) | 中等(需PWM+音符表) |
| 成本 | 略高(含振荡IC) | 略低 |
| 适用 | 简单报警/提示 | 旋律/多音调提示 |

### 选型决策树

```
只需简单"滴"声报警？ → 是 → 有源蜂鸣器(GPIO通断, 最简单)
需播放旋律/多音调？ → 是 → 无源蜂鸣器(PWM驱动)
需要大音量？ → 是 → 选大尺寸(Φ35以上) + 共鸣腔
需要防水？ → 是 → 选塑封型蜂鸣器
超低功耗？ → 是 → 压电式无源(电流最小)
空间受限？ → 是 → 选小尺寸(Φ10以下)
```

### 适用场景

| 场景 | 推荐类型 | 理由 |
|------|----------|------|
| 按键提示音 | 有源蜂鸣器 | 简单可靠 |
| 报警蜂鸣 | 有源蜂鸣器 | 固定频率醒目 |
| 门铃/音乐 | 无源蜂鸣器 | 可播放旋律 |
| 倒计时提示 | 无源蜂鸣器 | 不同频率表示不同状态 |
| 万用表蜂鸣 | 有源蜂鸣器 | 导通提示 |
| 电子琴玩具 | 无源蜂鸣器 | 多音阶演奏 |

## 2. 硬件设计规范

### 有源蜂鸣器电路

```
  方案1: 三极管驱动(推荐)

       VCC(5V)
        │
        ├── 蜂鸣器+
        │
        蜂鸣器- ──┬── C(S8050 NPN)
                  │
                 B ── 1kΩ ── MCU GPIO
                  │
                 E
                  │
                 GND

  方案2: 直接GPIO驱动(仅超小型蜂鸣器, 电流<20mA)

  MCU GPIO ── 100Ω ── 蜂鸣器+ ── GND

  ⚠ 警告: 大部分蜂鸣器电流 30mA, 超过 MCU 引脚上限(20mA)!
           必须使用三极管驱动!

  方案3: 低电平触发模块(常见蜂鸣器模块)

       VCC(5V) ── 蜂鸣器模块 VCC
       GND    ── 蜂鸣器模块 GND
  MCU GPIO ──── 蜂鸣器模块 I/O (低电平有效)

  模块内部已有三极管驱动和限流电阻
```

### 无源蜂鸣器电路

```
  方案1: 三极管 + PWM 驱动(推荐)

       VCC(5V)
        │
        ├── 蜂鸣器+
        │
        蜂鸣器- ──┬── C(S8050 NPN)
                  │
                 B ── 1kΩ ── MCU PWM
                  │
                 E
                  │
                 GND

  方案2: 直接 PWM 引脚驱动(小电流蜂鸣片)

  MCU PWM ── 100Ω ── 蜂鸣片 ── GND

  限流电阻计算:
    R = (V_GPIO - V_buzzer) / I_buzzer
    例: 3.3V, 蜂鸣器 3V/30mA → R = (3.3-3)/0.03 = 10Ω → 取 100Ω
```

### 三极管驱动设计

```
  三极管选型:
    蜂鸣器电流 < 100mA → S8050(NPN, Ic=500mA) 足够
    蜂鸣器电流 < 500mA → 2N2222(NPN, Ic=800mA)

  基极限流电阻:
    I_B = I_C / hFE × 2 (深度饱和)
    R_B = (V_GPIO - 0.7V) / I_B

    例: 蜂鸣器 30mA, S8050 hFE=100
    I_B = 30/100 × 2 = 0.6mA
    R_B = (3.3 - 0.7) / 0.6mA ≈ 4.3kΩ → 取 1kΩ

  MOS 管驱动(更简单, 无需基极电流):
    MCU GPIO ── 100Ω ── G(AO3400 N-MOS)
                        D ── 蜂鸣器-
                        S ── GND
    G-S 间加 10kΩ 下拉
```

### 限流电阻

```
  有源蜂鸣器:
    工作电流 ~30mA, 内部已有限流, 通常无需外加限流电阻

  无源蜂鸣器:
    阻抗通常 42Ω(电磁式) 或 > 1kΩ(压电式)
    电磁式: I = VCC / Z = 5V / 42Ω ≈ 120mA (峰值!)
    需串联限流电阻控制电流
    目标 30mA: R = (5/0.03) - 42 ≈ 125Ω → 取 150Ω
```

### 蜂鸣器共鸣腔设计

```
  ┌───────────────┐
  │  蜂鸣器本体    │
  │  ┌─────────┐  │
  │  │ 压电片   │  │  ← 振动产生声音
  │  └─────────┘  │
  └───────┬───────┘
          │
     ┌────┴────┐
     │ 共鸣腔   │  ← 外壳空间形成共鸣
     └─────────┘
  
  PCB 安装建议:
  - 蜂鸣器周围留 2~5mm 间隙形成音腔
  - 外壳开音孔, 孔面积 ≥ 蜂鸣器面积的 30%
```

### 硬件设计要点

- **三极管驱动**：蜂鸣器电流 30mA+，超过 MCU 引脚上限，必须用三极管/MOS 驱动
- **限流电阻**：无源蜂鸣器需串联限流电阻控制峰值电流
- **PWM 频率**：无源蜂鸣器频率 1~5kHz，人耳最敏感范围 2~4kHz
- **去耦电容**：蜂鸣器旁加 100nF 去耦电容，减少开关噪声
- **防反接**：有源蜂鸣器有极性（+/-），反接不发声

### PCB 设计要点

- 蜂鸣器引脚焊盘加粗，防止大电流发热
- 蜂鸣器周围避免高速信号走线，防止串扰
- 蜂鸣器底面 PCB 避免大面积铺铜，保留共鸣空间
- 外壳设计音孔，孔径 ≥ 2mm

## 3. 驱动开发规范

### 统一接口定义

```c
typedef enum {
    BUZZER_OK = 0,
    BUZZER_ERR_INIT,
    BUZZER_ERR_INVALID_PARAM,
} buzzer_status_t;

typedef enum {
    BUZZER_TYPE_ACTIVE,   /* 有源蜂鸣器(GPIO通断) */
    BUZZER_TYPE_PASSIVE,  /* 无源蜂鸣器(PWM驱动) */
} buzzer_type_t;

typedef struct {
    uint8_t pin;             /* GPIO 或 PWM 通道 */
    buzzer_type_t type;
    bool active_high;        /* true=高电平触发 */
} buzzer_config_t;

typedef struct {
    buzzer_config_t cfg;
    bool initialized;
    bool sounding;
} buzzer_handle_t;

buzzer_status_t buzzer_init(buzzer_handle_t *h, const buzzer_config_t *cfg);
buzzer_status_t buzzer_on(buzzer_handle_t *h);
buzzer_status_t buzzer_off(buzzer_handle_t *h);
buzzer_status_t buzzer_beep(buzzer_handle_t *h, uint16_t ms);
buzzer_status_t buzzer_set_freq(buzzer_handle_t *h, uint16_t freq_hz);
buzzer_status_t buzzer_play_note(buzzer_handle_t *h, uint8_t note, uint16_t ms);
buzzer_status_t buzzer_play_melody(buzzer_handle_t *h, const uint8_t *notes,
                                    const uint16_t *durations, uint16_t count);
```

### 有源蜂鸣器 GPIO 驱动

```c
/* buzzer_active.c - 有源蜂鸣器驱动(GPIO通断) */

buzzer_status_t buzzer_init(buzzer_handle_t *h, const buzzer_config_t *cfg) {
    if (!h || !cfg) return BUZZER_ERR_INVALID_PARAM;
    if (cfg->type != BUZZER_TYPE_ACTIVE)
        return BUZZER_ERR_INVALID_PARAM;

    h->cfg = *cfg;
    h->sounding = false;

    gpio_config_output(cfg->pin);
    gpio_set(cfg->pin, !cfg->active_high);  /* 初始静音 */

    h->initialized = true;
    return BUZZER_OK;
}

buzzer_status_t buzzer_on(buzzer_handle_t *h) {
    if (!h || !h->initialized) return BUZZER_ERR_INIT;
    gpio_set(h->cfg.pin, h->cfg.active_high);
    h->sounding = true;
    return BUZZER_OK;
}

buzzer_status_t buzzer_off(buzzer_handle_t *h) {
    if (!h || !h->initialized) return BUZZER_ERR_INIT;
    gpio_set(h->cfg.pin, !h->cfg.active_high);
    h->sounding = false;
    return BUZZER_OK;
}

/* 蜂鸣指定时长(阻塞) */
buzzer_status_t buzzer_beep(buzzer_handle_t *h, uint16_t ms) {
    if (!h || !h->initialized) return BUZZER_ERR_INIT;
    buzzer_on(h);
    delay_ms(ms);
    buzzer_off(h);
    return BUZZER_OK;
}
```

### 无源蜂鸣器 PWM 驱动（播放音符/旋律）

```c
/* buzzer_passive.c - 无源蜂鸣器驱动(PWM, 可播放旋律) */

/* 音符频率表 (Hz)
 *  音名    频率(Hz)
 *  C4(Do)  262    D4(Re)  294    E4(Mi)  330
 *  F4(Fa)  349    G4(Sol) 392    A4(La)  440
 *  B4(Si)  494    C5(Do)  523
 */

#define NOTE_REST  0    /* 休止符 */
#define NOTE_C4   262
#define NOTE_D4   294
#define NOTE_E4   330
#define NOTE_F4   349
#define NOTE_G4   392
#define NOTE_A4   440
#define NOTE_B4   494
#define NOTE_C5   523
#define NOTE_D5   587
#define NOTE_E5   659
#define NOTE_F5   698
#define NOTE_G5   784
#define NOTE_A5   880

/* 平台适配:
 * STM32 PWM 频率设置: 固定PSC=83(1MHz), 调ARR
 *   ARR = 1000000 / freq - 1
 *   C4(262Hz): ARR = 3815
 *   A4(440Hz): ARR = 2272
 */
static void pwm_set_freq(uint8_t ch, uint16_t freq_hz) { /* TODO */ }
static void pwm_set_duty_50(uint8_t ch) { /* TODO: 50%占空比 */ }
static void pwm_start_ch(uint8_t ch) { /* TODO */ }
static void pwm_stop_ch(uint8_t ch) { /* TODO */ }

buzzer_status_t buzzer_init(buzzer_handle_t *h, const buzzer_config_t *cfg) {
    if (!h || !cfg) return BUZZER_ERR_INVALID_PARAM;
    if (cfg->type != BUZZER_TYPE_PASSIVE)
        return BUZZER_ERR_INVALID_PARAM;

    h->cfg = *cfg;
    h->sounding = false;

    pwm_set_freq(cfg->pin, 2000);  /* 默认 2kHz */
    pwm_stop_ch(cfg->pin);

    h->initialized = true;
    return BUZZER_OK;
}

buzzer_status_t buzzer_on(buzzer_handle_t *h) {
    if (!h || !h->initialized) return BUZZER_ERR_INIT;
    pwm_set_duty_50(h->cfg.pin);  /* 50% 占空比 */
    pwm_start_ch(h->cfg.pin);
    h->sounding = true;
    return BUZZER_OK;
}

buzzer_status_t buzzer_off(buzzer_handle_t *h) {
    if (!h || !h->initialized) return BUZZER_ERR_INIT;
    pwm_stop_ch(h->cfg.pin);
    h->sounding = false;
    return BUZZER_OK;
}

buzzer_status_t buzzer_set_freq(buzzer_handle_t *h, uint16_t freq_hz) {
    if (!h || !h->initialized) return BUZZER_ERR_INIT;
    if (freq_hz < 20 || freq_hz > 20000) return BUZZER_ERR_INVALID_PARAM;
    pwm_set_freq(h->cfg.pin, freq_hz);
    return BUZZER_OK;
}

/* 播放单个音符 */
buzzer_status_t buzzer_play_note(buzzer_handle_t *h, uint8_t note, uint16_t ms) {
    if (!h || !h->initialized) return BUZZER_ERR_INIT;

    if (note == NOTE_REST) {
        buzzer_off(h);
        delay_ms(ms);
        return BUZZER_OK;
    }

    buzzer_set_freq(h, note);
    buzzer_on(h);
    delay_ms(ms);
    buzzer_off(h);

    /* 音符间短促停顿, 区分连续音符 */
    delay_ms(ms / 8);
    return BUZZER_OK;
}

/* 播放旋律 */
buzzer_status_t buzzer_play_melody(buzzer_handle_t *h, const uint8_t *notes,
                                    const uint16_t *durations, uint16_t count) {
    if (!h || !h->initialized) return BUZZER_ERR_INIT;
    if (!notes || !durations || count == 0) return BUZZER_ERR_INVALID_PARAM;

    for (uint16_t i = 0; i < count; i++) {
        buzzer_play_note(h, notes[i], durations[i]);
    }
    return BUZZER_OK;
}

buzzer_status_t buzzer_beep(buzzer_handle_t *h, uint16_t ms) {
    if (!h || !h->initialized) return BUZZER_ERR_INIT;
    buzzer_on(h);
    delay_ms(ms);
    buzzer_off(h);
    return BUZZER_OK;
}
```

### 旋律播放示例

```c
/* 示例: 播放《小星星》 */
static const uint8_t twinkle_notes[] = {
    NOTE_C4, NOTE_C4, NOTE_G4, NOTE_G4,
    NOTE_A4, NOTE_A4, NOTE_G4, NOTE_REST,
    NOTE_F4, NOTE_F4, NOTE_E4, NOTE_E4,
    NOTE_D4, NOTE_D4, NOTE_C4, NOTE_REST,
};
static const uint16_t twinkle_durations[] = {
    400, 400, 400, 400, 400, 400, 800, 200,
    400, 400, 400, 400, 400, 400, 800, 200,
};

void play_twinkle(buzzer_handle_t *h) {
    buzzer_play_melody(h, twinkle_notes, twinkle_durations,
                       sizeof(twinkle_notes) / sizeof(twinkle_notes[0]));
}

/* 示例: 报警音(交替高低频率) */
void alarm_sound(buzzer_handle_t *h, uint8_t times) {
    for (uint8_t i = 0; i < times; i++) {
        buzzer_set_freq(h, 3000);
        buzzer_beep(h, 200);
        buzzer_set_freq(h, 1500);
        buzzer_beep(h, 200);
    }
}

/* 示例: 按键提示音 */
void key_beep(buzzer_handle_t *h) {
    buzzer_set_freq(h, 2700);
    buzzer_beep(h, 50);
}

/* 示例: 开机音乐 */
void startup_sound(buzzer_handle_t *h) {
    uint8_t notes[] = {NOTE_C4, NOTE_E4, NOTE_G4, NOTE_C5};
    uint16_t durs[] = {150, 150, 150, 300};
    buzzer_play_melody(h, notes, durs, 4);
}
```

### 非阻塞旋律播放（状态机方式）

```c
/* buzzer_nonblocking.c - 非阻塞旋律播放(状态机) */

typedef struct {
    const uint8_t *notes;
    const uint16_t *durations;
    uint16_t count;
    uint16_t current_index;
    uint32_t note_start_time;
    bool playing;
} melody_player_t;

static melody_player_t player;

void melody_start(const uint8_t *notes, const uint16_t *durations, uint16_t count) {
    player.notes = notes;
    player.durations = durations;
    player.count = count;
    player.current_index = 0;
    player.playing = true;

    if (count > 0 && notes[0] != NOTE_REST) {
        buzzer_set_freq(&buzzer, notes[0]);
        buzzer_on(&buzzer);
    }
    player.note_start_time = millis();
}

/* 在主循环中调用 */
void melody_update(void) {
    if (!player.playing) return;

    uint32_t elapsed = millis() - player.note_start_time;
    uint16_t current_dur = player.durations[player.current_index];

    if (elapsed >= current_dur) {
        buzzer_off(&buzzer);
        /* 音符间间隔 */
        if (elapsed >= current_dur + current_dur / 8) {
            player.current_index++;
            if (player.current_index >= player.count) {
                player.playing = false;
                return;
            }
            uint8_t next = player.notes[player.current_index];
            if (next != NOTE_REST) {
                buzzer_set_freq(&buzzer, next);
                buzzer_on(&buzzer);
            }
            player.note_start_time = millis();
        }
    }
}

bool melody_is_playing(void) { return player.playing; }
```

## 4. 调试与测试规范

### 硬件验证清单

- [ ] 确认蜂鸣器类型（有源/无源），有源蜂鸣器有极性
- [ ] 万用表测量蜂鸣器电阻（无源 42Ω~∞，有源通常 > 100Ω）
- [ ] 确认三极管型号正确（S8050 NPN）
- [ ] 限流电阻已贴装（无源蜂鸣器需串联限流）
- [ ] 有源蜂鸣器：确认正负极正确（长脚=正，短脚=负）
- [ ] 示波器检查 PWM 波形（无源蜂鸣器）：频率、50% 占空比

### 功能验证

- **有源蜂鸣器**：GPIO 拉高/拉低，听蜂鸣器是否响/停
- **无源蜂鸣器**：输出 2kHz 方波，听是否发声
- **频率扫描**：无源蜂鸣器从 1kHz 扫到 5kHz，听音调是否变化
- **旋律测试**：播放《小星星》，确认音符正确

### 示波器测量

```
  无源蜂鸣器 PWM 信号:

  频率 = 262Hz (C4):
  ┌──1.9ms──┐
  └──────────┘  ← 周期 3.82ms, 占空比 50%

  频率 = 440Hz (A4):
  ┌──1.1ms──┐
  └──────────┘  ← 周期 2.27ms, 占空比 50%

  检查项:
  ✓ 频率与目标值匹配 (±2%)
  ✓ 占空比 = 50% (方波驱动效果最好)
  ✓ 电平幅值 ≥ 3.0V
```

### 性能指标

| 指标 | 测试方法 | 合格标准 |
|------|----------|----------|
| 音压 | 10cm处分贝计测量 | ≥ 85dB (有源) |
| 频率精度 | 示波器测 PWM 频率 | ±2% |
| 频率范围 | 扫频测试 | 100Hz~10kHz 可发声 |
| 谐振频率 | 找最大音压点 | 无源蜂鸣器 2~4kHz |
| 功耗 | 万用表测电流 | 有源 ~30mA, 无源 ~35mA |

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 有源蜂鸣器不响 | 正负极接反/电压不足 | 确认极性(长脚+)；检查电压 |
| 有源蜂鸣器声音小 | 电压不足/驱动能力差 | 提高电压；用三极管驱动 |
| 无源蜂鸣器不响 | PWM 频率不在谐振范围 | 调整频率到 2~4kHz |
| 无源蜂鸣器声音小 | 占空比不是 50% | 设置 50% 方波驱动 |
| 音调不准 | PWM 频率误差 | 校准定时器分频系数 |
| 旋律播放卡顿 | 阻塞式 delay 影响主循环 | 使用非阻塞状态机播放 |
| 蜂鸣器有杂音 | PWM 频率与机械谐振冲突 | 微调频率避开谐振点 |
| MCU 引脚驱动蜂鸣器后MCU复位 | 电流超限 | 必须用三极管驱动 |
| 蜂鸣器一直响 | GPIO 初始状态为高 | 初始化时先拉低关闭 |
| 无源蜂鸣器播放旋律断续 | 音符间间隔太长 | 缩短间隔时间(ms/8) |

### 设计注意事项

1. **三极管驱动必做**：蜂鸣器电流 30mA 超过 MCU 引脚上限，必须用三极管或 MOS 驱动
2. **无源蜂鸣器用 50% 方波**：方波驱动音量最大，其他占空比会导致音量减小
3. **频率选谐振点**：无源蜂鸣器在谐振频率（约 2~4kHz）音量最大
4. **非阻塞播放**：旋律播放用状态机实现，避免阻塞主循环
5. **上电静音**：MCU 上电时 GPIO 默认输出可能触发蜂鸣器，初始化时先关闭
6. **有源蜂鸣器有极性**：正负极不可接反，否则不发声甚至损坏

## 相关文档

- `../../templates/driver-template-pwm.c` — PWM 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `../../guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `../../guides/debugging-testing.md` — 调试与测试规范
- `../../guides/pitfalls.md` — 跨类别常见问题与避坑指南
