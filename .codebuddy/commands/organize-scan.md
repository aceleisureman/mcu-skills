---
description: 扫描项目现状（只读，不做任何修改）
argument-hint: [项目路径，默认当前目录]
---

加载 project-organizer Skill 的 SKILL.md，按以下顺序查找，命中即停止：
1. 当前项目内 `skills/project-organizer/SKILL.md`（仅当本项目就是 Skill 仓库时存在）
2. 全局 Skill 目录 `~/.claude/skills/project-organizer/SKILL.md`、`~/.codebuddy/skills/project-organizer/SKILL.md` 或 `~/.codex/skills/project-organizer/SKILL.md`

加载后严格按照其中 `/organize-scan` 命令的执行规范操作。

遵循核心原则：先扫描、后方案、经确认、再动手，未经用户确认不得删除或移动任何文件。

本命令为只读操作，不得修改任何文件。优先直接运行 Skill 内自带脚本获取确定性扫描结果：`python3 <Skill 目录>/scripts/scan_project.py <项目路径> --json`，再基于脚本输出补充分析。

用户补充参数：$ARGUMENTS
