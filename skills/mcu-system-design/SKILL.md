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

## 意图路由

> 下表由 `python3 tools/skill_registry.py --write` 从 skill.json 生成，请勿手工编辑。

<!-- GENERATED:ROUTE_TABLE:START -->
| 意图 | 关键词或型号 | 读取文件 |
|------|------------|----------|
| 加载共享驱动基础 | 系统驱动基础、系统硬件审查 | `skill://mcu-driver-core/SKILL.md` |
| 加载传感器领域 | 传感器子系统、采集子系统 | `skill://mcu-sensors/SKILL.md` |
| 加载执行器领域 | 执行器子系统、运动子系统 | `skill://mcu-actuators/SKILL.md` |
| 加载显示领域 | 显示子系统、人机界面子系统 | `skill://mcu-displays/SKILL.md` |
| 加载通信领域 | 通信子系统、联网子系统 | `skill://mcu-communication/SKILL.md` |
| 加载存储领域 | 存储子系统、日志子系统 | `skill://mcu-storage/SKILL.md` |
| 加载电源领域 | 电源子系统、供电子系统 | `skill://mcu-power/SKILL.md` |
| 加载输入与识别领域 | 输入子系统、识别子系统 | `skill://mcu-input/SKILL.md` |
| 系统级低功耗设计 | 低功耗设计、睡眠唤醒、功耗预算 | `guides/low-power.md` |
| Bootloader 与 OTA | Bootloader、双分区、OTA 回滚、固件签名 | `guides/ota-bootloader.md` |
| 多项目 STM32 工程模式 | STM32 工程模式、STM32 项目模式、跨项目模式 | `guides/stm32-project-patterns.md` |
| 环境监测站端到端示例 | 环境监测站、环境监测站示例、多传感器上云 | `examples/env-monitor-station.md` |
| 门控系统端到端示例 | 门控系统、PIR 继电器联动 | `examples/buzzer-pir-relay/` |
| 多节点气瓶柜安全监控端到端示例 | 气瓶柜、主从双 MCU、ZigBee 无线、OneNET MQTT 上云、指纹门禁、气体检测联动 | `examples/gas-cabinet-monitor.md` |
<!-- GENERATED:ROUTE_TABLE:END -->

## 输出要求

- 给出模块边界、接口契约和依赖方向，不让业务层直接操作寄存器。
- 共享总线必须定义互斥、超时、优先级和故障隔离。
- 资源预算至少覆盖 Flash、RAM、栈、CPU、带宽、峰值电流和平均功耗。
- 验证计划覆盖正常流程、边界、断线、掉电、升级失败和恢复。
