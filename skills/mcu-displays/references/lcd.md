# LCD 显示屏开发规范

## 1. 概述与选型指南

### 器件简介

LCD（Liquid Crystal Display，液晶显示屏）在 MCU 领域主要分为两类：**字符型 LCD**（内置字库芯片，按行列显示预设字符）和**图形型 LCD**（点阵式，可逐像素控制）。字符型 LCD 以 HD44780 兼容控制器为代表，图形型以 ST7920 为代表。LCD 本身不发光，依赖背光模组，功耗较低但对比度不如 OLED。

### 常见型号对比

| 型号 | 类型 | 尺寸 | 分辨率/容量 | 接口 | 背光 | 驱动IC | 工作电压 | 价格 |
|------|------|------|------------|------|------|--------|----------|------|
| LCD1602 | 字符型 | — | 16列x2行 | 并行4/8bit / I2C转接 | LED | HD44780 | 5V/3.3V | ¥3~8 |
| LCD2004 | 字符型 | — | 20列x4行 | 并行4/8bit / I2C转接 | LED | HD44780 | 5V/3.3V | ¥6~12 |
| LCD12864 | 图形型 | — | 128x64 | 并行/SPI | LED | ST7920 | 5V/3.3V | ¥8~15 |
| LCD1602+PCF8574 | 字符型 | — | 16列x2行 | I2C | LED | HD44780+PCF8574 | 5V | ¥5~10 |
| LCD2004+PCF8574 | 字符型 | — | 20列x4行 | I2C | LED | HD44780+PCF8574 | 5V | ¥8~15 |

### 选型决策树

```
需要图形显示(画点/画线)？ → 是 → ST7920 (128x64 图形LCD)
                            否 →
需要 I2C 接口(省引脚)？ → 是 → LCD1602/2004 + PCF8574 转接板
                         否 →
显示行数 > 2？ → 是 → LCD2004 (20x4)
               否 → LCD1602 (16x2, 最经典, 生态最完善)
需要中文显示？ → 是 → ST7920 (内置中文字库)
               否 → LCD1602/LCD2004 (内置ASCII+日文假名)
```

### 适用场景

- **LCD1602**：设备状态显示、简单参数显示、教学实验、调试输出
- **LCD2004**：需要更多信息量的仪表、菜单系统、多参数监控
- **ST7920**：图形仪表、简单波形显示、中文菜单界面

---

## 2. 硬件设计规范

### 引脚定义

#### LCD1602/LCD2004 并行接口 (16pin)

```
┌──────────────────────────────────────────────────────────┐
│                  LCD1602 / LCD2004                       │
│                                                          │
│  ① VSS  (GND)        ② VDD  (电源 5V/3.3V)              │
│  ③ V0   (对比度调节)  ④ RS   (寄存器选择: 0=命令,1=数据) │
│  ⑤ RW   (读写: 0=写,1=读) ⑥ E    (使能脉冲)              │
│  ⑦ D0   (数据bit0)   ⑧ D1   (数据bit1)                   │
│  ⑨ D2   (数据bit2)   ⑩ D3   (数据bit3)                   │
│  ⑪ D4   (数据bit4)   ⑫ D5   (数据bit5)                   │
│  ⑬ D6   (数据bit6)   ⑭ D7   (数据bit7)                   │
│  ⑮ A/BLA(背光正极)   ⑯ K/BLK(背光负极)                   │
└──────────────────────────────────────────────────────────┘
```

#### PCF8574 I2C 转接板 (LCD1602/2004 + I2C)

```
┌──────────────────────────────────────────────────────────┐
│              PCF8574 I2C 转接板                           │
│                                                          │
│  I2C 侧:                LCD 侧(连接LCD16pin):            │
│  GND ── ①              VSS ── ①                         │
│  VCC ── ②              VDD ── ②                         │
│  SDA ── ③              V0  ── ③ (转接板电位器)           │
│  SCL ── ④              RS  ── ④ (PCF8574 P0)            │
│                        RW  ── ⑤ (PCF8574 P1, 接GND)      │
│                        E   ── ⑥ (PCF8574 P2)             │
│                        D4~D7 ─ ⑪~⑭ (PCF8574 P4~P7)      │
│                        BLA ── ⑮ (PCF8574 P3, 背光控制)    │
│                        BLK ── ⑯ (GND)                    │
│                                                          │
│  地址跳线: A0/A1/A2 → 地址 0x27(全GND) ~ 0x3F(全VCC)     │
└──────────────────────────────────────────────────────────┘
```

#### ST7920 接口 (20pin)

```
┌──────────────────────────────────────────────────────────┐
│                    ST7920 LCD                            │
│                                                          │
│  ① VSS  (GND)        ② VDD  (5V/3.3V)                   │
│  ③ V0   (对比度)     ④ RS/CS (寄存器选择/片选)           │
│  ⑤ RW/SID (读写/数据) ⑥ E/SCLK (使能/时钟)              │
│  ⑦~⑭ D0~D7 (并行数据)                                  │
│  ⑮ PSB  (接口选择: 1=并行, 0=串行SPI)                    │
│  ⑯ NC/CS  (串行模式片选)                                │
│  ⑰ RST  (复位)       ⑱ VOUT (负压输出)                  │
│  ⑲ BLA  (背光+)      ⑳ BLK  (背光-)                     │
└──────────────────────────────────────────────────────────┘
```

### 典型应用电路

#### LCD1602 并行 4bit 模式

```
MCU                    LCD1602
─────                  ───────
PB0 ────────────────── RS (④)
GND ────────────────── RW (⑤, 始终写模式)
PB1 ────────────────── E  (⑥)
PB4 ────────────────── D4 (⑪)
PB5 ────────────────── D5 (⑫)
PB6 ────────────────── D6 (⑬)
PB7 ────────────────── D7 (⑭)

3.3V ──[10kΩ电位器]──┬── V0 (③, 对比度调节)
                      │
                     GND

5V ──[220Ω]── BLA (⑮, 背光限流)
GND ───────── BLK (⑯)

5V ────────── VDD (②)
GND ───────── VSS (①)
```

#### LCD1602 + PCF8574 I2C 转接

```
                    3.3V/5V
                      │
                    4.7kΩ        4.7kΩ
                      │            │
MCU SCL ─────────────┼────────────┼──── PCF8574 SCL
MCU SDA ─────────────┼────────────┼──── PCF8574 SDA
                      │            │
                   3.3V/5V      3.3V/5V

VCC ──────────────── PCF8574 VCC
GND ──────────────── PCF8574 GND
                     PCF8574 P0~P7 → LCD1602 RS/RW/E/D4~D7/BLA
```

#### ST7920 SPI 模式

```
MCU                    ST7920
─────                  ──────
CS  ────────────────── CS  (⑯, 串行片选)
SCK ────────────────── SCLK(⑥, 串行时钟)
MOSI────────────────── SID (⑤, 串行数据)

GND ────────────────── PSB (⑮, 接地选串行模式)
MCU GPIO ──────────── RST (⑰, 可选)

5V ──[10kΩ电位器]── V0  (③, 对比度)
5V ──[220Ω]─────── BLA (⑲)
GND ─────────────── BLK (⑳)
5V ──────────────── VDD (②)
GND ─────────────── VSS (①)
```

### PCB 设计要点

- **对比度电位器**：V0 引脚必须接 10kΩ 电位器分压，不可悬空（否则对比度不可控）
- **背光限流电阻**：LED 背光典型正向电压 4.2V，电流需限制在 60~120mA，串接 220Ω~100Ω 电阻
- **去耦电容**：VDD 旁放 100nF + 10μF
- **5V/3.3V 兼容**：HD44780 有 5V 和 3.3V 版本，确认版本后匹配供电；5V LCD 接 3.3V MCU 需电平转换
- **PSB 引脚**：ST7920 并行模式接 VDD，SPI 模式接 GND，不可悬空
- **RW 引脚**：如不需要读 LCD 状态，RW 直接接 GND（节省一根线）
- **背光控制**：如需软件控制背光，用三极管驱动（LCD 背光电流可能超过 MCU 引脚驱动能力）

### 电气参数

| 参数 | LCD1602 | LCD2004 | ST7920 | 单位 |
|------|---------|---------|--------|------|
| VDD | 4.5~5.5 / 3.0~3.6 | 同左 | 4.5~5.5 / 3.0~3.6 | V |
| 工作电流(无背光) | ~1.5 | ~2.0 | ~3.0 | mA |
| 背光电流 | ~60 | ~80 | ~120 | mA |
| 使能脉冲宽度 | ≥450 | ≥450 | ≥1000 | ns |
| 指令执行时间 | ~37(清屏1.53ms) | 同左 | ~72(清屏1.6ms) | μs |

---

## 3. 驱动开发规范

### 统一接口定义

```c
typedef enum {
    LCD_DEV_1602_PARALLEL,
    LCD_DEV_1602_I2C,
    LCD_DEV_2004_PARALLEL,
    LCD_DEV_2004_I2C,
    LCD_DEV_ST7920_PARALLEL,
    LCD_DEV_ST7920_SPI,
} lcd_device_t;

typedef enum {
    LCD_OK = 0,
    LCD_ERR_INIT,
    LCD_ERR_TIMEOUT,
    LCD_ERR_PARAM,
    LCD_ERR_NOT_FOUND,
} lcd_status_t;

typedef struct {
    lcd_device_t dev;
    void    *hw_ctx;
    uint8_t  cols;
    uint8_t  rows;
    uint8_t  i2c_addr;   /* I2C 模式有效 */
} lcd_handle_t;

/* 字符型 LCD API */
lcd_status_t lcd_init(lcd_handle_t *h);
lcd_status_t lcd_clear(lcd_handle_t *h);
lcd_status_t lcd_set_cursor(lcd_handle_t *h, uint8_t col, uint8_t row);
lcd_status_t lcd_print(lcd_handle_t *h, const char *str);
lcd_status_t lcd_create_char(lcd_handle_t *h, uint8_t location, const uint8_t charmap[8]);
lcd_status_t lcd_backlight(lcd_handle_t *h, bool on);

/* ST7920 图形 API */
lcd_status_t lcd_st7920_draw_pixel(lcd_handle_t *h, int16_t x, int16_t y, bool on);
lcd_status_t lcd_st7920_flush(lcd_handle_t *h);
```

### LCD1602 并行 4bit 驱动

```c
/* HD44780 命令定义 */
#define HD44780_CMD_CLEAR         0x01
#define HD44780_CMD_HOME          0x02
#define HD44780_CMD_ENTRY_MODE    0x06   /* 光标右移, 画面不动 */
#define HD44780_CMD_DISPLAY_ON    0x0C   /* 显示开, 光标关, 闪烁关 */
#define HD44780_CMD_DISPLAY_OFF   0x08
#define HD44780_CMD_FUNC_4BIT_2L  0x28   /* 4bit, 2行, 5x8点阵 */
#define HD44780_CMD_FUNC_8BIT_2L  0x38   /* 8bit, 2行, 5x8点阵 */
#define HD44780_CMD_SET_DDRAM     0x80   /* 设置 DDRAM 地址 */

/* 行首 DDRAM 地址映射 */
static const uint8_t row_offsets[] = {0x00, 0x40, 0x14, 0x54};

/* 平台适配层 */
typedef struct {
    void (*write_rs)(int val);
    void (*write_e)(int val);
    void (*write_d4_d7)(uint8_t nibble);   /* 写高4位 */
    void (*delay_us)(uint32_t us);
    void (*delay_ms)(uint32_t ms);
} lcd_parallel_hw_t;

static void lcd_write4(lcd_parallel_hw_t *hw, uint8_t value) {
    hw->write_d4_d7(value & 0x0F);
    hw->write_e(1);
    hw->delay_us(1);
    hw->write_e(0);
    hw->delay_us(50);   /* 大多数指令需 ≥37μs */
}

static void lcd_write_byte(lcd_parallel_hw_t *hw, uint8_t val, uint8_t rs) {
    hw->write_rs(rs);
    lcd_write4(hw, val >> 4);      /* 先写高4位 */
    lcd_write4(hw, val & 0x0F);    /* 再写低4位 */
}

static void lcd_cmd(lcd_handle_t *h, uint8_t cmd) {
    lcd_parallel_hw_t *hw = (lcd_parallel_hw_t *)h->hw_ctx;
    lcd_write_byte(hw, cmd, 0);
    if (cmd == HD44780_CMD_CLEAR || cmd == HD44780_CMD_HOME)
        hw->delay_ms(2);   /* 清屏/归位需 1.53ms */
}

static void lcd_data(lcd_handle_t *h, uint8_t data) {
    lcd_parallel_hw_t *hw = (lcd_parallel_hw_t *)h->hw_ctx;
    lcd_write_byte(hw, data, 1);
}

lcd_status_t lcd_init(lcd_handle_t *h) {
    lcd_parallel_hw_t *hw = (lcd_parallel_hw_t *)h->hw_ctx;
    hw->delay_ms(50);   /* 上电等待 > 40ms */

    /* 4bit 模式初始化序列 */
    hw->write_rs(0);
    lcd_write4(hw, 0x03);  hw->delay_ms(5);   /* 第1次 */
    lcd_write4(hw, 0x03);  hw->delay_us(150); /* 第2次 */
    lcd_write4(hw, 0x03);  hw->delay_us(150); /* 第3次 */
    lcd_write4(hw, 0x02);  hw->delay_us(150); /* 切换到4bit模式 */

    lcd_cmd(h, HD44780_CMD_FUNC_4BIT_2L);   /* 4bit, 2行 */
    lcd_cmd(h, HD44780_CMD_DISPLAY_OFF);
    lcd_cmd(h, HD44780_CMD_CLEAR);
    lcd_cmd(h, HD44780_CMD_ENTRY_MODE);
    lcd_cmd(h, HD44780_CMD_DISPLAY_ON);
    return LCD_OK;
}

lcd_status_t lcd_clear(lcd_handle_t *h) {
    lcd_cmd(h, HD44780_CMD_CLEAR);
    return LCD_OK;
}

lcd_status_t lcd_set_cursor(lcd_handle_t *h, uint8_t col, uint8_t row) {
    if (row >= h->rows) row = h->rows - 1;
    lcd_cmd(h, HD44780_CMD_SET_DDRAM | (col + row_offsets[row]));
    return LCD_OK;
}

lcd_status_t lcd_print(lcd_handle_t *h, const char *str) {
    while (*str) {
        lcd_data(h, (uint8_t)*str++);
    }
    return LCD_OK;
}

/* 自定义字符 (CGRAM, 8个自定义字符槽位 0~7) */
lcd_status_t lcd_create_char(lcd_handle_t *h, uint8_t location, const uint8_t charmap[8]) {
    if (location > 7) return LCD_ERR_PARAM;
    lcd_cmd(h, 0x40 | (location << 3));   /* 设置 CGRAM 地址 */
    for (int i = 0; i < 8; i++) {
        lcd_data(h, charmap[i]);
    }
    return LCD_OK;
}
```

### PCF8574 I2C 转接驱动

```c
/* PCF8574 引脚映射 (常见转接板):
   P0=RS, P1=RW, P2=E, P3=背光, P4=D4, P5=D5, P6=D6, P7=D7 */

#define PCF8574_BL_BIT  0x08
#define PCF8574_E_BIT   0x04
#define PCF8574_RW_BIT  0x02
#define PCF8574_RS_BIT  0x01

typedef struct {
    void *i2c;
    uint8_t addr;
    uint8_t backlight;   /* 0x08=开, 0x00=关 */
    uint8_t last_output;
} lcd_i2c_hw_t;

static void pcf8574_write(lcd_i2c_hw_t *hw, uint8_t val) {
    hw->last_output = val;
    i2c_write(hw->i2c, hw->addr, &val, 1);
}

static void lcd_i2c_write4(lcd_i2c_hw_t *hw, uint8_t nibble, uint8_t rs) {
    uint8_t hi = (nibble << 4) | rs | hw->backlight;
    /* E 脉冲: 高→低 */
    pcf8574_write(hw, hi | PCF8574_E_BIT);   /* E=1 */
    hw->delay_us(1);
    pcf8574_write(hw, hi & ~PCF8574_E_BIT);  /* E=0 */
    hw->delay_us(50);
}

static void lcd_i2c_write_byte(lcd_i2c_hw_t *hw, uint8_t val, uint8_t rs) {
    lcd_i2c_write4(hw, val >> 4, rs ? PCF8574_RS_BIT : 0);
    lcd_i2c_write4(hw, val & 0x0F, rs ? PCF8574_RS_BIT : 0);
}

lcd_status_t lcd_i2c_init(lcd_handle_t *h) {
    lcd_i2c_hw_t *hw = (lcd_i2c_hw_t *)h->hw_ctx;
    hw->backlight = PCF8574_BL_BIT;

    hw->delay_ms(50);
    lcd_i2c_write4(hw, 0x03, 0);  hw->delay_ms(5);
    lcd_i2c_write4(hw, 0x03, 0);  hw->delay_us(150);
    lcd_i2c_write4(hw, 0x03, 0);  hw->delay_us(150);
    lcd_i2c_write4(hw, 0x02, 0);  hw->delay_us(150);

    lcd_i2c_write_byte(hw, HD44780_CMD_FUNC_4BIT_2L, 0);
    lcd_i2c_write_byte(hw, HD44780_CMD_DISPLAY_OFF, 0);
    lcd_i2c_write_byte(hw, HD44780_CMD_CLEAR, 0);
    hw->delay_ms(2);
    lcd_i2c_write_byte(hw, HD44780_CMD_ENTRY_MODE, 0);
    lcd_i2c_write_byte(hw, HD44780_CMD_DISPLAY_ON, 0);
    return LCD_OK;
}

lcd_status_t lcd_backlight(lcd_handle_t *h, bool on) {
    lcd_i2c_hw_t *hw = (lcd_i2c_hw_t *)h->hw_ctx;
    hw->backlight = on ? PCF8574_BL_BIT : 0;
    pcf8574_write(hw, hw->last_output & ~PCF8574_BL_BIT | hw->backlight);
    return LCD_OK;
}
```

### ST7920 图形显示驱动 (SPI)

```c
/* ST7920 指令 */
#define ST7920_CMD_BASIC        0x30   /* 基本指令集 */
#define ST7920_CMD_EXTENDED     0x34   /* 扩展指令集 */
#define ST7920_CMD_CLEAR        0x01
#define ST7920_CMD_HOME         0x02
#define ST7920_CMD_DISPLAY_ON   0x0C
#define ST7920_CMD_GDRAM_ON     0x36   /* 扩展指令集 + 图形显示开 */

/* ST7920 串行通信: 每次传输 3 字节
   字节1: 0x1F (同步字节, RW=0, RS=0)
   字节2: 高4位命令/数据 + 高4位0
   字节3: 低4位命令/数据 + 低4位0 */

typedef struct {
    void (*spi_write)(const uint8_t *data, uint16_t len);
    void (*cs_write)(int val);
    void (*delay_us)(uint32_t us);
    void (*delay_ms)(uint32_t ms);
} st7920_hw_t;

static void st7920_write_cmd(st7920_hw_t *hw, uint8_t cmd) {
    hw->cs_write(1);
    uint8_t buf[3] = {
        0xF8,                    /* 11111 RW(0) RS(0) 0 → 写命令 */
        cmd & 0xF0,              /* 高4位 */
        (cmd << 4) & 0xF0,       /* 低4位 */
    };
    hw->spi_write(buf, 3);
    hw->cs_write(0);
    hw->delay_us(72);
}

static void st7920_write_data(st7920_hw_t *hw, uint8_t data) {
    hw->cs_write(1);
    uint8_t buf[3] = {
        0xFA,                    /* 11111 RW(0) RS(1) 0 → 写数据 */
        data & 0xF0,
        (data << 4) & 0xF0,
    };
    hw->spi_write(buf, 3);
    hw->cs_write(0);
    hw->delay_us(72);
}

lcd_status_t lcd_st7920_init(lcd_handle_t *h) {
    st7920_hw_t *hw = (st7920_hw_t *)h->hw_ctx;

    hw->delay_ms(40);
    st7920_write_cmd(hw, ST7920_CMD_BASIC);     /* 基本指令集 */
    hw->delay_ms(1);
    st7920_write_cmd(hw, ST7920_CMD_BASIC);
    st7920_write_cmd(hw, ST7920_CMD_CLEAR);
    hw->delay_ms(2);
    st7920_write_cmd(hw, ST7920_CMD_DISPLAY_ON);
    st7920_write_cmd(hw, ST7920_CMD_HOME);
    hw->delay_ms(2);

    /* 切换到扩展指令集, 开启图形显示 */
    st7920_write_cmd(hw, ST7920_CMD_EXTENDED);
    st7920_write_cmd(hw, ST7920_CMD_GDRAM_ON);
    return LCD_OK;
}

/* ST7920 GDRAM 布局: 128x64, 每16像素为1行(2字节), 共32行(行0和行16交替)
   实际地址: 偶数行 y=0,2,4... → GDRAM行 y/2
            奇数行 y=1,3,5... → GDRAM行 y/2 + 16 */

static uint8_t st7920_vram[64][16];  /* 64行 x 16字节(128像素) */

lcd_status_t lcd_st7920_draw_pixel(lcd_handle_t *h, int16_t x, int16_t y, bool on) {
    if (x < 0 || x >= 128 || y < 0 || y >= 64) return LCD_ERR_PARAM;
    uint8_t byte_idx = x / 8;
    uint8_t bit_idx  = 7 - (x % 8);
    if (on)
        st7920_vram[y][byte_idx] |= (1 << bit_idx);
    else
        st7920_vram[y][byte_idx] &= ~(1 << bit_idx);
    return LCD_OK;
}

lcd_status_t lcd_st7920_flush(lcd_handle_t *h) {
    st7920_hw_t *hw = (st7920_hw_t *)h->hw_ctx;

    for (uint8_t y = 0; y < 32; y++) {
        /* 设置 GDRAM 垂直地址 */
        st7920_write_cmd(hw, 0x80 | y);
        /* 设置 GDRAM 水平地址 */
        st7920_write_cmd(hw, 0x80 | 0);

        /* 写入偶数行 (y) */
        for (uint8_t x = 0; x < 16; x++) {
            st7920_write_data(hw, st7920_vram[y * 2][x]);
        }
        /* 写入奇数行 (y + 32 映射到物理行 y+16) */
        for (uint8_t x = 0; x < 16; x++) {
            st7920_write_data(hw, st7920_vram[y * 2 + 1][x]);
        }
    }
    return LCD_OK;
}
```

### 使用示例

```c
/* LCD1602 I2C 示例 */
lcd_handle_t lcd = {
    .dev      = LCD_DEV_1602_I2C,
    .cols     = 16,
    .rows     = 2,
    .i2c_addr = 0x27,
    .hw_ctx   = &i2c_hw,
};

void main(void) {
    lcd_i2c_init(&lcd);
    lcd_set_cursor(&lcd, 0, 0);
    lcd_print(&lcd, "Hello World!");
    lcd_set_cursor(&lcd, 0, 1);
    lcd_print(&lcd, "LCD1602 I2C");

    /* 自定义心形字符 */
    uint8_t heart[8] = {0x00, 0x0A, 0x1F, 0x1F, 0x0E, 0x04, 0x00, 0x00};
    lcd_create_char(&lcd, 0, heart);
    lcd_set_cursor(&lcd, 12, 0);
    lcd_data(&lcd, 0);   /* 显示自定义字符 */

    while (1) {
        lcd_set_cursor(&lcd, 0, 1);
        char buf[17];
        snprintf(buf, sizeof(buf), "Count: %5d", counter++);
        lcd_print(&lcd, buf);
        HAL_Delay(1000);
    }
}

/* ST7920 图形示例 */
lcd_handle_t lcd_st = {
    .dev    = LCD_DEV_ST7920_SPI,
    .hw_ctx = &st7920_hw,
};

void st7920_demo(void) {
    lcd_st7920_init(&lcd_st);
    /* 画边框 */
    for (int x = 0; x < 128; x++) {
        lcd_st7920_draw_pixel(&lcd_st, x, 0, true);
        lcd_st7920_draw_pixel(&lcd_st, x, 63, true);
    }
    for (int y = 0; y < 64; y++) {
        lcd_st7920_draw_pixel(&lcd_st, 0, y, true);
        lcd_st7920_draw_pixel(&lcd_st, 127, y, true);
    }
    /* 画对角线 */
    for (int i = 0; i < 64; i++) {
        lcd_st7920_draw_pixel(&lcd_st, i * 2, i, true);
    }
    lcd_st7920_flush(&lcd_st);
}
```

---

## 4. 调试与测试规范

### 硬件验证清单

- [ ] 万用表确认 VDD/GND 电压正确（5V 或 3.3V 版本）
- [ ] V0 对比度电位器已连接且可调节
- [ ] 背光限流电阻阻值正确，背光正常亮起
- [ ] 并行模式：确认 RS/RW/E/D4~D7 全部连接对应 MCU 引脚
- [ ] I2C 模式：SCL/SDA 上拉电阻 4.7kΩ 已接
- [ ] ST7920：PSB 引脚已按模式接 VDD(并行) 或 GND(SPI)
- [ ] 100nF 去耦电容已贴装

### 通信验证

```c
/* I2C 地址扫描确认 PCF8574 */
void lcd_scan_i2c(void *i2c_ctx) {
    /* PCF8574 地址范围: 0x20~0x27 (PCF8574) 或 0x38~0x3F (PCF8574A) */
    for (uint8_t addr = 0x20; addr <= 0x3F; addr++) {
        uint8_t dummy = 0;
        if (i2c_write(i2c_ctx, addr, &dummy, 0) == 0) {
            printf("Found I2C device at 0x%02X\n", addr);
        }
    }
}
/* 预期: 0x27 (最常见) 或 0x3F */
```

- **并行验证**：示波器检查 E 引脚脉冲宽度 ≥ 450ns
- **ST7920 SPI**：逻辑分析仪抓取 3 字节帧格式，核对同步字节 0xF8/0xFA
- **对比度**：调节电位器，确认屏幕从全白到清晰显示

### 显示验证

| 测试项 | 方法 | 预期结果 |
|--------|------|----------|
| 全屏字符 | 填满所有行列 | 每个位置字符清晰可读 |
| 光标移动 | 逐行写入 | 光标位置正确 |
| 自定义字符 | 写入 CGRAM 后显示 | 自定义图形正确显示 |
| 清屏 | 发送 0x01 | 屏幕全清，光标回原点 |
| 背光控制 | 软件开关背光(I2C模式) | 背光正常开关 |
| ST7920 图形 | 画点/线/矩形 | 图形正确无错位 |

### 性能指标

| 指标 | LCD1602 并行 | LCD1602 I2C | ST7920 SPI |
|------|-------------|-------------|------------|
| 单字符写入 | ~100μs | ~200μs | ~200μs |
| 清屏 | ~1.6ms | ~1.6ms | ~1.6ms |
| 全屏刷新(ST7920) | — | — | ~50ms |
| 初始化 | ~50ms | ~50ms | ~50ms |

---

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 屏幕只亮不显示字 | 对比度未调 | 调节 V0 电位器，不可悬空 |
| 只显示一排黑块 | 初始化未完成 | 检查 4bit 初始化序列 (0x03 三次再 0x02) |
| 显示乱码/错位 | DDRAM 地址映射错误 | 核对行偏移表: 行0=0x00, 行1=0x40 |
| I2C 找不到设备 | 地址错误 | PCF8574=0x20~0x27, PCF8574A=0x38~0x3F |
| I2C 通信时好时坏 | PCF8574 背光驱动不足 | 背光电流大时用三极管驱动，PCF8574 仅做控制 |
| 4bit 模式显示乱 | 高低4位顺序错误 | 确认先写高4位再写低4位 |
| 5V LCD 接 3.3V MCU | 电平不匹配 | 使用电平转换或选 3.3V 版本 LCD |
| 清屏后需等待 | 指令执行时间 | 清屏/归位需等 1.53ms，不可立即写下一条 |
| ST7920 图形错位 | GDRAM 地址交替 | 偶数行写地址 y，奇数行写地址 y+16 |
| ST7920 中英文混排乱码 | 指令集切换冲突 | 图形模式下文字字库不可用，需自行渲染中文字模 |
| 第二行不显示 | 行地址计算错误 | LCD1602 第二行起始地址为 0x40 不是 0x10 |
| 背光太暗/太亮 | 限流电阻不合适 | 5V 背光串 220Ω；调节电阻控制亮度 |
| 自定义字符不显示 | CGRAM 地址计算错 | CGRAM 地址 = 0x40 + (char_idx * 8) |
| ST7920 SPI 无显示 | PSB 悬空 | PSB 必须接 GND 选择串行模式 |
| 字符左右镜像 | 显示方向设置错 | 检查 entry mode (0x06=右移, 0x04=左移) |

## 相关文档

- `skill://mcu-driver-core/templates/driver-template-i2c.c` — I2C 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-spi.c` — SPI 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
- `skill://mcu-driver-core/guides/pitfalls.md` — 跨类别常见问题与避坑指南
