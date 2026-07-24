# MCU Skills 内部 MCP 服务

面向公司内网的 Agent MCP + 管理后台。内容仍以仓库 `skills/` 为真相源，元数据与训练文档索引使用 **MySQL**。

## 能力

| 类型 | 说明 |
|------|------|
| MCP Tools | `list_skills` / `get_skill` / `get_resource` / `route_skills` / `knowledge_search` / `knowledge_list_sources` / `validate_skills` / `evaluate_routing` / `organize_scan` / `organize_get_spec` / `organize_write_doc` |
| REST | `/api/skills`、`/api/route`、`/api/knowledge/search`、`/api/admin/*` |
| Admin Web | `/admin` — Key、训练数据上传、索引任务 |
| 鉴权 | Header `X-API-Key: key_id.secret`（Admin 页可用 Cookie） |

## 依赖

- Python 3.12+
- MySQL 8.x（utf8mb4）
- 本仓库根目录（含 `skills/`、`tools/`）

## 快速开始

### 1. 建库

```sql
CREATE DATABASE mcu_skills DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'mcu_skills'@'%' IDENTIFIED BY 'change-me';
GRANT ALL ON mcu_skills.* TO 'mcu_skills'@'%';
FLUSH PRIVILEGES;
```

或执行 `sql/schema.sql`。

### 2. 安装与配置

```sh
cd server
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env
# 编辑 MCU_DATABASE_URL / MCU_DB_* / MCU_BOOTSTRAP_ADMIN_KEY
```

### 3. 启动

在 `server/` 目录：

```sh
export PYTHONPATH=.
export MCU_SKILLS_ROOT=..
uvicorn app.main:app --host 0.0.0.0 --port 8080
```

首次启动若 `api_keys` 为空，会创建 admin Key 并写在日志中（或使用 `MCU_BOOTSTRAP_ADMIN_KEY`）。

### 4. 验证

```sh
curl -s http://127.0.0.1:8080/health
curl -s -H "X-API-Key: $TOKEN" http://127.0.0.1:8080/api/skills | head
```

管理后台：<http://127.0.0.1:8080/admin/login>

上传训练数据后，在后台「任务」或：

```sh
curl -X POST -H "X-API-Key: $ADMIN_TOKEN" http://127.0.0.1:8080/api/admin/jobs/reindex
```

## Docker Compose

```sh
cd server
docker compose up --build
```

包含 MySQL 8.4 + 应用。数据卷 `app_data` 存上传与 BM25 快照。

## 环境变量（`MCU_` 前缀）

| 变量 | 说明 |
|------|------|
| `DATABASE_URL` | `mysql+aiomysql://user:pass@host:3306/db?charset=utf8mb4` |
| `DB_HOST` / `DB_PORT` / `DB_USER` / `DB_PASSWORD` / `DB_NAME` | URL 为空时拼装 |
| `SKILLS_ROOT` | 仓库根 |
| `DATA_DIR` | 上传与索引快照 |
| `BOOTSTRAP_ADMIN_KEY` | 首次 admin token |
| `REINDEX_CRON` | APScheduler crontab，默认 `0 2 * * *` |
| `WORKSPACE_ALLOWLIST` | organize 路径白名单，逗号分隔 |

## MCP 客户端

HTTP 传输挂载在 `/mcp`（依赖已安装的 `mcp` 包提供的 streamable HTTP / SSE）。请求需带 `X-API-Key`。

## 说明

- **不改** Skill 内容语义；`tools/*` 通过 `sys.path` 复用。
- organize 长文由调用方 Agent 根据 `organize_get_spec` 生成；服务只提供扫描、规范与受控写回。
- 按仓库全局规则：**未授权不连接 MySQL 执行写操作**；部署时请自行建库并授权。

## 测试

```sh
cd server
PYTHONPATH=. pytest tests -q
```

单元测试不连真实 MySQL（鉴权哈希 / 路由 / 分块）。
