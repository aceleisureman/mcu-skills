"""Minimal admin Web UI (Jinja)."""

from __future__ import annotations

from pathlib import Path

from fastapi import APIRouter, Depends, Form, Request
from fastapi.responses import HTMLResponse, RedirectResponse
from fastapi.templating import Jinja2Templates
from sqlalchemy.ext.asyncio import AsyncSession

from app.auth.deps import authenticate_token
from app.config import Settings, get_settings
from app.db.session import get_session
from app.services.knowledge import KnowledgeService
from app.services.skill_catalog import SkillCatalog

router = APIRouter(tags=["admin-ui"])
TEMPLATES_DIR = Path(__file__).resolve().parent / "templates"
templates = Jinja2Templates(directory=str(TEMPLATES_DIR))


def _token_from_request(request: Request) -> str | None:
    return request.cookies.get("admin_session")


async def _require_admin_page(
    request: Request,
    session: AsyncSession = Depends(get_session),
):
    token = _token_from_request(request)
    if not token:
        return None
    try:
        auth = await authenticate_token(session, token)
    except Exception:
        return None
    if not auth.is_admin:
        return None
    return auth


@router.get("/admin/login", response_class=HTMLResponse)
async def login_page(request: Request) -> HTMLResponse:
    return templates.TemplateResponse(
        request,
        "login.html",
        {"error": None},
    )


@router.post("/admin/login", response_class=HTMLResponse)
async def login_submit(
    request: Request,
    session: AsyncSession = Depends(get_session),
    token: str = Form(...),
) -> HTMLResponse:
    try:
        auth = await authenticate_token(session, token.strip())
        if not auth.is_admin:
            raise ValueError("需要 admin 权限")
    except Exception as exc:  # noqa: BLE001
        return templates.TemplateResponse(
            request,
            "login.html",
            {"error": str(exc)},
            status_code=401,
        )
    resp = RedirectResponse(url="/admin", status_code=303)
    resp.set_cookie(
        "admin_session",
        token.strip(),
        httponly=True,
        samesite="lax",
        max_age=60 * 60 * 12,
    )
    return resp


@router.get("/admin/logout")
async def logout() -> RedirectResponse:
    resp = RedirectResponse(url="/admin/login", status_code=303)
    resp.delete_cookie("admin_session")
    return resp


@router.get("/admin", response_class=HTMLResponse)
async def admin_home(
    request: Request,
    session: AsyncSession = Depends(get_session),
    settings: Settings = Depends(get_settings),
) -> HTMLResponse:
    auth = await _require_admin_page(request, session)
    if auth is None:
        return RedirectResponse(url="/admin/login", status_code=303)
    catalog = SkillCatalog(settings.skills_root, settings.max_resource_bytes)
    knowledge = KnowledgeService(settings.skills_root, settings.index_dir)
    sources = await knowledge.list_sources(session)
    jobs = await request.app.state.jobs.list_jobs(limit=10)
    return templates.TemplateResponse(
        request,
        "index.html",
        {
            "auth": auth,
            "skill_count": len(catalog.list_skills()),
            "doc_count": len(sources),
            "jobs": jobs,
            "cron": settings.reindex_cron,
        },
    )


@router.get("/admin/keys", response_class=HTMLResponse)
async def keys_page(
    request: Request,
    session: AsyncSession = Depends(get_session),
) -> HTMLResponse:
    auth = await _require_admin_page(request, session)
    if auth is None:
        return RedirectResponse(url="/admin/login", status_code=303)
    from sqlalchemy import select

    from app.db.models import ApiKey

    result = await session.execute(select(ApiKey).order_by(ApiKey.id.desc()))
    rows = result.scalars().all()
    return templates.TemplateResponse(
        request,
        "keys.html",
        {"auth": auth, "keys": rows, "created_token": request.query_params.get("token")},
    )


@router.post("/admin/keys")
async def keys_create(
    request: Request,
    session: AsyncSession = Depends(get_session),
    name: str = Form("client"),
    role: str = Form("user"),
) -> RedirectResponse:
    auth = await _require_admin_page(request, session)
    if auth is None:
        return RedirectResponse(url="/admin/login", status_code=303)
    from app.auth.deps import generate_api_key, hash_secret
    from app.db.models import ApiKey

    if role not in {"user", "admin"}:
        role = "user"
    key_id, secret, token = generate_api_key()
    session.add(
        ApiKey(key_id=key_id, secret_hash=hash_secret(secret), name=name, role=role, enabled=True)
    )
    await session.commit()
    return RedirectResponse(url=f"/admin/keys?token={token}", status_code=303)


@router.get("/admin/docs", response_class=HTMLResponse)
async def docs_page(
    request: Request,
    session: AsyncSession = Depends(get_session),
    settings: Settings = Depends(get_settings),
) -> HTMLResponse:
    auth = await _require_admin_page(request, session)
    if auth is None:
        return RedirectResponse(url="/admin/login", status_code=303)
    knowledge = KnowledgeService(settings.skills_root, settings.index_dir)
    sources = await knowledge.list_sources(session)
    uploads = [s for s in sources if s["source"] == "upload"]
    return templates.TemplateResponse(
        request,
        "docs.html",
        {"auth": auth, "docs": uploads},
    )


@router.post("/admin/docs")
async def docs_upload(
    request: Request,
    session: AsyncSession = Depends(get_session),
    settings: Settings = Depends(get_settings),
) -> RedirectResponse:
    auth = await _require_admin_page(request, session)
    if auth is None:
        return RedirectResponse(url="/admin/login", status_code=303)
    form = await request.form()
    upload = form.get("file")
    if upload is None or not hasattr(upload, "read"):
        return RedirectResponse(url="/admin/docs", status_code=303)
    data = await upload.read()
    filename = getattr(upload, "filename", None) or "upload.txt"
    if not str(filename).lower().endswith((".md", ".txt", ".markdown")):
        return RedirectResponse(url="/admin/docs", status_code=303)
    from app.db.models import Document, utcnow
    from app.services.storage import content_hash, save_upload

    doc_id, _path, rel = save_upload(settings.uploads_dir, filename, data)
    session.add(
        Document(
            doc_id=doc_id,
            filename=filename,
            rel_path=rel,
            content_hash=content_hash(data),
            size_bytes=len(data),
            source="upload",
            created_at=utcnow(),
        )
    )
    await session.commit()
    return RedirectResponse(url="/admin/docs", status_code=303)


@router.get("/admin/jobs", response_class=HTMLResponse)
async def jobs_page(
    request: Request,
    session: AsyncSession = Depends(get_session),
    settings: Settings = Depends(get_settings),
) -> HTMLResponse:
    auth = await _require_admin_page(request, session)
    if auth is None:
        return RedirectResponse(url="/admin/login", status_code=303)
    jobs = await request.app.state.jobs.list_jobs(limit=30)
    return templates.TemplateResponse(
        request,
        "jobs.html",
        {"auth": auth, "jobs": jobs, "cron": settings.reindex_cron},
    )


@router.post("/admin/jobs/reindex")
async def jobs_reindex(
    request: Request,
    session: AsyncSession = Depends(get_session),
) -> RedirectResponse:
    auth = await _require_admin_page(request, session)
    if auth is None:
        return RedirectResponse(url="/admin/login", status_code=303)
    await request.app.state.jobs.enqueue_reindex()
    return RedirectResponse(url="/admin/jobs", status_code=303)
