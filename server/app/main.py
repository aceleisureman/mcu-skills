"""FastAPI application entrypoint."""

from __future__ import annotations

import logging
from contextlib import asynccontextmanager

from fastapi import Depends, FastAPI, Request
from fastapi.responses import JSONResponse
from sqlalchemy import select
from starlette.middleware.base import BaseHTTPMiddleware

from app import __version__
from app.admin import api as admin_api
from app.admin import ui as admin_ui
from app.auth.deps import (
    AuthContext,
    generate_api_key,
    hash_secret,
    parse_token,
    require_api_key,
)
from app.config import Settings, get_settings
from app.db.models import ApiKey
from app.db.session import init_db, init_engine, session_factory
from app.mcp_server.server import build_mcp
from app.services.jobs import JobService
from app.services.knowledge import KnowledgeService
from app.services.skill_catalog import SkillCatalog

logger = logging.getLogger(__name__)


class ApiKeyMiddleware(BaseHTTPMiddleware):
    """Require X-API-Key for /mcp and /api (admin UI uses cookie separately)."""

    async def dispatch(self, request: Request, call_next):
        path = request.url.path
        if path == "/health" or path.startswith("/admin"):
            return await call_next(request)
        if path.startswith("/api/") or path.startswith("/mcp"):
            token = request.headers.get("X-API-Key")
            if not token:
                auth = request.headers.get("Authorization", "")
                if auth.lower().startswith("bearer "):
                    token = auth[7:].strip()
            if not token:
                return JSONResponse({"detail": "缺少 API Key"}, status_code=401)
            try:
                factory = session_factory()
                async with factory() as session:
                    from app.auth.deps import authenticate_token

                    await authenticate_token(session, token)
            except Exception as exc:  # noqa: BLE001
                detail = getattr(exc, "detail", str(exc))
                return JSONResponse({"detail": detail}, status_code=401)
        return await call_next(request)


async def bootstrap_admin(settings: Settings) -> str | None:
    """Create bootstrap admin key when api_keys table is empty."""
    factory = session_factory()
    async with factory() as session:
        result = await session.execute(select(ApiKey).limit(1))
        if result.scalar_one_or_none() is not None:
            return None

        if settings.bootstrap_admin_key.strip():
            parsed = settings.bootstrap_admin_key.strip()
            parts = parse_token(parsed)
            if parts:
                key_id, secret = parts
                token = parsed
            else:
                key_id, _, _ = generate_api_key()
                secret = parsed
                token = f"{key_id}.{secret}"
                logger.warning(
                    "MCU_BOOTSTRAP_ADMIN_KEY 未使用 key_id.secret 格式，已自动生成 key_id"
                )
        else:
            key_id, secret, token = generate_api_key()

        session.add(
            ApiKey(
                key_id=key_id,
                secret_hash=hash_secret(secret),
                name="bootstrap-admin",
                role="admin",
                enabled=True,
            )
        )
        await session.commit()
        logger.warning("已创建初始 admin API Key，请立即保存: %s", token)
        return token


@asynccontextmanager
async def lifespan(app: FastAPI):
    settings = get_settings()
    settings.ensure_dirs()
    init_engine(settings)
    await init_db()

    catalog = SkillCatalog(settings.skills_root, settings.max_resource_bytes)
    knowledge = KnowledgeService(settings.skills_root, settings.index_dir)
    knowledge.load_snapshot()
    jobs = JobService(settings, knowledge)
    jobs.start()

    app.state.settings = settings
    app.state.catalog = catalog
    app.state.knowledge = knowledge
    app.state.jobs = jobs

    token = await bootstrap_admin(settings)
    if token:
        app.state.bootstrap_admin_token = token

    mcp = build_mcp(settings, catalog, knowledge)
    app.state.mcp = mcp
    try:
        mcp_app = mcp.streamable_http_app()
        app.mount("/mcp", mcp_app)
    except Exception:
        try:
            mcp_app = mcp.sse_app()
            app.mount("/mcp", mcp_app)
        except Exception as exc:  # noqa: BLE001
            logger.warning("MCP HTTP transport 不可用: %s", exc)

    yield

    jobs.shutdown()


def create_app() -> FastAPI:
    app = FastAPI(
        title="MCU Skills MCP Service",
        version=__version__,
        lifespan=lifespan,
    )
    app.add_middleware(ApiKeyMiddleware)
    app.include_router(admin_api.router)
    app.include_router(admin_ui.router)

    @app.get("/health")
    async def health() -> dict:
        return {"ok": True, "service": "mcu-skills-mcp", "version": __version__}

    @app.get("/api/skills")
    async def api_list_skills(
        request: Request,
        _: AuthContext = Depends(require_api_key),
    ) -> list:
        catalog: SkillCatalog = request.app.state.catalog
        return catalog.list_skills()

    @app.get("/api/route")
    async def api_route(
        request: Request,
        q: str,
        _: AuthContext = Depends(require_api_key),
    ) -> dict:
        catalog: SkillCatalog = request.app.state.catalog
        return catalog.route_skills(q)

    @app.get("/api/knowledge/search")
    async def api_knowledge_search(
        request: Request,
        q: str,
        top_k: int = 8,
        _: AuthContext = Depends(require_api_key),
    ) -> list:
        knowledge: KnowledgeService = request.app.state.knowledge
        return knowledge.search(q, top_k=top_k)

    return app


app = create_app()
