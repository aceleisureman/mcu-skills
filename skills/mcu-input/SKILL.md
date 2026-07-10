---
name: mcu-input
description: MCU 人机输入与识别模块 Skill，覆盖旋转编码器、独立按键、矩阵键盘、电容触摸，以及指纹、语音识别和视觉识别模块的接口设计、去抖、扫描、事件建模、驱动实现和故障排查。当用户提到 EC11、4x4 键盘、TTP223、MPR121、AS608、ASRPro 或 K210 输入识别时使用。
---

# MCU 输入与识别

## 使用流程

1. 先确认输入数量、响应时间、误触容忍度、环境干扰和交互状态。
2. 基础输入从路由表读取规范；识别类模块按示例读取协议和集成方式。
3. GPIO、ADC、I2C、UART 与中断骨架读取 `skill://mcu-driver-core/SKILL.md`。
4. 输出统一转换为按下、释放、长按、旋转或识别结果等业务事件。

## 意图路由

| 需求或型号 | 读取文件 |
|------------|----------|
| EC11、增量/绝对编码器 | `references/rotary-encoder.md` |
| 独立按键、矩阵键盘、ADC 键盘 | `references/keypad.md` |
| TTP223、TTP229、MPR121、MCU 电容触摸 | `references/touch.md` |
| AS608 指纹 | `examples/fingerprint/` |
| ASRPro 语音、K210 视觉识别 | `examples/asrpro-voice/` |
| 按键与矩阵键盘实现 | `examples/keyboard/` |

## 输出要求

- 去抖和手势识别使用非阻塞状态机，不在中断中延时。
- 矩阵扫描说明鬼键、上拉、扫描周期和多键冲突策略。
- 生物识别结果只输出必要标识，不记录或泄露原始敏感数据。
