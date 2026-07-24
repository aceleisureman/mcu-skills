"""Database engine and session helpers (MySQL + aiomysql)."""

from __future__ import annotations

from collections.abc import AsyncGenerator

from sqlalchemy.ext.asyncio import (
    AsyncSession,
    async_sessionmaker,
    create_async_engine,
)

from app.config import Settings
from app.db.models import Base

_engine = None
_session_factory: async_sessionmaker[AsyncSession] | None = None


def init_engine(settings: Settings):
    global _engine, _session_factory
    url = settings.database_url
    if not url.startswith("mysql+aiomysql://"):
        raise RuntimeError(
            "仅支持 MySQL：请设置 MCU_DATABASE_URL=mysql+aiomysql://user:pass@host:3306/db"
            f" 或 MCU_DB_* 分项；当前: {url.split('://', 1)[0]}://"
        )
    _engine = create_async_engine(
        url,
        echo=False,
        pool_pre_ping=True,
        pool_recycle=3600,
    )
    _session_factory = async_sessionmaker(_engine, expire_on_commit=False)
    return _engine


async def init_db() -> None:
    if _engine is None:
        raise RuntimeError("engine not initialized")
    async with _engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)


async def get_session() -> AsyncGenerator[AsyncSession, None]:
    if _session_factory is None:
        raise RuntimeError("session factory not initialized")
    async with _session_factory() as session:
        yield session


def session_factory() -> async_sessionmaker[AsyncSession]:
    if _session_factory is None:
        raise RuntimeError("session factory not initialized")
    return _session_factory
