# OLED 显示屏开发规范

## 1. 概述与选型指南

### 器件简介

OLED（Organic Light-Emitting Diode，有机发光二极管）显示屏是一种自发光显示器件，无需背光，对比度极高，功耗低，响应速度快。MCU 领域常用的单色 OLED 基于 SSD/SH 系列驱动 IC，通过 I2C 或 SPI 接口与主控通信，显存按页（Page）方式组织。

### 常见型号对比

| 型号 | 尺寸 | 分辨率 | 接口 | 颜色 | 驱动IC | 工作电压 | 功耗(全亮) | 价格 |
|------|------|--------|------|------|--------|----------|------------|------|
| SSD1306 | 0.96" | 128x64 | I2C/SPI | 单色(白/蓝/黄蓝) | SSD1306 | 3.3V | ~20mA | ¥5~10 |
| SH1106 | 1.3" | 128x64 | I2C/SPI | 单色(白) | SH1106 | 3.3V | ~25mA | ¥8~15 |
| SSD1309 | 2.42" | 128x64 | SPI | 单色(白) | SSD1309 | 3.3V | ~40mA | ¥15~25 |
| SSD1306 | 0.91" | 128x32 | I2C | 单色(白) | SSD1306 | 3.3V | ~12mA | ¥4~8 |
| SH1107 | 1.5" | 128x128 | I2C/SPI | 单色(白) | SH1107 | 3.3V | ~35mA | ¥15~25 |

### 选型决策树

```
需要彩色显示？ → 是 → 请转 TFT 规范文档
                否 →
需要大尺寸(>1.5")？ → 是 → SSD1309 (2.42", SPI)
                     否 →
需要低引脚数(I2C)？ → 是 → 屏幕尺寸 ≤ 0.96"？ → 是 → SSD1306 I2C
                                       否 → SH1106 I2C (1.3")
需要高刷新率？ → 是 → 选 SPI 接口版本 (I2C 带宽受限)
需要超低功耗？ → 是 → 请转 e-paper 规范文档
通用推荐 → SSD1306 0.96" I2C (性价比最高, 生态最完善)
```

### 适用场景

- **SSD1306**：仪表盘状态显示、温湿度显示、小型菜单、IoT 设备状态
- **SH1106**：需要稍大显示面积的手持设备、调试终端
- **SSD1309**：工业仪表、需要远距离可视的设备状态屏

---

## 2. 硬件设计规范

### 引脚定义

#### SSD1306 I2C 模式 (6pin/4pin)

```
┌──────────────────────────────────────────┐
│              SSD1306 OLED                │
│                                          │
│  GND ──①                    VCC ──②     │
│  SCL ──③                    SDA ──④     │
│                                          │
│  (4pin 模块仅 GND/VCC/SCL/SDA)           │
│  (6pin 模块额外有 RES/DC, I2C下DC接地)    │
└──────────────────────────────────────────┘
```

#### SSD1306 SPI 模式 (7pin)

```
┌──────────────────────────────────────────┐
│              SSD1306 OLED (SPI)          │
│                                          │
│  GND ──①                    VCC ──②     │
│  D0  ──③ (SCK)              D1  ──④ (MOSI)│
│  RES ──⑤ (Reset)            DC  ──⑥ (Data/Command)│
│  CS  ──⑦ (Chip Select)                   │
└──────────────────────────────────────────┘
```

### 典型应用电路

#### I2C 接线

```
                    3.3V
                     │
                    4.7kΩ          4.7kΩ
                     │              │
MCU SCL ────────────┼──────────────┼──── OLED SCL
MCU SDA ────────────┼──────────────┼──── OLED SDA
                     │              │
                    3.3V           3.3V
                     
MCU GPIO ────────────── 100nF ──── OLED RES (可选, 可接硬复位)

3.3V ────────────────────── OLED VCC
GND  ────────────────────── OLED GND
```

**I2C 地址说明**：
- SA0 引脚(D/C pin in I2C mode) 接 GND → 地址 `0x3C` (7-bit)
- SA0 引脚 接 VCC → 地址 `0x3D` (7-bit)
- 大多数模块默认 `0x3C`

#### SPI 接线

```
MCU SCK  ──────────── OLED D0 (CLK)
MCU MOSI ──────────── OLED D1 (DATA)
MCU GPIO ──────────── OLED DC  (0=命令, 1=数据)
MCU GPIO ──────────── OLED RES (复位)
MCU CS   ──────────── OLED CS  (片选)

3.3V ──────────────── OLED VCC
GND  ──────────────── OLED GND
```

### PCB 设计要点

- **上拉电阻**：I2C 模式 SCL/SDA 各接 4.7kΩ 上拉至 3.3V；总线电容大时降至 2.2kΩ
- **去耦电容**：VCC 旁放 100nF + 10μF，紧贴引脚
- **RES 引脚**：建议连接 MCU GPIO 实现硬复位；如引脚紧张可接 RC 复位电路 (10kΩ + 100nF)
- **电平匹配**：OLED 为 3.3V 器件，5V MCU 需电平转换（TXS0108E 或分立 MOS 管）
- **走线**：I2C 总线长度建议 < 30cm；SPI 时钟 > 4MHz 时走线需等长
- **GND**：确保大面积 GND 铺铜，降低回流阻抗

### 电气参数

| 参数 | 最小值 | 典型值 | 最大值 | 单位 |
|------|--------|--------|--------|------|
| VCC 供电 | 1.65 | 3.3 | 3.6 | V |
| I2C 时钟 | 10 | 400 | 1000 | kHz |
| SPI 时钟 | — | 4000 | 10000 | kHz |
| 工作电流(全亮) | — | 20 | 30 | mA |
| 工作温度 | -40 | 25 | 85 | °C |

---

## 3. 驱动开发规范

### 显存组织方式

SSD1306 显存按页组织，128x64 屏幕共 8 页，每页 128 列：

```
┌─────────────────────────────────────────────────┐
│  Page 0  │  Col 0  Col 1  Col 2 ... Col 127     │
│  Page 1  │  ...                                  │
│  Page 2  │  ...                                  │
│  ...     │  ...                                  │
│  Page 7  │  ...                                  │
└─────────────────────────────────────────────────┘

每列 8 个像素对应 1 字节 (D0=上, D7=下)
全屏显存 = 128 * 8 = 1024 字节
```

**关键区别**：
- **SSD1306**：列地址在页内自动递增，跨页不自动切换（页地址模式）或自动切换（水平/垂直地址模式）
- **SH1106**：列地址每页偏移 2 列（列起始地址为 0x02 而非 0x00），驱动需做偏移补偿

### 统一接口定义

```c
typedef enum {
    OLED_DEV_SSD1306_128x64,
    OLED_DEV_SSD1306_128x32,
    OLED_DEV_SH1106_128x64,
    OLED_DEV_SSD1309_128x64,
} oled_device_t;

typedef enum {
    OLED_IF_I2C,
    OLED_IF_SPI,
} oled_interface_t;

typedef enum {
    OLED_OK = 0,
    OLED_ERR_INIT,
    OLED_ERR_TIMEOUT,
    OLED_ERR_PARAM,
    OLED_ERR_NOT_FOUND,
} oled_status_t;

typedef struct {
    oled_device_t  dev;
    oled_interface_t iface;
    uint8_t  i2c_addr;       /* I2C 7-bit 地址 */
    void    *hw_ctx;          /* 平台硬件上下文 */
    uint8_t  *vram;           /* 显存缓冲区 (1024 bytes for 128x64) */
    uint8_t  width;
    uint8_t  height;
} oled_handle_t;

/* 公共 API */
oled_status_t oled_init(oled_handle_t *h);
oled_status_t oled_display_on(oled_handle_t *h);
oled_status_t oled_display_off(oled_handle_t *h);
oled_status_t oled_flush(oled_handle_t *h);           /* 将 VRAM 刷新到屏幕 */
void          oled_clear(oled_handle_t *h);
void          oled_set_pixel(oled_handle_t *h, int16_t x, int16_t y, bool on);
void          oled_draw_char(oled_handle_t *h, int16_t x, int16_t y, char ch, const uint8_t *font);
void          oled_draw_string(oled_handle_t *h, int16_t x, int16_t y, const char *str, const uint8_t *font);
void          oled_draw_line(oled_handle_t *h, int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void          oled_draw_rect(oled_handle_t *h, int16_t x, int16_t y, int16_t w, int16_t h);
void          oled_draw_circle(oled_handle_t *h, int16_t cx, int16_t cy, int16_t r);
oled_status_t oled_set_contrast(oled_handle_t *h, uint8_t val);
oled_status_t oled_set_invert(oled_handle_t *h, bool inv);
```

### SSD1306 I2C 初始化序列

```c
/* 平台适配层 - 需根据目标平台实现 */
static oled_status_t oled_write_cmd(oled_handle_t *h, uint8_t cmd) {
    uint8_t buf[2] = {0x00, cmd};   /* Co=0, D/C#=0 → 命令 */
    return (i2c_write(h->hw_ctx, h->i2c_addr, buf, 2) == 0) ? OLED_OK : OLED_ERR_TIMEOUT;
}

static oled_status_t oled_write_data(oled_handle_t *h, const uint8_t *data, uint16_t len) {
    /* 批量写数据: 首字节 0x40 表示后续为数据 */
    uint8_t buf[len + 1];
    buf[0] = 0x40;  /* Co=0, D/C#=1 → 数据 */
    memcpy(&buf[1], data, len);
    return (i2c_write(h->hw_ctx, h->i2c_addr, buf, len + 1) == 0) ? OLED_OK : OLED_ERR_TIMEOUT;
}

oled_status_t oled_init(oled_handle_t *h) {
    /* 分配显存 */
    h->vram = malloc(h->width * h->height / 8);
    if (!h->vram) return OLED_ERR_INIT;
    memset(h->vram, 0, h->width * h->height / 8);

    /* SSD1306 标准初始化序列 */
    const uint8_t init_cmds[] = {
        0xAE,             /* Display OFF */
        0xD5, 0x80,       /* Set display clock divide ratio/oscillator: 0x80 */
        0xA8, 0x3F,       /* Set multiplex ratio: 64 (128x64) / 0x1F (128x32) */
        0xD3, 0x00,       /* Set display offset: 0 */
        0x40,             /* Set start line: 0 */
        0x8D, 0x14,       /* Enable charge pump regulator */
        0x20, 0x00,       /* Memory addressing mode: horizontal */
        0xA1,             /* Segment remap: col127 = seg0 */
        0xC8,             /* COM output scan direction: remapped */
        0xDA, 0x12,       /* COM pins config: 0x12 (128x64) / 0x02 (128x32) */
        0x81, 0xCF,       /* Set contrast: 0xCF */
        0xD9, 0xF1,       /* Set pre-charge period: 0xF1 */
        0xDB, 0x40,       /* Set VCOMH deselect level: 0x40 */
        0xA4,             /* Display resume → RAM content */
        0xA6,             /* Normal display (not inverted) */
        0xAF,             /* Display ON */
    };

    for (int i = 0; i < sizeof(init_cmds); i++) {
        if (oled_write_cmd(h, init_cmds[i]) != OLED_OK)
            return OLED_ERR_INIT;
    }

    oled_clear(h);
    return oled_flush(h);
}
```

### 显存刷新（水平地址模式）

```c
/* 水平地址模式: 列地址自动递增, 到末尾自动跳到下一页 */
oled_status_t oled_flush(oled_handle_t *h) {
    uint8_t pages = h->height / 8;

    /* 设置列地址范围 0~127 */
    oled_write_cmd(h, 0x21);
    oled_write_cmd(h, 0x00);
    oled_write_cmd(h, h->width - 1);

    /* 设置页地址范围 0~7 */
    oled_write_cmd(h, 0x22);
    oled_write_cmd(h, 0x00);
    oled_write_cmd(h, pages - 1);

    /* SH1106 需要按页分别写入, 列地址不自动跨页 */
    if (h->dev == OLED_DEV_SH1106_128x64) {
        for (uint8_t p = 0; p < pages; p++) {
            oled_write_cmd(h, 0xB0 + p);           /* 设置页地址 */
            oled_write_cmd(h, 0x02);               /* 设置列低地址 (SH1106 偏移 2) */
            oled_write_cmd(h, 0x10);               /* 设置列高地址 */
            oled_write_data(h, &h->vram[p * h->width], h->width);
        }
    } else {
        /* SSD1306 水平模式: 一次性写入全部显存 */
        oled_write_data(h, h->vram, h->width * pages);
    }

    return OLED_OK;
}
```

### 像素操作

```c
void oled_set_pixel(oled_handle_t *h, int16_t x, int16_t y, bool on) {
    if (x < 0 || x >= h->width || y < 0 || y >= h->height) return;
    uint16_t idx = x + (y / 8) * h->width;
    if (on)
        h->vram[idx] |= (1 << (y & 7));
    else
        h->vram[idx] &= ~(1 << (y & 7));
}
```

### 字符显示（6x8 字体）

```c
/* 6x8 ASCII 字体 (32~127), 每字符 6 字节 */
extern const uint8_t oled_font_6x8[][6];

void oled_draw_char(oled_handle_t *h, int16_t x, int16_t y, char ch, const uint8_t *font) {
    if (ch < 32 || ch > 127) ch = '?';
    const uint8_t *glyph = &oled_font_6x8[ch - 32][0];

    for (uint8_t col = 0; col < 6; col++) {
        uint8_t line = glyph[col];
        for (uint8_t row = 0; row < 8; row++) {
            oled_set_pixel(h, x + col, y + row, (line >> row) & 1);
        }
    }
}

void oled_draw_string(oled_handle_t *h, int16_t x, int16_t y,
                      const char *str, const uint8_t *font) {
    while (*str) {
        oled_draw_char(h, x, y, *str, font);
        x += 6;   /* 字宽 6px */
        if (x + 6 > h->width) { x = 0; y += 8; }
        str++;
    }
}
```

### 图形绘制

#### 画线（Bresenham 算法）

```c
void oled_draw_line(oled_handle_t *h, int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    int16_t dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int16_t dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int16_t err = dx + dy, e2;

    for (;;) {
        oled_set_pixel(h, x0, y0, true);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
```

#### 画矩形

```c
void oled_draw_rect(oled_handle_t *h, int16_t x, int16_t y, int16_t w, int16_t hgt) {
    oled_draw_line(h, x, y, x + w - 1, y);           /* 上 */
    oled_draw_line(h, x, y + hgt - 1, x + w - 1, y + hgt - 1);  /* 下 */
    oled_draw_line(h, x, y, x, y + hgt - 1);          /* 左 */
    oled_draw_line(h, x + w - 1, y, x + w - 1, y + hgt - 1);    /* 右 */
}
```

#### 画圆（中点画圆算法）

```c
void oled_draw_circle(oled_handle_t *h, int16_t cx, int16_t cy, int16_t r) {
    if (r <= 0) return;
    int16_t x = r, y = 0, err = 0;

    while (x >= y) {
        oled_set_pixel(h, cx + x, cy + y, true);
        oled_set_pixel(h, cx + y, cy + x, true);
        oled_set_pixel(h, cx - y, cy + x, true);
        oled_set_pixel(h, cx - x, cy + y, true);
        oled_set_pixel(h, cx - x, cy - y, true);
        oled_set_pixel(h, cx - y, cy - x, true);
        oled_set_pixel(h, cx + y, cy - x, true);
        oled_set_pixel(h, cx + x, cy - y, true);

        if (err <= 0) { y += 1; err += 2 * y + 1; }
        if (err > 0)  { x -= 1; err -= 2 * x + 1; }
    }
}
```

### SPI 模式驱动适配

```c
/* SPI 模式: DC 引脚区分命令/数据, CS 控制片选 */
static oled_status_t oled_spi_write_cmd(oled_handle_t *h, uint8_t cmd) {
    gpio_write(h->dc_pin, 0);       /* DC=0 → 命令 */
    gpio_write(h->cs_pin, 0);       /* CS=0 → 选中 */
    spi_write(h->hw_ctx, &cmd, 1);
    gpio_write(h->cs_pin, 1);       /* CS=1 → 释放 */
    return OLED_OK;
}

static oled_status_t oled_spi_write_data(oled_handle_t *h, const uint8_t *data, uint16_t len) {
    gpio_write(h->dc_pin, 1);       /* DC=1 → 数据 */
    gpio_write(h->cs_pin, 0);
    spi_write(h->hw_ctx, data, len);
    gpio_write(h->cs_pin, 1);
    return OLED_OK;
}
```

### 使用示例

```c
/* STM32 HAL 示例 */
oled_handle_t oled = {
    .dev       = OLED_DEV_SSD1306_128x64,
    .iface     = OLED_IF_I2C,
    .i2c_addr  = 0x3C,
    .hw_ctx    = &hi2c1,        /* HAL I2C 句柄 */
    .width     = 128,
    .height    = 64,
};

void main(void) {
    oled_init(&oled);
    oled_draw_string(&oled, 0, 0, "Hello OLED!", oled_font_6x8);
    oled_draw_string(&oled, 0, 16, "SSD1306 128x64", oled_font_6x8);
    oled_draw_rect(&oled, 0, 0, 128, 64);
    oled_draw_circle(&oled, 64, 40, 15);
    oled_flush(&oled);

    while (1) {
        /* 动态更新 */
        char buf[20];
        snprintf(buf, sizeof(buf), "Count: %d", counter++);
        oled_draw_string(&oled, 0, 32, buf, oled_font_6x8);
        oled_flush(&oled);
        HAL_Delay(500);
    }
}
```

---

## 4. 调试与测试规范

### 硬件验证清单

- [ ] 万用表确认 VCC = 3.3V，GND 连通
- [ ] I2C 模式：确认 SCL/SDA 上拉电阻 4.7kΩ 已接
- [ ] SPI 模式：确认 CS/DC/RES/MOSI/SCK 全部连接正确
- [ ] 示波器检查 I2C/SPI 波形电平幅值 (3.3V)
- [ ] 确认 100nF 去耦电容已贴装且紧贴 VCC 引脚
- [ ] RES 引脚有稳定高电平（接 VCC 或 MCU GPIO 输出高）

### 通信验证

```c
/* I2C 地址扫描 */
void oled_scan_i2c(void *i2c_ctx) {
    for (uint8_t addr = 1; addr < 128; addr++) {
        uint8_t dummy = 0;
        if (i2c_write(i2c_ctx, addr, &dummy, 0) == 0) {
            printf("Found device at 0x%02X\n", addr);
        }
    }
}
/* 预期输出: Found device at 0x3C (或 0x3D) */
```

- **SPI 验证**：用逻辑分析仪抓取初始化序列，核对每条命令字节
- **RES 验证**：手动拉低 RES 再拉高，观察屏幕是否清空闪烁

### 显示验证

| 测试项 | 方法 | 预期结果 |
|--------|------|----------|
| 全屏点亮 | 显存全填 0xFF | 全屏白色像素 |
| 全屏熄灭 | 显存全清 0x00 | 全屏黑色 |
| 棋盘格 | 交替写入 0xAA/0x55 | 规律棋盘图案 |
| 逐行扫描 | 逐行点亮像素 | 无断行、无竖条 |
| 字符显示 | 写入 "ABC123" | 清晰可读 |
| 对比度 | 遍历 0x00~0xFF 对比度 | 亮度平滑变化 |

### 性能指标

| 指标 | I2C (400kHz) | SPI (4MHz) | SPI (10MHz) |
|------|-------------|------------|-------------|
| 全屏刷新耗时 | ~22ms | ~3.5ms | ~1.5ms |
| 最大帧率 | ~45 fps | ~285 fps | ~666 fps |
| 初始化耗时 | ~50ms | ~50ms | ~50ms |

---

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 屏幕完全不亮 | 接线错误/地址错误 | 用 I2C 扫描确认地址；检查 VCC/GND |
| 屏幕全白/全亮 | 初始化未完成 | 检查 RES 引脚电平；确认初始化序列完整发送 |
| 显示花屏/镜像 | 列/行方向反 | 调整 `0xA1`/`0xA0`(段重映射) 和 `0xC8`/`0xC0`(COM方向) |
| SH1106 显示偏移2列 | 列地址起始不同 | SH1106 列低地址设为 0x02 而非 0x00 |
| I2C 偶尔通信失败 | 上拉电阻过大/总线电容高 | 降至 2.2kΩ；缩短连线；降低 I2C 时钟 |
| 显示闪烁/水波纹 | 电源不稳/电荷泵未开 | 确认 `0x8D 0x14`(开启电荷泵)；加强电源滤波 |
| 刷新有明显延迟 | I2C 带宽不足 | 改用 SPI 接口；或只刷新脏区域 |
| 只显示上半屏 | 多路复用比设置错误 | 128x64 设 `0xA8 0x3F`，128x32 设 `0xA8 0x1F` |
| 文字反向显示 | 反色模式开启 | 发送 `0xA6`(正常) 而非 `0xA7`(反色) |
| SPI 模式无显示 | DC 引脚状态错误 | 确认 DC=0 写命令、DC=1 写数据 |
| 屏幕有残影 | 未正确关闭显示就断电 | 断电前先发 `0xAE`(Display OFF) |
| 多设备 I2C 冲突 | 地址相同 | 改 SA0 电平切换为 0x3D；或使用 I2C 多路复用器 |
| 字体显示乱码 | 字体表偏移错误 | 确认字体数组从 ASCII 32 (空格) 开始索引 |

## 相关文档

- `skill://mcu-driver-core/templates/driver-template-i2c.c` — I2C 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-spi.c` — SPI 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
- `skill://mcu-driver-core/guides/pitfalls.md` — 跨类别常见问题与避坑指南
