# 电源保护开发规范

## 1. 概述与选型指南

### 保护类型总览

| 保护类型 | 器件 | 作用 | 关键参数 | 价格 |
|----------|------|------|----------|------|
| TVS 二极管 | SMAJ12CA, PESD3V3L5UY | 瞬态电压抑制 | Vbr, Vc, Ppp | ¥0.3~2 |
| 自恢复保险丝 PTC | MF-MSMF050 | 过流保护 | Ih, It, Vmax | ¥0.5~3 |
| 压敏电阻 MOV | 10D431K | 浪涌保护 | Vvar, 能量耐量 | ¥0.5~3 |
| 肖特基二极管 | SS34, SS54 | 反接保护 | Vf, Vr, If | ¥0.2~1 |
| MOSFET 反接保护 | AO3401, SI4435 | 反接保护(低压降) | Vds, Rds(on), Vgs(th) | ¥1~5 |
| 气体放电管 GDT | GDT05 | 雷击浪涌 | 直流击穿电压 | ¥2~10 |
| ESD 保护阵列 | USBLC6-2 | 多线 ESD 保护 | 通道数, Vrm | ¥1~5 |

### 选型建议
- **USB 接口保护** → PTC + TVS + ESD 阵列
- **电源反接保护** → MOSFET（低压降，优于二极管）
- **工业浪涌保护** → GDT + MOV + TVS 三级保护
- **IO 口 ESD** → ESD 保护阵列（如 USBLC6）
- **过流保护** → PTC（自恢复，免维护）

## 2. 硬件设计规范

### 反接保护电路

#### 方案一：肖特基二极管（简单，但有压降）

```
   Vin ──┬──|>|──┬── Vout
         │  SS34  │
         │        │
        GND      GND

   优点: 简单、低成本
   缺点: 有 0.3~0.5V 压降, 大电流时功耗大
   适用: 小电流 (<1A) 场景
```

#### 方案二：P-MOSFET（低压降，推荐）

```
                 ┌─── Vout
                 │
            ┌────┴────┐
   Vin ─────┤ D     S ├──── Vin (输入)
            │  P-MOS  │
            │    G    │
            └────┬────┘
                 │
                10kΩ
                 │
                 ├──── D (接到输入端 Vin)
                 │
                GND

   正接时: MOS 导通, 压降 ≈ I × Rds(on) (极小)
   反接时: MOS 截止, 保护后级电路
   
   P-MOS 选型: Vds > 1.5×Vin, Rds(on) 尽量小
   常见: AO3401 (30V/4A, SOT-23), SI4435 (30V/8A, SOP-8)
```

### TVS 保护电路

```
   Vin ──┬── 保险丝/PTC ──┬── Vout
         │                 │
         │               ┌─┴─┐
         │               │TVS│  (双向, 如 SMAJ12CA)
         │               │   │
         │               └─┬─┘
         │                 │
        GND ───────────── GND

   工作原理:
   - 正常电压: TVS 截止, 不影响电路
   - 浪涌电压: TVS 钳位到安全电压, 吸收浪涌能量
   - 配合 PTC: TVS 导通时电流增大, PTC 断开保护

   TVS 选型:
   - Vrwm (工作电压) ≥ 最高正常工作电压
   - Vc (钳位电压) < 被保护器件耐压
   - Ppp (峰值功率) ≥ 预期浪涌能量

   常见选型:
   - 5V 系统: SMAJ5.0CA (Vrwm=5V, Vc=9.2V)
   - 3.3V 系统: SMAJ3.3CA (Vrwm=3.3V, Vc=6.0V)
   - 12V 系统: SMAJ12CA (Vrwm=12V, Vc=19.9V)
   - 24V 系统: SMAJ24CA (Vrwm=24V, Vc=38.9V)
```

### USB 接口完整保护

```
   USB VBUS ──┬── PTC (500mA) ──┬── 系统电源
              │                   │
              │                 ┌─┴─┐
              │                 │TVS│ SMAJ5.0CA
              │                 └─┬─┘
              │                   │
             GND ─────────────── GND

   USB D+ ───┬── 22Ω ────┬── MCU D+
             │            │
           ┌─┴─┐        ┌─┴─┐
           │ESD│        │ESD│  USBLC6-2SC6
           └─┬─┘        └─┬─┘
             │            │
   USB D- ───┼── 22Ω ────┼── MCU D-
             │            │
            GND ─────────┘

   保护策略:
   1. PTC: 过流保护 (500mA 自恢复保险丝)
   2. TVS: 电源浪涌保护
   3. ESD阵列: USB 数据线 ESD 保护
   4. 22Ω 电阻: 限流 + 阻抗匹配
```

### 浪涌保护（三级防护）

```
   线路输入
     │
   ┌─┴─┐
   │GDT │  第一级: 气体放电管 (雷击/大能量浪涌)
   └─┬─┘     放电电压 ~100V, 能量耐量大
     │
   ┌─┴─┐
   │MOV │  第二级: 压敏电阻 (中等能量浪涌)
   └─┬─┘     钳位电压 ~50V
     │
   ┌─┴─┐
   │TVS │  第三级: TVS 二极管 (精确钳位)
   └─┬─┘     钳位电压 ~12V
     │
   后级电路

   级间需加退耦电感或电阻 (10Ω~100Ω)
   确保: GDT 击穿电压 > MOV > TVS
```

### PTC 选型

```
PTC 选型参数:
- Ih (保持电流): 正常工作最大电流, 选 ≥ 工作电流 × 1.2
- It (触发电流): 触发保护电流, 通常 = 2×Ih
- Vmax: 最大工作电压
- Imax: 最大短路电流
- 动作时间: 过流到断开的时间

示例: USB 供电保护
- 工作电流: 500mA
- 选 MF-MSMF050: Ih=500mA, Vmax=15V, Imax=40A
- 或 miniSMDC050F: Ih=500mA, Vmax=15V, 贴片小型化
```

## 3. 驱动开发规范

电源保护主要为硬件实现，但固件中可增加软件保护层：

```c
/**
 * @file power_protection.c
 * @brief 软件电源保护 (配合硬件保护使用)
 */

/* 过压检测与保护 */
void power_overvoltage_protection(float voltage) {
    if (voltage > OVERVOLTAGE_THRESHOLD) {  /* 如 5.5V (5V系统) */
        /* 1. 立即断开负载 (通过控制使能引脚) */
        gpio_write_low(load_enable_port, load_enable_pin);

        /* 2. 记录事件 */
        log_error("Overvoltage: %.2fV > %.2fV", voltage, OVERVOLTAGE_THRESHOLD);

        /* 3. 报警 */
        alarm_trigger(ALARM_OVERVOLTAGE);

        /* 4. 进入安全模式 */
        enter_safe_mode();
    }
}

/* 过流检测与保护 (配合 INA219) */
void power_overcurrent_protection(float current) {
    static uint32_t overcurrent_start = 0;

    if (current > OVERCURRENT_THRESHOLD) {  /* 如 2.0A */
        if (overcurrent_start == 0) {
            overcurrent_start = get_tick_ms();
        } else if (get_tick_ms() - overcurrent_start > 100) {
            /* 持续过流 100ms → 断开 */
            gpio_write_low(load_enable_port, load_enable_pin);
            log_error("Overcurrent: %.2fA > %.2fA (100ms)",
                      current, OVERCURRENT_THRESHOLD);
        }
    } else {
        overcurrent_start = 0;  /* 恢复正常 */
    }
}

/* 过温保护 (配合 NTC) */
void power_overtemp_protection(float temp) {
    if (temp > OVERTEMP_THRESHOLD) {  /* 如 70°C */
        /* 降低系统功耗: 降频、关外设 */
        cpu_set_freq(48);  /* 降频 */
        disable_non_critical_peripherals();

        if (temp > OVERTEMP_CRITICAL) {  /* 如 85°C */
            /* 关机保护 */
            log_error("Critical temp: %.1f°C", temp);
            save_critical_data();
            system_shutdown();
        }
    }
}

/* 电池低压保护 */
void battery_lowvoltage_protection(float voltage) {
    static uint8_t warning_count = 0;

    if (voltage < LOW_BATTERY_WARNING) {  /* 3.3V 预警 */
        warning_count++;
        if (warning_count > 10) {  /* 连续10次低于3.3V */
            low_battery_notification();
        }
    }

    if (voltage < LOW_BATTERY_SHUTDOWN) {  /* 3.0V 关机 */
        log_warning("Battery low: %.2fV, shutting down", voltage);
        save_critical_data();
        system_shutdown();
    }
}
```

## 4. 调试与测试规范

### 验证清单
- [ ] 反接保护：反接电源确认后级无电压
- [ ] TVS 钳位电压：示波器测浪涌时的钳位电压
- [ ] PTC 动作电流：过流时确认 PTC 断开
- [ ] ESD 保护：ESD 枪测试各接口
- [ ] MOSFET 反接：反接时确认 MOS 截止

### 测试方法
- **反接测试**：反向接电源，确认无损坏
- **浪涌测试**：用浪涌发生器模拟雷击/开关浪涌
- **ESD 测试**：接触放电 ±8kV，空气放电 ±15kV
- **过流测试**：逐步增大负载，确认 PTC 动作
- **温度测试**：满载运行时保护器件温升

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| MOSFET 反接保护失效 | Vgs 不足 | P-MOS 的 Vgs(th) 要低于输入电压 |
| MOSFET 发热 | Rds(on) 过大 | 选低 Rds(on) 的 MOSFET |
| TVS 经常损坏 | 功率不足 | 选更大 Ppp 的封装 (SMB/SMC) |
| PTC 频繁断开 | Ih 选太小 | Ih ≥ 工作电流 × 1.2 |
| PTC 动作太慢 | PTC 响应慢 | PTC 为毫秒级，需配合 TVS |
| USB 口 ESD 死机 | 无 ESD 保护 | 加 USBLC6 保护数据线 |
| 浪涌后系统异常 | 仅单级保护 | 用三级保护 (GDT+MOV+TVS) |
| 反接后二极管烧毁 | 电流超额定值 | 选更大 If 的肖特基或改用 MOSFET |
| 工业现场干扰大 | 无隔离 | 加光耦/磁耦隔离 |
| 电池反接损坏 | 无反接保护 | 电池端加 P-MOS 反接保护 |

## 相关文档

- `skill://mcu-driver-core/guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
- `skill://mcu-driver-core/guides/pitfalls.md` — 跨类别常见问题与避坑指南
