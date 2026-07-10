# 音频播放模块开发规范

## 1. 概述与选型指南

### 常见型号对比

| 型号 | 类型 | 接口 | 音源 | 特点 | 价格 |
|------|------|------|------|------|------|
| DFPlayer Mini | MP3 模块 | UART | TF 卡 | 自带 3W 功放、指令简单 | ¥8~15 |
| JQ8900/JQ6500 | 语音模块 | UART/一线 | 内置 Flash | 出厂烧录语音、提示音场景 | ¥5~12 |
| MAX98357A | I2S 数字功放 | I2S | MCU 供流 | 无损数字音频、3.2W D类 | ¥6~12 |
| PCM5102A | I2S DAC | I2S | MCU 供流 | HiFi 输出接外部功放 | ¥8~15 |
| WT588D | 语音芯片 | 一线/三线 | 内置 Flash | 工业提示音 | ¥5~10 |

### 选型决策树

```
播放 TF 卡里的 MP3？ → DFPlayer Mini（UART 指令即可）
固定提示音（"开机成功"）？ → JQ8900/WT588D（出厂烧录）
ESP32 播放网络流/TTS？ → MAX98357A（I2S 直推喇叭）
音质要求高？ → PCM5102A + 独立功放
```

## 2. 硬件设计规范

### DFPlayer Mini 电路

```
VCC ── 4.2~5V（3.3V 供电音量大时会复位，建议 5V + 1000μF）
TX/RX ── MCU（3.3V MCU 的 TX 串 1kΩ 接模块 RX）
SPK1/SPK2 ── 4Ω/8Ω 喇叭（差分直推，勿接地）
BUSY ── MCU GPIO（低 = 播放中）
```

### MAX98357A（I2S）

```
BCLK / LRC / DIN ── MCU I2S
GAIN: 悬空 9dB, 接 GND 12dB, 接 VDD 6dB
SD 引脚: 拉高使能; 100k 上拉选左声道
输出直接接 4Ω/8Ω 喇叭（D 类桥式输出，两端都不可接地）
```

**要点**：
- 功放电源与 MCU 电源分开滤波（磁珠 + 大电容），防止播放时电源跌落复位
- 喇叭线用双绞线且尽量短；D 类输出线过长需加 LC 滤波（33μH+1μF）
- 音频线远离数字高速线，模拟地单点接数字地

## 3. 驱动开发规范

### 统一接口定义

```c
typedef enum { AUDIO_OK = 0, AUDIO_ERR_UART, AUDIO_ERR_BUSY, AUDIO_ERR_NO_FILE } audio_status_t;

audio_status_t audio_init(audio_handle_t *h);
audio_status_t audio_play(audio_handle_t *h, uint16_t track);  /* 播放第 n 首 */
audio_status_t audio_stop(audio_handle_t *h);
audio_status_t audio_set_volume(audio_handle_t *h, uint8_t vol /*0~30*/);
bool           audio_is_busy(audio_handle_t *h);
```

### DFPlayer 协议驱动（UART 9600bps）

```c
/* 帧: 7E FF 06 CMD FBACK PARA_H PARA_L CKS_H CKS_L EF */
static void df_send(audio_handle_t *h, uint8_t cmd, uint16_t param) {
    uint8_t f[10] = {0x7E, 0xFF, 0x06, cmd, 0x00,
                     (uint8_t)(param >> 8), (uint8_t)param, 0, 0, 0xEF};
    uint16_t sum = 0;
    for (int i = 1; i <= 6; i++) sum += f[i];
    sum = 0xFFFF - sum + 1;                 /* 校验 = 0 - 求和 */
    f[7] = sum >> 8; f[8] = sum & 0xFF;
    uart_write(h->uart, f, 10);
}

audio_status_t audio_play(audio_handle_t *h, uint16_t track) {
    df_send(h, 0x03, track);                /* 按索引播放 */
    return AUDIO_OK;
}
audio_status_t audio_set_volume(audio_handle_t *h, uint8_t vol) {
    df_send(h, 0x06, vol > 30 ? 30 : vol);
    return AUDIO_OK;
}
/* 上电后需等模块初始化完成(约1s, 或等 0x3F 上报)再发指令 */
```

### ESP32 + MAX98357A（I2S 播放 PCM）

```c
i2s_config_t cfg = {
    .mode = I2S_MODE_MASTER | I2S_MODE_TX,
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .dma_buf_count = 8, .dma_buf_len = 256,
};
i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
i2s_pin_config_t pins = {.bck_io_num = 26, .ws_io_num = 25,
                         .data_out_num = 22, .data_in_num = -1};
i2s_set_pin(I2S_NUM_0, &pins);

size_t written;
i2s_write(I2S_NUM_0, pcm_buf, pcm_len, &written, portMAX_DELAY);
```

## 4. 调试与测试规范

### 硬件验证清单
- [ ] DFPlayer 上电后 TF 卡识别（收到 0x3F 初始化上报）
- [ ] 播放时 BUSY 引脚变低
- [ ] 最大音量播放时电源电压跌落 < 0.3V

### 功能测试
- 曲目索引与文件对应正确（TF 卡文件按 0001.mp3 命名，复制顺序决定索引）
- 音量 0~30 全程无爆音；连续切歌 100 次无死机
- I2S 播放 1kHz 正弦测试音，示波器/听感无失真

## 5. 常见问题与避坑指南

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| DFPlayer 无响应 | 上电未就绪就发指令 | 上电延时 1~2s 或等待 0x3F 上报 |
| 播放顺序与文件名不符 | 索引按 FAT 写入顺序 | 文件命名 0001~9999.mp3 并按序复制 |
| 播放时模块复位 | 电源带不动功放峰值电流 | 5V 供电 + 1000μF 储能电容 |
| 3.3V MCU 通信误码 | 电平不匹配 | MCU TX 串 1kΩ；或用电平转换 |
| 有底噪/滋滋声 | 数字线干扰模拟部分 | 音频地单点接地、电源磁珠隔离 |
| I2S 无声 | 引脚/声道配置错 | 核对 BCLK/LRC/DIN、GAIN 与 SD 引脚电平 |
| 声音断续 | DMA 缓冲不足 | 增大 dma_buf_count/len，提高供流任务优先级 |

## 相关文档

- `skill://mcu-driver-core/templates/driver-template-uart.c` — UART 驱动模板（HAL 抽象层骨架）
- `skill://mcu-driver-core/guides/hardware-design.md` — 通用硬件设计规范（电源/去耦/PCB/EMC）
- `skill://mcu-driver-core/guides/debugging-testing.md` — 调试与测试规范
- `skill://mcu-driver-core/guides/pitfalls.md` — 跨类别常见问题与避坑指南
