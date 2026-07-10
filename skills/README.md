# Skills

本目录的每个子目录都是独立 Skill，必须包含 `SKILL.md`、`skill.json` 和 `CHANGELOG.md`。

<!-- GENERATED:DEPENDENCY_TABLE:START -->
| Skill | 层级 | 直接依赖 | 安装入口 |
|-------|------|----------|----------|
| `mcu-driver-core` | `foundation` | 无 | [`SKILL.md`](mcu-driver-core/SKILL.md) |
| `mcu-actuators` | `domain` | `mcu-driver-core` | [`SKILL.md`](mcu-actuators/SKILL.md) |
| `mcu-communication` | `domain` | `mcu-driver-core` | [`SKILL.md`](mcu-communication/SKILL.md) |
| `mcu-displays` | `domain` | `mcu-driver-core` | [`SKILL.md`](mcu-displays/SKILL.md) |
| `mcu-input` | `domain` | `mcu-driver-core` | [`SKILL.md`](mcu-input/SKILL.md) |
| `mcu-power` | `domain` | `mcu-driver-core` | [`SKILL.md`](mcu-power/SKILL.md) |
| `mcu-sensors` | `domain` | `mcu-driver-core` | [`SKILL.md`](mcu-sensors/SKILL.md) |
| `mcu-storage` | `domain` | `mcu-driver-core` | [`SKILL.md`](mcu-storage/SKILL.md) |
| `mcu-system-design` | `orchestrator` | `mcu-driver-core`、`mcu-sensors`、`mcu-actuators`、`mcu-displays`、`mcu-communication`、`mcu-storage`、`mcu-power`、`mcu-input` | [`SKILL.md`](mcu-system-design/SKILL.md) |
| `project-organizer` | `utility` | 无 | [`SKILL.md`](project-organizer/SKILL.md) |
<!-- GENERATED:DEPENDENCY_TABLE:END -->

- MCU 领域 Skill 需要驱动实现时依赖 `mcu-driver-core`。
- 跨领域产品由 `mcu-system-design` 编排。
- `project-organizer` 独立运行。
- 新增 Skill 后运行 `python3 tools/skill_registry.py --write`，再通过 `python3 tools/validate.py`。
