# TFT 彩屏开发规范

## 1. 概述与选型指南

### 器件简介

TFT（Thin Film Transistor，薄膜晶体管）液晶屏是有源矩阵 LCD，每个像素由独立晶体管控制，支持高速刷新和全彩显示。MCU 领域常用 ILI/ST 系列驱动 IC，通过 SPI 或并行接口通信，支持 16bit (RGB565) 或 18bit (RGB666) 色深。TFT 依赖 LED 背光，功耗高于 OLED 但色彩丰富。

### 常见型号对比

| 型号 | 尺寸 | 分辨率 | 接口 | 色深 | 驱动IC | SPI最大时钟 | 触摸 | 价格 |
|------|------|--------|------|------|--------|------------|------|------|
| ST7735 | 1.8" | 128x160 | SPI | 16bit | ST7735 | 15MHz | 可选电阻 | ¥8~15 |
| ST7789 | 1.3/1.54" | 240x240 | SPI | 16bit | ST7789 | 40MHz | 可选 | ¥10~20 |
| ILI9341 | 2.4" | 240x320 | SPI/并行 | 16bit | ILI9341 | 40MHz | XPT2046 | ¥15~25 |
| ILI9488 | 3.5" | 320x480 | SPI/并行 | 18bit | ILI9488 | 20MHz | XPT2046 | ¥25~40 |

### 选型决策树

```
需要触摸屏？ → 是 → ILI9341 2.4" + XPT2046
             否 →
分辨率 > 240x320？ → 是 → ILI9488 3.5" 320x480
                   否 →
尺寸 ≤ 1.8"？ → 是 → ST7735 1.8" 128x160
              否 →
方形显示？ → 是 → ST7789 1.3"/1.54" 240x240
           否 → ILI9341 2.4" 240x320 (性价比最优)
```

### 适用场景

- **ST7735**：可穿戴设备、小型仪表、低成本彩色显示
- **ST7789**：智能手表、圆形/方形 UI、紧凑型设备
- **ILI9341**：手持仪表、IoT 网关面板、迷你游戏机
- **ILI9488**：HMI 人机界面、工业控制面板

---

## 2. 硬件设计规范

### 引脚定义

```
┌──────────────────────────────────────────────────────────┐
│              TFT Module (SPI)                            │
│                                                          │
│  VCC  ── 电源 3.3V       GND  ── 地                     │
│  CS   ── 片选            RST  ── 复位                    │
│  DC   ── 数据/命令(0=cmd,1=data)                         │
│  SCK  ── SPI时钟         MOSI ── SPI数据                 │
│  MISO ── 读显存(可选)    LED  ── 背光(可PWM调光)         │
│                                                          │
│  触摸 XPT2046: T_CLK/T_CS/T_DIN/T_DOUT/T_IRQ           │
└──────────────────────────────────────────────────────────┘
```

### 典型应用电路

```
MCU                        ILI9341
─────                      ───────
SPI SCK  ───────────────── SCK
SPI MOSI ───────────────── MOSI (SDA)
SPI MISO ───────────────── MISO (可选)
GPIO     ───────────────── CS
GPIO     ───────────────── DC
GPIO     ───────────────── RST
GPIO/PWM ───────────────── LED (背光调光)

3.3V ───────────────────── VCC
GND  ───────────────────── GND
```

**背光驱动电路**（大屏背光电流可达 100mA+，不可直接接 MCU）：

```
3.3V ──[10Ω]── LED+
                │
              TFT背光
                │
GND ─── NPN C
         B ──[1kΩ]── MCU PWM
         E ── GND
```

### PCB 设计要点

- **SPI 时钟走线**：> 20MHz 时走线需短且等长，避免过孔
- **去耦电容**：VCC 旁放 100nF + 10μF，紧贴电源引脚
- **背光驱动**：大屏用三极管/MOS 驱动，不可直连 MCU 引脚
- **电平匹配**：TFT 为 3.3V，5V MCU 需电平转换
- **触摸屏**：XPT2046 可与 LCD 共用 SPI（不同 CS），T_IRQ 接中断引脚
- **GND**：大面积铺铜，SPI 信号线下方完整地平面

### 电气参数

| 参数 | ST7735 | ST7789 | ILI9341 | ILI9488 | 单位 |
|------|--------|--------|---------|---------|------|
| VCC | 3.0~3.6 | 2.4~3.3 | 2.5~3.3 | 2.5~3.3 | V |
| SPI最大时钟 | 15 | 40 | 40 | 20 | MHz |
| 背光电流 | ~20 | ~20 | ~50 | ~120 | mA |
| 帧率(SPI 40MHz) | ~60 | ~120 | ~60 | ~25 | fps |

---

## 3. 驱动开发规范

### 颜色定义

```c
/* RGB565: 高5位红, 中6位绿, 低5位蓝 */
#define RGB565(r, g, b)  (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
```

### 统一接口定义

```c
typedef enum {
    TFT_DEV_ST7735_128x160, TFT_DEV_ST7789_240x240,
    TFT_DEV_ILI9341_240x320, TFT_DEV_ILI9488_320x480,
} tft_device_t;

typedef enum {
    TFT_OK = 0, TFT_ERR_INIT, TFT_ERR_TIMEOUT, TFT_ERR_PARAM,
} tft_status_t;

typedef struct {
    tft_device_t dev;
    void    *hw_ctx;
    uint16_t width, height;
    uint8_t  *vram;       /* 可选帧缓冲 */
    bool     use_vram;
} tft_handle_t;

tft_status_t tft_init(tft_handle_t *h);
tft_status_t tft_set_addr_window(tft_handle_t *h, int16_t x0, int16_t y0, int16_t x1, int16_t y1);
tft_status_t tft_fill_rect(tft_handle_t *h, int16_t x, int16_t y, int16_t w, int16_t hgt, uint16_t color);
tft_status_t tft_fill_screen(tft_handle_t *h, uint16_t color);
void tft_draw_pixel(tft_handle_t *h, int16_t x, int16_t y, uint16_t color);
void tft_draw_line(tft_handle_t *h, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void tft_draw_rect(tft_handle_t *h, int16_t x, int16_t y, int16_t w, int16_t hgt, uint16_t color);
void tft_draw_circle(tft_handle_t *h, int16_t cx, int16_t cy, int16_t r, uint16_t color);
void tft_draw_string(tft_handle_t *h, int16_t x, int16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t size);
tft_status_t tft_set_rotation(tft_handle_t *h, uint8_t rotation);
```

### 平台适配层 (SPI)

```c
typedef struct {
    void (*spi_write)(const uint8_t *data, uint16_t len);
    void (*dc_write)(int val);    /* 0=命令, 1=数据 */
    void (*cs_write)(int val);
    void (*rst_write)(int val);
    void (*bl_write)(int val);
    void (*delay_ms)(uint32_t ms);
} tft_spi_hw_t;

static void tft_write_cmd(tft_handle_t *h, uint8_t cmd) {
    tft_spi_hw_t *hw = (tft_spi_hw_t *)h->hw_ctx;
    hw->dc_write(0); hw->cs_write(0);
    hw->spi_write(&cmd, 1);
    hw->cs_write(1);
}

static void tft_write_data(tft_handle_t *h, const uint8_t *data, uint16_t len) {
    tft_spi_hw_t *hw = (tft_spi_hw_t *)h->hw_ctx;
    hw->dc_write(1); hw->cs_write(0);
    hw->spi_write(data, len);
    hw->cs_write(1);
}
```

### ILI9341 初始化序列

```c
tft_status_t tft_init(tft_handle_t *h) {
    tft_spi_hw_t *hw = (tft_spi_hw_t *)h->hw_ctx;

    /* 硬件复位 */
    hw->rst_write(0); hw->delay_ms(20);
    hw->rst_write(1); hw->delay_ms(120);

    /* 软件复位 */
    tft_write_cmd(h, 0x01);  /* SWRESET */
    hw->delay_ms(120);

    /* 电源与伽马配置序列 */
    tft_write_cmd(h, 0xCF); uint8_t d1[] = {0x00,0xC1,0x30}; tft_write_data(h,d1,3);
    tft_write_cmd(h, 0xED); uint8_t d2[] = {0x64,0x03,0x12,0x81}; tft_write_data(h,d2,4);
    tft_write_cmd(h, 0xE8); uint8_t d3[] = {0x85,0x00,0x78}; tft_write_data(h,d3,3);
    tft_write_cmd(h, 0xCB); uint8_t d4[] = {0x39,0x2C,0x00,0x34,0x02}; tft_write_data(h,d4,5);

    /* 电源控制 */
    tft_write_cmd(h, 0xC0); uint8_t d5[] = {0x23}; tft_write_data(h,d5,1);  /* PWCTR1 */
    tft_write_cmd(h, 0xC1); uint8_t d6[] = {0x10}; tft_write_data(h,d6,1);  /* PWCTR2 */
    tft_write_cmd(h, 0xC5); uint8_t d7[] = {0x3E,0x28}; tft_write_data(h,d7,2); /* VMCTR1 */
    tft_write_cmd(h, 0xC7); uint8_t d8[] = {0x86}; tft_write_data(h,d8,1);  /* VMCTR2 */

    /* 显示方向: 竖屏, BGR色序 */
    tft_write_cmd(h, 0x36); uint8_t d9[] = {0x48}; tft_write_data(h,d9,1);  /* MADCTL */

    /* 像素格式: 16bit RGB565 */
    tft_write_cmd(h, 0x3A); uint8_t d10[] = {0x55}; tft_write_data(h,d10,1); /* COLMOD */

    /* 帧率控制 */
    tft_write_cmd(h, 0xB1); uint8_t d11[] = {0x00,0x18}; tft_write_data(h,d11,2);

    /* 显示功能控制 */
    tft_write_cmd(h, 0xB6); uint8_t d12[] = {0x08,0x82,0x27}; tft_write_data(h,d12,3);

    /* 正向伽马校正 */
    tft_write_cmd(h, 0xE0);
    uint8_t pg[] = {0x0F,0x31,0x2B,0x0C,0x0E,0x08,0x4E,0xF1,0x37,0x07,0x10,0x03,0x0E,0x09,0x00};
    tft_write_data(h, pg, 15);

    /* 负向伽马校正 */
    tft_write_cmd(h, 0xE1);
    uint8_t ng[] = {0x00,0x0E,0x14,0x03,0x11,0x07,0x31,0xC1,0x48,0x08,0x0F,0x0C,0x31,0x36,0x0F};
    tft_write_data(h, ng, 15);

    /* 退出睡眠 → 开显示 */
    tft_write_cmd(h, 0x11);  /* SLPOUT */
    hw->delay_ms(120);
    tft_write_cmd(h, 0x29);  /* DISPON */
    hw->delay_ms(20);

    hw->bl_write(1);  /* 开背光 */
    return TFT_OK;
}
```

### 地址窗口与颜色填充

```c
tft_status_t tft_set_addr_window(tft_handle_t *h, int16_t x0, int16_t y0,
                                  int16_t x1, int16_t y1) {
    tft_write_cmd(h, 0x2A);  /* CASET */
    uint8_t col[4] = {(x0>>8)&0xFF, x0&0xFF, (x1>>8)&0xFF, x1&0xFF};
    tft_write_data(h, col, 4);

    tft_write_cmd(h, 0x2B);  /* RASET */
    uint8_t row[4] = {(y0>>8)&0xFF, y0&0xFF, (y1>>8)&0xFF, y1&0xFF};
    tft_write_data(h, row, 4);

    tft_write_cmd(h, 0x2C);  /* RAMWR */
    return TFT_OK;
}

tft_status_t tft_fill_rect(tft_handle_t *h, int16_t x, int16_t y,
                            int16_t w, int16_t hgt, uint16_t color) {
    if (x < 0 || y < 0 || x+w > h->width || y+hgt > h->height) return TFT_ERR_PARAM;
    tft_set_addr_window(h, x, y, x+w-1, y+hgt-1);

    uint32_t total = (uint32_t)w * hgt;
    tft_spi_hw_t *hw = (tft_spi_hw_t *)h->hw_ctx;
    hw->dc_write(1); hw->cs_write(0);

    /* 分块发送减少内存占用 */
    uint8_t buf[512];  /* 256像素 */
    for (int i = 0; i < 256; i++) { buf[i*2] = color>>8; buf[i*2+1] = color&0xFF; }
    while (total >= 256) { hw->spi_write(buf, 512); total -= 256; }
    if (total > 0) { hw->spi_write(buf, total*2); }
    hw->cs_write(1);
    return TFT_OK;
}

tft_status_t tft_fill_screen(tft_handle_t *h, uint16_t color) {
    return tft_fill_rect(h, 0, 0, h->width, h->height, color);
}
```

### 图形绘制

```c
void tft_draw_pixel(tft_handle_t *h, int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= h->width || y < 0 || y >= h->height) return;
    tft_set_addr_window(h, x, y, x, y);
    uint8_t buf[2] = {color >> 8, color & 0xFF};
    tft_write_data(h, buf, 2);
}

void tft_draw_line(tft_handle_t *h, int16_t x0, int16_t y0,
                    int16_t x1, int16_t y1, uint16_t color) {
    int16_t dx = abs(x1-x0), sx = x0<x1 ? 1:-1;
    int16_t dy = -abs(y1-y0), sy = y0<y1 ? 1:-1;
    int16_t err = dx+dy, e2;
    while (1) {
        tft_draw_pixel(h, x0, y0, color);
        if (x0==x1 && y0==y1) break;
        e2 = 2*err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void tft_draw_rect(tft_handle_t *h, int16_t x, int16_t y,
                    int16_t w, int16_t hgt, uint16_t color) {
    tft_draw_line(h, x, y, x+w-1, y, color);
    tft_draw_line(h, x, y+hgt-1, x+w-1, y+hgt-1, color);
    tft_draw_line(h, x, y, x, y+hgt-1, color);
    tft_draw_line(h, x+w-1, y, x+w-1, y+hgt-1, color);
}

void tft_draw_circle(tft_handle_t *h, int16_t cx, int16_t cy,
                      int16_t r, uint16_t color) {
    if (r <= 0) return;
    int16_t x = r, y = 0, err = 0;
    while (x >= y) {
        tft_draw_pixel(h, cx+x, cy+y, color);
        tft_draw_pixel(h, cx+y, cy+x, color);
        tft_draw_pixel(h, cx-y, cy+x, color);
        tft_draw_pixel(h, cx-x, cy+y, color);
        tft_draw_pixel(h, cx-x, cy-y, color);
        tft_draw_pixel(h, cx-y, cy-x, color);
        tft_draw_pixel(h, cx+y, cy-x, color);
        tft_draw_pixel(h, cx+x, cy-y, color);
        if (err <= 0) { y++; err += 2*y+1; }
        if (err > 0)  { x--; err -= 2*x+1; }
    }
}
```

### 帧缓冲管理

```c
/* 240x320 x 2 = 150KB, ESP32 可容纳; STM32F1 不够, 用无缓冲模式 */
tft_status_t tft_vram_init(tft_handle_t *h) {
    h->vram = malloc(h->width * h->height * 2);
    if (!h->vram) return TFT_ERR_INIT;
    h->use_vram = true;
    return TFT_OK;
}

void tft_vram_set_pixel(tft_handle_t *h, int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= h->width || y < 0 || y >= h->height) return;
    h->vram[(y*h->width + x)*2]     = color >> 8;
    h->vram[(y*h->width + x)*2 + 1] = color & 0xFF;
}

tft_status_t tft_vram_flush(tft_handle_t *h) {
    tft_set_addr_window(h, 0, 0, h->width-1, h->height-1);
    tft_spi_hw_t *hw = (tft_spi_hw_t *)h->hw_ctx;
    hw->dc_write(1); hw->cs_write(0);
    hw->spi_write(h->vram, h->width * h->height * 2);  /* DMA发送 */
    hw->cs_write(1);
    return TFT_OK;
}

/* 局部刷新: 只推送脏区域 */
tft_status_t tft_vram_flush_region(tft_handle_t *h, int16_t x, int16_t y, int16_t w, int16_t hgt) {
    tft_set_addr_window(h, x, y, x+w-1, y+hgt-1);
    tft_spi_hw_t *hw = (tft_spi_hw_t *)h->hw_ctx;
    hw->dc_write(1); hw->cs_write(0);
    for (int16_t row = y; row < y+hgt; row++)
        hw->spi_write(&h->vram[(row*h->width + x)*2], w*2);
    hw->cs_write(1);
    return TFT_OK;
}
```

### 屏幕旋转

```c
tft_status_t tft_set_rotation(tft_handle_t *h, uint8_t rotation) {
    uint8_t madctl;
    switch (rotation % 4) {
        case 0: madctl = 0x48; h->width = 240; h->height = 320; break; /* 竖屏 */
        case 1: madctl = 0x28; h->width = 320; h->height = 240; break; /* 横屏 */
        case 2: madctl = 0x88; h->width = 240; h->height = 320; break; /* 竖屏翻转 */
        case 3: madctl = 0xE8; h->width = 320; h->height = 240; break; /* 横屏翻转 */
    }
    tft_write_cmd(h, 0x36);  /* MADCTL */
    tft_write_data(h, &madctl, 1);
    return TFT_OK;
}
```

### 使用示例

```c
tft_handle_t tft = { .dev = TFT_DEV_ILI9341_240x320, .hw_ctx = &spi_hw, .width = 240, .height = 320 };

void main(void) {
    tft_init(&tft);
    tft_fill_screen(&tft, COLOR_BLACK);

    tft_fill_rect(&tft, 0, 0, 240, 30, COLOR_BLUE);   /* 标题栏 */
    tft_draw_string(&tft, 10, 10, "ILI9341 Demo", COLOR_WHITE, COLOR_BLUE, 2);

    tft_draw_rect(&tft, 10, 40, 100, 60, COLOR_RED);
    tft_fill_rect(&tft, 120, 40, 100, 60, COLOR_GREEN);
    tft_draw_circle(&tft, 60, 150, 30, COLOR_YELLOW);
    tft_fill_circle(&tft, 180, 150, 30, COLOR_CYAN);

    int count = 0; char buf[32];
    while (1) {
        snprintf(buf, sizeof(buf), "Count: %d", count++);
        tft_fill_rect(&tft, 10, 210, 200, 20, COLOR_BLACK);
        tft_draw_string(&tft, 10, 215, buf, COLOR_WHITE, COLOR_BLACK, 2);
        HAL_Delay(100);
    }
}
```

---

## 4. 调试与测试规范

### 硬件验证清单

- [ ] 万用表确认 VCC = 3.3V，GND 连通
- [ ] 背光 LED 正常亮起（限流电阻/驱动三极管正确）
- [ ] SPI 信号 CS/DC/RST/SCK/MOSI 全部连接正确
- [ ] 示波器检查 SPI 时钟波形（幅值 3.3V）
- [ ] 100nF + 10μF 去耦电容已贴装
- [ ] 触摸屏 XPT2046 接线完整

### 通信验证

- **SPI 验证**：逻辑分析仪抓初始化序列，核对命令字节和 DC 时序
- **复位验证**：拉低 RST 再拉高，屏幕应闪白
- **XPT2046 验证**：读取 Z1/Z2 通道，触摸时压力值 > 阈值

### 显示验证

| 测试项 | 方法 | 预期结果 |
|--------|------|----------|
| 全屏填色 | 依次填红/绿/蓝/白/黑 | 颜色正确，无坏点 |
| 棋盘格 | 8x8 格交替黑白 | 边界清晰 |
| 渐变色 | 水平渐变红→蓝 | 过渡平滑 |
| 文字 | 各字号显示 "ABC123" | 清晰可读 |
| 线条 | 画各角度线 | 无断裂 |
| 触摸校准 | 四角校准法 | 坐标映射准确 |

### 性能指标

| 操作 | SPI 10MHz | SPI 40MHz | SPI+DMA 40MHz |
|------|-----------|-----------|---------------|
| 全屏填充 | ~160ms | ~40ms | ~22ms |
| 局部刷新(100x100) | ~25ms | ~6ms | ~3ms |
| 单像素写入 | ~15μs | ~4μs | ~4μs |

---

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 屏幕全白 | 初始化失败/接线错 | 检查 RST/DC/CS 接线；确认初始化序列完整 |
| 颜色偏红/蓝 | BGR/RGB 色序错误 | 修改 MADCTL(0x36) 的 bit3: 0=RGB, 1=BGR |
| 显示镜像/翻转 | 方向参数不对 | 调整 MADCTL 的 MY(bit7)/MX(bit6)/MV(bit5) |
| 刷新太慢 | SPI 时钟低/无 DMA | 提高 SPI 时钟至 40MHz；启用 DMA；用帧缓冲 |
| 屏幕闪烁 | 背光 PWM 频率太低 | PWM 频率提高到 ≥ 500Hz |
| 花屏/雪花 | SPI 时钟过高/走线差 | 降低 SPI 时钟；缩短走线；加等长处理 |
| 颜色错误(RGB565) | 高低字节顺序错 | 确认发送时先发高字节再发低字节 |
| 触摸不准 | 未校准 | 四角校准法，存储校准矩阵 |
| 触摸无反应 | T_IRQ/T_CS 接线错 | 检查 XPT2046 接线；确认 T_CS 拉低时才能通信 |
| ILI9488 颜色少 | 18bit vs 16bit | ILI9488 原生 18bit，16bit 模式需设 COLMOD=0x55 |
| RAM 不够用帧缓冲 | MCU 内存不足 | 用无缓冲模式逐区域刷新；或用局部帧缓冲 |
| 旋转后坐标错 | 旋转未更新宽高 | set_rotation 时同步更新 width/height |
| ST7789 无 CS 引脚 | 部分 ST7789 模块无 CS | DC 引脚区分命令/数据，CS 可省（但总线不可共用） |
| DMA 传输数据错乱 | 缓冲区不在 DMA 可访问区 | 确保帧缓冲在 DMA 可访问内存区域 (如 ESP32 的 IRAM) |

## 相关文档

- `../../templates/driver-template-spi.c` — SPI 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-pwm.c` — PWM 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `../../guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `../../guides/debugging-testing.md` — 调试与测试规范
- `../../guides/pitfalls.md` — 跨类别常见问题与避坑指南
