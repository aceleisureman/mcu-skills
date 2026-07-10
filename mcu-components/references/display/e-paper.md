# 电子墨水屏开发规范

## 1. 概述与选型指南

### 器件简介

电子墨水屏（E-Paper / E-Ink Display）是一种基于电泳显示技术（EPD）的反射式显示屏。微胶囊中的带电颜料粒子在电场作用下上下移动呈现黑白颜色。最大特点是**双稳态**——断电后仍保持最后画面，仅刷新时消耗电能，功耗极低。

### 常见型号对比

| 型号 | 尺寸 | 分辨率 | 颜色 | 驱动IC | 接口 | 工作电压 | 刷新方式 | 价格 |
|------|------|--------|------|--------|------|----------|----------|------|
| SSD1680 | 1.54" | 200x200 | 黑白 | SSD1680 | SPI | 3.3V | 全刷/局刷 | ¥25~40 |
| UC8176 | 2.9" | 296x128 | 黑白 | UC8176 | SPI | 3.3V | 全刷/局刷 | ¥30~50 |
| SSD1675 | 1.54" | 200x200 | 黑白 | SSD1675 | SPI | 3.3V | 全刷/局刷 | ¥25~40 |
| IL3829 | 2.13" | 250x122 | 黑白 | IL3829 | SPI | 3.3V | 全刷/局刷 | ¥28~45 |
| SSD1681 | 1.54" | 200x200 | 黑白红 | SSD1681 | SPI | 3.3V | 全刷 | ¥35~55 |
| UC8176C | 2.9" | 296x128 | 黑白红 | UC8176 | SPI | 3.3V | 全刷 | ¥40~60 |

### 选型决策树

```
需要彩色(红黑白)？ → 是 → SSD1681/UC8176C (仅全刷)
                   否 →
尺寸 ≤ 1.54"？ → 是 → SSD1680 1.54" 200x200
               否 →
需要宽屏？ → 是 → UC8176 2.9" 296x128
           否 → IL3829 2.13" 250x122
需要局刷？ → 确认驱动IC支持局刷LUT
需要超低功耗？ → E-Paper 是最优选 (断电保持画面)
```

### 适用场景

- **电子价签**：超市/仓储商品标签，电池供电数月~数年
- **电子书阅读器**：长时间阅读，护眼无背光
- **物联网状态屏**：传感器数据展示，低频更新
- **智能家居面板**：温湿度/日历/待办事项显示
- **工业铭牌**：设备信息长期静态显示

---

## 2. 硬件设计规范

### 引脚定义

```
┌──────────────────────────────────────────────────────────┐
│              E-Paper Module (SPI)                        │
│                                                          │
│  VCC  ── 电源 3.3V       GND  ── 地                     │
│  SCL  ── SPI时钟 (SCK)   SDA  ── SPI数据 (MOSI)          │
│  CS   ── 片选            DC   ── 数据/命令(0=cmd,1=data) │
│  RST  ── 复位            BUSY ── 忙信号(低=忙,高=空闲)   │
└──────────────────────────────────────────────────────────┘
```

### 典型应用电路

```
MCU                        E-Paper
─────                      ────────
SPI SCK  ───────────────── SCL
SPI MOSI ───────────────── SDA
GPIO     ───────────────── CS
GPIO     ───────────────── DC
GPIO     ───────────────── RST
GPIO(输入) ─────────────── BUSY  (忙信号, MCU需轮询)

3.3V ───────────────────── VCC
GND  ───────────────────── GND

/* 可选: 电源控制(进一步降低功耗) */
3.3V ── P-MOS ── VCC_EPD
         │
MCU GPIO ──[10kΩ]── G  (高=关断, 低=导通)
         │
       10kΩ 上拉到 3.3V
```

### PCB 设计要点

- **BUSY 引脚**：必须连接 MCU GPIO 输入，不可悬空；用于判断刷新是否完成
- **去耦电容**：VCC 旁放 100nF + 10μF，驱动 IC 内部升压需稳定供电
- **RST 引脚**：建议接 MCU GPIO，上电时先拉低复位再初始化
- **电平匹配**：3.3V 器件，5V MCU 需电平转换
- **FPC 排线**：连接器需可靠接触，避免反复弯折
- **电源管理**：超低功耗场景用 MOS 管控制供电，刷新时通电、平时断电
- **SPI 时钟**：建议 ≤ 10MHz
- **保护**：面板脆弱避免按压弯折；数据线加 ESD 保护

### 电气参数

| 参数 | 最小值 | 典型值 | 最大值 | 单位 |
|------|--------|--------|--------|------|
| VCC | 2.3 | 3.3 | 3.6 | V |
| SPI 时钟 | — | 2000 | 10000 | kHz |
| 工作电流(刷新中) | — | 8 | 15 | mA |
| 待机电流(保持画面) | — | 0.01 | 0.1 | mA |
| 全刷耗时 | — | 3~5 | 8 | s |
| 局刷耗时 | — | 0.5~1 | 2 | s |
| BUSY 超时(全刷) | — | 5 | 15 | s |
| 工作温度 | 0 | 25 | 50 | °C |

> E-Paper 不适合在低温 (< 0°C) 下工作，刷新波形受温度影响。

---

## 3. 驱动开发规范

### 波形 LUT 说明

E-Paper 刷新依赖波形查找表（Waveform LUT），定义像素从旧状态到新状态的电压序列：

- **全刷 LUT (Full Update)**：黑白交替闪烁多次后稳定，消除残影。耗时 3~5 秒。适合首次显示或画面大幅变化。
- **局刷 LUT (Partial Update)**：直接翻转像素，无闪烁。耗时 0.5~1 秒。适合频繁更新的小区域。但有**残影累积**问题，需定期全刷消除。
- **LUT 来源**：内置在驱动 IC 的 OTP 中，或由 MCU 通过 SPI 下发自定义波形。

### 统一接口定义

```c
typedef enum {
    EPD_DEV_SSD1680_154, EPD_DEV_UC8176_290,
    EPD_DEV_SSD1675_154, EPD_DEV_IL3829_213,
} epd_device_t;

typedef enum {
    EPD_OK = 0, EPD_ERR_INIT, EPD_ERR_TIMEOUT, EPD_ERR_PARAM, EPD_ERR_BUSY,
} epd_status_t;

typedef enum {
    EPD_UPDATE_FULL,     /* 全刷: 消除残影, 3~5秒 */
    EPD_UPDATE_PARTIAL,  /* 局刷: 快速刷新, 0.5~1秒 */
} epd_update_mode_t;

typedef struct {
    epd_device_t dev;
    void    *hw_ctx;
    uint16_t width, height;
    uint8_t *vram_black;   /* 黑白显存 */
    uint8_t *vram_red;     /* 红色显存(仅三色屏) */
} epd_handle_t;

epd_status_t epd_init(epd_handle_t *h);
epd_status_t epd_clear(epd_handle_t *h, epd_update_mode_t mode);
epd_status_t epd_display(epd_handle_t *h, epd_update_mode_t mode);
epd_status_t epd_sleep(epd_handle_t *h);
void         epd_draw_pixel(epd_handle_t *h, int16_t x, int16_t y, uint8_t color);
epd_status_t epd_wait_busy(epd_handle_t *h);
```

### 颜色定义

```c
#define EPD_COLOR_WHITE  0   /* 白色 */
#define EPD_COLOR_BLACK  1   /* 黑色 */
#define EPD_COLOR_RED    2   /* 红色(仅三色屏) */
```

### 平台适配层

```c
typedef struct {
    void (*spi_write)(const uint8_t *data, uint16_t len);
    void (*cs_write)(int val);
    void (*dc_write)(int val);    /* 0=命令, 1=数据 */
    void (*rst_write)(int val);
    int  (*busy_read)(void);      /* 1=空闲, 0=忙 */
    void (*delay_ms)(uint32_t ms);
} epd_spi_hw_t;

static void epd_write_cmd(epd_handle_t *h, uint8_t cmd) {
    epd_spi_hw_t *hw = (epd_spi_hw_t *)h->hw_ctx;
    hw->dc_write(0); hw->cs_write(0);
    hw->spi_write(&cmd, 1);
    hw->cs_write(1);
}

static void epd_write_data(epd_handle_t *h, const uint8_t *data, uint16_t len) {
    epd_spi_hw_t *hw = (epd_spi_hw_t *)h->hw_ctx;
    hw->dc_write(1); hw->cs_write(0);
    hw->spi_write(data, len);
    hw->cs_write(1);
}

epd_status_t epd_wait_busy(epd_handle_t *h) {
    epd_spi_hw_t *hw = (epd_spi_hw_t *)h->hw_ctx;
    uint32_t timeout = 15000;  /* 全刷最多15秒 */
    while (hw->busy_read() == 0) {
        hw->delay_ms(10);
        if ((timeout -= 10) == 0) return EPD_ERR_TIMEOUT;
    }
    return EPD_OK;
}
```

### SSD1680 驱动

```c
/* SSD1680 命令定义 */
#define SSD1680_CMD_SWRESET         0x12
#define SSD1680_CMD_SLEEP_EXIT      0x20
#define SSD1680_CMD_SLEEP_ENTER     0x10
#define SSD1680_CMD_RAM_BW          0x24   /* 黑白显存写入 */
#define SSD1680_CMD_RAM_RED         0x26   /* 红色显存写入 */
#define SSD1680_CMD_UPDATE_FULL     0x22
#define SSD1680_CMD_DISP_REFRESH    0x2A
#define SSD1680_CMD_VCOM            0x2C
#define SSD1680_CMD_SET_RAMX        0x44
#define SSD1680_CMD_SET_RAMY        0x45
#define SSD1680_CMD_SET_RAMX_CNT    0x4E
#define SSD1680_CMD_SET_RAMY_CNT    0x4F

epd_status_t epd_init(epd_handle_t *h) {
    epd_spi_hw_t *hw = (epd_spi_hw_t *)h->hw_ctx;

    /* 分配显存 (每像素1bit) */
    uint16_t buf_size = (h->width * h->height) / 8;
    h->vram_black = malloc(buf_size);
    if (!h->vram_black) return EPD_ERR_INIT;
    memset(h->vram_black, 0xFF, buf_size);  /* 初始全白 */

    /* 硬件复位 */
    hw->rst_write(0); hw->delay_ms(10);
    hw->rst_write(1); hw->delay_ms(10);

    /* 软件复位 */
    epd_write_cmd(h, SSD1680_CMD_SWRESET);
    if (epd_wait_busy(h) != EPD_OK) return EPD_ERR_INIT;

    /* 退出睡眠 */
    epd_write_cmd(h, SSD1680_CMD_SLEEP_EXIT);
    epd_wait_busy(h);

    /* 设置 VCOM 电压 */
    epd_write_cmd(h, SSD1680_CMD_VCOM);
    epd_write_data(h, (uint8_t[]){0x26}, 1);

    return EPD_OK;
}

/* 设置显存写入区域 */
static void epd_set_window(epd_handle_t *h, int16_t x, int16_t y, int16_t w, int16_t hgt) {
    uint8_t x_start = x / 8;
    uint8_t x_end   = (x + w - 1) / 8;

    epd_write_cmd(h, SSD1680_CMD_SET_RAMX);
    uint8_t x_data[2] = {x_start, x_end};
    epd_write_data(h, x_data, 2);

    epd_write_cmd(h, SSD1680_CMD_SET_RAMY);
    uint8_t y_data[4] = {
        y & 0xFF, (y >> 8) & 0xFF,
        (y + hgt - 1) & 0xFF, ((y + hgt - 1) >> 8) & 0xFF
    };
    epd_write_data(h, y_data, 4);

    epd_write_cmd(h, SSD1680_CMD_SET_RAMX_CNT);
    epd_write_data(h, &x_start, 1);

    epd_write_cmd(h, SSD1680_CMD_SET_RAMY_CNT);
    epd_write_data(h, y_data, 2);
}

/* 全刷 */
epd_status_t epd_display_full(epd_handle_t *h) {
    uint16_t buf_size = (h->width * h->height) / 8;

    epd_set_window(h, 0, 0, h->width, h->height);

    /* 写入黑白显存 */
    epd_write_cmd(h, SSD1680_CMD_RAM_BW);
    epd_write_data(h, h->vram_black, buf_size);

    /* 触发全刷 */
    epd_write_cmd(h, SSD1680_CMD_UPDATE_FULL);
    epd_write_data(h, (uint8_t[]){0xF7}, 1);  /* 全刷序列 */

    epd_write_cmd(h, SSD1680_CMD_DISP_REFRESH);
    return epd_wait_busy(h);
}

/* 局刷 */
epd_status_t epd_display_partial(epd_handle_t *h, int16_t x, int16_t y,
                                   int16_t w, int16_t hgt) {
    uint16_t buf_size = (w * hgt) / 8;
    uint8_t *partial_buf = malloc(buf_size);
    if (!partial_buf) return EPD_ERR_PARAM;

    /* 从显存提取局部区域 */
    for (int16_t row = 0; row < hgt; row++) {
        for (int16_t col = 0; col < w / 8; col++) {
            uint16_t src_idx = ((y + row) * h->width + x) / 8 + col;
            partial_buf[row * (w / 8) + col] = h->vram_black[src_idx];
        }
    }

    epd_set_window(h, x, y, w, hgt);

    epd_write_cmd(h, SSD1680_CMD_RAM_BW);
    epd_write_data(h, partial_buf, buf_size);

    /* 触发局刷 */
    epd_write_cmd(h, SSD1680_CMD_UPDATE_FULL);
    epd_write_data(h, (uint8_t[]){0xFF}, 1);  /* 局刷序列 */

    epd_write_cmd(h, SSD1680_CMD_DISP_REFRESH);

    free(partial_buf);
    return epd_wait_busy(h);
}

epd_status_t epd_display(epd_handle_t *h, epd_update_mode_t mode) {
    if (mode == EPD_UPDATE_FULL)
        return epd_display_full(h);
    else
        return epd_display_partial(h, 0, 0, h->width, h->height);
}

epd_status_t epd_clear(epd_handle_t *h, epd_update_mode_t mode) {
    memset(h->vram_black, 0xFF, (h->width * h->height) / 8);
    return epd_display(h, mode);
}

void epd_draw_pixel(epd_handle_t *h, int16_t x, int16_t y, uint8_t color) {
    if (x < 0 || x >= h->width || y < 0 || y >= h->height) return;
    uint16_t idx = (y * h->width + x) / 8;
    uint8_t bit = 7 - (x % 8);
    if (color == EPD_COLOR_BLACK)
        h->vram_black[idx] &= ~(1 << bit);   /* 0=黑 */
    else
        h->vram_black[idx] |= (1 << bit);     /* 1=白 */
}

/* 进入睡眠(断电保持画面) */
epd_status_t epd_sleep(epd_handle_t *h) {
    epd_write_cmd(h, SSD1680_CMD_SLEEP_ENTER);
    epd_wait_busy(h);
    epd_write_cmd(h, 0x10);
    epd_write_data(h, (uint8_t[]){0x01}, 1);  /* 深度睡眠 */
    return EPD_OK;
}
```

### UC8176 驱动差异

```c
/* UC8176 与 SSD1680 大部分兼容, 主要差异:
   1. 分辨率: 296x128
   2. VCOM/LUT 寄存器地址不同
   3. 驱动输出控制需配置门极线数 */

#define UC8176_CMD_DRV_OUT_CTRL   0x01
#define UC8176_CMD_SRC_DRV_VOLT   0x04
#define UC8176_CMD_VCOM           0x2C
#define UC8176_CMD_LUT            0x32

epd_status_t uc8176_init(epd_handle_t *h) {
    epd_spi_hw_t *hw = (epd_spi_hw_t *)h->hw_ctx;
    uint16_t buf_size = (h->width * h->height) / 8;
    h->vram_black = malloc(buf_size);
    if (!h->vram_black) return EPD_ERR_INIT;
    memset(h->vram_black, 0xFF, buf_size);

    hw->rst_write(0); hw->delay_ms(10);
    hw->rst_write(1); hw->delay_ms(10);

    /* 驱动输出控制: 门极线数 */
    epd_write_cmd(h, UC8176_CMD_DRV_OUT_CTRL);
    uint8_t drv_out[3] = {(h->height-1)&0xFF, ((h->height-1)>>8)&0xFF, 0x00};
    epd_write_data(h, drv_out, 3);

    /* 源极驱动电压 15V */
    epd_write_cmd(h, UC8176_CMD_SRC_DRV_VOLT);
    epd_write_data(h, (uint8_t[]){0x41}, 1);

    /* VCOM 电压 */
    epd_write_cmd(h, UC8176_CMD_VCOM);
    epd_write_data(h, (uint8_t[]){0x28}, 1);  /* -2.0V */

    /* 使用内置 LUT */
    epd_write_cmd(h, 0x00);
    epd_write_data(h, (uint8_t[]){0x1F}, 1);

    return EPD_OK;
}
```

### 使用示例

```c
epd_handle_t epd = {
    .dev = EPD_DEV_SSD1680_154, .hw_ctx = &spi_hw, .width = 200, .height = 200,
};

void main(void) {
    epd_init(&epd);
    epd_clear(&epd, EPD_UPDATE_FULL);

    /* 画边框 (全刷) */
    for (int i = 0; i < 200; i++) {
        epd_draw_pixel(&epd, i, 0, EPD_COLOR_BLACK);
        epd_draw_pixel(&epd, i, 199, EPD_COLOR_BLACK);
        epd_draw_pixel(&epd, 0, i, EPD_COLOR_BLACK);
        epd_draw_pixel(&epd, 199, i, EPD_COLOR_BLACK);
    }
    epd_display(&epd, EPD_UPDATE_FULL);

    /* 局刷显示计数器 */
    int count = 0;
    for (int i = 0; i < 20; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "Count:%d", count++);
        /* 绘制文字到显存... */
        epd_display_partial(&epd, 10, 10, 120, 20);
        HAL_Delay(1000);
    }

    /* 局刷20次后全刷消除残影 */
    epd_clear(&epd, EPD_UPDATE_FULL);
    epd_sleep(&epd);  /* 断电保持画面 */
}
```

---

## 4. 调试与测试规范

### 硬件验证清单

- [ ] 万用表确认 VCC = 3.3V，GND 连通
- [ ] SPI 信号 CS/DC/RST/SCK/MOSI 全部连接正确
- [ ] BUSY 引脚已连接 MCU GPIO 输入
- [ ] 上电后 BUSY 先低（复位中）后高（就绪）
- [ ] 100nF + 10μF 去耦电容已贴装
- [ ] FPC 排线插入方向正确且到位

### 通信验证

- **SPI 验证**：逻辑分析仪抓初始化序列，核对命令字节
- **BUSY 验证**：复位后 BUSY 应在 200ms 内变高
- **RST 验证**：拉低 RST 10ms 再拉高，BUSY 应变为低再变高

### 显示验证

| 测试项 | 方法 | 预期结果 |
|--------|------|----------|
| 全刷清屏 | 全刷清白 | 屏幕闪烁后全白 |
| 全黑测试 | 全刷填黑 | 屏幕闪烁后全黑 |
| 棋盘格 | 全刷棋盘图案 | 图案清晰无残影 |
| 局刷测试 | 局刷小区域 | 0.5~1秒快速刷新无闪烁 |
| 残影测试 | 连续局刷20次 | 观察残影程度 |
| 睡眠保持 | 刷画面后断电 | 画面保持不变 |
| 温度测试 | 不同温度下全刷 | 低温下刷新变慢/不完整 |

### 性能指标

| 指标 | 全刷 | 局刷 |
|------|------|------|
| 刷新耗时 | 3~5s | 0.5~1s |
| 刷新电流 | 8~15mA | 8~15mA |
| 待机电流 | 0.01mA | 0.01mA |
| 残影 | 无 | 有(累积) |
| 建议间隔 | 无限制 | 每20次后全刷一次 |

---

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 屏幕不刷新 | BUSY 未释放 | 检查 BUSY 引脚接线；确认 RST 复位成功 |
| 刷新后画面不完整 | 刷新未完成就断电 | 必须等 BUSY 变高后再操作 |
| 局刷有严重残影 | 局刷次数过多 | 每 10~20 次局刷后做一次全刷 |
| 全刷一直闪烁 | LUT 配置错误 | 确认使用正确的全刷 LUT 序列 |
| 屏幕发灰/对比度差 | VCOM 电压不对 | 调整 VCOM 寄存器值 |
| 三色屏红色不显示 | 未写红色显存 | 需同时写入黑白和红色显存 |
| 局刷不支持三色 | 三色屏仅支持全刷 | 三色屏无法局刷，只能全刷 |
| 低温下刷新异常 | 温度影响波形 | 0°C 以下不建议使用；读取温度传感器选择对应 LUT |
| BUSY 一直为低 | 复位失败/接线错 | 检查 RST 接线；尝试硬件复位 |
| 断电后画面消失 | 未完成刷新就断电 | 必须等 BUSY 变高后才能断电/睡眠 |
| 首次使用全白 | 未做首次全刷 | 首次上电必须全刷一次清屏 |
| SPI 通信失败 | 时钟过高 | 降低 SPI 时钟至 ≤ 2MHz |
| 局刷区域错位 | 窗口设置错误 | 核对 RAMX/RAMY 地址计算，注意字节对齐 |
| 长期不刷新屏幕残影 | 双稳态漂移 | 定期(每天)全刷一次消除残影 |
| 睡眠后无法唤醒 | 睡眠模式不对 | 深度睡眠需重新初始化整个序列 |

## 相关文档

- `../../templates/driver-template-spi.c` — SPI 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `../../guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `../../guides/debugging-testing.md` — 调试与测试规范
- `../../guides/pitfalls.md` — 跨类别常见问题与避坑指南
