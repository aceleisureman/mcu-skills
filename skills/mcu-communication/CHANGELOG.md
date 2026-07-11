# Changelog

## v2.2.2 - 2026-07-11

- skill.json 新增 license/author/platforms 元数据字段（docs/skill.schema.json 校验）
## v2.2.1 - 2026-07-11

- 意图路由表改为由 skill.json 自动生成（GENERATED:ROUTE_TABLE 区块）
## v2.2.0 - 2026-07-11

- 新增 ZigBee 透传通信规范
## v2.1.0 - 2026-07-10

- 增加 `skill.json` 路由与依赖元数据
- 跨 Skill 资源统一改用 `skill://` URI

## v2.0.0 - 2026-07-10

- 从原 `mcu-components` 拆出 8 篇通信规范和 ESP8266 示例
- 增加协议分层、状态机和连接恢复工作流
