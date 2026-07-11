---
description: 用户确认方案后执行整理
argument-hint: [无参数，需先确认方案]
---

加载 project-organizer Skill 的 SKILL.md，按以下顺序查找，命中即停止：
1. 当前项目内 `skills/project-organizer/SKILL.md`（仅当本项目就是 Skill 仓库时存在）
2. 全局 Skill 目录 `~/.claude/skills/project-organizer/SKILL.md`、`~/.codebuddy/skills/project-organizer/SKILL.md` 或 `~/.codex/skills/project-organizer/SKILL.md`

加载后严格按照其中 `/organize-apply` 命令的执行规范操作。

遵循核心原则：先扫描、后方案、经确认、再动手，未经用户确认不得删除或移动任何文件。

前置条件：本会话中必须已有经用户明确确认的整理方案（来自 /organize-plan 或 /organize）。若没有，不得执行任何改动，提示用户先运行 /organize-plan。

用户补充参数：$ARGUMENTS
