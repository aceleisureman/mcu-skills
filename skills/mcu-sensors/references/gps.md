# GPS/GNSS 定位模块开发规范

## 1. 概述与选型指南

### 常见型号对比

| 型号 | 系统 | 接口 | 定位精度 | 冷启动 | 特点 | 价格 |
|------|------|------|----------|--------|------|------|
| NEO-6M | GPS | UART | 2.5m | 27s | 经典入门 | ¥15~30 |
| NEO-M8N | GPS+北斗+GLONASS | UART/I2C | 2m | 26s | 多系统、搜星快 | ¥30~60 |
| ATGM336H | GPS+北斗 | UART | 2.5m | 32s | 国产低价 | ¥10~20 |
| L76K | GPS+北斗+GLONASS | UART | 2m | 30s | 低功耗、可穿戴 | ¥15~30 |
| ZED-F9P | 多系统 RTK | UART/SPI/I2C | 1cm(RTK) | 24s | 厘米级、需基准站 | ¥1000+ |

### 选型决策树

```
需要厘米级精度？ → 是 → ZED-F9P + RTK 差分服务
低功耗电池设备？ → 是 → L76K（间歇定位模式）
国内使用/成本敏感？ → 是 → ATGM336H（支持北斗）
通用推荐 → NEO-M8N
```

## 2. 硬件设计规范

### 典型电路

```
模块 VCC ── 3.3V（LDO 独立供电优先，≥100mA）
模块 TX ── MCU RX（默认 9600bps）
模块 RX ── MCU TX
V_BKP ── 纽扣电池/超级电容（保存星历，热启动 1s）
天线: 有源陶瓷天线，馈线 50Ω
```

**要点**：
- 天线必须朝天且远离金属遮挡；室内基本无法定位，调试放窗边
- V_BKP 备份电源强烈建议接（否则每次都是冷启动 30s+）
- 有源天线馈电由模块提供，检查天线短路会导致模块发热
- GPS 射频线走 50Ω 阻抗、包地，远离开关电源与 MCU 晶振

## 3. 驱动开发规范

### 通信协议说明

NMEA-0183 文本协议，每条以 `$` 开头 `\r\n` 结尾，常用语句：
- `$GNRMC`：推荐最简定位信息（时间/状态/经纬度/速度/日期）
- `$GNGGA`：定位数据（经纬度/海拔/卫星数/HDOP）
- 校验：`*` 后两位十六进制 = `$` 与 `*` 之间字符异或

### 统一接口定义

```c
typedef struct {
    bool   fixed;          /* 是否已定位 */
    double lat, lon;       /* 十进制度, 南纬/西经为负 */
    float  altitude_m;
    float  speed_kmh;
    uint8_t satellites;
    struct { uint8_t hh, mm, ss; } utc;
} gps_data_t;

void gps_init(gps_handle_t *h, uart_hal_t *uart);
void gps_feed(gps_handle_t *h, uint8_t byte);   /* UART 每收 1 字节喂入 */
bool gps_get(gps_handle_t *h, gps_data_t *out);
```

### NMEA 解析核心（RMC）

```c
static bool nmea_checksum_ok(const char *s) {          /* s 指向 '$' */
    uint8_t cs = 0; s++;
    while (*s && *s != '*') cs ^= (uint8_t)*s++;
    return *s == '*' && cs == (uint8_t)strtol(s + 1, NULL, 16);
}

/* ddmm.mmmm → 十进制度 */
static double nmea_to_deg(const char *f, char hemi) {
    double v = atof(f);
    double deg = (int)(v / 100) + fmod(v, 100.0) / 60.0;
    return (hemi == 'S' || hemi == 'W') ? -deg : deg;
}

bool gps_parse_rmc(const char *line, gps_data_t *d) {  /* $GNRMC,... */
    if (!nmea_checksum_ok(line)) return false;
    char buf[96]; strncpy(buf, line, sizeof(buf) - 1);
    char *tok[13] = {0}; int n = 0;
    for (char *p = strtok(buf, ","); p && n < 13; p = strtok(NULL, ","))
        tok[n++] = p;
    if (n < 10 || tok[2][0] != 'A') { d->fixed = false; return true; }
    d->fixed = true;
    d->lat = nmea_to_deg(tok[3], tok[4][0]);
    d->lon = nmea_to_deg(tok[5], tok[6][0]);
    d->speed_kmh = atof(tok[7]) * 1.852f;              /* 节 → km/h */
    return true;
}
```

**注意**：UART 接收用环形缓冲 + 按行分割后再解析；不要在中断里做字符串解析。

## 4. 调试与测试规范

### 硬件验证清单
- [ ] 模块 PPS 灯定位成功后 1Hz 闪烁
- [ ] 串口 9600bps 能收到 `$GN…`/`$GP…` 语句流
- [ ] 天线接触良好，有源天线电流 5~20mA

### 功能测试
- 室外空旷处冷启动 ≤ 60s 定位；接 V_BKP 后热启动 ≤ 5s
- 与手机地图对比坐标偏差 < 10m
- 校验和错误语句必须被丢弃（人为注入错误帧验证）

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| 一直无法定位 | 室内/天线遮挡 | 移至室外空旷处，检查天线连接 |
| 每次上电都冷启动很慢 | V_BKP 未接 | 接纽扣电池保存星历 |
| 收到乱码 | 波特率不匹配 | 默认 9600，被配置过则尝试 38400/115200 |
| 经纬度偏差几百米 | ddmm.mmmm 当十进制度用 | 按 度+分/60 正确转换 |
| 数据偶尔跳变 | 未校验直接使用 | 校验和验证 + fixed 状态判断 + HDOP 过滤 |
| 与 WiFi/4G 模块同板干扰 | 射频干扰 | GPS 天线远离发射天线 ≥ 5cm，加屏蔽罩 |

## 相关文档

- `skill://mcu-driver-core/templates/driver-template-uart.c` — UART 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
- `skill://mcu-driver-core/guides/pitfalls.md` — 跨类别常见问题与避坑指南
