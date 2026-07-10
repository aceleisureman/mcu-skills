# Skills 知识库

公司共享的 AI Agent Skill 集合。每个 skill 为一个独立目录，入口为该目录下带 YAML frontmatter 的 `SKILL.md`。

## Skill 列表

| Skill | 用途 | 入口 |
|-------|------|------|
| `mcu-components/` | 单片机元器件开发规范：七大类 42 种元器件的选型/硬件设计/驱动开发/调试/避坑 + 驱动代码模板 + 通用指南 | `mcu-components/SKILL.md` |
| `project-organizer/` | 项目整理：目录规范化、垃圾清理，以及嵌入式项目功能清单/硬件清单/引脚表/使用说明生成（`/organize` 系列命令） | `project-organizer/SKILL.md` |

## 仓库整体结构

```
skills/
├── README.md                    # 本文件 - 仓库总索引
├── .gitignore                   # 忽略系统/编辑器/临时文件
├── .github/workflows/           # CI: 推送时自动运行 tools/validate.py
├── tools/
│   └── validate.py              # 全仓库校验脚本（提交前运行）
├── mcu-components/              # Skill 1: 元器件开发规范
└── project-organizer/           # Skill 2: 项目整理
```

## 各 Skill 内部结构

每个 skill 遵循统一的目录约定：

```
<skill-name>/
├── SKILL.md          # 唯一入口（必须）: frontmatter + 使用流程 + 意图路由表
├── CHANGELOG.md      # 版本记录（必须）: 每次修改追加条目
├── references/       # 详细规范文档（按需加载, 不在 SKILL.md 内展开）
├── templates/        # 可直接套用的代码模板（如有）
├── guides/           # 跨主题通用指南（如有）
├── examples/         # 完整示例与索引（如有）
└── scripts/          # 配套可执行脚本（如有）
```

**设计原则（按需加载）**：`SKILL.md` 只做路由——frontmatter 的 description 决定何时自动触发，正文的「意图 → 文档」映射表引导 Agent 只读取所需的 references/templates 文件，避免一次性加载全部内容浪费上下文。

### mcu-components（元器件开发规范）

```
mcu-components/
├── SKILL.md                     # 路由入口: 意图映射表 + 型号快速索引
├── CHANGELOG.md
├── references/                  # 42 篇元器件规范（统一五段式结构）
│   ├── sensors/                 # 传感器: 温度/湿度/压力/光照/IMU/气体/距离/磁性/GPS
│   ├── actuators/               # 执行器: 直流电机/步进/舵机/继电器/电磁阀/蜂鸣器/音频
│   ├── display/                 # 显示: OLED/LCD/TFT/墨水屏/LED点阵
│   ├── communication/           # 通信: WiFi/蓝牙/LoRa/NB-IoT/CAN/RS485/NFC/以太网
│   ├── storage/                 # 存储: EEPROM/Flash/SD卡/FRAM/RTC
│   ├── power/                   # 电源: LDO/DC-DC/充电/电池监控/保护
│   └── input/                   # 输入: 旋转编码器/按键键盘/电容触摸
├── templates/                   # I2C/SPI/UART/ADC/PWM/GPIO 驱动模板
│                                # + driver-template.h + driver-template-rtos.c
├── guides/                      # 硬件设计/调试测试/避坑/低功耗/OTA/代码风格
└── examples/                    # 示例索引 + 端到端完整示例
```

每篇 references 文档统一五段式：①选型指南 → ②硬件设计 → ③驱动开发 → ④调试测试 → ⑤避坑指南，末尾附「相关文档」交叉引用。

### project-organizer（项目整理）

```
project-organizer/
├── SKILL.md                     # 命令定义: /organize 系列 + /organize-docs 等 10 个命令
├── CHANGELOG.md
├── references/
│   ├── organize-rules.md        # 整理规则: 命名约定/目录结构/垃圾清单/.gitignore 基线
│   └── feature-hardware-inventory.md  # 功能清单/硬件清单/引脚表/使用说明的执行规范与模板
└── scripts/
    └── scan_project.py          # 只读扫描脚本（支持 --json 与失效链接检查）
```

## 使用方式

### Claude Code / Claude 平台
将 skill 目录放入项目 `.claude/skills/`（或个人 `~/.claude/skills/`）即可，Agent 会根据 `SKILL.md` frontmatter 的 description 自动触发。

### CodeBuddy 等其他 Agent 平台
将 skill 目录放入对应平台的 skills 目录，或在会话中引用 `SKILL.md` 作为系统知识。

## 新增一个 Skill

1. 新建目录 `<skill-name>/`（kebab-case 命名），按上方「各 Skill 内部结构」创建 `SKILL.md` 与 `CHANGELOG.md`
2. `SKILL.md` 必须含 frontmatter（`name` 与 `description`，description 写清覆盖范围和触发场景）
3. 详细内容放 `references/`，`SKILL.md` 内只写路由表，保持按需加载
4. 更新本 README 的「Skill 列表」，运行 `python3 tools/validate.py` 通过后提交

## 维护规范

1. 修改内容后在对应 skill 的 `CHANGELOG.md` 追加条目（版本号递增）
2. 提交前运行校验：`python3 tools/validate.py`（校验 frontmatter、文档结构、内部链接、索引一致性）
3. 新增元器件文档需遵循 `mcu-components/SKILL.md` 中的五段式标准结构，并同步更新 SKILL.md 目录树、型号索引和 `examples/README.md`
4. 通过 git 管理，一类改动一个提交；重大结构调整先在团队内评审
