"""Upload storage helpers."""

from __future__ import annotations

import hashlib
import re
import secrets
from pathlib import Path

SAFE_NAME = re.compile(r"[^A-Za-z0-9._一-鿿-]+")


def sanitize_filename(name: str) -> str:
    base = Path(name).name
    cleaned = SAFE_NAME.sub("_", base).strip("._")
    return cleaned or f"file_{secrets.token_hex(4)}"


def content_hash(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def save_upload(uploads_dir: Path, filename: str, data: bytes) -> tuple[str, Path, str]:
    """Save bytes under uploads_dir. Returns (doc_id, abs_path, rel_path)."""
    uploads_dir.mkdir(parents=True, exist_ok=True)
    doc_id = secrets.token_hex(12)
    safe = sanitize_filename(filename)
    rel = f"{doc_id}_{safe}"
    path = uploads_dir / rel
    path.write_bytes(data)
    return doc_id, path, rel


def delete_upload(uploads_dir: Path, rel_path: str) -> None:
    path = (uploads_dir / rel_path).resolve()
    if not str(path).startswith(str(uploads_dir.resolve())):
        raise ValueError("非法路径")
    if path.is_file():
        path.unlink()
