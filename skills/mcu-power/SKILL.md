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

| 需求或型号 | 读取文件 |
|------------|----------|
| AMS1117、ME6211、HT7333、低噪声线性稳压 | `references/ldo.md` |
| MP1584、LM2596、TPS5430、升降压 | `references/dc-dc.md` |
| TP4056、BQ24075、锂电池充电 | `references/battery-charger.md` |
| MAX1704x、INA219、BQ27441、电量计 | `references/battery-monitor.md` |
| TVS、PTC、反接、浪涌、ESD | `references/protection.md` |

## 输出要求

- 计算最坏条件下的电流裕量、效率、损耗、结温和电池续航。
- 开关电源说明电感、二极管/同步管、电容 ESR、环路和关键 PCB 回路。
- 充电设计说明电芯体系、充电电流、温度保护、负载共享和安全边界。
