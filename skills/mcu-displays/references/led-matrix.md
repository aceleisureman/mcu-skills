# LED 点阵/数码管开发规范

## 1. 概述与选型指南

### 器件简介

LED 点阵和数码管是 MCU 领域最基础的显示器件。点阵由行列 LED 矩阵组成，可显示文字、图形和动画；数码管用于数字显示。由于直接驱动需大量引脚，通常使用专用驱动 IC（MAX7219、TM1637、HT16K33）或单线协议芯片（WS2812）。WS2812 是内置控制 IC 的全彩 LED，通过单线 NZP 协议级联控制。

### 常见型号对比

| 型号 | 类型 | 显示能力 | 接口 | 驱动IC | 级联 | 工作电压 | 价格 |
|------|------|----------|------|--------|------|----------|------|
| MAX7219 | LED点阵 | 8x8点阵 | SPI | MAX7219 | 菊花链 | 5V | ¥3~8 |
| TM1637 | 4位数码管 | 4位7段 | 2线 | TM1637 | 不支持 | 5V | ¥3~6 |
| HT16K33 | 数码管/点阵 | 16x8 | I2C | HT16K33 | 地址级联 | 5V/3.3V | ¥4~8 |
| WS2812 | 全彩LED | 单颗RGB | 单线NZP | WS2812 | 串联 | 5V | ¥0.3~1/颗 |
| TM1640 | 16位数码管 | 16位7段 | 2线 | TM1640 | 不支持 | 5V | ¥5~10 |

### 选型决策树

```
需要全彩？ → 是 → WS2812 (单线级联)
           否 →
需要LED点阵(画图/文字)？ → 是 → MAX7219 8x8 (可级联拼大屏)
                         否 →
需要数码管显示数字？ → I2C接口？ → 是 → HT16K33
                    │           否 → 引脚少？ → TM1637 (2线)
                    │                    否 → 多位？ → TM1640
                    └── 通用推荐 → TM1637 4位
```

### 适用场景

- **MAX7219**：LED 点阵广告牌、时钟显示、动画、信息滚动屏
- **TM1637**：电子钟、温度显示、计数器、电压表头
- **HT16K33**：I2C 总线数码管应用、多组级联
- **WS2812**：氛围灯、LED 灯带、全彩装饰、状态指示灯阵列

---

## 2. 硬件设计规范

### 引脚定义

```
MAX7219 Module:                TM1637 Module:
  VCC/GND                        VCC/GND
  DIN (SPI MOSI)                 CLK (时钟, 需上拉)
  CS  (SPI CS)                   DIO (数据, 需上拉)
  CLK (SPI SCK)
  DOUT → 下一模块 DIN            HT16K33:
                                  VCC/GND, SDA, SCL
WS2812 Strip:                     A0~A2 (地址跳线 0x70~0x77)
  VCC (5V独立电源)
  GND (共地)                     WS2812:
  DIN (MCU GPIO)                  VCC, GND, DIN, DOUT
  DOUT → 下一颗 DIN
```

### 典型应用电路

#### MAX7219 级联

```
MCU              MAX7219#1           MAX7219#2
SPI MOSI ─────── DIN                 DOUT ── DIN
SPI SCK  ─────── CLK        ───────── CLK
GPIO     ─────── CS         ───────── CS
5V ───────────── VCC        ───────── VCC
GND ──────────── GND        ───────── GND

ISET ──[10kΩ]── VCC  (限流电阻, 设定段电流)
```

#### WS2812 灯带

```
5V(独立电源) ──[1000μF]── VCC  (大电流滤波)
GND(电源) ──────────────── GND
GND(MCU)  ──────────────── GND  (必须共地!)
MCU GPIO ──[470Ω]─────── DIN   (保护电阻)
```

### PCB 设计要点

- **MAX7219 限流**：ISET 引脚接 10kΩ 到 VCC，对应约 40mA/段
- **TM1637 上拉**：CLK/DIO 需 4.7kΩ 上拉（部分模块已内置）
- **HT16K33 地址**：A0/A1/A2 必须接 VDD 或 GND，不可悬空
- **WS2812 电源**：每颗全白约 60mA，60颗=3.6A，必须独立电源
- **WS2812 数据保护**：DIN 串联 470Ω；电源并联 1000μF + 每10颗 100nF
- **共地**：WS2812 独立电源时必须与 MCU 共地
- **去耦电容**：每个驱动 IC 旁放 100nF

### 电气参数

| 参数 | MAX7219 | TM1637 | HT16K33 | WS2812 | 单位 |
|------|---------|--------|---------|--------|------|
| VCC | 4.0~5.5 | 4.5~5.5 | 2.4~5.5 | 3.5~5.3 | V |
| 段电流 | 40 | 20 | 24 | — | mA |
| WS2812单颗 | — | — | — | 60 | mA |
| 时钟频率 | 10 | 250 | 400 | 800 | kHz |
| 复位时间 | — | — | — | >50 | μs |
| 最大级联 | 8 | 不支持 | 8 | 1024 | 个 |

---

## 3. 驱动开发规范

### MAX7219 驱动 (SPI)

```c
#define MAX7219_REG_DIGIT0    0x01
#define MAX7219_REG_DECODE    0x09
#define MAX7219_REG_INTENSITY 0x0A
#define MAX7219_REG_SCANLIMIT 0x0B
#define MAX7219_REG_SHUTDOWN  0x0C
#define MAX7219_REG_TEST      0x0F
#define MAX7219_REG_NOOP      0x00

typedef struct {
    void (*spi_write)(const uint8_t *data, uint16_t len);
    void (*cs_write)(int val);
    uint8_t cascaded;
} max7219_hw_t;

static void max7219_write_reg(max7219_hw_t *hw, uint8_t reg, uint8_t data) {
    uint8_t buf[2] = {reg, data};
    hw->cs_write(0);
    hw->spi_write(buf, 2);
    hw->cs_write(1);
}

void max7219_init(max7219_hw_t *hw) {
    max7219_write_reg(hw, MAX7219_REG_SHUTDOWN, 0x00);
    max7219_write_reg(hw, MAX7219_REG_DECODE, 0x00);    /* 无译码(点阵) */
    max7219_write_reg(hw, MAX7219_REG_SCANLIMIT, 0x07); /* 扫描8位 */
    max7219_write_reg(hw, MAX7219_REG_INTENSITY, 0x08); /* 亮度50% */
    for (int i = 1; i <= 8; i++) max7219_write_reg(hw, i, 0x00);
    max7219_write_reg(hw, MAX7219_REG_SHUTDOWN, 0x01);  /* 开启 */
}

void max7219_set_intensity(max7219_hw_t *hw, uint8_t val) {
    if (val > 15) val = 15;
    max7219_write_reg(hw, MAX7219_REG_INTENSITY, val);
}

/* 写入8x8点阵数据 */
void max7219_set_matrix(max7219_hw_t *hw, const uint8_t matrix[8]) {
    for (int row = 0; row < 8; row++) {
        uint8_t buf[2] = {MAX7219_REG_DIGIT0 + row, matrix[row]};
        hw->cs_write(0);
        hw->spi_write(buf, 2);
        hw->cs_write(1);
    }
}

/* 级联写入: 同时更新N个模块的同一行 */
void max7219_set_matrix_cascade(max7219_hw_t *hw, uint8_t row, const uint8_t *data) {
    hw->cs_write(0);
    for (int i = 0; i < hw->cascaded; i++) {
        uint8_t buf[2] = {MAX7219_REG_DIGIT0 + row, data[i]};
        hw->spi_write(buf, 2);
    }
    hw->cs_write(1);
}

/* 8x8 字符点阵 (数字0~9) */
static const uint8_t font_8x8_digits[][8] = {
    {0x3C,0x42,0x42,0x42,0x42,0x42,0x42,0x3C},  /* 0 */
    {0x08,0x18,0x28,0x08,0x08,0x08,0x08,0x3E},  /* 1 */
    {0x3C,0x42,0x02,0x04,0x08,0x10,0x20,0x7E},  /* 2 */
    {0x3C,0x42,0x02,0x1C,0x02,0x02,0x42,0x3C},  /* 3 */
    {0x04,0x0C,0x14,0x24,0x44,0x7E,0x04,0x04},  /* 4 */
    {0x7E,0x40,0x40,0x7C,0x02,0x02,0x42,0x3C},  /* 5 */
    {0x1C,0x20,0x40,0x7C,0x42,0x42,0x42,0x3C},  /* 6 */
    {0x7E,0x02,0x04,0x08,0x10,0x10,0x10,0x10},  /* 7 */
    {0x3C,0x42,0x42,0x3C,0x42,0x42,0x42,0x3C},  /* 8 */
    {0x3C,0x42,0x42,0x42,0x3E,0x02,0x04,0x38},  /* 9 */
};
```

### TM1637 驱动 (2线协议)

```c
/* TM1637 协议: 类I2C, 起始/停止/ACK时序类似但不同
   起始: CLK高时 DIO由高变低
   停止: CLK高时 DIO由低变高
   数据: CLK下降沿写DIO, 上升沿锁存 */

typedef struct {
    void (*clk_write)(int val);
    void (*dio_write)(int val);
    void (*dio_set_input)(void);
    void (*delay_us)(uint32_t us);
} tm1637_hw_t;

/* 7段数码管编码 (共阴) */
static const uint8_t tm1637_digits[] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
    0x7F, 0x6F, 0x00, 0x40,  /* 0~9, 空, 负号 */
};

#define TM1637_CMD_DATA   0x40
#define TM1637_CMD_CTRL   0x80
#define TM1637_CMD_ADDR   0xC0

static void tm1637_start(tm1637_hw_t *hw) {
    hw->clk_write(1); hw->dio_write(1); hw->delay_us(2);
    hw->dio_write(0); hw->delay_us(2); hw->clk_write(0);
}

static void tm1637_stop(tm1637_hw_t *hw) {
    hw->clk_write(0); hw->dio_write(0); hw->delay_us(2);
    hw->clk_write(1); hw->delay_us(2); hw->dio_write(1); hw->delay_us(2);
}

static void tm1637_write_byte(tm1637_hw_t *hw, uint8_t data) {
    for (int i = 0; i < 8; i++) {
        hw->clk_write(0);
        hw->dio_write(data & 1);
        hw->delay_us(3);
        hw->clk_write(1);
        hw->delay_us(3);
        data >>= 1;
    }
    hw->clk_write(0); hw->dio_set_input();
    hw->delay_us(3); hw->clk_write(1); hw->delay_us(3); hw->clk_write(0);
}

void tm1637_display(tm1637_hw_t *hw, uint8_t d[4], uint8_t brightness) {
    tm1637_start(hw);
    tm1637_write_byte(hw, TM1637_CMD_DATA);
    tm1637_stop(hw);

    tm1637_start(hw);
    tm1637_write_byte(hw, TM1637_CMD_ADDR);
    for (int i = 0; i < 4; i++) tm1637_write_byte(hw, tm1637_digits[d[i]]);
    tm1637_stop(hw);

    tm1637_start(hw);
    tm1637_write_byte(hw, TM1637_CMD_CTRL | (brightness & 0x07) | 0x08);
    tm1637_stop(hw);
}

void tm1637_show_number(tm1637_hw_t *hw, int16_t num, uint8_t brightness) {
    uint8_t d[4];
    bool neg = num < 0;
    if (neg) num = -num;
    d[0] = (num/1000)%10; d[1] = (num/100)%10;
    d[2] = (num/10)%10;   d[3] = num%10;
    /* 前导零消隐 */
    if (d[0]==0) { d[0]=10; if (d[1]==0) { d[1]=10; if (d[2]==0) d[2]=10; } }
    if (neg && d[1]==10) d[1]=11; else if (neg) d[0]=11;
    tm1637_display(hw, d, brightness);
}
```

### WS2812 驱动 (单线 NZP, SPI 编码法)

```c
/* WS2812 时序 (800kHz):
   T0H=0.4us高+0.85us低  T1H=0.8us高+0.45us低  RES=>50us低
   精度要求±150ns, 用 SPI@4MHz 3bit编码法最通用 */

typedef struct {
    void (*spi_write)(const uint8_t *data, uint16_t len);
    void (*delay_us)(uint32_t us);
} ws2812_hw_t;

/* 1字节 → 3字节SPI编码 (每bit用3个SPI bit表示) */
static void ws2812_encode(uint8_t data, uint8_t *out) {
    for (int i = 0; i < 3; i++) out[i] = 0;
    for (int i = 7; i >= 0; i--) {
        uint8_t bit = (data >> i) & 1;
        int pos = (7 - i) * 3;
        /* bit=1 → 0b100, bit=0 → 0b110 */
        out[pos/8] |= 1 << (7 - pos%8);
        if (bit == 0) out[(pos+1)/8] |= 1 << (7 - (pos+1)%8);
    }
}

/* 发送颜色 (GRB格式) */
void ws2812_send(ws2812_hw_t *hw, const uint8_t *grb, uint16_t count) {
    uint16_t buf_len = count * 3 * 3;
    uint8_t *buf = malloc(buf_len);
    if (!buf) return;

    uint16_t pos = 0;
    for (uint16_t i = 0; i < count; i++) {
        ws2812_encode(grb[i*3],   &buf[pos]); pos += 3;  /* G */
        ws2812_encode(grb[i*3+1], &buf[pos]); pos += 3;  /* R */
        ws2812_encode(grb[i*3+2], &buf[pos]); pos += 3;  /* B */
    }
    hw->spi_write(buf, buf_len);
    free(buf);
    hw->delay_us(60);  /* 复位 >50us */
}

void ws2812_set_color(uint8_t *grb, uint8_t r, uint8_t g, uint8_t b) {
    grb[0]=g; grb[1]=r; grb[2]=b;  /* GRB顺序 */
}

void ws2812_hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint8_t region = h / 43, rem = (h - region*43) * 6;
    uint8_t p = (v*(255-s))>>8;
    uint8_t q = (v*(255-((s*rem)>>8)))>>8;
    uint8_t t = (v*(255-((s*(255-rem))>>8)))>>8;
    switch (region) {
        case 0: *r=v;*g=t;*b=p; break;
        case 1: *r=q;*g=v;*b=p; break;
        case 2: *r=p;*g=v;*b=t; break;
        case 3: *r=p;*g=q;*b=v; break;
        case 4: *r=t;*g=p;*b=v; break;
        default:*r=v;*g=p;*b=q; break;
    }
}
```

### 使用示例

```c
/* MAX7219: 显示数字 */
max7219_hw_t max_hw = { .spi_write=spi_write, .cs_write=cs_write, .cascaded=1 };
void max7219_demo(void) {
    max7219_init(&max_hw);
    max7219_set_matrix(&max_hw, font_8x8_digits[5]);  /* 显示 "5" */
}

/* TM1637: 显示计数器 */
tm1637_hw_t tm_hw = { .clk_write=clk_write, .dio_write=dio_write, ... };
void tm1637_demo(void) {
    int count = 0;
    while (1) { tm1637_show_number(&tm_hw, count++, 5); HAL_Delay(1000); }
}

/* WS2812: 彩虹灯效 */
ws2812_hw_t ws_hw = { .spi_write=spi_write, .delay_us=delay_us };
void ws2812_rainbow(void) {
    uint8_t colors[30][3]; uint16_t hue = 0;
    while (1) {
        for (int i=0; i<30; i++) {
            uint8_t r,g,b;
            ws2812_hsv_to_rgb((hue+i*8)%360, 255, 100, &r,&g,&b);
            ws2812_set_color(colors[i], r,g,b);
        }
        ws2812_send(&ws_hw, (uint8_t*)colors, 30);
        hue = (hue+4) % 360;
        HAL_Delay(30);
    }
}
```

---

## 4. 调试与测试规范

### 硬件验证清单

- [ ] **MAX7219**：ISET 限流电阻已接（10kΩ）；VCC=5V
- [ ] **MAX7219**：DIN/CS/CLK 连接正确；级联 DOUT→DIN
- [ ] **TM1637**：CLK/DIO 上拉 4.7kΩ 已接
- [ ] **HT16K33**：I2C 扫描确认地址 0x70~0x77
- [ ] **WS2812**：独立 5V 电源；GND 与 MCU 共地
- [ ] **WS2812**：DIN 串联 470Ω；电源并联 1000μF
- [ ] 所有模块：100nF 去耦电容已贴

### 通信验证

```c
/* MAX7219 全亮测试 */
max7219_write_reg(&hw, MAX7219_REG_TEST, 0x01);  /* 全亮 */
HAL_Delay(2000);
max7219_write_reg(&hw, MAX7219_REG_TEST, 0x00);  /* 退出 */

/* HT16K33 I2C 扫描 */
for (uint8_t a=0x70; a<=0x77; a++) {
    if (i2c_write(i2c, a, &dummy, 0)==0) printf("HT16K33 at 0x%02X\n", a);
}

/* WS2812 单颗验证: 发红色 */
uint8_t red[3] = {0, 255, 0};  /* GRB */
ws2812_send(&ws_hw, red, 1);
```

### 显示验证

| 器件 | 测试项 | 方法 | 预期结果 |
|------|--------|------|----------|
| MAX7219 | 全亮 | 0x0F 0x01 | 64颗全亮 |
| MAX7219 | 字符 | 数字0~9 | 清晰可辨 |
| MAX7219 | 亮度 | 0~15遍历 | 平滑变化 |
| TM1637 | 全段 | 0xFF×4 | 所有段亮 |
| TM1637 | 数字 | 1234 | 正确显示 |
| WS2812 | 单色 | 全红/绿/蓝 | 颜色正确 |
| WS2812 | 级联 | 30颗流水 | 逐颗点亮无丢失 |

### 性能指标

| 指标 | MAX7219 | TM1637 | WS2812(30颗) |
|------|---------|--------|--------------|
| 刷新耗时 | <1ms | ~5ms | ~2.7ms |
| 最大帧率 | >1000fps | ~200fps | ~370fps |
| 全亮电流(8x8) | ~320mA | ~80mA | ~1.8A |

---

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| MAX7219 不亮 | SHUTDOWN 未开启 | 发 0x0C 0x01 开启显示 |
| MAX7219 亮度低 | ISET 电阻过大 | 减小 ISET 电阻；检查 VCC 电压 |
| MAX7219 只亮一半 | SCANLIMIT 设置错 | 设 0x0B 0x07 扫描8位 |
| MAX7219 级联显示错乱 | 数据顺序 | 菊花链最后模块先发数据 |
| TM1637 显示乱码 | 段码表不匹配 | 确认共阴/共阳，调整段码表 |
| TM1637 无显示 | 亮度设为0 | CTRL 命令 bit3=1 开显示 |
| WS2812 颜色错乱 | RGB/GRB 顺序 | WS2812 为 GRB 顺序，非 RGB |
| WS2812 颜色闪烁不稳 | SPI 时序偏差 | 调整 SPI 频率至 4MHz；检查编码 |
| WS2812 首颗颜色异常 | DIN 信号反射 | DIN 串联 470Ω 电阻 |
| WS2812 后半段不亮 | 电源不足 | 电压跌落，加粗电源线/就近供电 |
| WS2812 中途断裂 | 数据线干扰 | 缩短数据线；降低级联数量 |
| WS2812 刷新有撕裂 | 中断打断时序 | 发送期间关中断；或用 DMA+PWM |
| HT16K33 I2C 无响应 | 地址错误 | A0~A2 全GND=0x70，全VCC=0x77 |
| 全亮时电源跌落 | 电流过大 | 使用独立电源；分段供电 |
| 数码管某段不亮 | LED 损坏/虚焊 | 万用表二极管档逐段检测 |
| MAX7219 显示抖动 | 电源不足 | 加强电源滤波；降低亮度 |

## 相关文档

- `skill://mcu-driver-core/templates/driver-template-i2c.c` — I2C 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-spi.c` — SPI 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-pwm.c` — PWM 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
- `skill://mcu-driver-core/guides/pitfalls.md` — 跨类别常见问题与避坑指南
