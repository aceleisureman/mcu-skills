# 电池充电管理开发规范

## 1. 概述与选型指南

### 器件简介

电池充电管理 IC 用于安全地为可充电电池（锂离子/锂聚合物）充电。充电过程通常分为三个阶段：预充电（涓流）→ 恒流充电（CC）→ 恒压充电（CV），并在充电完成后自动终止。充电管理 IC 负责监控电池电压/温度，防止过充、过热等安全隐患。

### 锂电池充电曲线

```
电压/电流
  │     恒流区(CC)     恒压区(CV)
  │     ──────────     ───────────
  │     │  充电电流     ╲
  │     │               ╲
  │     │                ╲
  │  ───┤                 ╲── 终止电流
  │  预充│                  ╲──
  │  (涓流)                   ──
  │     │   电池电压 ──────── 到达4.2V后切换恒压
  │  ───┘
  └────────────────────────────────── 时间
       预充电    恒流充电    恒压充电    充电完成

预充电: 电池电压 < 2.9V, 以 0.1C 涓流充电 (保护过放电池)
恒流充电: 电池电压 2.9~4.2V, 以设定电流充电 (主要充电阶段)
恒压充电: 电压恒定 4.2V, 电流逐渐减小 (补足充电)
充电终止: 电流降至 C/10 时停止充电
```

### 常见型号对比

| 型号 | 输入 | 充电电流 | 电池类型 | 特殊功能 | 封装 | 价格 |
|------|------|----------|----------|----------|------|------|
| TP4056 | 5V USB | 1A (可调) | 单节锂电 | 充电状态指示 | SOP-8 | ¥0.5~1.5 |
| MCP73831 | 5V USB | 500mA (可调) | 单节锂电 | 小封装, 低功耗 | SOT-23-5 | ¥1.5~3.0 |
| BQ24075 | 5V USB | 1.5A (可调) | 单节锂电 | 路径管理, 安全定时 | QFN-16 | ¥5~10 |
| CN3791 | 6~28V | 4A (可调) | 单节锂电 | MPPT 太阳能充电 | SSOP-10 | ¥3~6 |
| TP4057 | 5V USB | 500mA | 单节锂电 | 低成本 | SOP-8 | ¥0.3~0.8 |
| LTC4054 | 5V USB | 800mA | 单节锂电 | 兼容TP4056 | DFN-8 | ¥2~5 |
| MAX1555 | 5V USB | 280mA | 单节锂电 | 超小封装 | TDFN-6 | ¥3~6 |

### 选型决策树

```
USB 充电 (5V输入)？
  → 是 →
    充电电流 ≤ 500mA？ → MCP73831 (小封装, 精密)
    充电电流 ≤ 1A？    → TP4056 (最常用, 廉价, 带状态指示)
    需要路径管理？     → BQ24075 (1.5A, 充电同时供电)
    需要超小封装？     → MAX1555 (TDFN-6)

太阳能充电？ → CN3791 (MPPT, 6~28V输入, 4A)
需要大电流充电？ → CN3791 (4A) / BQ24075 (1.5A)
需要温度监测？ → BQ24075 (TS引脚) / TP4056 (TEMP引脚)

通用推荐:
  USB充电 → TP4056 (1A, 廉价, 带指示灯, 最常用)
  小体积  → MCP73831 (500mA, SOT-23-5)
  太阳能  → CN3791 (MPPT, 大功率)
  路径管理 → BQ24075 (充电+供电同时进行)
```

### 适用场景

- **TP4056**：USB 充电宝、移动设备、DIY 项目（最常用）
- **MCP73831**：小型可穿戴设备、低功耗 IoT 设备
- **BQ24075**：需要边充电边使用的设备（手机/平板类）
- **CN3791**：太阳能路灯、户外设备、太阳能充电器

## 2. 硬件设计规范

### TP4056 电路

```
USB 5V 输入
  │
  ├── 4.7μF (陶瓷) ── GND    (输入电容)
  │
  │     ┌──────────────────────────┐
  │     │        TP4056            │
  │     │       (SOP-8)            │
  │     │                          │
  └─────┤ VCC(4)                   │
        │                          │
        │ CHRG(7) ── LED红 ── VCC  │   (充电中指示, 低有效)
        │ STDBY(6) ── LED绿 ── VCC │   (充满指示, 低有效)
        │                          │
        │ PROG(2) ── R_PROG ── GND │   (充电电流设置电阻)
        │                          │
        │ TEMP(1) ── GND           │   (温度监测, 不用接GND)
        │ CE(8) ── VCC             │   (芯片使能, 高有效)
        │                          │
        │ BAT(5) ──────────┬───────┤   (电池正极)
        │                  │       │
        │             10μF (陶瓷)  │   (BAT滤波电容)
        │                  │       │
        GND(3) ────────────┴───────┘
                            │
                       锂电池 (+)
                       锂电池 (-) ── GND

  充电电流计算: I_bat = 1200 / R_PROG (mA)
  R_PROG = 1.2kΩ → I_bat = 1A
  R_PROG = 2.4kΩ → I_bat = 500mA
  R_PROG = 10kΩ  → I_bat = 120mA
```

### MCP73831 电路 (小封装)

```
USB 5V 输入
  │
  ├── 4.7μF (陶瓷) ── GND    (输入电容)
  │
  │     ┌──────────────────────┐
  │     │    MCP73831          │
  │     │   (SOT-23-5)         │
  │     │                      │
  └─────┤ VDD(4)               │
        │ STAT(1) ── LED ── VDD│   (充电状态, 开漏输出)
        │                      │
        │ PROG(5) ── R_PROG ──GND  (电流设置)
        │                      │
        │ VBAT(3) ────┬────────┤   (电池正极)
        │             │        │
        │        4.7μF(陶瓷)   │   (BAT电容)
        │             │        │
  GND ──┤ VSS(2) ─────┴────────┘
                       │
                  锂电池 (+)
                  锂电池 (-) ── GND

  充电电流: I = 1000 / R_PROG (mA)
  R_PROG = 2kΩ → I = 500mA
  R_PROG = 10kΩ → I = 100mA
```

### BQ24075 电路 (路径管理)

```
USB 5V 输入
  │
  ├── 1μF (陶瓷) ── GND
  │
  │     ┌──────────────────────────┐
  │     │      BQ24075             │
  │     │     (QFN-16)             │
  │     │                          │
  └─────┤ IN(1,2)                  │
        │ OUT(13,14) ── 系统负载   │   (路径管理: 优先给系统供电)
        │ BAT(10,11) ──┬───────────┤   (电池)
        │              │           │
        │         10μF (陶瓷)      │
        │ ISET(8) ── R_ISET ── GND │   (充电电流设置)
        │ ITERM(7) ── R_ITERM─GND  │   (终止电流设置)
        │ TS(9) ── NTC(10kΩ) ──GND │   (温度监测)
        │ CHG(15) ── LED ── IN     │   (充电状态指示)
        │ EN1(3), EN2(4) ── MCU    │   (充电模式控制)
  GND ──┤ PGND(5,6), SGND(12)     │
        └──────────────────────────┘

  路径管理说明:
  - 有USB输入时: USB → OUT (直接给系统), 同时 USB → BAT (充电)
  - 无USB输入时: BAT → OUT (电池给系统供电)
  - 优势: 充电时系统不受电池电压波动影响
```

### CN3791 太阳能充电电路

```
太阳能板 (6~28V)
  │
  ├── 10μF (陶瓷) ── GND
  │
  │     ┌──────────────────────────┐
  │     │      CN3791              │
  │     │     (SSOP-10)            │
  │     │                          │
  └─────┤ VCC(3)                   │
        │ MPPT(2) ── 分压电阻 ── VCC  (MPPT电压设置)
        │ ICHG(4) ── R_ISET ── GND   (充电电流设置)
        │ SW(7) ──┬── L (47μH) ──┬─ BAT
        │         │              │   (降压式充电, 内置开关管)
        │         └── SS34 ──GND │
        │ BAT(8) ────────────────┤   (电池电压检测)
        │ CHRG(6) ── LED ── VCC  │   (充电指示)
        │ STDBY(5) ── LED ── VCC │   (充满指示)
  GND ──┤ GND(1,9,10)            │
        └──────────────────────────┘

  MPPT 设置: R_mppt1/R_mppt2 分压 = 太阳能板最大功率点电压
  充电电流: I = 1200 / R_ISET (mA)
```

### 充电电流设置电阻计算

```c
/*
 * 充电电流设置公式 (以 TP4056 为例):
 *
 * I_bat = 1200 / R_PROG (mA, R_PROG 单位: kΩ)
 *
 * | R_PROG (kΩ) | 充电电流 (mA) | 充电时间 (1000mAh电池) |
 * |-------------|---------------|----------------------|
 * |    1.2      |    1000       |   ~1.5h              |
 * |    2.4      |     500       |   ~2.5h              |
 * |    4.0      |     300       |   ~4h                |
 * |    10.0     |     120       |   ~10h               |
 *
 * 充电电流选择建议:
 * - 电池容量 C, 推荐充电电流 0.5C~1C
 * - 1000mAh 电池: 500mA~1000mA
 * - USB 2.0 最大 500mA, USB 3.0 最大 900mA
 * - 太阳能板功率有限, 通常 < 0.5C
 *
 * 锂电池充电终止电压: 4.20V ± 1% (精度要求高!)
 * 过充 > 4.25V 有安全风险
 */
```

### USB 输入保护

```
USB 输入保护电路:

  VBUS ── PTC(自恢复保险丝, 1.1A) ──┬── 充电IC VCC
                                     │
                                     ├── TVS (5V钳位)
                                     │
                                     └── ESD保护 (USBLC6)

  保护功能:
  - PTC: 过流保护 (USB短路或过载)
  - TVS: 瞬态电压抑制 (ESD/浪涌)
  - ESD: 静电放电保护
```

### PCB 设计要点

- 充电 IC 底部铺铜散热（TP4056 1A 充电时功耗约 0.8W）
- BAT 引脚走线加宽，承载充电电流
- 输入/输出电容尽量靠近 IC 引脚
- PROG 引脚走线短，远离开关噪声
- NTC 温度传感器靠近电池安装
- USB 输入端加 TVS 二极管（ESD 保护）
- 电池连接器加反接保护

### 电气参数限制

| 参数 | TP4056 | MCP73831 | BQ24075 | CN3791 |
|------|--------|----------|---------|--------|
| 输入电压 | 4.0~8.0V | 3.75~6.0V | 4.3~6.4V | 6.0~28V |
| 充电电压 | 4.2V ±1% | 4.2V ±0.75% | 4.2V ±0.5% | 4.2V ±1% |
| 最大充电电流 | 1A | 500mA | 1.5A | 4A |
| 预充电阈值 | 2.9V | 3.0V | 2.5V | 2.0V |
| 终止电流 | C/10 | C/10 | 可调 | C/10 |
| 涓流电流 | 1/10 Iset | 1/10 Iset | 0.1Iset | 可调 |
| 工作温度 | -40~+85°C | -40~+85°C | -40~+125°C | -40~+85°C |

## 3. 驱动开发规范

### TP4056 充电状态读取代码

```c
/*
 * TP4056 充电状态指示引脚:
 *   CHRG (Pin7): 充电中=低电平, 非充电=高电平
 *   STDBY (Pin6): 充满=低电平, 未充满=高电平
 *
 * 状态组合:
 *   CHRG=L, STDBY=H → 正在充电
 *   CHRG=H, STDBY=L → 充电完成
 *   CHRG=H, STDBY=H → 待机 (无电池或输入电压不足)
 *   CHRG=L, STDBY=L → 异常 (不应出现)
 */

typedef struct {
    gpio_pin_t chrg_pin;    /* CHRG 状态引脚 (输入) */
    gpio_pin_t stdby_pin;   /* STDBY 状态引脚 (输入) */
} charger_handle_t;

typedef enum {
    CHARGER_IDLE = 0,       /* 待机/无电池 */
    CHARGER_CHARGING,       /* 充电中 */
    CHARGER_FULL,           /* 充满 */
    CHARGER_FAULT,          /* 异常 */
} charger_status_t;

/* 读取充电状态 */
charger_status_t charger_read_status(charger_handle_t *h) {
    bool chrg = gpio_read(h->chrg_pin);    /* 0=充电中 */
    bool stdby = gpio_read(h->stdby_pin);  /* 0=充满 */

    if (!chrg && stdby)  return CHARGER_CHARGING;
    if (chrg && !stdby)  return CHARGER_FULL;
    if (chrg && stdby)   return CHARGER_IDLE;
    return CHARGER_FAULT;  /* !chrg && !stdby: 异常 */
}

const char *charger_status_str(charger_status_t s) {
    switch (s) {
        case CHARGER_IDLE:     return "Idle (No battery)";
        case CHARGER_CHARGING: return "Charging";
        case CHARGER_FULL:     return "Charge Complete";
        case CHARGER_FAULT:    return "Fault";
        default:               return "Unknown";
    }
}

/* 充电监控任务 (周期调用) */
void charger_monitor_task(charger_handle_t *h) {
    static charger_status_t last_status = CHARGER_IDLE;
    static uint32_t charge_start_time = 0;

    charger_status_t status = charger_read_status(h);

    if (status != last_status) {
        printf("Charger: %s -> %s\n",
               charger_status_str(last_status),
               charger_status_str(status));

        if (status == CHARGER_CHARGING) {
            charge_start_time = get_tick_ms();
            printf("Charging started\n");
        } else if (status == CHARGER_FULL) {
            uint32_t duration = get_tick_ms() - charge_start_time;
            printf("Charge complete, duration: %lu min %lu sec\n",
                   duration / 60000, (duration / 1000) % 60);
        }
        last_status = status;
    }
}
```

### 充电电流计算与电池监测

```c
typedef struct {
    float battery_capacity_mah;  /* 电池容量 (mAh) */
    float charge_current_ma;     /* 设定充电电流 (mA) */
    charger_handle_t charger;
    adc_channel_t vbat_ch;       /* 电池电压 ADC 通道 */
} battery_manager_t;

/* 计算预估充电时间 */
float battery_estimate_charge_time(battery_manager_t *bm, float current_soc) {
    /* SOC: State of Charge (0~100%) */
    float remaining_mah = bm->battery_capacity_mah * (1.0f - current_soc / 100.0f);
    /* 考虑充电效率 ~85% (CC+CV阶段) */
    float time_hours = remaining_mah / bm->charge_current_ma / 0.85f;
    return time_hours;
}

/* 电池电压监测 (通过分压 ADC) */
float battery_read_voltage(battery_manager_t *bm) {
    uint16_t adc_val = adc_read(bm->vbat_ch);
    /* 假设分压比 1:2 (R1=R2=100kΩ), Vref=3.3V, 12bit ADC */
    float v_adc = adc_val * 3.3f / 4095.0f;
    float v_bat = v_adc * 2.0f;  /* 分压比还原 */
    return v_bat;
}

/* 估算电池 SOC (基于电压, 粗略) */
float battery_voltage_to_soc(float voltage) {
    /* 锂电池电压-SOC 对应关系 (近似值):
     * 4.20V → 100%, 3.90V → ~70%, 3.70V → ~40%
     * 3.50V → ~15%, 3.30V → ~5%, 3.00V → 0%
     */
    if (voltage >= 4.20f) return 100.0f;
    if (voltage <= 3.00f) return 0.0f;
    /* 线性插值 (简化, 实际曲线非线性) */
    return (voltage - 3.00f) / (4.20f - 3.00f) * 100.0f;
}

/* 完整电池管理 */
void battery_monitor_task(battery_manager_t *bm) {
    charger_status_t chg = charger_read_status(&bm->charger);
    float vbat = battery_read_voltage(bm);
    float soc = battery_voltage_to_soc(vbat);

    printf("Battery: %.2fV, SOC: %.0f%%, Charger: %s\n",
           vbat, soc, charger_status_str(chg));

    /* 安全检查 */
    if (vbat > 4.25f) {
        printf("WARNING: Battery over-voltage (%.2fV > 4.25V)!\n", vbat);
    }
    if (vbat < 3.00f) {
        printf("WARNING: Battery under-voltage (%.2fV < 3.0V)!\n", vbat);
    }
}
```

## 4. 调试与测试规范

### 硬件验证清单

- [ ] 万用表确认 USB 输入电压 4.75~5.25V
- [ ] PROG 引脚电阻值正确（对应充电电流）
- [ ] 输入/输出电容已正确安装
- [ ] 电池正负极连接正确（注意反接保护）
- [ ] CHRG/STDBY LED 指示灯极性正确
- [ ] 散热焊盘已焊接
- [ ] NTC 温度传感器已连接（BQ24075/CN3791）

### 充电电压验证

```c
/* 充电电压精度验证 (关键安全指标!) */
void charger_voltage_test(battery_manager_t *bm) {
    printf("=== Charger Voltage Test ===\n");
    printf("Target: 4.20V ± 0.02V (4.18~4.22V)\n");

    while (charger_read_status(&bm->charger) == CHARGER_CHARGING) {
        float v = battery_read_voltage(bm);
        printf("BAT: %.3fV\n", v);
        if (v > 4.18f && v < 4.22f) {
            printf("OK: Charge voltage within spec\n");
            break;
        }
        if (v > 4.25f) {
            printf("DANGER: Over-voltage! Check charger IC!\n");
            break;
        }
        delay_ms(5000);
    }
}
```

### 充电电流与温度测试

```c
/* 充电电流测试 */
void charger_current_test(battery_manager_t *bm) {
    printf("Expected: %.0fmA (R_PROG=%.1fkΩ)\n",
           bm->charge_current_ma, 1200.0f / bm->charge_current_ma);
    printf("Measure current with multimeter in series with battery\n");
    /* 电池电压 < 2.9V 时应涓流充电 (1/10 Iset) */
    /* 电池电压 2.9~4.2V 时应恒流充电 (Iset) */
    /* 电池电压 ~4.2V 时应进入恒压, 电流逐渐减小 */
}

/* 温度保护测试 (BQ24075 等) */
void charger_temp_test(void) {
    printf("Temperature Protection:\n");
    printf("  Normal: 0~45°C → Full current charging\n");
    printf("  Cool:   0~10°C → Reduced current (0.1C)\n");
    printf("  Warm:   45~60°C → Reduced current\n");
    printf("  Hot:    > 60°C  → Charge suspended\n");
    printf("  Cold:   < 0°C   → Charge suspended\n");
}
```

### 性能测试指标

| 指标 | TP4056 | MCP73831 | BQ24075 | CN3791 |
|------|--------|----------|---------|--------|
| 充电电压精度 | ±1% | ±0.75% | ±0.5% | ±1% |
| 1000mAh 充满时间 | ~1.5h (1A) | ~2.5h (500mA) | ~1h (1.5A) | ~0.5h (4A) |
| 待机电流 | 6μA | 3μA | 1.5μA | 1mA |
| 热调节 | 有 (120°C) | 无 | 有 | 有 |
| 路径管理 | 无 | 无 | 有 | 无 |
| MPPT | 无 | 无 | 无 | 有 |

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 充不进电 | 输入电压不足或电池过放 | 检查 USB 电压 > 4.5V; 过放电池需涓流恢复 |
| 充电电流偏小 | PROG 电阻值不对或输入功率不足 | 核对 R_PROG; USB 供电能力不足时降电流 |
| 充电发热严重 | 充电电流大 + 压差大 | P=(Vin-Vbat)×I, 降低充电电流或改善散热 |
| 充不满到 4.2V | 充电 IC 精度不足或电池老化 | 检查 BAT 引脚电压; 老化电池需更换 |
| 电池过充 | 充电 IC 故障 | 充电电压 > 4.25V 必须停用, 更换 IC |
| 充电时系统重启 | 无路径管理, 电池电压被拉低 | 用 BQ24075 路径管理型充电 IC |
| LED 指示不对 | LED 极性接反 | CHRG/STDBY 为开漏低有效, LED 阳极接 VCC |
| USB 热插拔烧 IC | 无 ESD 保护 | USB 输入加 TVS 和 PTC 保护 |
| 太阳能充电效率低 | MPPT 电压设置不当 | 调整 MPPT 分压电阻匹配太阳能板 Vmp |
| 低温充不进电 | 温度保护触发 | 正常行为, 0°C 以下不应充电 |
| TP4056 1A 发热大 | SOP-8 散热受限 | 降低充电电流至 500mA 或加散热铜箔 |
| 电池鼓包 | 过充或高温充电 | 检查充电电压, 确保温度保护正常 |

## 相关文档

- `skill://mcu-driver-core/templates/driver-template-adc.c` — ADC 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
- `skill://mcu-driver-core/guides/pitfalls.md` — 跨类别常见问题与避坑指南
