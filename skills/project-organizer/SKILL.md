---
name: project-organizer
description: 项目整理与规范化工具。对代码/文档项目进行结构扫描、目录重组、垃圾文件清理、命名规范化、文档索引更新；并可对嵌入式项目生成分级编号的功能清单、硬件清单（含 MCU）、引脚分配表和使用说明文档。当用户说"整理项目"、"清理项目"、"规范目录结构"、"整理功能/硬件/引脚"、"生成使用说明"，或使用 /organize 系列命令时使用。
---

# 项目整理 (Project Organizer)

## 概述

本 Skill 用于对项目进行系统化整理：扫描现状 → 生成整理方案 → 用户确认 → 执行整理 → 输出报告。适用于代码仓库、文档库、Skill 知识库等任意目录型项目。

**核心原则：先扫描、后方案、经确认、再动手。未经用户确认不得删除或移动任何文件。**

## 命令

| 命令 | 功能 | 说明 |
|------|------|------|
| `/organize` | 完整整理流程 | 依次执行 scan → plan → （确认后）apply → report |
| `/organize-scan` | 扫描项目现状 | 只读分析，输出问题清单，不做任何修改 |
| `/organize-plan` | 生成整理方案 | 基于扫描结果输出「操作清单」供用户确认 |
| `/organize-apply` | 执行整理 | 仅在用户确认方案后执行；逐项执行并记录 |
| `/organize-report` | 输出整理报告 | 汇总变更前后对比、已清理项、遗留待办 |
| `/organize-features` | 整理项目功能点 | 扫描源码/文档，输出 `1 / 1.1 / 1.2` 分级编号的详细功能清单 |
| `/organize-hardware` | 整理硬件清单 | 列出全部使用硬件（含 MCU 型号/参数）、接口、用途、驱动文件 |
| `/organize-pinout` | 整理硬件引脚 | 单独输出 MCU 引脚 ↔ 器件引脚分配表，含上拉/中断/冲突检查 |
| `/organize-manual` | 生成使用说明文档 | 基于功能/硬件/引脚整理结果生成 Markdown 使用说明（docs/manual.md） |
| `/organize-docs` | 一键汇总生成 | 依次执行 features → hardware → pinout → manual，产出全套项目文档 |

用户未使用命令但表达了整理意图时，等同于 `/organize`；表达"整理功能/硬件/引脚"、"生成使用说明"意图时，对应相应命令；表达"整理项目功能文档/全部文档"意图时，等同于 `/organize-docs`。这五个命令的详细执行规范与输出模板见 `references/feature-hardware-inventory.md`。

### `/organize-docs` 汇总流程

1. 依次执行 `/organize-features` → `/organize-hardware` → `/organize-pinout` → `/organize-manual`，共产出 4 份文档：`output://docs/features.md`、`output://docs/hardware.md`、`output://docs/pinout.md`、`output://docs/manual.md`
2. 后一阶段引用前面阶段的结果，保证四份文档中型号、引脚、功能编号一致
3. 完成后生成 `output://docs/README.md` 索引，列出四份文档及其用途，并汇报待确认项（标注为「待确认」的条目清单）

## 各阶段执行规范

### 1. `/organize-scan` — 扫描

按以下维度检查并输出问题清单（只读，不修改）：

- **垃圾文件**：`.DS_Store`、`Thumbs.db`、`*.tmp`、`*.bak`、`*.swp`、空文件、空目录、编辑器/系统缓存
- **构建产物**：`node_modules/`、`dist/`、`build/`、`__pycache__/`、`*.o`、`*.pyc` 等应被忽略而未忽略的目录
- **命名规范**：文件/目录命名风格是否统一（推荐 kebab-case，中文文档除外）；有无空格、特殊字符、大小写混乱
- **目录结构**：层级是否清晰、同类文件是否分散、是否有职责不明的目录
- **重复与冗余**：内容重复的文件、多份旧版本副本（如 `xxx-旧.md`、`xxx (1).txt`、`final_v2`）
- **文档一致性**：README/索引文件是否与实际目录结构一致、有无失效的内部链接
- **大文件**：> 10MB 的文件单独列出，确认是否应入库

### 2. `/organize-plan` — 方案

- 输出结构化「操作清单」，每项标明：操作类型（删除/移动/重命名/新建/修改）、目标路径、原因
- 给出整理后的目标目录树
- 按风险分级：**安全**（删缓存垃圾）/ **低风险**（重命名、移动）/ **需确认**（删除内容文件、合并重复文件）
- 将方案发给用户确认，未确认前不执行任何写操作

### 3. `/organize-apply` — 执行

- 严格按已确认的清单逐项执行，不擅自扩大范围
- 若项目为 git 仓库：整理前确认工作区干净，使用 `git mv` 保留历史，整理完成后单独提交（不与功能改动混在一起）
- 移动/重命名后同步更新所有引用该路径的文档链接与代码 import
- 遇到清单外的意外情况（文件被占用、路径冲突）暂停并询问用户

### 4. `/organize-report` — 报告

输出：变更统计（删除/移动/重命名数量）、整理前后目录树对比、已更新的引用链接清单、建议的后续维护规则（如 .gitignore 增补、命名约定）。

## 相关文档

- `references/organize-rules.md` — 详细整理规则（命名约定、目录结构模式、垃圾文件清单、.gitignore 推荐）
- `references/feature-hardware-inventory.md` — 功能点/硬件清单/引脚表/使用说明的执行规范与输出模板
- `scripts/scan_project.py` — 项目扫描脚本（只读），`python3 scripts/scan_project.py <项目路径>` 直接输出问题清单
