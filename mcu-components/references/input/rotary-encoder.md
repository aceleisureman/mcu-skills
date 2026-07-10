# 旋转编码器开发规范

## 1. 概述与选型指南

### 常见型号对比

| 型号 | 类型 | 分辨率 | 接口 | 特点 | 价格 |
|------|------|--------|------|------|------|
| EC11 | 机械增量式 | 20 脉冲/圈 | A/B 相 + 按键 | 带按压开关、面板旋钮首选 | ¥1~3 |
| EC16 | 机械增量式 | 24 脉冲/圈 | A/B 相 | 体积小 | ¥1~3 |
| 400 线光电编码器 | 光电增量式 | 400 PPR | A/B(/Z) 相 | 高速、无抖动、电机测速 | ¥15~40 |
| AS5600 | 磁编码器 | 12bit 绝对角度 | I2C / PWM / 模拟 | 非接触、无磨损 | ¥5~12 |
| HN3806 | 光电增量式 | 100~600 PPR | A/B/Z 相 NPN | 工业级、5~24V | ¥25~60 |

### 选型决策树

```
人机交互旋钮？ → 是 → EC11（带按键，注意消抖）
需要绝对角度？ → 是 → AS5600（I2C 读角度，无需计数）
电机测速/闭环？ → 是 → 光电编码器（400 线以上，接定时器编码器模式）
高转速（>3000rpm）？ → 是 → 光电/磁编码器，禁用机械式
```

## 2. 硬件设计规范

### EC11 典型电路

```
VCC ── 10kΩ ──┬── A 相 ── MCU GPIO (EXTI)
VCC ── 10kΩ ──┬── B 相 ── MCU GPIO
VCC ── 10kΩ ──┬── SW  ── MCU GPIO
公共端 C ───────── GND
各信号线对地并 100nF（硬件消抖）
```

**要点**：
- 机械编码器抖动严重，必须硬件 RC 消抖（10kΩ + 100nF）或软件滤波
- A/B 相上拉不可省略（编码器内部是开关对地）
- 光电编码器 NPN 开集输出需上拉至 MCU 电平；5V 输出接 3.3V MCU 需分压或电平转换
- AS5600 需径向磁铁（直径 6mm）与芯片中心对准，间距 0.5~3mm

## 3. 驱动开发规范

### 统一接口定义

```c
typedef enum { ENC_OK = 0, ENC_ERR_INIT, ENC_ERR_PARAM } enc_status_t;

typedef struct {
    int32_t count;        /* 累计计数 */
    int8_t  direction;    /* +1 正转 / -1 反转 / 0 静止 */
    bool    button;       /* EC11 按键状态 */
} encoder_state_t;

enc_status_t encoder_init(encoder_handle_t *h, const encoder_config_t *cfg);
enc_status_t encoder_read(encoder_handle_t *h, encoder_state_t *state);
void         encoder_reset(encoder_handle_t *h);
```

### EC11 四倍频状态机解码（抗抖动）

```c
/* 状态转移表法: 用 AB 相前后两次状态查表, 天然滤除非法跳变 */
static const int8_t enc_table[16] = {
    0, -1,  1, 0,
    1,  0,  0, -1,
   -1,  0,  0, 1,
    0,  1, -1, 0
};

/* 在 A/B 任一沿中断或 1kHz 定时器中调用 */
void encoder_poll(encoder_handle_t *h) {
    uint8_t ab = (gpio_read(h->pin_a) << 1) | gpio_read(h->pin_b);
    h->prev_state = ((h->prev_state << 2) | ab) & 0x0F;
    int8_t delta = enc_table[h->prev_state];
    h->raw_count += delta;
    /* EC11 一格 = 4 个状态变化, 除 4 得到格数 */
    h->count = h->raw_count / 4;
}
```

### STM32 定时器编码器模式（光电编码器）

```c
/* TIM3 CH1/CH2 接 A/B 相, 硬件自动计数, 零 CPU 开销 */
htim3.Instance = TIM3;
htim3.Init.Period = 0xFFFF;
TIM_Encoder_InitTypeDef enc = {
    .EncoderMode = TIM_ENCODERMODE_TI12,   /* 四倍频 */
    .IC1Polarity = TIM_ICPOLARITY_RISING,
    .IC1Filter   = 8,                      /* 输入滤波 */
    .IC2Polarity = TIM_ICPOLARITY_RISING,
    .IC2Filter   = 8,
};
HAL_TIM_Encoder_Init(&htim3, &enc);
HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);

int16_t encoder_get_delta(void) {
    static uint16_t last = 0;
    uint16_t now = __HAL_TIM_GET_COUNTER(&htim3);
    int16_t delta = (int16_t)(now - last);   /* 依赖 16bit 回绕 */
    last = now;
    return delta;
}
```

### AS5600 角度读取（I2C）

```c
#define AS5600_ADDR      0x36
#define AS5600_RAW_ANGLE 0x0C

enc_status_t as5600_read_angle(i2c_device_t *dev, float *deg) {
    uint8_t buf[2];
    if (i2c_reg_read_burst(dev, AS5600_RAW_ANGLE, buf, 2) != I2C_OK)
        return ENC_ERR_INIT;
    uint16_t raw = ((buf[0] & 0x0F) << 8) | buf[1];  /* 12bit */
    *deg = raw * 360.0f / 4096.0f;
    return ENC_OK;
}
```

## 4. 调试与测试规范

### 硬件验证清单
- [ ] 万用表确认 A/B 相静止时电平（上拉后应为高）
- [ ] 示波器观察旋转时 A/B 相位差 90°
- [ ] EC11 按键通断正常

### 功能测试
- 慢速正转 10 格 / 反转 10 格，计数应精确 ±0（状态机法）
- 快速旋转不丢步（定时器模式）；机械编码器快速旋转允许少量丢步
- AS5600：旋转一圈角度应从 0 平滑到 360 无跳变

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 计数乱跳/一格计多次 | 机械抖动 | 用状态转移表法解码 + RC 硬件消抖 |
| 只能单向计数 | B 相未接或读反 | 检查 B 相接线与引脚配置 |
| 快速旋转丢步 | 中断处理过慢 | 改用定时器编码器模式（硬件计数） |
| 方向与预期相反 | A/B 接反 | 交换 A/B 或计数取反 |
| AS5600 读数跳动 | 磁铁偏心/距离不当 | 磁铁对准芯片中心，间距 0.5~3mm |
| 光电编码器无输出 | NPN 开集未上拉 | 输出线加 4.7kΩ 上拉至 MCU 电平 |

## 相关文档

- `../../templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-i2c.c` — I2C 驱动模板（HAL 抽象层骨架）
- `../../guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `../../guides/debugging-testing.md` — 调试与测试规范
- `../../guides/pitfalls.md` — 跨类别常见问题与避坑指南
