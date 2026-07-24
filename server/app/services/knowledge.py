"""Knowledge index: chunking + BM25 over skills and uploads."""

from __future__ import annotations

import json
import re
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

from rank_bm25 import BM25Okapi
from sqlalchemy import delete, select
from sqlalchemy.ext.asyncio import AsyncSession

from app.db.models import Chunk, Document, utcnow

TOKEN_RE = re.compile(r"[\w一-鿿]+", re.UNICODE)
HEADING_RE = re.compile(r"(?m)^(#{1,3})\s+(.+)$")
CODE_EXTS = {".c", ".h", ".cpp", ".hpp", ".cc", ".py", ".js", ".ts", ".ino", ".md", ".txt", ".json"}


def tokenize(text: str) -> list[str]:
    return [t.casefold() for t in TOKEN_RE.findall(text)]


@dataclass
class IndexChunk:
    doc_id: str
    chunk_index: int
    source: str
    uri: str
    title: str
    text: str
    content_hash: str


def chunk_markdown(text: str, *, doc_id: str, source: str, uri: str) -> list[IndexChunk]:
    parts: list[tuple[str, str]] = []
    current_title = Path(uri).name
    buffer: list[str] = []

    def flush() -> None:
        body = "\n".join(buffer).strip()
        if body:
            parts.append((current_title, body))
        buffer.clear()

    for line in text.splitlines():
        m = HEADING_RE.match(line)
        if m:
            flush()
            current_title = m.group(2).strip()
            buffer.append(line)
        else:
            buffer.append(line)
    flush()

    if not parts:
        parts = [(Path(uri).name, text.strip() or "")]

    chunks: list[IndexChunk] = []
    idx = 0
    for title, body in parts:
        # further split long sections
        for piece in _split_long(body, max_chars=1800):
            if not piece.strip():
                continue
            chunks.append(
                IndexChunk(
                    doc_id=doc_id,
                    chunk_index=idx,
                    source=source,
                    uri=uri,
                    title=title,
                    text=piece,
                    content_hash="",
                )
            )
            idx += 1
    return chunks


def _split_long(text: str, max_chars: int) -> list[str]:
    if len(text) <= max_chars:
        return [text]
    out: list[str] = []
    start = 0
    while start < len(text):
        end = min(len(text), start + max_chars)
        if end < len(text):
            cut = text.rfind("\n\n", start, end)
            if cut > start + max_chars // 2:
                end = cut
        out.append(text[start:end].strip())
        start = end
    return out


def chunk_code_file(text: str, *, doc_id: str, source: str, uri: str) -> list[IndexChunk]:
    pieces = _split_long(text, max_chars=2500)
    return [
        IndexChunk(
            doc_id=doc_id,
            chunk_index=i,
            source=source,
            uri=uri,
            title=Path(uri).name,
            text=piece,
            content_hash="",
        )
        for i, piece in enumerate(pieces)
        if piece.strip()
    ]


class KnowledgeService:
    def __init__(self, skills_root: Path, index_dir: Path) -> None:
        self.skills_root = skills_root.resolve()
        self.index_dir = index_dir
        self.index_dir.mkdir(parents=True, exist_ok=True)
        self._bm25: BM25Okapi | None = None
        self._corpus: list[IndexChunk] = []
        self._tokenized: list[list[str]] = []

    def _snapshot_path(self) -> Path:
        return self.index_dir / "bm25_corpus.json"

    def load_snapshot(self) -> None:
        path = self._snapshot_path()
        if not path.is_file():
            self._bm25 = None
            self._corpus = []
            self._tokenized = []
            return
        data = json.loads(path.read_text(encoding="utf-8"))
        self._corpus = [IndexChunk(**item) for item in data]
        self._tokenized = [tokenize(c.text) for c in self._corpus]
        self._bm25 = BM25Okapi(self._tokenized) if self._tokenized else None

    def save_snapshot(self) -> None:
        payload = [asdict(c) for c in self._corpus]
        self._snapshot_path().write_text(
            json.dumps(payload, ensure_ascii=False),
            encoding="utf-8",
        )

    def build_from_chunks(self, chunks: list[IndexChunk]) -> None:
        self._corpus = chunks
        self._tokenized = [tokenize(c.text) for c in chunks]
        self._bm25 = BM25Okapi(self._tokenized) if self._tokenized else None
        self.save_snapshot()

    def search(self, query: str, top_k: int = 8) -> list[dict[str, Any]]:
        if not query.strip():
            return []
        if self._bm25 is None:
            self.load_snapshot()
        if self._bm25 is None or not self._corpus:
            return []
        tokens = tokenize(query)
        if not tokens:
            return []
        scores = self._bm25.get_scores(tokens)
        ranked = sorted(enumerate(scores), key=lambda x: x[1], reverse=True)
        results: list[dict[str, Any]] = []
        for idx, score in ranked[: max(1, top_k)]:
            if score <= 0:
                continue
            c = self._corpus[idx]
            snippet = c.text[:400].replace("\n", " ")
            results.append(
                {
                    "score": float(score),
                    "doc_id": c.doc_id,
                    "source": c.source,
                    "uri": c.uri,
                    "title": c.title,
                    "snippet": snippet,
                    "chunk_index": c.chunk_index,
                }
            )
        return results

    def iter_skill_files(self) -> list[tuple[str, Path, str]]:
        """Yield (doc_id, path, uri) for skill content files."""
        out: list[tuple[str, Path, str]] = []
        skills_dir = self.skills_root / "skills"
        if not skills_dir.is_dir():
            return out
        for skill_dir in sorted(skills_dir.iterdir()):
            if not skill_dir.is_dir():
                continue
            skill_name = skill_dir.name
            for sub in ("references", "guides", "templates", "examples", "scripts"):
                base = skill_dir / sub
                if not base.is_dir():
                    continue
                for path in sorted(base.rglob("*")):
                    if not path.is_file():
                        continue
                    if path.suffix.lower() not in CODE_EXTS and path.name != "SKILL.md":
                        continue
                    rel = path.relative_to(skill_dir).as_posix()
                    uri = f"skill://{skill_name}/{rel}"
                    doc_id = f"skill:{skill_name}:{rel}"
                    out.append((doc_id, path, uri))
            skill_md = skill_dir / "SKILL.md"
            if skill_md.is_file():
                uri = f"skill://{skill_name}/SKILL.md"
                out.append((f"skill:{skill_name}:SKILL.md", skill_md, uri))
        return out

    async def reindex(
        self,
        session: AsyncSession,
        uploads_dir: Path,
    ) -> dict[str, Any]:
        chunks: list[IndexChunk] = []

        # clear previous skill chunks and re-add; keep document rows for uploads
        await session.execute(delete(Chunk))
        await session.execute(delete(Document).where(Document.source == "skill"))

        skill_count = 0
        for doc_id, path, uri in self.iter_skill_files():
            try:
                text = path.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            source = "skill"
            if path.suffix.lower() == ".md":
                file_chunks = chunk_markdown(text, doc_id=doc_id, source=source, uri=uri)
            else:
                file_chunks = chunk_code_file(text, doc_id=doc_id, source=source, uri=uri)
            chunks.extend(file_chunks)
            session.add(
                Document(
                    doc_id=doc_id,
                    filename=path.name,
                    rel_path=uri,
                    content_hash="",
                    size_bytes=path.stat().st_size,
                    source="skill",
                    indexed_at=utcnow(),
                )
            )
            skill_count += 1

        upload_count = 0
        result = await session.execute(select(Document).where(Document.source == "upload"))
        docs = list(result.scalars().all())
        for doc in docs:
            path = uploads_dir / doc.rel_path
            if not path.is_file():
                continue
            try:
                text = path.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            uri = f"upload://{doc.doc_id}/{doc.filename}"
            file_chunks = chunk_markdown(text, doc_id=doc.doc_id, source="upload", uri=uri)
            chunks.extend(file_chunks)
            doc.indexed_at = utcnow()
            upload_count += 1

        for c in chunks:
            session.add(
                Chunk(
                    doc_id=c.doc_id,
                    chunk_index=c.chunk_index,
                    source=c.source,
                    uri=c.uri,
                    title=c.title,
                    text=c.text,
                    content_hash=c.content_hash,
                )
            )

        await session.commit()
        self.build_from_chunks(chunks)
        return {
            "skill_files": skill_count,
            "upload_docs": upload_count,
            "chunks": len(chunks),
        }

    async def list_sources(self, session: AsyncSession) -> list[dict[str, Any]]:
        result = await session.execute(select(Document).order_by(Document.created_at.desc()))
        rows = result.scalars().all()
        return [
            {
                "doc_id": r.doc_id,
                "filename": r.filename,
                "source": r.source,
                "size_bytes": r.size_bytes,
                "indexed_at": r.indexed_at.isoformat() if r.indexed_at else None,
                "created_at": r.created_at.isoformat() if r.created_at else None,
            }
            for r in rows
        ]
