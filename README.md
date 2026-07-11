# MCU Skills 知识库

面向 AI Agent 和嵌入式团队的模块化 Skill 集合。仓库采用 `skills/<skill-name>/SKILL.md` 结构，每个 Skill 都有独立触发范围、路由入口、版本记录和按需加载资源。

## Skill 目录

<!-- GENERATED:SKILL_TABLE:START -->
| Skill | 层级 | 版本 | 依赖 | 用途 |
|-------|------|------|------|------|
| [`mcu-driver-core`](skills/mcu-driver-core/SKILL.md) | `foundation` | `v2.1.0` | 无 | 提供可移植驱动模板、硬件设计、调试测试、常见陷阱和代码规范。 |
| [`mcu-actuators`](skills/mcu-actuators/SKILL.md) | `domain` | `v2.1.0` | `mcu-driver-core` | 覆盖电机、舵机、继电器、电磁负载、蜂鸣器和音频模块的驱动与保护。 |
| [`mcu-communication`](skills/mcu-communication/SKILL.md) | `domain` | `v2.2.0` | `mcu-driver-core` | 覆盖 WiFi、蓝牙、LoRa、蜂窝、CAN、RS485、NFC 和以太网通信。 |
| [`mcu-displays`](skills/mcu-displays/SKILL.md) | `domain` | `v2.2.0` | `mcu-driver-core` | 覆盖 OLED、LCD、TFT、电子纸和 LED 显示的接口、显存与刷新优化。 |
| [`mcu-input`](skills/mcu-input/SKILL.md) | `domain` | `v2.1.0` | `mcu-driver-core` | 覆盖编码器、键盘、触摸、指纹、语音和视觉识别输入。 |
| [`mcu-power`](skills/mcu-power/SKILL.md) | `domain` | `v2.1.0` | `mcu-driver-core` | 覆盖稳压、变换、充电、电量监控和保护电路的选型、热设计与 PCB。 |
| [`mcu-sensors`](skills/mcu-sensors/SKILL.md) | `domain` | `v2.2.0` | `mcu-driver-core` | 覆盖环境、运动、气体、距离、磁性和定位传感器的选型、硬件与驱动。 |
| [`mcu-storage`](skills/mcu-storage/SKILL.md) | `domain` | `v2.1.0` | `mcu-driver-core` | 覆盖 EEPROM、Flash、SD 卡、FRAM 和 RTC 的驱动、寿命与掉电一致性。 |
| [`mcu-system-design`](skills/mcu-system-design/SKILL.md) | `orchestrator` | `v2.2.0` | `mcu-driver-core`、`mcu-sensors`、`mcu-actuators`、`mcu-displays`、`mcu-communication`、`mcu-storage`、`mcu-power`、`mcu-input` | 编排多个硬件领域，覆盖系统架构、功耗、任务、OTA、故障降级和验证。 |
| [`project-organizer`](skills/project-organizer/SKILL.md) | `utility` | `v2.1.0` | 无 | 扫描和规范化项目，并生成嵌入式功能、硬件、引脚与使用说明文档。 |
<!-- GENERATED:SKILL_TABLE:END -->

完整文件索引见 [`docs/content-catalog.md`](docs/content-catalog.md)，依赖图见 [`docs/dependency-graph.md`](docs/dependency-graph.md)，架构说明见 [`docs/architecture.md`](docs/architecture.md)。

## 为什么拆分

原 `mcu-components` 同时覆盖 42 类器件、通用模板、工程指南和完整项目，触发范围过宽。拆分后的结构具备：

- **精确触发**：每个 frontmatter 只描述一个领域。
- **按需加载**：Agent 只读取当前领域的一篇参考文档。
- **单一维护职责**：通用模板集中在 `mcu-driver-core`，领域内容不重复。
- **可组合**：复杂产品由 `mcu-system-design` 编排多个领域 Skill。
- **机器可读**：每个 `skill.json` 声明版本、层级、依赖、触发词和资源路由。
- **可扩展校验**：自动检查依赖、循环、孤儿资源、重复路由、URI 和生成漂移。
- **可量化路由**：55 个典型问题持续评测单领域与组合 Skill 的精确率和召回率。

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
│   └── routing-cases.json  # 路由评测数据集
├── docs/                   # 架构、依赖图、迁移和生成索引
├── tools/
│   ├── skill_registry.py   # registry、索引和依赖图生成/URI 解析
│   ├── evaluate_routing.py # 路由评测与报告生成
│   └── validate.py         # 结构、语义和生成漂移校验
├── tests/
│   ├── test_validate.py
│   └── test_routing.py
└── .github/workflows/
    └── validate.yml
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

### Claude Code / Claude 平台

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

## 维护

```sh
python3 tools/skill_registry.py --write
python3 tools/evaluate_routing.py --write-report
python3 tools/validate.py
python3 tools/evaluate_routing.py
python3 -m unittest discover -s tests -v
```

不要手工编辑 `skills/registry.json`、内容索引、依赖图或路由评测报告。新增或修改 Skill 前阅读 [`CONTRIBUTING.md`](CONTRIBUTING.md)。从旧版 `mcu-components` 迁移请参考 [`docs/migration-v2.md`](docs/migration-v2.md)。
