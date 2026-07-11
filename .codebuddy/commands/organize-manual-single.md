---
description: 生成面向最终用户的单文件项目手册（概述+功能+硬件+使用说明，不含引脚表）
argument-hint: [输出路径，默认 docs/project-manual.md]
---

加载 project-organizer Skill 的 SKILL.md，按以下顺序查找，命中即停止：
1. 当前项目内 `skills/project-organizer/SKILL.md`（仅当本项目就是 Skill 仓库时存在）
2. 全局 Skill 目录 `~/.claude/skills/project-organizer/SKILL.md`、`~/.codebuddy/skills/project-organizer/SKILL.md` 或 `~/.codex/skills/project-organizer/SKILL.md`

加载后严格按照其中 `/organize-manual-single` 命令的执行规范操作。

遵循核心原则：先扫描、后方案、经确认、再动手，未经用户确认不得删除或移动任何文件。

用户补充参数：$ARGUMENTS
