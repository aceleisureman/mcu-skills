# 按键与矩阵键盘开发规范

## 1. 概述与选型指南

### 常见方案对比

| 方案 | 按键数 | 占用 IO | 接口 | 特点 | 价格 |
|------|--------|---------|------|------|------|
| 独立按键 | 1~4 | 每键 1 IO | GPIO | 最简单、响应快 | ¥0.1~0.5/个 |
| 4x4 矩阵键盘 | 16 | 8 IO | GPIO 扫描 | IO 高效利用 | ¥3~8 |
| ADC 分压键盘 | 3~8 | 1 IO | ADC | 单线多键、不可组合按 | ¥1~3 |
| TM1638 按键模块 | 8~24 | 3 IO | 类 SPI | 带数码管/LED 一体 | ¥5~10 |
| 74HC165 扩展 | 8n | 3 IO | SPI 移位 | 按键数可级联扩展 | ¥1~2/片 |

### 选型决策树

```
≤ 4 键？ → 独立按键（EXTI 中断 + 消抖）
5~16 键 且 IO 富余？ → 矩阵键盘（行列扫描）
IO 极度紧张？ → ADC 分压键盘（1 个 ADC 通道）
需同时显示？ → TM1638（键盘 + 数码管一体）
```

## 2. 硬件设计规范

### 独立按键

```
MCU GPIO（内部上拉）── 按键 ── GND
可选硬件消抖: 按键两端并 100nF
```

### 4x4 矩阵键盘

```
行 R1~R4: MCU 输出（扫描时逐行拉低）
列 C1~C4: MCU 输入（内部上拉）
需支持多键同按时每个按键串联二极管（防鬼键）
```

**要点**：
- 独立按键优先用内部上拉 + 低电平有效（抗干扰好于下拉）
- 长引线按键（面板延长线 >20cm）需外部上拉 + RC 滤波 + TVS
- 矩阵键盘不加二极管时，同按 3 键可能误报第 4 键（鬼键）

## 3. 驱动开发规范

### 统一接口定义

```c
typedef enum { KEY_IDLE = 0, KEY_DOWN, KEY_UP, KEY_LONG, KEY_REPEAT } key_event_t;

typedef struct {
    uint8_t     code;     /* 键码 */
    key_event_t event;
} key_msg_t;

void key_scan_init(const key_config_t *cfg);
void key_scan_tick(void);                 /* 10ms 周期调用 */
bool key_get_msg(key_msg_t *msg);         /* 从事件队列取消息 */
```

### 独立按键消抖 + 长按状态机（10ms tick）

```c
#define DEBOUNCE_TICKS  2     /* 20ms 消抖 */
#define LONG_TICKS      100   /* 1s 长按 */

typedef struct {
    uint8_t  stable;      /* 消抖后的稳定电平 */
    uint8_t  cnt;
    uint16_t hold;
} key_fsm_t;

void key_scan_tick(void) {
    for (int i = 0; i < KEY_NUM; i++) {
        key_fsm_t *k = &keys[i];
        uint8_t raw = !gpio_read(key_pins[i]);   /* 低有效 → 1=按下 */
        if (raw != k->stable) {
            if (++k->cnt >= DEBOUNCE_TICKS) { k->stable = raw; k->cnt = 0;
                key_push(i, raw ? KEY_DOWN : KEY_UP);
                if (!raw) k->hold = 0;
            }
        } else k->cnt = 0;
        if (k->stable && ++k->hold == LONG_TICKS) key_push(i, KEY_LONG);
    }
}
```

### 4x4 矩阵扫描

```c
uint16_t keypad_scan(void) {              /* 返回 16 位按键位图 */
    uint16_t map = 0;
    for (int r = 0; r < 4; r++) {
        for (int i = 0; i < 4; i++) gpio_write(row_pins[i], i != r); /* 仅当前行拉低 */
        delay_us(10);                     /* 等待电平稳定 */
        for (int c = 0; c < 4; c++)
            if (!gpio_read(col_pins[c])) map |= 1u << (r * 4 + c);
    }
    return map;                           /* 位图交上层消抖(同独立按键逻辑) */
}
```

### ADC 分压键盘

```c
/* 电阻链: VCC-2k-K1-2k-K2-2k-K3-...-GND, 每键对应一段电压窗口 */
int8_t adc_keypad_read(uint16_t adc_val, uint16_t adc_max) {
    static const uint16_t th[] = {100, 300, 500, 700, 900}; /* 千分比阈值 */
    uint32_t p = (uint32_t)adc_val * 1000 / adc_max;
    for (int i = 0; i < 5; i++)
        if (p < th[i]) return i;
    return -1;   /* 无按键 */
}
/* 注意: 判定窗口留 ±5% 裕量, 连续 2 次相同才输出 */
```

## 4. 调试与测试规范

### 硬件验证清单
- [ ] 按键未按时输入脚为高（上拉有效）
- [ ] 矩阵逐行拉低时，按下对应键列线变低
- [ ] ADC 键盘各键电压值与设计分压一致（±5%）

### 功能测试
- 每键连按 100 次无丢键、无重报（消抖有效）
- 长按 1s 触发 KEY_LONG 且只触发一次
- 矩阵同按 2 键均正确识别；3 键同按验证鬼键屏蔽逻辑

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 一次按键触发多次 | 未消抖 | 10ms 周期采样 + 连续 2 次一致再确认 |
| 按键偶尔失灵 | 中断沿丢失 | 改为定时器轮询扫描，不依赖 EXTI |
| 矩阵出现鬼键 | 3 键同按导通回路 | 每键串二极管，或软件检测到 ≥3 键时丢弃 |
| ADC 键盘误判 | 电压窗口过窄/温漂 | 阈值窗口留 ±5%，用 1% 精度电阻 |
| 长线按键误触发 | 引线耦合干扰 | RC 滤波 + 软件加长消抖时间 |
| 低功耗唤醒失效 | 扫描停止 | 独立按键接唤醒引脚；矩阵行全拉低后列设为中断唤醒 |

## 相关文档

- `../../templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-adc.c` — ADC 驱动模板（HAL 抽象层骨架）
- `../../guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `../../guides/debugging-testing.md` — 调试与测试规范
- `../../guides/pitfalls.md` — 跨类别常见问题与避坑指南
