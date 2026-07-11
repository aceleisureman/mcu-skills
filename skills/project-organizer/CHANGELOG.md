# Changelog

## v2.2.0 - 2026-07-12

- 新增 /organize-manual-single 命令：单文件汇总功能/硬件/引脚/使用说明
## v2.1.0 - 2026-07-10

- 增加工具层路由元数据
- 生成目标统一使用 `output://` URI，避免与仓库静态资源混淆

## v1.2.0 - 2026-07-10

- 迁移到统一的 `skills/project-organizer/` 多 Skill 目录
- 保持命令、参考文档和扫描脚本的独立可安装性
- 扫描器允许有语义的源码实现版本名，并检查反引号路径引用

## v1.1.0 - 2026-07-10

- 新增项目功能整理命令组：`/organize-features`（分级编号功能清单）、`/organize-hardware`（硬件清单含 MCU）、`/organize-pinout`（引脚分配表）、`/organize-manual`（使用说明文档）
- 新增一键汇总命令 `/organize-docs`：依次产出 features/hardware/pinout/manual 四份文档 + 索引
- 新增 `references/feature-hardware-inventory.md` 执行规范与输出模板
- `scripts/scan_project.py` 增加失效 Markdown 链接检查与 `--json` 输出

## v1.0.0 - 2026-07-10

- 初始版本：`/organize` 系列整理命令（scan/plan/apply/report）+ 整理规则 + 扫描脚本
