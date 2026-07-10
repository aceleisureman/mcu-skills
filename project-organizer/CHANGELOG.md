# Changelog

## v1.1.0 - 2026-07-10

- 新增项目功能整理命令组：`/organize-features`（分级编号功能清单）、`/organize-hardware`（硬件清单含 MCU）、`/organize-pinout`（引脚分配表）、`/organize-manual`（使用说明文档）
- 新增一键汇总命令 `/organize-docs`：依次产出 features/hardware/pinout/manual 四份文档 + 索引
- 新增 `references/feature-hardware-inventory.md` 执行规范与输出模板
- `scripts/scan_project.py` 增加失效 Markdown 链接检查与 `--json` 输出

## v1.0.0 - 2026-07-10

- 初始版本：`/organize` 系列整理命令（scan/plan/apply/report）+ 整理规则 + 扫描脚本
