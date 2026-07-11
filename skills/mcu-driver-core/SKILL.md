---
name: mcu-driver-core
description: MCU 驱动开发的共享基础 Skill，提供 I2C、SPI、UART、ADC、PWM、GPIO、FreeRTOS 与标准头文件模板，以及硬件设计、调试测试、常见陷阱和代码风格指南。当用户需要搭建可移植驱动骨架、设计 HAL、审查嵌入式代码、调试总线或建立 MCU 工程规范时使用。
---

# MCU 驱动开发核心

## 使用流程

1. 先确认 MCU 平台、外设接口、电压等级、实时性与功耗约束。
2. 从下表只加载当前任务需要的模板和指南，不要一次读取全部文件。
3. 器件寄存器、选型和典型电路由对应领域 Skill 提供；本 Skill 负责通用工程骨架。
4. 输出代码时保持 HAL 与器件逻辑分离，检查参数、超时、返回值和并发访问。

## 意图路由

> 下表由 `python3 tools/skill_registry.py --write` 从 skill.json 生成，请勿手工编辑。

<!-- GENERATED:ROUTE_TABLE:START -->
| 意图 | 关键词或型号 | 读取文件 |
|------|------------|----------|
| 统一驱动 API、状态码和配置结构 | 驱动 API、状态码、配置结构 | `templates/driver-template.h` |
| I2C 驱动模板 | I2C 驱动、I2C 模板 | `templates/driver-template-i2c.c` |
| SPI 驱动模板 | SPI 驱动、SPI 模板 | `templates/driver-template-spi.c` |
| UART 与 AT 指令驱动模板 | UART 驱动、AT 指令驱动 | `templates/driver-template-uart.c` |
| ADC 采样模板 | ADC 采样、ADC 模板 | `templates/driver-template-adc.c` |
| PWM 输出模板 | PWM 输出、PWM 模板 | `templates/driver-template-pwm.c` |
| GPIO 与外部中断模板 | GPIO 驱动、外部中断 | `templates/driver-template-gpio.c` |
| FreeRTOS 任务、互斥和 ISR 通知模板 | FreeRTOS 驱动、总线互斥、ISR 通知 | `templates/driver-template-rtos.c` |
| 原理图、PCB、EMC 与电平匹配 | 硬件设计审查、PCB EMC、电平匹配 | `guides/hardware-design.md` |
| 调试流程、协议分析和测试用例 | 协议分析、逻辑分析仪调试、驱动测试 | `guides/debugging-testing.md` |
| 常见硬件与固件陷阱 | 嵌入式避坑、硬件陷阱、固件陷阱 | `guides/pitfalls.md` |
| 命名、格式、版本和评审规范 | 嵌入式代码规范、驱动代码评审、C 代码风格 | `guides/coding-style.md` |
| BSP 分层示例 | BSP 示例、板级接口示例 | `examples/bsp/` |
| GPIO 模拟 I2C 示例 | 软件 I2C、模拟 I2C | `examples/soft-i2c/` |
<!-- GENERATED:ROUTE_TABLE:END -->

## 依赖边界

本 Skill 是无上游依赖的基础层。器件规范由领域 Skill 提供，多器件组合由 `mcu-system-design` 在编排层处理；基础层不反向引用这些 Skill。

## 输出要求

- 明确平台适配点，不把 STM32 HAL、ESP-IDF 或 Arduino API 写入通用核心层。
- 所有阻塞操作必须有超时；外部输入和硬件返回值必须校验。
- 中断只做最小工作量，耗时处理下沉到主循环或任务。
- 给出初始化、读写、错误恢复和释放/休眠路径。
