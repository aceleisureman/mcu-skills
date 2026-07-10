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

| 需求 | 读取文件 |
|------|----------|
| 统一驱动 API、状态码、配置结构 | `templates/driver-template.h` |
| I2C 驱动 | `templates/driver-template-i2c.c` |
| SPI 驱动 | `templates/driver-template-spi.c` |
| UART/AT 指令驱动 | `templates/driver-template-uart.c` |
| ADC 采样 | `templates/driver-template-adc.c` |
| PWM 输出 | `templates/driver-template-pwm.c` |
| GPIO/外部中断 | `templates/driver-template-gpio.c` |
| FreeRTOS 总线互斥、任务和 ISR 通知 | `templates/driver-template-rtos.c` |
| 原理图、PCB、EMC、电平匹配 | `guides/hardware-design.md` |
| 调试流程、协议分析和测试用例 | `guides/debugging-testing.md` |
| 常见硬件与固件陷阱 | `guides/pitfalls.md` |
| 命名、格式、版本和评审规范 | `guides/coding-style.md` |

## 示例

| 场景 | 路径 |
|------|------|
| BSP 分层与板级接口 | `examples/bsp/` |
| GPIO 模拟 I2C | `examples/soft-i2c/` |

## 依赖边界

本 Skill 是无上游依赖的基础层。器件规范由领域 Skill 提供，多器件组合由 `mcu-system-design` 在编排层处理；基础层不反向引用这些 Skill。

## 输出要求

- 明确平台适配点，不把 STM32 HAL、ESP-IDF 或 Arduino API 写入通用核心层。
- 所有阻塞操作必须有超时；外部输入和硬件返回值必须校验。
- 中断只做最小工作量，耗时处理下沉到主循环或任务。
- 给出初始化、读写、错误恢复和释放/休眠路径。
