# MCU Skills 知识库

面向 AI Agent 和嵌入式团队的模块化 Skill 集合。仓库采用 `skills/<skill-name>/SKILL.md` 结构，每个 Skill 都有独立触发范围、路由入口、版本记录和按需加载资源。

安装器包版本：`mcu-skills@1.3.0`（见 [`package.json`](package.json)）。

## Skill 目录

<!-- GENERATED:SKILL_TABLE:START -->
| Skill | 层级 | 版本 | 依赖 | 用途 |
|-------|------|------|------|------|
| [`mcu-driver-core`](skills/mcu-driver-core/SKILL.md) | `foundation` | `v2.1.2` | 无 | 提供可移植驱动模板、硬件设计、调试测试、常见陷阱和代码规范。 |
| [`mcu-actuators`](skills/mcu-actuators/SKILL.md) | `domain` | `v2.1.2` | `mcu-driver-core` | 覆盖电机、舵机、继电器、电磁负载、蜂鸣器和音频模块的驱动与保护。 |
| [`mcu-communication`](skills/mcu-communication/SKILL.md) | `domain` | `v2.2.2` | `mcu-driver-core` | 覆盖 WiFi、蓝牙、LoRa、蜂窝、CAN、RS485、NFC 和以太网通信。 |
| [`mcu-displays`](skills/mcu-displays/SKILL.md) | `domain` | `v2.2.2` | `mcu-driver-core` | 覆盖 OLED、LCD、TFT、电子纸和 LED 显示的接口、显存与刷新优化。 |
| [`mcu-input`](skills/mcu-input/SKILL.md) | `domain` | `v2.1.2` | `mcu-driver-core` | 覆盖编码器、键盘、触摸、指纹、语音和视觉识别输入。 |
| [`mcu-power`](skills/mcu-power/SKILL.md) | `domain` | `v2.1.2` | `mcu-driver-core` | 覆盖稳压、变换、充电、电量监控和保护电路的选型、热设计与 PCB。 |
| [`mcu-sensors`](skills/mcu-sensors/SKILL.md) | `domain` | `v2.4.2` | `mcu-driver-core` | 覆盖环境、运动、气体、距离、磁性和定位传感器的选型、硬件与驱动。 |
| [`mcu-storage`](skills/mcu-storage/SKILL.md) | `domain` | `v2.1.2` | `mcu-driver-core` | 覆盖 EEPROM、Flash、SD 卡、FRAM 和 RTC 的驱动、寿命与掉电一致性。 |
| [`mcu-system-design`](skills/mcu-system-design/SKILL.md) | `orchestrator` | `v2.2.2` | `mcu-driver-core`、`mcu-sensors`、`mcu-actuators`、`mcu-displays`、`mcu-communication`、`mcu-storage`、`mcu-power`、`mcu-input` | 编排多个硬件领域，覆盖系统架构、功耗、任务、OTA、故障降级和验证。 |
| [`project-organizer`](skills/project-organizer/SKILL.md) | `utility` | `v2.3.1` | 无 | 扫描和规范化项目，并生成嵌入式功能、硬件、引脚与使用说明文档。 |
<!-- GENERATED:SKILL_TABLE:END -->

完整文件索引见 [`docs/content-catalog.md`](docs/content-catalog.md)，依赖图见 [`docs/dependency-graph.md`](docs/dependency-graph.md)，架构说明见 [`docs/architecture.md`](docs/architecture.md)，元数据 Schema 见 [`docs/skill.schema.json`](docs/skill.schema.json)。

## 为什么拆分

原 `mcu-components` 同时覆盖 42 类器件、通用模板、工程指南和完整项目，触发范围过宽。拆分后的结构具备：

- **精确触发**：每个 frontmatter 只描述一个领域。
- **按需加载**：Agent 只读取当前领域的一篇参考文档。
- **单一维护职责**：通用模板集中在 `mcu-driver-core`，领域内容不重复。
- **可组合**：复杂产品由 `mcu-system-design` 编排多个领域 Skill。
- **机器可读**：每个 `skill.json` 声明版本、层级、依赖、触发词和资源路由。
- **可扩展校验**：自动检查依赖、循环、孤儿资源、重复路由、URI 和生成漂移。
- **可量化路由**：62 个典型问题（含口语化提问与负样本）持续评测单领域与组合 Skill 的精确率和召回率。

## 依赖模型

```text
mcu-system-design
├── mcu-driver-core
├── mcu-sensors
├── mcu-actuators
├── mcu-displays
├── mcu-communication
├── mcu-storage
├── mcu-power
└── mcu-input

各 MCU 领域 Skill ──> mcu-driver-core
project-organizer     （独立）
```

领域 Skill 可独立用于选型和规范查询；需要生成驱动或做硬件/调试审查时，应同时安装 `mcu-driver-core`。完整产品设计建议安装 `mcu-system-design` 及其声明的依赖闭包。确定性依赖图由 [`skills/registry.json`](skills/registry.json) 提供。

## 仓库结构

```text
.
├── skills/                 # 10 个独立 Skill
│   └── registry.json       # 自动汇总的机器可读 registry
├── evals/
│   └── routing-cases.json  # 路由评测数据集（62 用例）
├── docs/
│   ├── architecture.md     # 四层架构与 URI 规则
│   ├── content-catalog.md  # 自动生成的内容索引
│   ├── dependency-graph.md # 自动生成的依赖图
│   ├── migration-v2.md     # 从 mcu-components 迁移说明
│   ├── routing-evaluation.json  # 路由评测报告
│   └── skill.schema.json   # skill.json JSON Schema
├── bin/
│   └── mcu-skills.js       # npm 安装器（Claude Code / Codex / CodeBuddy）
├── package.json            # npm 包 mcu-skills@1.3.0
├── .claude-plugin/         # Claude Code 插件与 marketplace 清单
├── .codebuddy/
│   └── commands/           # /organize 系列斜杠命令定义
├── tools/
│   ├── skill_registry.py   # registry、索引、依赖图与 SKILL.md 路由表生成/URI 解析
│   ├── evaluate_routing.py # 路由评测与报告生成
│   ├── validate.py         # 结构、语义和生成漂移校验
│   ├── package_skills.py   # 生成可 ZIP 导入的 Skill 压缩包
│   ├── bump_version.py     # Skill 版本升级与发布
│   └── extract_changelog.py # Release Notes 提取
├── tests/
│   ├── test_validate.py
│   ├── test_routing.py
│   └── test_tools.py
└── .github/workflows/
    ├── validate.yml        # PR/推送结构与路由校验
    ├── release.yml         # Skill tag 发布 GitHub Release
    └── publish.yml         # npm-v* tag 发布到 npm
```

每个 Skill 遵循：

```text
skills/<skill-name>/
├── SKILL.md                # 必须：name/description frontmatter + 路由
├── skill.json              # 必须：版本、层级、依赖、触发词与资源路由
├── CHANGELOG.md            # 必须：版本记录
├── references/             # 详细规范，可选
├── templates/              # 可复用模板，可选
├── guides/                 # 通用指南，可选
├── examples/               # 示例，可选
└── scripts/                # 配套脚本，可选
```

## 安装

> **一键装全部（推荐）**：不必逐个 Skill 安装。

| 方式 | 一条命令 / 一个文件 | 说明 |
|------|---------------------|------|
| npm 安装器 | `npx mcu-skills install` | 一次装入当前机器上检测到的 Claude Code / Codex / CodeBuddy |
| Claude 插件 | `/plugin install mcu-skills@mcu-skills` | 一次拿到全部 Skill + `/organize` 命令 |
| ZIP 全量包 | 只下 [`mcu-skills-bundle.zip`](https://github.com/aceleisureman/mcu-skills/releases/latest) | 10 个 Skill 都在一个 ZIP 里（`<skill>/SKILL.md`） |

### npm 安装器（推荐）

```sh
# 一条命令：安装全部 10 个 Skill（自动装到已检测到的平台）
npx mcu-skills install

# 只装某一个（仍会自动带上依赖闭包）
npx mcu-skills install mcu-sensors

# 指定平台
npx mcu-skills install --target claude     # ~/.claude
npx mcu-skills install --target codex      # ~/.codex
npx mcu-skills install --target codebuddy  # ~/.codebuddy

npx mcu-skills list                        # 查看安装状态
npx mcu-skills uninstall mcu-sensors       # 卸载指定 Skill
```

安装 `project-organizer` 时会同时注册 `/organize` 系列斜杠命令（Claude Code/CodeBuddy 为 `commands/`，Codex 为 `prompts/`）；Codex 还会在 `~/.codex/AGENTS.md` 中维护一个 Skill 索引区块。

### Claude Code 插件安装

仓库内置插件与 marketplace 清单（`.claude-plugin/`），在 Claude Code 中执行：

```text
/plugin marketplace add aceleisureman/mcu-skills
/plugin install mcu-skills@mcu-skills
```

即可一次获得全部 Skill 与 `/organize` 系列命令。

### ZIP 导入（平台「从 ZIP 导入技能」）

**不要用 GitHub 的 Source code ZIP**，路径过深会报「未找到技能」。

**想一次导入全部：只下一个文件**

1. 打开 [最新 Release](https://github.com/aceleisureman/mcu-skills/releases/latest)
2. 下载 **`mcu-skills-bundle.zip`**（不是 10 个单包，也不是 Source code）
3. 用平台的 ZIP 导入该文件

bundle 内结构为：

```text
mcu-driver-core/SKILL.md
mcu-sensors/SKILL.md
...（共 10 个 Skill 目录）
```

若平台**只认 ZIP 根目录一个 `SKILL.md`**（不支持多子目录），请改用上面的 `npx mcu-skills install`，不要逐个手导 10 个单包。

仅在需要单独分发某个 Skill 时，再下 `<skill>-v<version>.zip`。

本地生成同样布局：

```sh
python3 tools/package_skills.py                 # dist/ 下全部单包 + 全量包
python3 tools/package_skills.py mcu-sensors     # 只打一个 Skill
```

### 手动安装：Claude Code / Claude 平台

复制需要的 Skill 目录到项目或个人 Skill 目录：

```sh
cp -R skills/mcu-sensors .claude/skills/
cp -R skills/mcu-driver-core .claude/skills/
```

依赖信息以对应 `skill.json` 为准。复制一个 Skill 时，同时复制其 `dependencies` 的传递闭包。

### 其他 Agent 平台

将 `skills/` 下所需目录复制到平台约定的 Skill 目录，或把对应 `SKILL.md` 注册为入口。跨 Skill 资源使用 `skill://<skill-name>/<path>`，不依赖安装目录的相对层级；可用以下命令解析：

```sh
python3 tools/skill_registry.py --resolve \
  skill://mcu-driver-core/templates/driver-template-i2c.c
```

## `/organize` 系列命令

安装 `project-organizer` 后可用（命令定义见 [`.codebuddy/commands/`](.codebuddy/commands/)）：

| 命令 | 功能 |
|------|------|
| `/organize` | 完整整理流程：scan → plan →（确认后）apply → report |
| `/organize-scan` | 只读扫描项目现状 |
| `/organize-plan` | 生成整理方案供确认 |
| `/organize-apply` | 在用户确认后执行整理 |
| `/organize-report` | 输出整理报告 |
| `/organize-features` | 生成分级编号功能清单 |
| `/organize-hardware` | 生成硬件清单（含 MCU） |
| `/organize-pinout` | 生成引脚分配表 |
| `/organize-manual` | 生成使用说明文档 |
| `/organize-manual-single` | 合并为单文件用户手册 |
| `/organize-docs` | 一键产出功能/硬件/引脚/说明全套文档 |

## 维护

```sh
python3 tools/skill_registry.py --write
python3 tools/evaluate_routing.py --write-report
python3 tools/validate.py
python3 tools/evaluate_routing.py
python3 -m unittest discover -s tests -v
```

CI 等效检查（不改动工作区）：

```sh
python3 tools/skill_registry.py --check
python3 tools/evaluate_routing.py --check-report
ruff check tools tests && ruff format --check tools tests
```

版本升级与发布：

```sh
# 单 Skill 升级
python3 tools/bump_version.py mcu-sensors --minor -m "变更说明"
python3 tools/bump_version.py mcu-sensors --minor --release  # 升级 + commit + tag + push

# npm 包发布（tag 需与 package.json version 一致，触发 publish.yml）
# 例如 package.json 为 1.3.0 时：
git tag npm-v1.3.0
git push origin npm-v1.3.0
```

不要手工编辑 `skills/registry.json`、内容索引、依赖图、路由评测报告，以及各 `SKILL.md` 中的 `GENERATED:ROUTE_TABLE` 意图路由表区块（由 `skill_registry.py --write` 从 `skill.json` 生成）。根 `README.md` 与 `skills/README.md` 中的 Skill 索引表区块也由同一命令生成；若只改用途文案，请同步更新对应 `skill.json` 的 `summary` 后重新 `--write`。

新增或修改 Skill 前阅读 [`CONTRIBUTING.md`](CONTRIBUTING.md)。从旧版 `mcu-components` 迁移请参考 [`docs/migration-v2.md`](docs/migration-v2.md)。
