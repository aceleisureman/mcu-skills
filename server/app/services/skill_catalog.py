"""Skill catalog: wrap tools/skill_registry and evaluate_routing."""

from __future__ import annotations

import sys
import time
from pathlib import Path
from typing import Any


def _ensure_tools_path(skills_root: Path) -> None:
    tools_dir = str((skills_root / "tools").resolve())
    if tools_dir not in sys.path:
        sys.path.insert(0, tools_dir)


class SkillCatalog:
    """Cached registry access for MCP and REST."""

    def __init__(self, skills_root: Path, max_resource_bytes: int = 512_000) -> None:
        self.skills_root = skills_root.resolve()
        self.max_resource_bytes = max_resource_bytes
        self._registry: dict[str, Any] | None = None
        self._registry_mtime: float = 0.0
        _ensure_tools_path(self.skills_root)

    def _skills_mtime(self) -> float:
        latest = 0.0
        skills_dir = self.skills_root / "skills"
        if not skills_dir.is_dir():
            return latest
        for path in skills_dir.rglob("skill.json"):
            try:
                latest = max(latest, path.stat().st_mtime)
            except OSError:
                continue
        return latest

    def get_registry(self, force: bool = False) -> dict[str, Any]:
        mtime = self._skills_mtime()
        if force or self._registry is None or mtime > self._registry_mtime:
            from skill_registry import build_registry

            self._registry = build_registry(self.skills_root)
            self._registry_mtime = mtime or time.time()
        return self._registry

    def list_skills(self) -> list[dict[str, Any]]:
        registry = self.get_registry()
        out: list[dict[str, Any]] = []
        for item in registry.get("skills", []):
            if not isinstance(item, dict):
                continue
            out.append(
                {
                    "name": item.get("name"),
                    "version": item.get("version"),
                    "title": item.get("title"),
                    "summary": item.get("summary"),
                    "layer": item.get("layer"),
                    "dependencies": item.get("dependencies", []),
                    "triggers": item.get("triggers", []),
                    "route_count": len(item.get("routes", []) or []),
                }
            )
        return out

    def get_skill(self, name: str) -> dict[str, Any] | None:
        registry = self.get_registry()
        for item in registry.get("skills", []):
            if isinstance(item, dict) and item.get("name") == name:
                return item
        return None

    def route_skills(self, query: str) -> dict[str, Any]:
        from evaluate_routing import route_query

        registry = self.get_registry()
        skills, targets, matches = route_query(registry, query)
        return {
            "query": query,
            "skills": skills,
            "targets": targets,
            "matches": matches,
        }

    def get_resource(
        self,
        *,
        uri: str | None = None,
        skill: str | None = None,
        path: str | None = None,
    ) -> dict[str, Any]:
        from skill_registry import parse_skill_uri, resolve_skill_uri

        if uri:
            skill_name, resource = parse_skill_uri(uri)
            full_uri = uri.split("#", maxsplit=1)[0]
        elif skill and path:
            skill_name, resource = skill, path.lstrip("/")
            full_uri = f"skill://{skill_name}/{resource}"
        else:
            raise ValueError("必须提供 uri 或 skill+path")

        resolved = resolve_skill_uri(self.skills_root, full_uri)
        # resolve returns path; re-check containment
        try:
            resolved = resolved.resolve()
            skills_base = (self.skills_root / "skills").resolve()
            resolved.relative_to(skills_base)
        except (ValueError, OSError) as exc:
            raise FileNotFoundError(f"资源不在 skills 目录内: {full_uri}") from exc

        if not resolved.is_file():
            # directory: return listing
            if resolved.is_dir():
                files = sorted(
                    p.relative_to(resolved).as_posix()
                    for p in resolved.rglob("*")
                    if p.is_file()
                )
                return {
                    "uri": full_uri,
                    "skill": skill_name,
                    "path": resource,
                    "type": "directory",
                    "files": files,
                }
            raise FileNotFoundError(f"资源不存在: {full_uri}")

        size = resolved.stat().st_size
        if size > self.max_resource_bytes:
            raise ValueError(
                f"资源过大 ({size} bytes > {self.max_resource_bytes})，请缩小范围或分页读取"
            )
        text = resolved.read_text(encoding="utf-8", errors="replace")
        return {
            "uri": full_uri,
            "skill": skill_name,
            "path": resource,
            "type": "file",
            "size": size,
            "content": text,
        }

    def validate_skills(self) -> dict[str, Any]:
        from validate import validate

        errors, stats = validate(self.skills_root)
        return {"ok": not errors, "errors": errors, "stats": stats}

    def evaluate_routing(self, verbose: bool = False) -> dict[str, Any]:
        from evaluate_routing import evaluate, thresholds_pass

        report = evaluate(self.skills_root, self.get_registry())
        summary = report.get("summary") if isinstance(report.get("summary"), dict) else {}
        out: dict[str, Any] = {
            "ok": thresholds_pass(report),
            "metrics": summary,
        }
        if verbose:
            out["report"] = report
        else:
            results = report.get("results")
            if isinstance(results, list):
                failed = []
                for r in results:
                    if not isinstance(r, dict):
                        continue
                    if r.get("passed") is False or r.get("ok") is False or r.get("pass") is False:
                        failed.append(r.get("id"))
                out["failed_case_ids"] = [x for x in failed if x][:50]
        return out
