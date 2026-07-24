"""MCP tool registration via FastMCP (mounted under FastAPI)."""

from __future__ import annotations

import json
from typing import Any

from mcp.server.fastmcp import FastMCP

from app.config import Settings
from app.db.session import session_factory
from app.services.knowledge import KnowledgeService
from app.services.organize import OrganizeService
from app.services.skill_catalog import SkillCatalog


def build_mcp(settings: Settings, catalog: SkillCatalog, knowledge: KnowledgeService) -> FastMCP:
    mcp = FastMCP(
        "mcu-skills",
        instructions=(
            "MCU Skills 内部知识库 MCP。"
            "提供 Skill 路由/读取、知识检索、校验评测与 organize 协作工具。"
            "调用需有效 API Key（由网关/HTTP 层校验）。"
        ),
    )
    organize = OrganizeService(settings.skills_root, settings.allowlist_roots())

    def _err(exc: Exception) -> str:
        return json.dumps({"ok": False, "error": str(exc)}, ensure_ascii=False)

    def _ok(data: Any) -> str:
        return json.dumps({"ok": True, "data": data}, ensure_ascii=False, default=str)

    @mcp.tool()
    def list_skills() -> str:
        """列出全部 Skill（name/layer/version/summary/triggers）。"""
        try:
            return _ok(catalog.list_skills())
        except Exception as exc:  # noqa: BLE001
            return _err(exc)

    @mcp.tool()
    def get_skill(name: str) -> str:
        """获取单个 Skill 的完整元数据（含 routes 与 resources）。"""
        try:
            item = catalog.get_skill(name)
            if item is None:
                return _err(ValueError(f"Skill 不存在: {name}"))
            return _ok(item)
        except Exception as exc:  # noqa: BLE001
            return _err(exc)

    @mcp.tool()
    def get_resource(uri: str = "", skill: str = "", path: str = "") -> str:
        """读取 Skill 资源。优先 uri=skill://name/path；也可 skill+path。"""
        try:
            data = catalog.get_resource(
                uri=uri or None,
                skill=skill or None,
                path=path or None,
            )
            return _ok(data)
        except Exception as exc:  # noqa: BLE001
            return _err(exc)

    @mcp.tool()
    def route_skills(query: str) -> str:
        """按关键词路由到 Skill 与 skill:// 目标（复用 evaluate_routing.route_query）。"""
        try:
            return _ok(catalog.route_skills(query))
        except Exception as exc:  # noqa: BLE001
            return _err(exc)

    @mcp.tool()
    def knowledge_search(query: str, top_k: int = 8) -> str:
        """在 Skill 正文与已索引训练数据中 BM25 检索。"""
        try:
            return _ok(knowledge.search(query, top_k=top_k))
        except Exception as exc:  # noqa: BLE001
            return _err(exc)

    @mcp.tool()
    async def knowledge_list_sources() -> str:
        """列出已登记的知识来源（skill + 上传文档）。"""
        try:
            factory = session_factory()
            async with factory() as session:
                data = await knowledge.list_sources(session)
            return _ok(data)
        except Exception as exc:  # noqa: BLE001
            return _err(exc)

    @mcp.tool()
    def validate_skills() -> str:
        """运行仓库 Skill 结构校验（tools/validate.py）。"""
        try:
            return _ok(catalog.validate_skills())
        except Exception as exc:  # noqa: BLE001
            return _err(exc)

    @mcp.tool()
    def evaluate_routing(verbose: bool = False) -> str:
        """运行路由评测摘要（tools/evaluate_routing.py）。"""
        try:
            return _ok(catalog.evaluate_routing(verbose=verbose))
        except Exception as exc:  # noqa: BLE001
            return _err(exc)

    @mcp.tool()
    def organize_scan(project_path: str) -> str:
        """对白名单内的项目路径执行 organize-scan（JSON 问题清单）。"""
        try:
            return _ok(organize.scan(project_path))
        except Exception as exc:  # noqa: BLE001
            return _err(exc)

    @mcp.tool()
    def organize_get_spec() -> str:
        """返回 project-organizer 规范与模板（由调用方 Agent 成文）。"""
        try:
            return _ok(organize.get_spec())
        except Exception as exc:  # noqa: BLE001
            return _err(exc)

    @mcp.tool()
    def organize_write_doc(project_path: str, relative_path: str, content: str) -> str:
        """在白名单项目下写入 docs/ 相对路径文档。"""
        try:
            return _ok(organize.write_doc(project_path, relative_path, content))
        except Exception as exc:  # noqa: BLE001
            return _err(exc)

    return mcp
