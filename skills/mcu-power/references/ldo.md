# LDO 线性稳压器开发规范

## 1. 概述与选型指南

### 常见型号对比

| 型号 | 输出电压 | 最大电流 | 压差 | 静态电流 | 封装 | 特点 | 价格 |
|------|----------|----------|------|----------|------|------|------|
| AMS1117-3.3 | 3.3V | 1A | 1.3V | 5mA | SOT-223 | 经典、廉价 | ¥0.3~1 |
| HT7333 | 3.3V | 250mA | 0.1V | 5μA | SOT-89 | 超低功耗 | ¥0.5~2 |
| ME6211 | 3.3V | 500mA | 0.1V | 6μA | SOT-23-5 | 低压差、小封装 | ¥0.5~2 |
| RT9013 | 3.3V | 300mA | 0.1V | 50μA | SOT-23-5 | 低噪声 | ¥0.5~1.5 |
| MIC5217 | 3.3V | 500mA | 0.1V | 65μA | SOT-23-5 | 电池友好 | ¥1~3 |
| LP2985 | 3.3V | 150mA | 0.1V | 85μA | SOT-23-5 | 低噪声 | ¥1~3 |
| AP2112 | 3.3V | 600mA | 0.25V | 55μA | SOT-23-5 | 通用 | ¥0.3~1 |

### 选型建议
- **5V→3.3V 大电流** → AMS1117-3.3（经典，但压差大、功耗高）
- **电池供电低功耗** → HT7333（静态电流 5μA）
- **低压差小封装** → ME6211 / AP2112
- **低噪声(ADC/传感器)** → RT9013 / LP2985
- **3.3V→1.8V** → ME6211（低压差合适）

### 功耗计算

```
LDO 功耗: P = (Vin - Vout) × Iout

示例: AMS1117-3.3, Vin=5V, Vout=3.3V, Iout=300mA
P = (5 - 3.3) × 0.3 = 0.51W
→ SOT-223 热阻约 60°C/W, 温升 ≈ 30.6°C
→ 室温 25°C 时芯片温度 ≈ 55.6°C (可接受)

如果 Iout=800mA:
P = 1.7 × 0.8 = 1.36W
→ 温升 ≈ 81.6°C → 芯片温度 ≈ 106.6°C (危险!)
→ 需改用 DC-DC 或加散热
```

## 2. 硬件设计规范

### 典型应用电路

```
    Vin (5V)
      │
      ├──── 10μF ────┐  (输入电容)
      │              │
      │         ┌────┴────┐
      │         │   LDO   │
      ├─────────┤ Vin     │
      │         │    GND  │── GND
      │         │     Vout├────┬──── Vout (3.3V)
      │         └─────────┘    │
      │                        ├── 10μF ────┐ (输出电容)
      │                        │             │
      │                        ├── 100nF ────┤ (高频去耦)
      │                        │             │
     GND ──────────────────────┴─────────────┴── GND
```

### 关键设计要点

1. **输入电容**：10μF 电解/陶瓷 + 100nF 陶瓷（靠近 IC）
2. **输出电容**：10μF + 100nF（陶瓷电容，ESR 要低）
3. **电容选择**：必须使用低 ESR 陶瓷电容（X5R/X7R），避免高 ESR 导致振荡
4. **散热处理**：大电流时需铺铜散热，SOT-223 的 GND tab 需大面积铺铜
5. **布局**：LDO 尽量靠近负载，减少走线压降
6. **旁路**：每个 IC 旁再加 100nF 去耦电容

### 散热计算

```
芯片温度: Tj = Ta + P × Rθja

Rθja (结到环境热阻):
- SOT-23-5:  220~350 °C/W (大电流时不适用)
- SOT-89:    180~250 °C/W
- SOT-223:   50~90 °C/W (有散热铺铜)

安全工作温度: Tj < 125°C (建议 < 85°C 留余量)
```

## 3. 驱动开发规范

LDO 为纯硬件器件，无需软件驱动。但需在固件中考虑：

### 电源监测代码

```c
/**
 * @file power_monitor.c
 * @brief 电源电压监测 (通过 ADC 读取 LDO 输出)
 */

/* 通过 ADC 监测 LDO 输出电压 */
bool power_check_voltage(adc_config_t *adc, float vref,
                         float expected_v, float tolerance) {
    uint16_t adc_val;
    if (adc_read(adc, &adc_val) != 0) return false;

    float actual_v = adc_val * vref / 4095.0f;  /* 12bit ADC */

    /* 检查电压是否在容差范围内 */
    if (fabs(actual_v - expected_v) > tolerance) {
        /* 电压异常, 记录日志/报警 */
        log_error("Voltage: %.2fV (expected %.2fV ± %.2fV)",
                  actual_v, expected_v, tolerance);
        return false;
    }
    return true;
}

/* 低电压检测 (使用 MCU 内部参考电压反算 VCC) */
float power_read_vcc(void) {
    /* 读取内部 1.2V 参考电压通道, 反算 VCC */
    uint16_t adc_val = adc_read_internal_ref();
    float vcc = 1.2f * 4095.0f / adc_val;
    return vcc;
}

/* 低电压报警阈值 */
#define LOW_VOLTAGE_THRESHOLD  3.0f  /* V */
#define HIGH_VOLTAGE_THRESHOLD 3.6f  /* V */

void power_monitor_task(void) {
    float vcc = power_read_vcc();
    if (vcc < LOW_VOLTAGE_THRESHOLD) {
        /* 低电压处理: 保存数据、关外设、进入低功耗 */
        emergency_shutdown();
    } else if (vcc > HIGH_VOLTAGE_THRESHOLD) {
        /* 过压报警 */
        overvoltage_alert();
    }
}
```

## 4. 调试与测试规范

### 验证清单
- [ ] 输入电压在 LDO 耐压范围内
- [ ] 输入输出电容容量和 ESR 满足要求
- [ ] 负载电流未超过额定值
- [ ] 温升在安全范围内（< 85°C）
- [ ] 输出纹波 < 50mV

### 测试方法
- **万用表测电压**：空载和满载时输出电压稳定性
- **示波器测纹波**：交流耦合，测输出纹波和噪声
- **负载阶跃测试**：负载电流突变时观察输出电压响应
- **温度测试**：满载运行 10 分钟，用热像仪测 IC 温度
- ** dropout 测试**：逐步降低输入电压，记录输出开始下降的临界点

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 输出电压偏低 | 负载电流过大 | 计算功耗，换更大电流 LDO 或 DC-DC |
| 输出振荡 | 输出电容 ESR 过高 | 换低 ESR 陶瓷电容(X5R/X7R) |
| 输出振荡 | 输出电容容量不足 | 确保 ≥10μF |
| 发热严重 | 压差×电流太大 | P=(Vin-Vout)×I，改用 DC-DC |
| 输出纹波大 | 输入纹波大 | 输入端加大电容或加 LC 滤波 |
| 上电瞬间过冲 | 软启动电容缺失 | 选带软启动的 LDO 或加延迟 |
| 电池供电寿命短 | 静态电流大 | 换低 Iq 的 LDO (如 HT7333) |
| ADC 噪声大 | LDO 噪声大 | 换低噪声 LDO (RT9013/LP2985) |
| 3.3V MCU 供电不稳 | LDO 输入不足 | 确保 Vin > Vout + dropout |

## 相关文档

- `skill://mcu-driver-core/templates/driver-template-adc.c` — ADC 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
- `skill://mcu-driver-core/guides/pitfalls.md` — 跨类别常见问题与避坑指南
