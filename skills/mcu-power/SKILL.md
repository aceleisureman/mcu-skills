---
name: mcu-power
description: MCU 电源架构与保护 Skill，覆盖 LDO、降压/升压 DC-DC、电池充电、电量监控和输入/接口保护的器件选型、功耗预算、热设计、稳定性、PCB 布局、测试和故障排查。当用户提到 AMS1117、MP1584、LM2596、TP4056、BQ 系列、INA219、TVS、反接或过流保护时使用。
---

# MCU 电源

## 使用流程

1. 建立输入范围、各电源轨电压、峰值/平均电流、纹波和热预算。
2. 从路由表读取对应规范，完成拓扑与器件选型。
3. 原理图、PCB、EMC 和测试方法联合读取 `skill://mcu-driver-core/SKILL.md`。
4. 整机低功耗策略由 `mcu-system-design` 在编排层处理。

## 意图路由

> 下表由 `python3 tools/skill_registry.py --write` 从 skill.json 生成，请勿手工编辑。

<!-- GENERATED:ROUTE_TABLE:START -->
| 意图 | 关键词或型号 | 读取文件 |
|------|------------|----------|
| LDO 线性稳压 | LDO、AMS1117、ME6211、HT7333 | `references/ldo.md` |
| DC-DC 变换 | DC-DC、MP1584、LM2596、TPS5430、升降压 | `references/dc-dc.md` |
| 电池充电 | 电池充电、TP4056、BQ24075、锂电充电 | `references/battery-charger.md` |
| 电池与电量监控 | 电量计、MAX17048、INA219、BQ27441 | `references/battery-monitor.md` |
| 输入与接口保护 | TVS、反接保护、浪涌保护、过流保护、ESD 保护 | `references/protection.md` |
| INA219 电流电压监控示例 | INA219 示例、电流监控示例、功耗测量示例 | `examples/ina219-monitor/` |
<!-- GENERATED:ROUTE_TABLE:END -->

## 输出要求

- 计算最坏条件下的电流裕量、效率、损耗、结温和电池续航。
- 开关电源说明电感、二极管/同步管、电容 ESR、环路和关键 PCB 回路。
- 充电设计说明电芯体系、充电电流、温度保护、负载共享和安全边界。
