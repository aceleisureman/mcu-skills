"""Admin REST API: keys, documents, jobs."""

from __future__ import annotations

from fastapi import APIRouter, Depends, File, Form, HTTPException, Request, UploadFile

from pydantic import BaseModel, Field
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.auth.deps import AuthContext, generate_api_key, hash_secret, require_admin
from app.config import Settings, get_settings
from app.db.models import ApiKey, Document, utcnow
from app.db.session import get_session
from app.services.jobs import JobService
from app.services.knowledge import KnowledgeService
from app.services.skill_catalog import SkillCatalog
from app.services.storage import content_hash, delete_upload, save_upload

router = APIRouter(prefix="/api/admin", tags=["admin"])


class CreateKeyRequest(BaseModel):
    name: str = Field(default="client", max_length=128)
    role: str = Field(default="user", pattern="^(user|admin)$")


class KeyCreated(BaseModel):
    key_id: str
    name: str
    role: str
    token: str
    note: str = "请立即保存 token，服务端仅存哈希，之后无法再次查看明文"


class KeyInfo(BaseModel):
    key_id: str
    name: str
    role: str
    enabled: bool
    created_at: str | None
    last_used_at: str | None


def get_catalog(request_settings: Settings = Depends(get_settings)) -> SkillCatalog:
    return SkillCatalog(request_settings.skills_root, request_settings.max_resource_bytes)


@router.get("/health")
async def admin_health(
    _: AuthContext = Depends(require_admin),
    settings: Settings = Depends(get_settings),
    catalog: SkillCatalog = Depends(get_catalog),
) -> dict:
    skills = catalog.list_skills()
    return {
        "ok": True,
        "skills": len(skills),
        "data_dir": str(settings.data_dir),
        "db": settings.database_url.split("@")[-1] if "@" in settings.database_url else "mysql",
    }


@router.get("/keys", response_model=list[KeyInfo])
async def list_keys(
    _: AuthContext = Depends(require_admin),
    session: AsyncSession = Depends(get_session),
) -> list[KeyInfo]:
    result = await session.execute(select(ApiKey).order_by(ApiKey.id.desc()))
    rows = result.scalars().all()
    return [
        KeyInfo(
            key_id=r.key_id,
            name=r.name,
            role=r.role,
            enabled=r.enabled,
            created_at=r.created_at.isoformat() if r.created_at else None,
            last_used_at=r.last_used_at.isoformat() if r.last_used_at else None,
        )
        for r in rows
    ]


@router.post("/keys", response_model=KeyCreated)
async def create_key(
    body: CreateKeyRequest,
    _: AuthContext = Depends(require_admin),
    session: AsyncSession = Depends(get_session),
) -> KeyCreated:
    key_id, secret, token = generate_api_key()
    row = ApiKey(
        key_id=key_id,
        secret_hash=hash_secret(secret),
        name=body.name,
        role=body.role,
        enabled=True,
    )
    session.add(row)
    await session.commit()
    return KeyCreated(key_id=key_id, name=body.name, role=body.role, token=token)


@router.post("/keys/{key_id}/disable")
async def disable_key(
    key_id: str,
    _: AuthContext = Depends(require_admin),
    session: AsyncSession = Depends(get_session),
) -> dict:
    result = await session.execute(select(ApiKey).where(ApiKey.key_id == key_id))
    row = result.scalar_one_or_none()
    if row is None:
        raise HTTPException(status_code=404, detail="Key 不存在")
    row.enabled = False
    await session.commit()
    return {"ok": True, "key_id": key_id, "enabled": False}


@router.post("/keys/{key_id}/enable")
async def enable_key(
    key_id: str,
    _: AuthContext = Depends(require_admin),
    session: AsyncSession = Depends(get_session),
) -> dict:
    result = await session.execute(select(ApiKey).where(ApiKey.key_id == key_id))
    row = result.scalar_one_or_none()
    if row is None:
        raise HTTPException(status_code=404, detail="Key 不存在")
    row.enabled = True
    await session.commit()
    return {"ok": True, "key_id": key_id, "enabled": True}


@router.delete("/keys/{key_id}")
async def delete_key(
    key_id: str,
    _: AuthContext = Depends(require_admin),
    session: AsyncSession = Depends(get_session),
) -> dict:
    result = await session.execute(select(ApiKey).where(ApiKey.key_id == key_id))
    row = result.scalar_one_or_none()
    if row is None:
        raise HTTPException(status_code=404, detail="Key 不存在")
    await session.delete(row)
    await session.commit()
    return {"ok": True, "deleted": key_id}


@router.get("/documents")
async def list_documents(
    _: AuthContext = Depends(require_admin),
    session: AsyncSession = Depends(get_session),
    settings: Settings = Depends(get_settings),
) -> list[dict]:
    knowledge = KnowledgeService(settings.skills_root, settings.index_dir)
    return await knowledge.list_sources(session)


@router.post("/documents")
async def upload_document(
    _: AuthContext = Depends(require_admin),
    session: AsyncSession = Depends(get_session),
    settings: Settings = Depends(get_settings),
    file: UploadFile = File(...),
) -> dict:
    if not file.filename:
        raise HTTPException(status_code=400, detail="缺少文件名")
    lower = file.filename.lower()
    if not lower.endswith((".md", ".txt", ".markdown")):
        raise HTTPException(status_code=400, detail="一期仅支持 .md / .txt")
    data = await file.read()
    if not data:
        raise HTTPException(status_code=400, detail="空文件")
    if len(data) > 10 * 1024 * 1024:
        raise HTTPException(status_code=400, detail="文件超过 10MB")

    doc_id, _path, rel = save_upload(settings.uploads_dir, file.filename, data)
    row = Document(
        doc_id=doc_id,
        filename=file.filename,
        rel_path=rel,
        content_hash=content_hash(data),
        size_bytes=len(data),
        source="upload",
        created_at=utcnow(),
    )
    session.add(row)
    await session.commit()
    return {
        "ok": True,
        "doc_id": doc_id,
        "filename": file.filename,
        "size_bytes": len(data),
        "hint": "请调用 POST /api/admin/jobs/reindex 重建索引后再检索",
    }


@router.delete("/documents/{doc_id}")
async def delete_document(
    doc_id: str,
    _: AuthContext = Depends(require_admin),
    session: AsyncSession = Depends(get_session),
    settings: Settings = Depends(get_settings),
) -> dict:
    result = await session.execute(
        select(Document).where(Document.doc_id == doc_id, Document.source == "upload")
    )
    row = result.scalar_one_or_none()
    if row is None:
        raise HTTPException(status_code=404, detail="文档不存在或不可删（skill 源）")
    try:
        delete_upload(settings.uploads_dir, row.rel_path)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    await session.delete(row)
    await session.commit()
    return {"ok": True, "deleted": doc_id}


@router.post("/jobs/reindex")
async def trigger_reindex(
    request: Request,
    _: AuthContext = Depends(require_admin),
) -> dict:
    jobs: JobService = request.app.state.jobs
    job_id = await jobs.enqueue_reindex()
    return {"ok": True, "job_id": job_id}


@router.get("/jobs")
async def list_jobs(
    request: Request,
    _: AuthContext = Depends(require_admin),
    limit: int = 20,
) -> list[dict]:
    jobs: JobService = request.app.state.jobs
    return await jobs.list_jobs(limit=limit)


@router.post("/login")
async def login_form(
    session: AsyncSession = Depends(get_session),
    token: str = Form(...),
) -> dict:
    """供管理页表单登录：校验 admin token。"""
    from app.auth.deps import authenticate_token

    auth = await authenticate_token(session, token)
    if not auth.is_admin:
        raise HTTPException(status_code=403, detail="需要 admin 权限")
    return {"ok": True, "role": auth.role, "key_id": auth.key_id, "token": token}
