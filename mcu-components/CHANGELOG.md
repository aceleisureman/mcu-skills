# Changelog

本 Skill 的版本记录。团队更新规范时请在此追加条目（新版本在上）。

## v1.2.0 - 2026-07-10

- 新增输入类 references（旋转编码器/按键与矩阵键盘/电容触摸）
- 新增 references：GPS 定位模块、以太网模块、音频播放模块（共 42 种元器件）
- 新增模板：driver-template.h（驱动头文件标准形态）、driver-template-rtos.c（FreeRTOS 总线互斥/采集任务/ISR 通知）
- 新增 guides：低功耗设计（low-power.md）、OTA 与 Bootloader（ota-bootloader.md）、代码风格与版本规范（coding-style.md）
- 新增端到端完整示例：examples/env-monitor-station.md（SHT30 + OLED + MQTT）
- SKILL.md 目录树/意图映射/型号索引与 examples/README.md 索引同步更新

## v1.1.0 - 2026-07-10

- SKILL.md 增加 YAML frontmatter（name/description），适配云端 Agent Skill 平台的识别与自动触发
- SKILL.md 增加「使用流程（AI Agent 必读）」：按需加载、驱动开发顺序、排查顺序
- SKILL.md 增加「芯片/器件型号快速索引」：支持按具体型号（如 MPU6050、W25Q64）直达文档
- 全部 36 篇 references 文档追加「相关文档」交叉引用（对应总线模板 + guides）
- examples/README.md 索引修正：补全全部文档的实际代码示例清单（原大量标注"待创建"已过时）
- 清理 .DS_Store 等杂项文件

## v1.0.0

- 初始版本：六大类 36 种元器件规范 + 6 套驱动模板 + 3 篇通用指南
