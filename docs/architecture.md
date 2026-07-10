# 多 Skill 架构

## 设计目标

1. frontmatter 的触发范围与目录职责一一对应。
2. `SKILL.md` 只负责工作流和路由，详细内容按需加载。
3. 通用工程知识只保留一份，领域 Skill 不复制模板和指南。
4. 单领域任务只触发一个领域 Skill；跨领域任务由系统设计 Skill 编排。
5. `skill.json` 是版本、依赖、触发与资源路由的单一机器可读来源。
6. 所有跨 Skill 引用使用 `skill://` URI，不依赖安装目录布局。
7. 生成物和路由质量可在 CI 中确定性复现。

## 四层模型

### 编排层

`mcu-system-design` 处理完整产品、跨领域资源预算、共享总线、RTOS 任务、低功耗和 OTA。

### 领域层

`mcu-sensors`、`mcu-actuators`、`mcu-displays`、`mcu-communication`、`mcu-storage`、`mcu-power`、`mcu-input` 负责器件选型、硬件与领域驱动知识。

### 基础层

`mcu-driver-core` 负责 HAL、总线模板、硬件审查、调试测试和代码规范。

### 工具层

`project-organizer` 是无 MCU 领域依赖的独立仓库治理 Skill。

## 依赖规则

- 依赖方向只能从编排层指向领域层/基础层，或从领域层指向基础层。
- 基础层不得引用具体领域 Skill。
- 领域 Skill 之间不直接依赖；跨领域组合进入 `mcu-system-design`。
- 工具层保持独立，不进入 MCU 领域依赖图。
- reference 文档可通过 `skill://<dependency>/<resource>` 引用共享模板和指南，但不得复制它们。
- 每个 Skill 必须能够从自身 `SKILL.md` 路由到目录内的所有主要资源。
- 所有声明依赖必须存在；禁止自依赖、循环依赖和未声明的跨 Skill URI。

## 元数据模型

每个 Skill 根目录包含 `skill.json`：

```json
{
  "schema_version": 1,
  "name": "mcu-sensors",
  "version": "2.1.0",
  "title": "MCU 传感器",
  "summary": "传感器选型、硬件与驱动。",
  "layer": "domain",
  "dependencies": ["mcu-driver-core"],
  "triggers": ["传感器选型", "传感器驱动"],
  "routes": [
    {
      "id": "temperature",
      "intent": "温度传感器",
      "keywords": ["DS18B20", "SHT30"],
      "target": "references/temperature.md"
    }
  ]
}
```

- `version` 必须与当前 `CHANGELOG.md` 条目一致。
- `triggers` 负责 Skill 级发现，`routes` 负责 Skill 内按需加载。
- 每个 `references/`、`templates/`、`guides/`、`examples/`、`scripts/` 顶层资源都必须由 route 覆盖。
- `skills/registry.json` 是所有 `skill.json` 加资源清单的确定性汇总，不手工维护。

## URI 规则

- `skill://<skill-name>/<resource>`：已安装 Skill 内的静态资源。目标必须存在，跨 Skill 使用时必须声明依赖。
- `output://<relative-path>`：Skill 运行后在用户项目中创建的目标，不作为仓库静态文件校验。
- 普通相对路径：仅用于当前 Skill 内或仓库文档内的链接，不得穿越到兄弟 Skill。

解析示例：

```sh
python3 tools/skill_registry.py --resolve \
  skill://mcu-driver-core/guides/debugging-testing.md
```

## 内容归属

- 器件级选型、电路和驱动：领域 Skill 的 `references/`
- 跨器件驱动骨架：`mcu-driver-core/templates/`
- 通用硬件、调试和代码规范：`mcu-driver-core/guides/`
- 系统级低功耗与升级：`mcu-system-design/guides/`
- 单器件示例：对应领域 Skill 的 `examples/`
- 多器件完整示例：`mcu-system-design/examples/`

## 生成与评测

`python3 tools/skill_registry.py --write` 确定性生成：

- `skills/registry.json`
- `README.md` 和 `skills/README.md` 中的索引区块
- `docs/content-catalog.md`
- `docs/dependency-graph.md`

`python3 tools/evaluate_routing.py --write-report` 使用 `evals/routing-cases.json` 生成 `docs/routing-evaluation.json`。校验阈值为 Skill precision/recall/top-1 不低于 95%，目标资源 recall 不低于 90%。

## 版本策略

- v2.0.0 完成多 Skill 拆分，v2.1.0 增加元数据、URI、自动生成和路由评测。
- 后续每个 Skill 独立递增版本并维护自己的 `CHANGELOG.md`。
- 仅修改共享基础内容时，不要求所有领域 Skill 同步发版。
- 破坏性路径调整必须更新 [`migration-v2.md`](migration-v2.md)。
