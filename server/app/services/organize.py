"""Organize helpers: scan + specs + safe write under allowlist."""

from __future__ import annotations

import importlib.util
import io
from contextlib import redirect_stdout
from pathlib import Path
from typing import Any


class OrganizeService:
    def __init__(self, skills_root: Path, allowlist: list[Path]) -> None:
        self.skills_root = skills_root.resolve()
        self.allowlist = [p.resolve() for p in allowlist]
        self._scan_mod = None

    def _load_scan(self):
        if self._scan_mod is not None:
            return self._scan_mod
        script = (
            self.skills_root
            / "skills"
            / "project-organizer"
            / "scripts"
            / "scan_project.py"
        )
        if not script.is_file():
            raise FileNotFoundError(f"找不到扫描脚本: {script}")
        spec = importlib.util.spec_from_file_location("scan_project", script)
        if spec is None or spec.loader is None:
            raise RuntimeError("无法加载 scan_project")
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        self._scan_mod = mod
        return mod

    def resolve_allowed(self, project_path: str) -> Path:
        path = Path(project_path).expanduser().resolve()
        if not path.is_dir():
            raise FileNotFoundError(f"不是目录: {path}")
        for root in self.allowlist:
            try:
                path.relative_to(root)
                return path
            except ValueError:
                continue
        allowed = ", ".join(str(p) for p in self.allowlist)
        raise PermissionError(f"路径不在白名单内: {path}；允许: {allowed}")

    def scan(self, project_path: str) -> dict[str, Any]:
        import json

        path = self.resolve_allowed(project_path)
        mod = self._load_scan()
        buf = io.StringIO()
        with redirect_stdout(buf):
            mod.scan(str(path), as_json=True)
        raw = buf.getvalue().strip()
        if not raw:
            return {}
        return json.loads(raw)

    def get_spec(self) -> dict[str, Any]:
        base = self.skills_root / "skills" / "project-organizer"
        files = {
            "skill": base / "SKILL.md",
            "rules": base / "references" / "organize-rules.md",
            "inventory": base / "references" / "feature-hardware-inventory.md",
            "manual_example": base / "examples" / "project-manual.md",
        }
        content: dict[str, str] = {}
        for key, path in files.items():
            if path.is_file():
                content[key] = path.read_text(encoding="utf-8", errors="replace")
        return {
            "description": (
                "organize 文档由调用方 Agent 根据规范扫描用户项目后成文；"
                "本服务提供 scan / 规范读取 / 白名单写回，不内嵌 LLM 生成。"
            ),
            "commands": [
                "organize-scan",
                "organize-features",
                "organize-hardware",
                "organize-pinout",
                "organize-manual",
                "organize-docs",
                "organize-manual-single",
            ],
            "files": content,
        }

    def write_doc(self, project_path: str, relative_path: str, content: str) -> dict[str, Any]:
        root = self.resolve_allowed(project_path)
        rel = Path(relative_path)
        if rel.is_absolute() or ".." in rel.parts:
            raise PermissionError("相对路径不得包含 .. 或绝对路径")
        # only allow writing under docs/ by default
        if rel.parts and rel.parts[0] not in {"docs", "output"}:
            # still allow docs/* and any single md under project if starts with docs
            if not str(rel).startswith("docs/"):
                raise PermissionError("仅允许写入项目下 docs/ 相对路径")
        target = (root / rel).resolve()
        target.relative_to(root)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(content, encoding="utf-8")
        return {"path": str(target), "relative": rel.as_posix(), "bytes": len(content.encode())}
