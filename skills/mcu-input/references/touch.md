# 电容触摸开发规范

## 1. 概述与选型指南

### 常见方案对比

| 方案 | 通道数 | 接口 | 特点 | 价格 |
|------|--------|------|------|------|
| TTP223 | 1 | GPIO 电平输出 | 零开发量、模块化 | ¥0.5~2 |
| TTP226/229 | 8/16 | 并口 / 2线串行 | 多键触摸板 | ¥2~5 |
| MPR121 | 12 | I2C | 可调灵敏度、带中断 | ¥5~10 |
| ESP32 内置 TouchPad | 10 | 片内外设 | 免外部芯片、支持深睡唤醒 | 0 |
| STM32 TSC | 视引脚 | 片内外设 | 需采样电容、群组扫描 | 0 |

### 选型决策树

```
单键开关？ → TTP223（直接当按键用）
多键且要调灵敏度？ → MPR121（I2C，12 通道）
主控是 ESP32？ → 内置 TouchPad（省 BOM，可触摸唤醒）
金属面板/防水面板？ → 电极加大 + 灵敏度调高，选 MPR121 可调方案
```

## 2. 硬件设计规范

### 触摸电极设计要点

- 电极形状：实心圆/方形，直径 8~15mm（隔面板越厚电极越大）
- 面板：亚克力/玻璃 ≤ 3mm；不可用金属面板直接覆盖电极
- 走线：触摸引线尽量短（< 10cm）、细（0.15~0.2mm）、远离高频信号与电源线，底层不铺地（或网格地）
- 电极周围留 0.5~1mm 间隙环形地，提高抗干扰
- MPR121 的 REXT 接 75kΩ 1% 至地

### TTP223 电路

```
VCC(2.0~5.5V) ── TTP223 VDD（100nF 去耦）
OUT ── MCU GPIO
AHLB/TOG 焊盘: 配置输出极性/翻转模式
```

## 3. 驱动开发规范

### 统一接口定义

```c
typedef enum { TOUCH_OK = 0, TOUCH_ERR_INIT, TOUCH_ERR_I2C } touch_status_t;

touch_status_t touch_init(touch_handle_t *h, const touch_config_t *cfg);
touch_status_t touch_read(touch_handle_t *h, uint16_t *bitmap); /* bit=1 触摸中 */
touch_status_t touch_calibrate(touch_handle_t *h);              /* 基线重校准 */
```

### MPR121 驱动核心（I2C 0x5A）

```c
#define MPR121_TOUCH_STATUS  0x00
#define MPR121_ECR           0x5E

touch_status_t mpr121_init(i2c_device_t *dev) {
    i2c_reg_write(dev, 0x80, 0x63);          /* 软复位 */
    /* 触摸/释放阈值: 触摸阈值 > 释放阈值 形成迟滞 */
    for (uint8_t ch = 0; ch < 12; ch++) {
        i2c_reg_write(dev, 0x41 + ch * 2, 12);  /* touch th */
        i2c_reg_write(dev, 0x42 + ch * 2, 6);   /* release th */
    }
    i2c_reg_write(dev, MPR121_ECR, 0x8F);    /* 使能 12 通道 + 基线跟踪 */
    return TOUCH_OK;
}

touch_status_t mpr121_read(i2c_device_t *dev, uint16_t *bitmap) {
    uint8_t buf[2];
    if (i2c_reg_read_burst(dev, MPR121_TOUCH_STATUS, buf, 2) != I2C_OK)
        return TOUCH_ERR_I2C;
    *bitmap = ((buf[1] & 0x1F) << 8) | buf[0];
    if (buf[1] & 0x80) return TOUCH_ERR_INIT;  /* 过流标志 */
    return TOUCH_OK;
}
```

### ESP32 内置 TouchPad

```c
#include "driver/touch_pad.h"

void touch_setup(void) {
    touch_pad_init();
    touch_pad_config(TOUCH_PAD_NUM0, 0);
    /* 基线自校准: 上电取 64 次平均作为未触摸基线 */
    uint16_t base = 0, v;
    for (int i = 0; i < 64; i++) { touch_pad_read(TOUCH_PAD_NUM0, &v); base += v / 64; }
    threshold = base * 2 / 3;   /* 读数低于基线 2/3 判定触摸 */
}

bool touch_pressed(void) {
    uint16_t v; touch_pad_read(TOUCH_PAD_NUM0, &v);
    return v < threshold;
}
/* 深睡触摸唤醒: esp_sleep_enable_touchpad_wakeup() */
```

## 4. 调试与测试规范

### 硬件验证清单
- [ ] 电极对地电容基线值稳定（波动 < ±2%）
- [ ] 隔面板触摸时信号变化量 > 基线的 10%
- [ ] 靠近电源线/电机等干扰源时无误触发

### 功能测试
- 连续触摸/释放 100 次识别率 100%，无粘连
- 湿手、戴手套场景按产品需求验证
- 温度变化（-10~50°C）基线跟踪正常不误触发

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 上电误触发 | 上电时手在电极附近，基线校准错误 | 延迟 500ms 校准；MPR121 开启基线自动跟踪 |
| 隔面板不灵敏 | 面板过厚/电极过小 | 电极加大、灵敏度阈值调低、面板减薄 |
| 电机启动时误触发 | 电源/空间耦合干扰 | 触摸线远离干扰源、增加判定次数、软件滤波 |
| 长时间后失灵 | 温漂导致基线漂移 | 启用周期性基线重校准 |
| 粘连（一直报触摸） | 释放阈值过低无迟滞 | 触摸/释放双阈值形成迟滞 |
| ESP32 WiFi 开启后触摸抖动 | 射频干扰 | 采样取平均、阈值留裕量、触摸线远离天线 |

## 相关文档

- `skill://mcu-driver-core/templates/driver-template-i2c.c` — I2C 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
- `skill://mcu-driver-core/guides/pitfalls.md` — 跨类别常见问题与避坑指南
