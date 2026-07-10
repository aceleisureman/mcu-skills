# Skills 知识库

公司共享的 AI Agent Skill 集合。每个 skill 为一个独立目录，入口为该目录下带 YAML frontmatter 的 `SKILL.md`。

## Skill 列表

| Skill | 用途 | 入口 |
|-------|------|------|
| `mcu-components/` | 单片机元器件开发规范：六大类 40+ 种元器件的选型/硬件设计/驱动开发/调试/避坑 + 驱动代码模板 | `mcu-components/SKILL.md` |
| `project-organizer/` | 项目整理：目录规范化、垃圾清理，以及嵌入式项目功能清单/硬件清单/引脚表/使用说明生成（`/organize` 系列命令） | `project-organizer/SKILL.md` |

## 使用方式

### Claude Code / Claude 平台
将 skill 目录放入项目 `.claude/skills/`（或个人 `~/.claude/skills/`）即可，Agent 会根据 `SKILL.md` frontmatter 的 description 自动触发。

### CodeBuddy 等其他 Agent 平台
将 skill 目录放入对应平台的 skills 目录，或在会话中引用 `SKILL.md` 作为系统知识。

## 维护规范

1. 修改内容后在对应 skill 的 `CHANGELOG.md` 追加条目（版本号递增）
2. 提交前运行校验：`python3 tools/validate.py`（校验 frontmatter、文档结构、内部链接、索引一致性）
3. 新增元器件文档需遵循 `mcu-components/SKILL.md` 中的五段式标准结构，并同步更新 SKILL.md 目录树、型号索引和 `examples/README.md`
4. 通过 git 管理，一类改动一个提交；重大结构调整先在团队内评审
