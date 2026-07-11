# Changelog

## v2.1.2 - 2026-07-11

- 新增 INA219 电流电压监控示例（examples/ina219-monitor）
- skill.json 新增 license/author/platforms 元数据字段
## v2.1.1 - 2026-07-11

- 意图路由表改为由 skill.json 自动生成（GENERATED:ROUTE_TABLE 区块）
## v2.1.0 - 2026-07-10

- 增加 `skill.json` 路由与依赖元数据
- 明确电源领域与系统低功耗编排的单向边界

## v2.0.0 - 2026-07-10

- 从原 `mcu-components` 拆出 5 篇电源与保护规范
- 增加功耗预算、热设计和最坏条件分析工作流
