# 贡献指南

## 新增 Skill

1. 在 `skills/<kebab-case-name>/` 创建 `SKILL.md`、`skill.json` 和 `CHANGELOG.md`。
2. `SKILL.md` frontmatter 必须包含非空 `name` 与 `description`，且 `name` 与目录名一致。
3. `skill.json` 必须声明 SemVer、层级、直接依赖、Skill 触发词和资源 routes。
4. description 与 triggers 写清覆盖范围、典型型号/任务和触发时机，避免与现有 Skill 重叠。
5. 入口只保留工作流、路由和输出约束；详细内容放入 `references/`、`guides/`、`templates/` 或 `examples/`。
6. 运行生成器更新索引；不要手工编辑自动生成区块或 `skills/registry.json`。

## 修改内容

- 修改对应 Skill 的 `CHANGELOG.md`。
- 移动文件时同步更新 Markdown 链接、反引号路径、`skill.json` routes 和 `SKILL.md` 路由。
- 跨 Skill 静态资源必须使用 `skill://<skill-name>/<path>` 并在 `dependencies` 声明。
- 运行时输出路径必须使用 `output://<relative-path>`，不得伪装成仓库静态引用。
- MCU 器件开发规范保持五段式：选型、硬件、驱动、调试、避坑。
- 通用模板和指南优先放入 `mcu-driver-core`，不要在多个 Skill 复制。
- 多领域流程放入 `mcu-system-design`，不要制造领域 Skill 的循环依赖。

## 提交前校验

```sh
python3 tools/skill_registry.py --write
python3 tools/evaluate_routing.py --write-report
python3 tools/validate.py
python3 tools/evaluate_routing.py
python3 -m unittest discover -s tests -v
python3 skills/project-organizer/scripts/scan_project.py . --json
```

检查结果中不应包含失效链接、未声明依赖、循环依赖、孤儿资源、重复路由、生成漂移、空目录、临时文件或无意的重复内容。
