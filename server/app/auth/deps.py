"""API Key helpers and FastAPI dependencies."""

from __future__ import annotations

import hashlib
import secrets
from dataclasses import dataclass
from datetime import datetime, timezone

from fastapi import Depends, Header, HTTPException, Request, status
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.db.models import ApiKey
from app.db.session import get_session


def hash_secret(secret: str) -> str:
    return hashlib.sha256(secret.encode("utf-8")).hexdigest()


def generate_api_key() -> tuple[str, str, str]:
    """Return (key_id, secret, full_token) where full_token is key_id.secret."""
    key_id = secrets.token_hex(8)
    secret = secrets.token_urlsafe(32)
    return key_id, secret, f"{key_id}.{secret}"


def parse_token(token: str) -> tuple[str, str] | None:
    token = token.strip()
    if "." not in token:
        return None
    key_id, secret = token.split(".", maxsplit=1)
    if not key_id or not secret:
        return None
    return key_id, secret


@dataclass
class AuthContext:
    key_id: str
    name: str
    role: str
    record_id: int

    @property
    def is_admin(self) -> bool:
        return self.role == "admin"


async def authenticate_token(session: AsyncSession, token: str) -> AuthContext:
    parsed = parse_token(token)
    if not parsed:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="无效的 API Key 格式")
    key_id, secret = parsed
    result = await session.execute(select(ApiKey).where(ApiKey.key_id == key_id))
    row = result.scalar_one_or_none()
    if row is None or not row.enabled:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="API Key 无效或已禁用")
    if row.secret_hash != hash_secret(secret):
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="API Key 无效或已禁用")
    row.last_used_at = datetime.now(timezone.utc)
    await session.commit()
    return AuthContext(key_id=row.key_id, name=row.name, role=row.role, record_id=row.id)


async def require_api_key(
    request: Request,
    x_api_key: str | None = Header(default=None, alias="X-API-Key"),
    session: AsyncSession = Depends(get_session),
) -> AuthContext:
    token = x_api_key
    if not token:
        auth = request.headers.get("Authorization", "")
        if auth.lower().startswith("bearer "):
            token = auth[7:].strip()
    if not token:
        token = request.cookies.get("admin_session")
    if not token:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="缺少 API Key")
    return await authenticate_token(session, token)


async def require_admin(auth: AuthContext = Depends(require_api_key)) -> AuthContext:
    if not auth.is_admin:
        raise HTTPException(status_code=status.HTTP_403_FORBIDDEN, detail="需要 admin 权限")
    return auth
