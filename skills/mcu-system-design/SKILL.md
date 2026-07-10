---
name: mcu-system-design
description: 多器件 MCU 系统设计与编排 Skill，负责把传感器、显示、通信、执行器、存储、电源和 RTOS 组合成完整产品架构，并覆盖功耗预算、任务划分、共享总线、数据流、OTA/Bootloader、故障降级和端到端验证。当用户设计完整嵌入式产品、环境监测站、智能控制器或跨多个硬件领域的方案时使用。
---

# MCU 系统设计

## 使用流程

1. 建立功能、硬件、电源、接口、实时性、可靠性和升级约束清单。
2. 按领域加载所需 Skill，不直接用本入口替代器件规范。
3. 画出电源树、总线拓扑、任务/中断关系和端到端数据流。
4. 检查资源预算、并发访问、错误传播、降级策略和验证计划。

## 领域路由

| 领域 | 读取入口 |
|------|----------|
| 驱动骨架、硬件设计、调试测试 | `skill://mcu-driver-core/SKILL.md` |
| 传感器 | `skill://mcu-sensors/SKILL.md` |
| 执行器 | `skill://mcu-actuators/SKILL.md` |
| 显示 | `skill://mcu-displays/SKILL.md` |
| 通信 | `skill://mcu-communication/SKILL.md` |
| 存储与 RTC | `skill://mcu-storage/SKILL.md` |
| 电源 | `skill://mcu-power/SKILL.md` |
| 输入与识别 | `skill://mcu-input/SKILL.md` |

## 专项指南

| 需求 | 读取文件 |
|------|----------|
| 功耗预算、睡眠/唤醒、低功耗检查 | `guides/low-power.md` |
| Bootloader、双分区、OTA、回滚和签名验证 | `guides/ota-bootloader.md` |
| STM32 工程分层、模块组织与跨项目模式 | `guides/stm32-project-patterns.md` |

## 端到端示例

| 场景 | 路径 |
|------|------|
| SHT30 + OLED + MQTT 环境监测站 | `examples/env-monitor-station.md` |
| 蜂鸣器 + PIR + 继电器门控系统 | `examples/buzzer-pir-relay/` |

## 输出要求

- 给出模块边界、接口契约和依赖方向，不让业务层直接操作寄存器。
- 共享总线必须定义互斥、超时、优先级和故障隔离。
- 资源预算至少覆盖 Flash、RAM、栈、CPU、带宽、峰值电流和平均功耗。
- 验证计划覆盖正常流程、边界、断线、掉电、升级失败和恢复。
