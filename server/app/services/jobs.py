"""Background / scheduled jobs."""

from __future__ import annotations

import logging
from pathlib import Path

from apscheduler.schedulers.asyncio import AsyncIOScheduler
from apscheduler.triggers.cron import CronTrigger
from sqlalchemy import select

from app.config import Settings
from app.db.models import Job, utcnow
from app.db.session import session_factory
from app.services.knowledge import KnowledgeService

logger = logging.getLogger(__name__)


class JobService:
    def __init__(self, settings: Settings, knowledge: KnowledgeService) -> None:
        self.settings = settings
        self.knowledge = knowledge
        self.scheduler = AsyncIOScheduler()

    def start(self) -> None:
        cron = self.settings.reindex_cron.strip()
        if cron:
            try:
                trigger = CronTrigger.from_crontab(cron)
                self.scheduler.add_job(
                    self.run_reindex_job,
                    trigger=trigger,
                    id="reindex",
                    replace_existing=True,
                )
            except ValueError:
                logger.warning("无效的 REINDEX_CRON: %s", cron)
        if not self.scheduler.running:
            self.scheduler.start()

    def shutdown(self) -> None:
        if self.scheduler.running:
            self.scheduler.shutdown(wait=False)

    async def enqueue_reindex(self) -> int:
        factory = session_factory()
        async with factory() as session:
            job = Job(job_type="reindex", status="pending", message="queued")
            session.add(job)
            await session.commit()
            await session.refresh(job)
            job_id = job.id
        # fire and forget on scheduler
        self.scheduler.add_job(
            self.run_reindex_job,
            kwargs={"job_id": job_id},
            id=f"reindex-{job_id}",
            replace_existing=True,
        )
        return job_id

    async def run_reindex_job(self, job_id: int | None = None) -> None:
        factory = session_factory()
        async with factory() as session:
            job: Job | None = None
            if job_id is not None:
                job = await session.get(Job, job_id)
            if job is None:
                job = Job(job_type="reindex", status="pending")
                session.add(job)
                await session.commit()
                await session.refresh(job)

            job.status = "running"
            job.started_at = utcnow()
            job.message = "reindexing..."
            await session.commit()

            try:
                stats = await self.knowledge.reindex(
                    session, Path(self.settings.uploads_dir)
                )
                job.status = "ok"
                job.message = (
                    f"skill_files={stats['skill_files']} "
                    f"uploads={stats['upload_docs']} chunks={stats['chunks']}"
                )
            except Exception as exc:  # noqa: BLE001 - surface in job log
                logger.exception("reindex failed")
                job.status = "error"
                job.message = str(exc)
            finally:
                job.finished_at = utcnow()
                await session.commit()

    async def list_jobs(self, limit: int = 20) -> list[dict]:
        factory = session_factory()
        async with factory() as session:
            result = await session.execute(
                select(Job).order_by(Job.id.desc()).limit(limit)
            )
            rows = result.scalars().all()
            return [
                {
                    "id": r.id,
                    "job_type": r.job_type,
                    "status": r.status,
                    "message": r.message,
                    "created_at": r.created_at.isoformat() if r.created_at else None,
                    "started_at": r.started_at.isoformat() if r.started_at else None,
                    "finished_at": r.finished_at.isoformat() if r.finished_at else None,
                }
                for r in rows
            ]
