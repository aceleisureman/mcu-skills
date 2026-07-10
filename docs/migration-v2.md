# 从 mcu-components 迁移到 v2 多 Skill

v2 删除单体 `mcu-components` 入口，将内容迁移到 `skills/` 下 9 个 MCU Skill。

## 路径映射

| v1 路径 | v2 路径 |
|---------|---------|
| `mcu-components/references/sensors/` | `skills/mcu-sensors/references/` |
| `mcu-components/references/actuators/` | `skills/mcu-actuators/references/` |
| `mcu-components/references/display/` | `skills/mcu-displays/references/` |
| `mcu-components/references/communication/` | `skills/mcu-communication/references/` |
| `mcu-components/references/storage/` | `skills/mcu-storage/references/` |
| `mcu-components/references/power/` | `skills/mcu-power/references/` |
| `mcu-components/references/input/` | `skills/mcu-input/references/` |
| `mcu-components/templates/` | `skills/mcu-driver-core/templates/` |
| `mcu-components/guides/hardware-design.md` 等工程指南 | `skills/mcu-driver-core/guides/` |
| `mcu-components/guides/low-power.md` | `skills/mcu-system-design/guides/low-power.md` |
| `mcu-components/guides/ota-bootloader.md` | `skills/mcu-system-design/guides/ota-bootloader.md` |
| `mcu-components/examples/` | 按示例主要职责分配到各 Skill 的 `examples/` |

## 安装迁移

原来只安装 `mcu-components` 的用户：

1. 删除旧 `mcu-components` 目录。
2. 至少安装任务对应的领域 Skill 和 `mcu-driver-core`。
3. 完整产品开发安装全部 `mcu-*` Skill。
4. 更新脚本或文档中的旧路径。

v2.1 起，跨 Skill 静态资源不再依靠兄弟目录相对寻址，而统一使用 `skill://<skill-name>/<path>`。运行时生成目标使用 `output://<relative-path>`。

## 触发迁移

- “写 MPU6050 驱动”现在触发 `mcu-sensors`，并按需联合 `mcu-driver-core`。
- “设计 CAN 通信”现在触发 `mcu-communication`。
- “做低功耗环境监测站并支持 OTA”现在触发 `mcu-system-design`，再编排传感器、显示、通信、电源和驱动核心。
