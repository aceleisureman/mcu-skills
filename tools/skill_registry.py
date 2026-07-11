#!/usr/bin/env python3
"""Generate and inspect the machine-readable Skill registry."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path, PurePosixPath


RESOURCE_DIRS = ("references", "templates", "guides", "examples", "scripts")
LAYER_ORDER = {"foundation": 0, "domain": 1, "orchestrator": 2, "utility": 3}
SKILL_URI_RE = re.compile(r"skill://([a-z0-9]+(?:-[a-z0-9]+)*)/(.+)")
README_START = "<!-- GENERATED:SKILL_TABLE:START -->"
README_END = "<!-- GENERATED:SKILL_TABLE:END -->"
SKILLS_START = "<!-- GENERATED:DEPENDENCY_TABLE:START -->"
SKILLS_END = "<!-- GENERATED:DEPENDENCY_TABLE:END -->"
ROUTES_START = "<!-- GENERATED:ROUTE_TABLE:START -->"
ROUTES_END = "<!-- GENERATED:ROUTE_TABLE:END -->"


def load_metadata(root: Path) -> list[dict[str, object]]:
    metadata: list[dict[str, object]] = []
    for path in sorted((root / "skills").glob("*/skill.json")):
        value = json.loads(path.read_text(encoding="utf-8"))
        if not isinstance(value, dict):
            raise ValueError(f"{path}: 顶层必须是 JSON object")
        value["_path"] = path.relative_to(root).as_posix()
        metadata.append(value)
    return sorted(
        metadata,
        key=lambda item: (
            LAYER_ORDER.get(str(item.get("layer", "")), 99),
            str(item.get("name", "")),
        ),
    )


def inventory_skill(skill_dir: Path) -> dict[str, list[str]]:
    inventory: dict[str, list[str]] = {}
    for resource_dir in RESOURCE_DIRS:
        base = skill_dir / resource_dir
        inventory[resource_dir] = (
            sorted(
                path.relative_to(skill_dir).as_posix() for path in base.rglob("*") if path.is_file()
            )
            if base.is_dir()
            else []
        )
    return inventory


def build_registry(root: Path) -> dict[str, object]:
    skills: list[dict[str, object]] = []
    for item in load_metadata(root):
        name = str(item["name"])
        entry = {key: value for key, value in item.items() if key != "_path"}
        entry["resources"] = inventory_skill(root / "skills" / name)
        skills.append(entry)
    return {
        "schema_version": 1,
        "generated_by": "tools/skill_registry.py",
        "skills": skills,
    }


def parse_skill_uri(uri: str) -> tuple[str, str]:
    target = uri.split("#", maxsplit=1)[0]
    match = SKILL_URI_RE.fullmatch(target)
    if not match:
        raise ValueError(f"无效 Skill URI: {uri}")
    skill_name, resource = match.groups()
    path = PurePosixPath(resource)
    if path.is_absolute() or ".." in path.parts:
        raise ValueError(f"Skill URI 不得越过 Skill 根目录: {uri}")
    return skill_name, path.as_posix()


def resolve_skill_uri(root: Path, uri: str) -> Path:
    skill_name, resource = parse_skill_uri(uri)
    return root / "skills" / skill_name / resource


def as_list(value: object) -> list[object]:
    if not isinstance(value, list):
        return []
    return value


def dependency_text(item: dict[str, object]) -> str:
    dependencies = [str(value) for value in as_list(item.get("dependencies"))]
    return "、".join(f"`{name}`" for name in dependencies) if dependencies else "无"


def render_skill_table(registry: dict[str, object]) -> str:
    lines = [
        "| Skill | 层级 | 版本 | 依赖 | 用途 |",
        "|-------|------|------|------|------|",
    ]
    for item_object in as_list(registry["skills"]):
        if not isinstance(item_object, dict):
            continue
        name = str(item_object["name"])
        lines.append(
            f"| [`{name}`](skills/{name}/SKILL.md) | "
            f"`{item_object['layer']}` | `v{item_object['version']}` | "
            f"{dependency_text(item_object)} | {item_object['summary']} |"
        )
    return "\n".join(lines)


def render_dependency_table(registry: dict[str, object], prefix: str = "") -> str:
    lines = [
        "| Skill | 层级 | 直接依赖 | 安装入口 |",
        "|-------|------|----------|----------|",
    ]
    for item_object in as_list(registry["skills"]):
        if not isinstance(item_object, dict):
            continue
        name = str(item_object["name"])
        lines.append(
            f"| `{name}` | `{item_object['layer']}` | {dependency_text(item_object)} | "
            f"[`SKILL.md`]({prefix}{name}/SKILL.md) |"
        )
    return "\n".join(lines)


def render_route_table(item: dict[str, object]) -> str:
    lines = [
        "| 意图 | 关键词或型号 | 读取文件 |",
        "|------|------------|----------|",
    ]
    for route_object in as_list(item.get("routes")):
        if not isinstance(route_object, dict):
            continue
        keywords = "、".join(str(value) for value in as_list(route_object.get("keywords")))
        lines.append(f"| {route_object['intent']} | {keywords} | `{route_object['target']}` |")
    return "\n".join(lines)


def replace_generated_block(text: str, start: str, end: str, body: str) -> str:
    if start not in text or end not in text:
        raise ValueError(f"缺少生成标记: {start} / {end}")
    prefix, remainder = text.split(start, maxsplit=1)
    _, suffix = remainder.split(end, maxsplit=1)
    return f"{prefix}{start}\n{body}\n{end}{suffix}"


def route_target(name: str, target: str) -> str:
    if target.startswith("skill://"):
        return f"`{target}`"
    suffix = "/" if target.endswith("/") else ""
    label = PurePosixPath(target.rstrip("/")).name + suffix
    return f"[`{label}`](../skills/{name}/{target})"


def resource_entries(resources: dict[str, object], kind: str) -> list[str]:
    values = [str(value) for value in as_list(resources.get(kind))]
    if kind != "examples":
        return values
    roots: set[str] = set()
    for value in values:
        parts = PurePosixPath(value).parts
        roots.add("/".join(parts[:2]) + ("/" if len(parts) > 2 else ""))
    return sorted(roots)


def render_catalog(registry: dict[str, object]) -> str:
    lines = [
        "# 内容索引",
        "",
        "> 此文件由 `python3 tools/skill_registry.py --write` 生成，请勿手工编辑。",
        "",
        "日常使用应从对应 `SKILL.md` 的意图路由进入，只加载当前任务需要的资源。",
    ]
    for item_object in as_list(registry["skills"]):
        if not isinstance(item_object, dict):
            continue
        name = str(item_object["name"])
        lines.extend(
            [
                "",
                f"## {item_object['title']}",
                "",
                str(item_object["summary"]),
                "",
                f"- Skill：[`{name}`](../skills/{name}/SKILL.md)",
                f"- 版本：`v{item_object['version']}`",
                f"- 层级：`{item_object['layer']}`",
                f"- 依赖：{dependency_text(item_object)}",
                "",
                "### 路由",
                "",
                "| 意图 | 关键词 | 目标 |",
                "|------|--------|------|",
            ]
        )
        for route_object in as_list(item_object.get("routes")):
            if not isinstance(route_object, dict):
                continue
            keywords = "、".join(str(value) for value in as_list(route_object.get("keywords")))
            target = str(route_object["target"])
            lines.append(
                f"| {route_object['intent']} | {keywords} | {route_target(name, target)} |"
            )

        resources_object = item_object.get("resources")
        resources = resources_object if isinstance(resources_object, dict) else {}
        lines.extend(["", "### 资源", ""])
        for kind in RESOURCE_DIRS:
            entries = resource_entries(resources, kind)
            if not entries:
                continue
            links = "、".join(route_target(name, entry) for entry in entries)
            lines.append(f"- {kind}（{len(entries)}）：{links}")
    return "\n".join(lines) + "\n"


def node_id(name: str) -> str:
    return name.replace("-", "_")


def render_dependency_graph(registry: dict[str, object]) -> str:
    skills = [item for item in as_list(registry["skills"]) if isinstance(item, dict)]
    lines = [
        "# Skill 依赖图",
        "",
        "> 此文件由 `python3 tools/skill_registry.py --write` 生成，请勿手工编辑。",
        "",
        "```mermaid",
        "graph TD",
    ]
    for item in skills:
        name = str(item["name"])
        lines.append(f'  {node_id(name)}["{name}<br/>{item["layer"]}"]')
    for item in skills:
        source = node_id(str(item["name"]))
        for dependency in as_list(item.get("dependencies")):
            lines.append(f"  {source} --> {node_id(str(dependency))}")
    lines.extend(
        [
            "```",
            "",
            "## 直接依赖",
            "",
            render_dependency_table(registry, "../skills/"),
            "",
        ]
    )
    return "\n".join(lines)


def generated_outputs(root: Path, registry: dict[str, object] | None = None) -> dict[Path, str]:
    if registry is None:
        registry = build_registry(root)
    readme = root / "README.md"
    skills_readme = root / "skills" / "README.md"
    outputs = {
        root / "skills" / "registry.json": json.dumps(registry, ensure_ascii=False, indent=2)
        + "\n",
        root / "docs" / "content-catalog.md": render_catalog(registry),
        root / "docs" / "dependency-graph.md": render_dependency_graph(registry),
        readme: replace_generated_block(
            readme.read_text(encoding="utf-8"),
            README_START,
            README_END,
            render_skill_table(registry),
        ),
        skills_readme: replace_generated_block(
            skills_readme.read_text(encoding="utf-8"),
            SKILLS_START,
            SKILLS_END,
            render_dependency_table(registry),
        ),
    }
    for item_object in as_list(registry["skills"]):
        if not isinstance(item_object, dict):
            continue
        skill_md = root / "skills" / str(item_object["name"]) / "SKILL.md"
        if not skill_md.is_file():
            continue
        text = skill_md.read_text(encoding="utf-8")
        if ROUTES_START not in text or ROUTES_END not in text:
            continue
        outputs[skill_md] = replace_generated_block(
            text,
            ROUTES_START,
            ROUTES_END,
            render_route_table(item_object),
        )
    return outputs


def write_outputs(root: Path, verbose: bool = True) -> None:
    for path, content in generated_outputs(root).items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
        if verbose:
            print(f"updated {path.relative_to(root)}")


def check_outputs(root: Path, registry: dict[str, object] | None = None) -> list[Path]:
    stale = []
    for path, expected in generated_outputs(root, registry).items():
        if not path.is_file() or path.read_text(encoding="utf-8") != expected:
            stale.append(path)
    return stale


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--write", action="store_true", help="写入生成文件")
    action.add_argument("--check", action="store_true", help="检查生成文件是否最新")
    action.add_argument("--resolve", metavar="URI", help="解析 skill:// URI")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    args = parser.parse_args()
    root = args.root.resolve()

    if args.resolve:
        path = resolve_skill_uri(root, args.resolve)
        if not path.exists():
            print(f"资源不存在: {path}")
            return 1
        print(path)
        return 0
    if args.write:
        write_outputs(root)
        return 0

    stale = check_outputs(root)
    if stale:
        print("生成文件已过期:")
        for path in stale:
            print(f"  - {path.relative_to(root)}")
        return 1
    print("registry 与生成文档均为最新")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
