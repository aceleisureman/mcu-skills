# RTC 实时时钟开发规范

## 1. 概述与选型指南

### 器件简介

RTC（Real-Time Clock，实时时钟）是一种专用于保持时间和日期的计时芯片，通过外部晶振（32.768kHz）或内置振荡器提供精确的秒/分/时/日/月/年计时。配合备份电池，可在主系统断电后持续计时。广泛应用于数据日志时间戳、定时唤醒、闹钟事件等场景。

### 常见型号对比

| 型号 | 接口 | 精度 | 振荡器 | 闹钟 | 温补 | 内置温控 | 备用电池 | 价格 |
|------|------|------|--------|------|------|----------|----------|------|
| DS3231 | I2C | ±2ppm (~1分钟/年) | 内置 TCXO | 2路 | 是 | 是 | 是 | ¥8~15 |
| PCF8563 | I2C | ±5ppm (需外部晶振) | 外部 32.768kHz | 1路 | 否 | 否 | 是 | ¥1~3 |
| DS1307 | I2C | ±20ppm (需外部晶振) | 外部 32.768kHz | 1路(简单) | 否 | 否 | 是 | ¥3~6 |
| DS1302 | 3线 | ±20ppm | 外部 32.768kHz | 否 | 否 | 否 | 是 | ¥1~3 |
| DS3234 | SPI | ±2ppm | 内置 TCXO | 2路 | 是 | 是 | 是 | ¥15~25 |
| RV-3028 | I2C | ±1ppm | 内置 RC | 1路 | 是 | 否 | 是 | ¥10~20 |

### 选型决策树

```
需要高精度 (≤ ±2ppm)？ → 是 → DS3231 (I2C, 内置TCXO, 最常用高精度)
                          否 → DS3234 (SPI 接口高精度)
需要低成本？ → 是 → PCF8563 (I2C, ¥1~3, 性价比最高)
需要多路闹钟？ → 是 → DS3231 (2路闹钟)
需要温度补偿但不用 DS3231？ → 是 → RV-3028 (±1ppm, 低功耗)
MCU 无 I2C？ → 是 → DS1302 (3线接口)
通用推荐:
  高精度 → DS3231 (±2ppm, 内置TCXO, 带温度)
  低成本 → PCF8563 (¥1~3, 功能足够)
```

### 适用场景

- **DS3231**：高精度计时、数据日志时间戳、定时唤醒、需要温度监测
- **PCF8563**：低成本计时、定时闹钟、低功耗应用
- **DS1307**：旧设计兼容、简单计时需求

## 2. 硬件设计规范

### DS3231 电路 (I2C, 高精度)

```
VCC(3.3V/5V)
  │
  ├── 4.7kΩ ── SCL ── MCU I2C_SCL ──┐
  ├── 4.7kΩ ── SDA ── MCU I2C_SDA ──┤
  │                                   │
  │     ┌─────────────────────────┐   │
  │     │       DS3231            │   │
  │     │  (SOIC-16 封装)         │   │
  │     │                         │   │
  └─────┤ VCC(2)     SCL(8) ──────┼───┘
        │                         │
        │ 32kHz(1) ── NC          │    (方波输出, 不用悬空)
        │ INT/SQW(3) ── MCU GPIO  │    (中断/方波输出)
        │ RST(4) ── NC            │    (复位, 不用悬空)
        │                         │
  BAT+ ─┤ VBAT(6) ── CR2032(3V)   │    (备份电池)
        │                         │
        │ GND(5)                  │
        │  32.768kHz 晶振已内置!   │    (无需外部晶振)
        │     SDA(7) ─────────────┼───┘
        └─────────────────────────┘

  VCC ── 100nF ── GND  (去耦电容)
  VBAT ── 100nF ── GND (电池去耦)
```

### PCF8563 电路 (I2C, 低成本)

```
VCC(3.3V)
  │
  ├── 4.7kΩ ── SCL ── MCU I2C_SCL ──┐
  ├── 4.7kΩ ── SDA ── MCU I2C_SDA ──┤
  │                                   │
  │     ┌─────────────────────────┐   │
  │     │       PCF8563           │   │
  │     │  (SOIC-8/TSSOP-8)      │   │
  │     │                         │   │
  └─────┤ VDD(8)     SCL(6) ──────┼───┘
        │                         │
        │ OSCI(1) ─┐              │
        │          │  32.768kHz   │   (外部晶振, 12.5pF负载)
        │ OSCO(2) ─┘  晶振        │
        │                         │
        │ INT(3) ── MCU GPIO      │   (闹钟中断, 开漏输出)
        │                         │
  BAT+ ─┤ VBAT(6) ── CR2032(3V)   │   (备份电池)
   GND ─┤ VSS(4)                  │
        │     SDA(5) ─────────────┼───┘
        └─────────────────────────┘

  VDD ── 100nF ── GND
  晶振两端各接 12.5pF 负载电容到 GND
```

### DS1307 电路 (I2C, 经典)

```
VCC(5V)  ← 注意: DS1307 需要 5V 供电!
  │
  ├── 4.7kΩ ── SCL ── MCU I2C_SCL ──┐
  ├── 4.7kΩ ── SDA ── MCU I2C_SDA ──┤
  │                                   │
  │     ┌─────────────────────────┐   │
  │     │       DS1307            │   │
  │     │  (DIP-8/SOIC-8)        │   │
  │     │                         │   │
  └─────┤ VCC(8)     SCL(6) ──────┼───┘
        │                         │
        │ X1(1) ─┐                │
        │        │  32.768kHz     │   (外部晶振, 12.5pF)
        │ X2(2) ─┘  晶振          │
        │                         │
        │ SQW(7) ── MCU GPIO/NC   │   (方波输出)
        │                         │
  BAT+ ─┤ VBAT(3) ── CR2032(3V)   │   (备份电池)
   GND ─┤ GND(4)                  │
        │     SDA(5) ─────────────┼───┘
        └─────────────────────────┘

  注意: DS1307 只支持 5V 供电, I2C 为 5V 电平
        与 3.3V MCU 连接需电平转换!
```

### 备份电池电路

```
方案1: CR2032 纽扣电池 (最常用)
  CR2032(+) ──┬── VBAT
              │
              ├── 100nF ── GND  (去耦)
              │
              └── (无需限流电阻)

方案2: 法拉电容/超级电容 (免维护)
  VCC ── 1N4148(或肖特基) ──┬── VBAT
                             │
                             ├── 0.47F 超级电容 ── GND
                             │
                             └── 100nF ── GND

  超级电容方案: 主电源断电后电容供电
  充电时间: τ = R × C, 1kΩ × 0.47F ≈ 470s (需限流充电)
  备用时间: 0.47F / 1μA(RTC待机) ≈ 117500s ≈ 32小时
```

### 晶振电路要点

```
32.768kHz 晶振 (PCF8563 / DS1307):
  OSCI ─┬── 晶振 ──┐ OSCO
        │           │
        ├── CL1 ── GND
        └── CL2 ── GND

  CL1, CL2 = 负载电容, 计算: CL = (CL1 × CL2)/(CL1+CL2) + Cstray
  典型值: CL1=CL2=12.5pF (晶振规格 12.5pF)
  Cstray (杂散电容) ≈ 3~5pF (PCB走线寄生)
  实际选值: (12.5-3.5)×2 = 18pF → 用 18~22pF

  DS3231 内置 TCXO, 无需外部晶振和负载电容!
```

### PCB 设计要点

- 晶振尽量靠近 RTC 芯片，走线尽量短
- 晶振下方禁止走信号线，铺地铜隔离干扰
- VBAT 旁放 100nF 去耦电容
- 电池正极走线加宽，避免与高频信号平行走线
- I2C 上拉电阻 4.7kΩ~10kΩ
- INT/SQW 中断引脚为开漏输出，需上拉
- DS1307 为 5V 器件，与 3.3V MCU 需电平转换

### 电气参数限制

| 参数 | DS3231 | PCF8563 | DS1307 |
|------|--------|---------|--------|
| 工作电压 | 2.3~5.5V | 1.0~3.6V | 4.5~5.5V |
| VBAT 电压 | 2.3~5.5V | 1.0~3.6V | 2.0~5.5V |
| I2C 最大频率 | 400kHz | 400kHz | 100kHz |
| 计时精度 | ±2ppm (-40~+85°C) | ±5ppm (@25°C) | ±20ppm |
| VBAT 电流 | 0.8μA (typ) | 0.25μA (typ) | 0.3μA (typ) |
| 工作电流 | 0.7mA (typ) | 0.25μA (typ) | 1.5mA (typ) |
| 温度范围 | -40~+85°C | -40~+85°C | 0~+70°C |

## 3. 驱动开发规范

### BCD 码转换

RTC 时间数据以 BCD（Binary-Coded Decimal）码格式存储，需要与二进制互转。

```c
/* === BCD 码转换工具函数 === */

/* BCD 转十进制: 如 0x59 → 59 */
static uint8_t bcd_to_dec(uint8_t bcd) {
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

/* 十进制转 BCD: 如 59 → 0x59 */
static uint8_t dec_to_bcd(uint8_t dec) {
    return ((dec / 10) << 4) | (dec % 10);
}

/* BCD 码格式说明:
 * 高4位: 十位 (0~5)
 * 低4位: 个位 (0~9)
 * 示例: 十进制 23 → BCD 0x23, 十进制 59 → BCD 0x59
 * 秒寄存器 Bit7: CH (Clock Halt, DS1307) / VL (PCF8563 低电压标志)
 * 小时寄存器 Bit6: 12/24小时模式选择 (DS1307)
 */
```

### 时间数据结构

```c
typedef struct {
    uint16_t year;    /* 年: 2000~2099 */
    uint8_t  month;   /* 月: 1~12 */
    uint8_t  day;     /* 日: 1~31 */
    uint8_t  weekday; /* 星期: 1=周一 ~ 7=周日 */
    uint8_t  hour;    /* 时: 0~23 */
    uint8_t  minute;  /* 分: 0~59 */
    uint8_t  second;  /* 秒: 0~59 */
} rtc_time_t;

typedef struct {
    i2c_bus_t *bus;
    uint8_t dev_addr;
} rtc_handle_t;

typedef enum {
    RTC_OK = 0,
    RTC_ERR_NOT_FOUND,
    RTC_ERR_TIMEOUT,
    RTC_ERR_INVALID_TIME,
    RTC_ERR_LOW_VOLTAGE,
} rtc_status_t;
```

### DS3231 寄存器表

| 地址 | 寄存器 | 说明 |
|------|--------|------|
| 0x00 | 秒 | BCD 格式, Bit7=0 |
| 0x01 | 分 | BCD 格式 |
| 0x02 | 时 | BCD, Bit6=24h模式 |
| 0x03 | 星期 | 1~7 |
| 0x04 | 日 | BCD 格式 |
| 0x05 | 月 | BCD, Bit7=世纪 |
| 0x06 | 年 | BCD, 00~99 |
| 0x07-0x0A | 闹钟1 | 秒/分/时/日 |
| 0x0B-0x0D | 闹钟2 | 分/时/日 |
| 0x0E | 控制寄存器 | EOSC/BBSQW/CONV/INTCN/A2IE/A1IE |
| 0x0F | 状态寄存器 | OSF/EN32kHz/BSY/A2F/A1F |
| 0x11 | 温度高位 | 符号位 + 8bit 温度 |
| 0x12 | 温度低位 | Bit7-6: 0.25°C 分辨率 |

### DS3231 I2C 驱动核心代码

```c
#define DS3231_ADDR  0x68  /* DS3231 I2C 设备地址 */

/* === 初始化 === */
rtc_status_t ds3231_init(rtc_handle_t *h, i2c_bus_t *bus) {
    h->bus = bus;
    h->dev_addr = DS3231_ADDR;

    if (!i2c_check_ack(bus, h->dev_addr))
        return RTC_ERR_NOT_FOUND;

    /* 读取状态寄存器, 检查振荡器停机标志 (OSF) */
    uint8_t status;
    if (i2c_read_reg(bus, h->dev_addr, 0x0F, &status, 1) != I2C_OK)
        return RTC_ERR_TIMEOUT;

    if (status & 0x80) {
        /* OSF=1: 振荡器停机 (首次上电或电池耗尽), 需重新设时间 */
        return RTC_ERR_INVALID_TIME;
    }

    /* 清除 OSF 标志 */
    status &= ~0x80;
    i2c_write_reg(bus, h->dev_addr, 0x0F, &status, 1);

    /* 控制寄存器: INTCN=1 (中断输出), 关闭方波 */
    uint8_t ctrl = 0x04;  /* INTCN=1 */
    i2c_write_reg(bus, h->dev_addr, 0x0E, &ctrl, 1);

    return RTC_OK;
}

/* === 设置时间 === */
rtc_status_t ds3231_set_time(rtc_handle_t *h, const rtc_time_t *t) {
    if (t->second > 59 || t->minute > 59 || t->hour > 23 ||
        t->day < 1 || t->day > 31 || t->month < 1 || t->month > 12 ||
        t->year < 2000 || t->year > 2099)
        return RTC_ERR_INVALID_TIME;

    uint8_t buf[7];
    buf[0] = dec_to_bcd(t->second);
    buf[1] = dec_to_bcd(t->minute);
    buf[2] = dec_to_bcd(t->hour);        /* 24小时模式 */
    buf[3] = dec_to_bcd(t->weekday);
    buf[4] = dec_to_bcd(t->day);
    buf[5] = dec_to_bcd(t->month);
    buf[6] = dec_to_bcd(t->year - 2000);

    if (i2c_write_reg(h->bus, h->dev_addr, 0x00, buf, 7) != I2C_OK)
        return RTC_ERR_TIMEOUT;

    /* 清除振荡器停机标志 */
    uint8_t status;
    i2c_read_reg(h->bus, h->dev_addr, 0x0F, &status, 1);
    status &= ~0x80;
    i2c_write_reg(h->bus, h->dev_addr, 0x0F, &status, 1);

    return RTC_OK;
}

/* === 读取时间 === */
rtc_status_t ds3231_get_time(rtc_handle_t *h, rtc_time_t *t) {
    uint8_t buf[7];
    if (i2c_read_reg(h->bus, h->dev_addr, 0x00, buf, 7) != I2C_OK)
        return RTC_ERR_TIMEOUT;

    t->second  = bcd_to_dec(buf[0] & 0x7F);  /* 屏蔽 CH 位 */
    t->minute  = bcd_to_dec(buf[1] & 0x7F);
    t->hour    = bcd_to_dec(buf[2] & 0x3F);  /* 24h, 屏蔽 Bit6-7 */
    t->weekday = bcd_to_dec(buf[3] & 0x07);
    t->day     = bcd_to_dec(buf[4] & 0x3F);
    t->month   = bcd_to_dec(buf[5] & 0x1F);  /* 屏蔽世纪位 */
    t->year    = bcd_to_dec(buf[6]) + 2000;

    return RTC_OK;
}

/* === 读取温度 (DS3231 内置温度传感器, ±3°C 精度) === */
rtc_status_t ds3231_get_temp(rtc_handle_t *h, float *temp) {
    uint8_t buf[2];
    if (i2c_read_reg(h->bus, h->dev_addr, 0x11, buf, 2) != I2C_OK)
        return RTC_ERR_TIMEOUT;

    /* 温度: 高8位为整数部分 (有符号), 低2位为小数 (0.25°C/LSB) */
    int8_t temp_msb = (int8_t)buf[0];
    uint8_t temp_lsb = buf[1];
    *temp = temp_msb + (temp_lsb >> 6) * 0.25f;

    return RTC_OK;
}

/* === 设置闹钟1 (每日定时) === */
rtc_status_t ds3231_set_alarm1(rtc_handle_t *h,
                                uint8_t hour, uint8_t minute,
                                uint8_t second) {
    /* 闹钟1 匹配模式: 时分秒匹配 (A1M4=1, A1M3=0, A1M2=0, A1M1=0) */
    uint8_t alarm[4];
    alarm[0] = dec_to_bcd(second) | 0x80;  /* A1M1=1: 秒匹配 */
    alarm[1] = dec_to_bcd(minute);          /* A1M2=0: 分匹配 */
    alarm[2] = dec_to_bcd(hour);            /* A1M3=0: 时匹配 */
    alarm[3] = 0x80;                        /* A1M4=1: 日期不匹配(每日触发) */

    if (i2c_write_reg(h->bus, h->dev_addr, 0x07, alarm, 4) != I2C_OK)
        return RTC_ERR_TIMEOUT;

    /* 使能闹钟1中断 */
    uint8_t ctrl;
    i2c_read_reg(h->bus, h->dev_addr, 0x0E, &ctrl, 1);
    ctrl |= 0x05;  /* INTCN=1, A1IE=1 */
    i2c_write_reg(h->bus, h->dev_addr, 0x0E, &ctrl, 1);

    return RTC_OK;
}

/* === 清除闹钟标志 === */
rtc_status_t ds3231_clear_alarm(rtc_handle_t *h) {
    uint8_t status;
    i2c_read_reg(h->bus, h->dev_addr, 0x0F, &status, 1);
    status &= ~0x03;  /* 清除 A1F 和 A2F */
    i2c_write_reg(h->bus, h->dev_addr, 0x0F, &status, 1);
    return RTC_OK;
}

/* === 格式化时间字符串 === */
void rtc_format_time(const rtc_time_t *t, char *buf, size_t len) {
    snprintf(buf, len, "%04d-%02d-%02d %02d:%02d:%02d",
             t->year, t->month, t->day, t->hour, t->minute, t->second);
}
```

### PCF8563 驱动差异

```c
#define PCF8563_ADDR  0x51  /* PCF8563 I2C 地址 */

/* PCF8563 与 DS3231 主要差异:
 * 1. I2C 地址不同: 0x51 (vs DS3231 的 0x68)
 * 2. 秒寄存器 Bit7 = VL (Voltage Low), 电池低电压标志
 * 3. 无内置温度传感器
 * 4. 仅1路闹钟 (DS3231有2路)
 * 5. 需外部晶振 (DS3231内置TCXO)
 */

rtc_status_t pcf8563_get_time(rtc_handle_t *h, rtc_time_t *t) {
    uint8_t buf[7];
    if (i2c_read_reg(h->bus, h->dev_addr, 0x02, buf, 7) != I2C_OK)
        return RTC_ERR_TIMEOUT;

    /* 检查低电压标志 (VL bit) */
    if (buf[0] & 0x80) {
        return RTC_ERR_LOW_VOLTAGE;  /* 电池电压低, 数据可能不可靠 */
    }

    t->second  = bcd_to_dec(buf[0] & 0x7F);
    t->minute  = bcd_to_dec(buf[1] & 0x7F);
    t->hour    = bcd_to_dec(buf[2] & 0x3F);
    t->day     = bcd_to_dec(buf[3] & 0x3F);
    t->weekday = bcd_to_dec(buf[4] & 0x07);
    t->month   = bcd_to_dec(buf[5] & 0x1F);
    t->year    = bcd_to_dec(buf[6]) + 2000;

    return RTC_OK;
}
```

## 4. 调试与测试规范

### 硬件验证清单

- [ ] 万用表确认 VCC 电压正确（DS3231: 2.3~5.5V, PCF8563: 1.0~3.6V）
- [ ] I2C SCL/SDA 上拉电阻（4.7kΩ）已安装
- [ ] 备份电池 CR2032 电压 > 3.0V
- [ ] DS3231 无需晶振；PCF8563/DS1307 外部晶振已正确焊接
- [ ] 100nF 去耦电容已贴装
- [ ] INT/SQW 引脚接 MCU GPIO 并加上拉

### 通信验证

```c
/* RTC 连接验证 */
rtc_status_t rtc_verify(rtc_handle_t *h) {
    if (!i2c_check_ack(h->bus, h->dev_addr)) {
        printf("RTC not found at addr 0x%02X\n", h->dev_addr);
        return RTC_ERR_NOT_FOUND;
    }

    rtc_time_t t;
    rtc_status_t st = ds3231_get_time(h, &t);
    if (st != RTC_OK) return st;

    /* 检查时间合理性 */
    if (t.year < 2000 || t.year > 2099 || t.second > 59) {
        printf("RTC time invalid, needs initialization\n");
        return RTC_ERR_INVALID_TIME;
    }

    char time_str[32];
    rtc_format_time(&t, time_str, sizeof(time_str));
    printf("RTC time: %s\n", time_str);
    return RTC_OK;
}
```

### 精度测试

```c
/* RTC 精度测试: 对比 NTP/GPS 时间 */
void rtc_accuracy_test(rtc_handle_t *h) {
    printf("RTC accuracy test (run for 24 hours):\n");
    printf("1. Record RTC time and reference time (NTP/GPS)\n");
    printf("2. Wait 24 hours\n");
    printf("3. Compare RTC time with reference\n");

    rtc_time_t t;
    ds3231_get_time(h, &t);
    char time_str[32];
    rtc_format_time(&t, time_str, sizeof(time_str));
    printf("Start: %s\n", time_str);

    /* 24小时后预期漂移:
     * DS3231: ±2ppm → ±0.17s/天 → ±1分钟/年
     * PCF8563: ±5ppm → ±0.43s/天 → ±2.6分钟/年
     * DS1307: ±20ppm → ±1.7s/天 → ±10.5分钟/年
     */
}
```

### 备份电池测试

```c
/* 掉电保持测试 */
void rtc_battery_test(rtc_handle_t *h) {
    printf("RTC battery backup test:\n");
    printf("1. Set RTC time to current time\n");
    printf("2. Disconnect main power (keep battery)\n");
    printf("3. Wait 1 minute (or longer)\n");
    printf("4. Reconnect power, check if time continued\n");

    rtc_time_t t;
    /* 设置时间为当前 */
    t.year = 2024; t.month = 1; t.day = 15;
    t.hour = 10; t.minute = 30; t.second = 0;
    t.weekday = 1;
    ds3231_set_time(h, &t);
    printf("Time set. Now disconnect power and test.\n");
}
```

### 性能测试指标

| 指标 | DS3231 | PCF8563 | DS1307 |
|------|--------|---------|--------|
| 计时精度 (@25°C) | ±2ppm | ±5ppm | ±20ppm |
| 年漂移 | ~1分钟 | ~2.6分钟 | ~10.5分钟 |
| I2C 读取时间 | <1ms | <1ms | <1ms |
| VBAT 待机电流 | 0.8μA | 0.25μA | 0.3μA |
| 温度精度 | ±3°C | N/A | N/A |
| 闹钟路数 | 2 | 1 | 1(简单) |

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| I2C 扫描找不到 RTC | 地址错误或接线问题 | DS3231=0x68, PCF8563=0x51, 检查接线 |
| 时间读到全 0 | 振荡器未启动 (OSF=1) | 首次使用需设置时间，清除 OSF 标志 |
| 时间走时偏快/偏慢 | 晶振精度不足 | DS3231 内置 TCXO; PCF8563 调整负载电容 |
| 掉电后时间丢失 | 电池未安装或耗尽 | 检查 CR2032 电压 > 3.0V, 正确接入 VBAT |
| DS1307 无法与 3.3V MCU 通信 | DS1307 为 5V 器件 | 使用电平转换或改用 DS3231/PCF8563 |
| 读取时间偶尔错误 | I2C 读取时跨秒翻转 | 读取后验证连续性，异常则重读 |
| 闰年/闰月日期错误 | 软件未处理闰年 | RTC 硬件自动处理闰年，检查软件解析 |
| PCF8563 VL 标志置位 | 电池电压低 | 更换电池，清除 VL 标志 |
| DS3231 温度读数偏差 | 温度精度 ±3°C | 仅作参考温度，不可用于精确测温 |
| 闹钟不触发 | INTCN 未置位或 A1IE/A2IE 未使能 | 设置控制寄存器 INTCN=1, A1IE=1 |
| 闹钟只触发一次 | 闹钟标志未清除 | 中断处理中清除 A1F/A2F 标志 |
| 晶振不起振 | 负载电容不匹配 | 核对晶振规格，调整 CL1/CL2 电容值 |
| 32kHz 输出异常 | 控制寄存器配置错误 | 检查 EN32kHz 位和 RS1/RS2 分频设置 |

## 相关文档

- `../../templates/driver-template-i2c.c` — I2C 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-spi.c` — SPI 驱动模板（HAL 抽象层骨架）
- `../../templates/driver-template-gpio.c` — GPIO 驱动模板（HAL 抽象层骨架）
- `../../guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `../../guides/debugging-testing.md` — 调试与测试规范
- `../../guides/pitfalls.md` — 跨类别常见问题与避坑指南
