# 项目整理规则

## 1. 命名约定

| 对象 | 约定 | 示例 |
|------|------|------|
| 目录 | kebab-case，全小写 | `driver-templates/`、`user-guides/` |
| 代码文件 | 跟随语言惯例（C: `snake_case.c`；JS/TS: `kebab-case.ts` 或框架约定） | `motor_driver.c` |
| Markdown 文档 | kebab-case 英文名；中文标题写在文件首行 `#` | `hardware-design.md` |
| 禁止 | 空格、括号副本名、中英混杂、`final/新建/副本/v2` 等版本后缀 | ~~`设计 (1) final_v2.md`~~ |

版本管理交给 git，文件名中不出现版本号；确需保留历史快照的放 `archive/` 目录。

## 2. 推荐目录结构模式

### 代码项目
```
project/
├── README.md          # 项目说明 + 快速开始
├── src/               # 源码
├── include/           # 公共头文件（C/C++）
├── tests/             # 测试
├── docs/              # 文档
├── scripts/           # 构建/工具脚本
├── examples/          # 示例
└── archive/           # 归档（可选，明确标注不再维护）
```

### 文档/知识库项目（含 Skill）
```
project/
├── SKILL.md 或 README.md   # 入口 + 路由
├── references/             # 按主题分类的参考文档
├── templates/              # 模板
├── guides/                 # 通用指南
├── examples/               # 示例
└── CHANGELOG.md            # 版本记录
```

整理原则：
- 单个目录直接子项 ≤ 15 个，超出则按主题建子目录
- 同类文件必须同目录；一个文件只有一个"正版"，不留多处副本
- 入口文件（README/SKILL.md）必须与实际结构保持同步

## 3. 垃圾文件清单（可安全删除）

- 系统缓存：`.DS_Store`、`Thumbs.db`、`desktop.ini`
- 编辑器残留：`*.swp`、`*.swo`、`*~`、`.idea/`（未共享配置时）、`.vscode/`（未共享配置时）
- 临时/备份：`*.tmp`、`*.bak`、`*.orig`、`*.rej`
- 构建产物（应通过 .gitignore 排除而非入库）：`__pycache__/`、`*.pyc`、`node_modules/`、`dist/`、`build/`、`*.o`、`*.obj`、`.cache/`
- 空文件与空目录（git 需保留的用 `.gitkeep`）

## 4. .gitignore 推荐基线

```gitignore
# 系统
.DS_Store
Thumbs.db

# 编辑器
*.swp
*~
.idea/
.vscode/

# 构建产物
__pycache__/
*.py[cod]
node_modules/
dist/
build/
*.o
*.obj

# 环境与密钥
.env
.env.*
*.pem
```

## 5. 重复文件处理

1. 内容完全相同：保留规范路径的一份，其余删除，引用统一指向保留份
2. 内容相近（新旧版本）：diff 确认后保留最新，旧版视价值移入 `archive/` 或删除
3. 无法判断新旧：列出差异交用户决定，不擅自合并

## 6. 引用同步检查

移动/重命名后必须检查并更新：
- Markdown 内部链接与图片路径
- 代码中的 `#include` / `import` / 配置文件路径
- CI / 构建脚本中的路径引用
- 入口文件（README/SKILL.md）中的目录树与索引表
