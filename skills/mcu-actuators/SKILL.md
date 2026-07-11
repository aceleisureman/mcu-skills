---
name: mcu-actuators
description: MCU 执行器选型与驱动 Skill，覆盖直流电机、步进电机、舵机、继电器、电磁阀、电磁铁、蜂鸣器和音频播放模块的功率电路、保护设计、PWM/方向控制、运动控制、驱动实现和故障排查。当用户提到 L298N、TB6612、A4988、舵机、继电器、DFPlayer、蜂鸣器等执行器时使用。
---

# MCU 执行器

## 使用流程

1. 先确认负载电压、电流、启动/堵转电流、动作频率和安全状态。
2. 从路由表读取对应规范，再选择功率器件、保护电路与控制接口。
3. 通用 GPIO、PWM、UART 和工程规范读取 `skill://mcu-driver-core/SKILL.md`。
4. 输出必须包含失效保护、上电默认状态和感性负载续流方案。

## 意图路由

> 下表由 `python3 tools/skill_registry.py --write` 从 skill.json 生成，请勿手工编辑。

<!-- GENERATED:ROUTE_TABLE:START -->
| 意图 | 关键词或型号 | 读取文件 |
|------|------------|----------|
| 直流电机与 H 桥 | 直流电机、L298N、TB6612、H 桥 | `references/dc-motor.md` |
| 步进电机 | 步进电机、28BYJ-48、ULN2003、A4988、TMC2209 | `references/stepper-motor.md` |
| PWM 与总线舵机 | 舵机、PWM 舵机、总线舵机、连续旋转舵机 | `references/servo.md` |
| 机械与固态继电器 | 继电器、固态继电器、继电器互锁 | `references/relay.md` |
| 电磁阀与电磁铁 | 电磁阀、电磁铁、保持电流 | `references/solenoid.md` |
| 有源与无源蜂鸣器 | 蜂鸣器、有源蜂鸣器、无源蜂鸣器、蜂鸣器旋律 | `references/buzzer.md` |
| 音频播放模块 | DFPlayer、I2S 功放、音频播放模块 | `references/audio.md` |
| 四轮直流电机示例 | 四轮电机示例 | `examples/dc-motor/` |
| DFPlayer 示例 | DFPlayer 示例 | `examples/dfplayer/` |
| PWM 舵机示例 | 舵机示例 | `examples/servo-pwm/` |
<!-- GENERATED:ROUTE_TABLE:END -->

## 输出要求

- 区分逻辑电源和负载电源，计算驱动裕量与散热。
- 电机控制说明加减速、刹车、反转死区和堵转处理。
- 继电器、电磁阀等危险负载必须说明默认关闭与故障安全策略。
