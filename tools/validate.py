#!/usr/bin/env python3
"""Validate Skill structure, metadata, dependencies, routes and references."""

from __future__ import annotations

import json
import re
import sys
from functools import lru_cache
from pathlib import Path, PurePosixPath
from urllib.parse import unquote

from evaluate_routing import evaluate, report_path, report_text, thresholds_pass
from skill_registry import build_registry, check_outputs, parse_skill_uri, resolve_skill_uri


FRONTMATTER_RE = re.compile(r"\A---\r?\n(.*?)\r?\n---\r?\n", re.DOTALL)
SKILL_NAME_RE = re.compile(r"[a-z0-9]+(?:-[a-z0-9]+)*\Z")
SEMVER_RE = re.compile(r"\d+\.\d+\.\d+\Z")
MARKDOWN_LINK_RE = re.compile(r"\]\(([^)]+)\)")
FULL_MARKDOWN_LINK_RE = re.compile(r"\[[^\]]*\]\([^)]+\)")
INLINE_CODE_RE = re.compile(r"`([^`\n]+)`")
FENCED_CODE_RE = re.compile(r"```.*?```", re.DOTALL)
COMPONENT_TITLE_RE = re.compile(r"^# .+开发规范\s*$", re.MULTILINE)
LEGACY_CROSS_SKILL_RE = re.compile(r"(?:\.\./)+(mcu-[a-z0-9-]+)/")
PATH_PREFIXES = (
    "./",
    "../",
    "skills/",
    "references/",
    "templates/",
    "guides/",
    "examples/",
    "scripts/",
    "tools/",
    "docs/",
    "skill://",
    "output://",
)
URL_PREFIXES = ("http://", "https://", "mailto:", "tel:", "data:")
GENERIC_DIR_TARGETS = {
    "references/",
    "templates/",
    "guides/",
    "examples/",
    "scripts/",
    "tools/",
    "skill://",
    "output://",
}
RESOURCE_DIRS = ("references", "templates", "guides", "examples", "scripts")
LAYERS = {"foundation", "domain", "orchestrator", "utility"}
SKIP_DIRS = {".git", ".codebuddy", "__pycache__"}


@lru_cache(maxsize=None)
def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def parse_frontmatter(path: Path, errors: list[str]) -> dict[str, str]:
    text = read_text(path)
    match = FRONTMATTER_RE.match(text)
    if not match:
        errors.append(f"{path}: 缺少 YAML frontmatter")
        return {}

    values: dict[str, str] = {}
    for line in match.group(1).splitlines():
        key, separator, value = line.partition(":")
        if not separator or line.startswith((" ", "\t")):
            if line.strip():
                print(
                    f"[warning] {path}: frontmatter 行未被识别（仅支持顶层 key: value）-> {line}",
                    file=sys.stderr,
                )
            continue
        values[key.strip()] = value.strip().strip("\"'")
    return values


def discover_skills(root: Path, errors: list[str]) -> list[Path]:
    skills_dir = root / "skills"
    if not skills_dir.is_dir():
        errors.append(f"{skills_dir}: 缺少 skills 目录")
        return []

    skills = sorted(
        path for path in skills_dir.iterdir() if path.is_dir() and (path / "SKILL.md").is_file()
    )
    if not skills:
        errors.append(f"{skills_dir}: 未找到任何 Skill")

    expected = {(skill / "SKILL.md").resolve() for skill in skills}
    for skill_md in root.rglob("SKILL.md"):
        if any(part in SKIP_DIRS for part in skill_md.parts):
            continue
        if skill_md.resolve() not in expected:
            errors.append(f"{skill_md}: SKILL.md 必须位于 skills/<name>/")
    return skills


def string_list(value: object) -> list[str]:
    if not isinstance(value, list):
        return []
    return [item for item in value if isinstance(item, str)]


def object_list(value: object) -> list[dict[str, object]]:
    if not isinstance(value, list):
        return []
    return [item for item in value if isinstance(item, dict)]


def load_metadata(
    skills: list[Path],
    errors: list[str],
) -> dict[str, dict[str, object]]:
    metadata: dict[str, dict[str, object]] = {}
    for skill in skills:
        path = skill / "skill.json"
        if not path.is_file():
            errors.append(f"{skill}: 缺少 skill.json")
            continue
        try:
            value = json.loads(read_text(path))
        except (json.JSONDecodeError, OSError) as exc:
            errors.append(f"{path}: 无效 JSON -> {exc}")
            continue
        if not isinstance(value, dict):
            errors.append(f"{path}: 顶层必须是 JSON object")
            continue
        metadata[skill.name] = value
    return metadata


def check_skill_shell(
    root: Path,
    skills: list[Path],
    metadata: dict[str, dict[str, object]],
    errors: list[str],
) -> None:
    names: dict[str, Path] = {}
    readme = read_text(root / "README.md") if (root / "README.md").is_file() else ""

    for skill in skills:
        skill_md = skill / "SKILL.md"
        values = parse_frontmatter(skill_md, errors)
        name = values.get("name", "")
        description = values.get("description", "")

        if not name:
            errors.append(f"{skill_md}: frontmatter 缺少非空 name")
        elif not SKILL_NAME_RE.fullmatch(name):
            errors.append(f"{skill_md}: name 必须使用 kebab-case -> {name}")
        elif name != skill.name:
            errors.append(f"{skill_md}: name '{name}' 与目录名 '{skill.name}' 不一致")

        if not description:
            errors.append(f"{skill_md}: frontmatter 缺少非空 description")

        if name in names:
            errors.append(f"{skill_md}: name '{name}' 与 {names[name]} 重复")
        elif name:
            names[name] = skill_md

        if not (skill / "CHANGELOG.md").is_file():
            errors.append(f"{skill}: 缺少 CHANGELOG.md")

        entry = f"skills/{skill.name}/SKILL.md"
        if entry not in readme:
            errors.append(f"README.md: 缺少 Skill 索引 -> {entry}")

        data = metadata.get(skill.name)
        if data is not None and data.get("name") != name:
            errors.append(f"{skill / 'skill.json'}: name 与 SKILL.md frontmatter 不一致")


def local_route_target(skill: Path, target: str) -> Path | None:
    if target.startswith(("skill://", "output://")):
        return None
    clean = target.split("#", maxsplit=1)[0]
    path = PurePosixPath(clean)
    if path.is_absolute() or ".." in path.parts:
        return None
    return skill / path


def resource_roots(skill: Path) -> set[str]:
    roots: set[str] = set()
    for name in RESOURCE_DIRS:
        base = skill / name
        if not base.is_dir():
            continue
        for child in base.iterdir():
            if child.name == "__pycache__":
                continue
            suffix = "/" if child.is_dir() else ""
            roots.add(f"{name}/{child.name}{suffix}")
    return roots


def route_covers_resource(target: str, resource: str) -> bool:
    if target.startswith(("skill://", "output://")):
        return False
    clean = target.split("#", maxsplit=1)[0]
    return clean == resource or clean.startswith(resource)


def check_metadata(
    root: Path,
    skills: list[Path],
    metadata: dict[str, dict[str, object]],
    errors: list[str],
) -> int:
    route_count = 0
    known = set(metadata)
    term_owners: dict[str, tuple[str, str]] = {}

    for skill in skills:
        data = metadata.get(skill.name)
        if data is None:
            continue
        path = skill / "skill.json"
        if data.get("schema_version") != 1:
            errors.append(f"{path}: schema_version 必须为 1")

        name = data.get("name")
        version = data.get("version")
        title = data.get("title")
        summary = data.get("summary")
        layer = data.get("layer")
        dependencies_value = data.get("dependencies")
        triggers_value = data.get("triggers")
        routes_value = data.get("routes")
        dependencies = string_list(dependencies_value)
        triggers = string_list(triggers_value)
        routes = object_list(routes_value)

        if name != skill.name:
            errors.append(f"{path}: name 必须与目录名一致")
        if not isinstance(version, str) or not SEMVER_RE.fullmatch(version):
            errors.append(f"{path}: version 必须使用 SemVer x.y.z")
        elif (skill / "CHANGELOG.md").is_file():
            changelog = read_text(skill / "CHANGELOG.md")
            if f"## v{version}" not in changelog:
                errors.append(f"{skill / 'CHANGELOG.md'}: 缺少 v{version} 条目")
        if not isinstance(title, str) or not title.strip():
            errors.append(f"{path}: title 必须为非空字符串")
        if not isinstance(summary, str) or not summary.strip():
            errors.append(f"{path}: summary 必须为非空字符串")
        if layer not in LAYERS:
            errors.append(f"{path}: layer 必须是 {sorted(LAYERS)} 之一")
        if not isinstance(dependencies_value, list) or len(dependencies) != len(dependencies_value):
            errors.append(f"{path}: dependencies 必须全部为字符串")
        if (
            not isinstance(triggers_value, list)
            or len(triggers) != len(triggers_value)
            or not triggers
        ):
            errors.append(f"{path}: triggers 必须是非空字符串数组")
        if not isinstance(routes_value, list) or len(routes) != len(routes_value) or not routes:
            errors.append(f"{path}: routes 必须是非空对象数组")

        for dependency in dependencies:
            if dependency == skill.name:
                errors.append(f"{path}: 不得依赖自身")
            elif dependency not in known:
                errors.append(f"{path}: 未知依赖 -> {dependency}")

        if layer in {"foundation", "utility"} and dependencies:
            errors.append(f"{path}: {layer} 层不得声明依赖")
        for dependency in dependencies:
            dependency_layer = metadata.get(dependency, {}).get("layer")
            if layer == "domain" and dependency_layer != "foundation":
                errors.append(f"{path}: domain 层只能依赖 foundation -> {dependency}")
            if layer == "orchestrator" and dependency_layer not in {"foundation", "domain"}:
                errors.append(f"{path}: orchestrator 只能依赖 foundation/domain -> {dependency}")

        for term in triggers:
            normalized = term.casefold().strip()
            owner = term_owners.get(normalized)
            if owner and owner[0] != skill.name:
                errors.append(f"{path}: 触发词 '{term}' 与 {owner[0]} 的 {owner[1]} 重复")
            else:
                term_owners[normalized] = (skill.name, "trigger")

        route_ids: set[str] = set()
        route_targets: list[str] = []
        skill_text = read_text(skill / "SKILL.md")
        for route in routes:
            route_count += 1
            route_id = route.get("id")
            intent = route.get("intent")
            keywords_value = route.get("keywords")
            keywords = string_list(keywords_value)
            target = route.get("target")

            if not isinstance(route_id, str) or not SKILL_NAME_RE.fullmatch(route_id):
                errors.append(f"{path}: route id 必须使用 kebab-case -> {route_id}")
                continue
            if route_id in route_ids:
                errors.append(f"{path}: route id 重复 -> {route_id}")
            route_ids.add(route_id)
            if not isinstance(intent, str) or not intent.strip():
                errors.append(f"{path}: route '{route_id}' 缺少 intent")
            if (
                not isinstance(keywords_value, list)
                or len(keywords) != len(keywords_value)
                or not keywords
            ):
                errors.append(f"{path}: route '{route_id}' keywords 必须为非空字符串数组")
            if not isinstance(target, str) or not target:
                errors.append(f"{path}: route '{route_id}' 缺少 target")
                continue

            route_targets.append(target)
            if target not in skill_text:
                errors.append(f"{path}: route '{route_id}' target 未出现在 SKILL.md -> {target}")

            for keyword in keywords:
                normalized = keyword.casefold().strip()
                owner = term_owners.get(normalized)
                if owner and owner[0] != skill.name:
                    errors.append(f"{path}: 路由词 '{keyword}' 与 {owner[0]} 的 {owner[1]} 重复")
                else:
                    term_owners[normalized] = (skill.name, f"route:{route_id}")

            if target.startswith("skill://"):
                try:
                    target_skill, _ = parse_skill_uri(target)
                    resolved = resolve_skill_uri(root, target)
                except ValueError as exc:
                    errors.append(f"{path}: route '{route_id}' -> {exc}")
                    continue
                if not resolved.exists():
                    errors.append(f"{path}: route '{route_id}' 资源不存在 -> {target}")
                if target_skill != skill.name and target_skill not in dependencies:
                    errors.append(f"{path}: route '{route_id}' 使用未声明依赖 -> {target_skill}")
                continue

            resolved = local_route_target(skill, target)
            if resolved is None or not resolved.exists():
                errors.append(f"{path}: route '{route_id}' 资源不存在 -> {target}")

        for resource in sorted(resource_roots(skill)):
            if not any(route_covers_resource(target, resource) for target in route_targets):
                errors.append(f"{path}: 孤儿资源未被 route 覆盖 -> {resource}")
    return route_count


def check_dependency_cycles(
    metadata: dict[str, dict[str, object]],
    errors: list[str],
) -> None:
    state: dict[str, int] = {}
    stack: list[str] = []

    def visit(name: str) -> None:
        state[name] = 1
        stack.append(name)
        for dependency in string_list(metadata[name].get("dependencies")):
            if dependency not in metadata:
                continue
            if state.get(dependency) == 1:
                start = stack.index(dependency)
                errors.append(f"Skill 依赖存在循环: {' -> '.join(stack[start:] + [dependency])}")
            elif state.get(dependency, 0) == 0:
                visit(dependency)
        stack.pop()
        state[name] = 2

    for name in sorted(metadata):
        if state.get(name, 0) == 0:
            visit(name)


def normalize_target(raw_target: str) -> str:
    target = raw_target.strip()
    if target.startswith("<") and target.endswith(">"):
        target = target[1:-1]
    if " " in target:
        target = target.split(maxsplit=1)[0]
    return unquote(target.split("#", maxsplit=1)[0])


def iter_path_targets(text: str) -> set[str]:
    targets = {normalize_target(match.group(1)) for match in MARKDOWN_LINK_RE.finditer(text)}
    inline_text = FULL_MARKDOWN_LINK_RE.sub("", FENCED_CODE_RE.sub("", text))
    for match in INLINE_CODE_RE.finditer(inline_text):
        target = normalize_target(match.group(1))
        if target.startswith(PATH_PREFIXES):
            targets.add(target)
    return targets


def source_skill_name(path: Path, root: Path) -> str | None:
    parts = path.relative_to(root).parts
    if len(parts) >= 3 and parts[0] == "skills":
        return parts[1]
    return None


def valid_output_uri(target: str) -> bool:
    value = target.removeprefix("output://")
    path = PurePosixPath(value)
    return bool(value) and not path.is_absolute() and ".." not in path.parts


def check_paths(
    path: Path,
    root: Path,
    metadata: dict[str, dict[str, object]],
    errors: list[str],
) -> tuple[int, int]:
    checked = 0
    skill_uris = 0
    text = read_text(path)
    source_skill = source_skill_name(path, root)

    if source_skill and LEGACY_CROSS_SKILL_RE.search(text):
        errors.append(f"{path}: 跨 Skill 引用必须使用 skill:// URI")

    for target in sorted(iter_path_targets(text)):
        if not target or target.startswith("#") or target.startswith(URL_PREFIXES):
            continue
        if any(token in target for token in ("<", ">", "*", "{", "}")):
            continue
        if target in GENERIC_DIR_TARGETS:
            continue
        checked += 1

        if target.startswith("output://"):
            if not valid_output_uri(target):
                errors.append(f"{path}: 无效 output:// URI -> {target}")
            continue

        if target.startswith("skill://"):
            skill_uris += 1
            try:
                target_skill, _ = parse_skill_uri(target)
                resolved = resolve_skill_uri(root, target)
            except ValueError as exc:
                errors.append(f"{path}: {exc}")
                continue
            if not resolved.exists():
                errors.append(f"{path}: 失效 Skill URI -> {target}")
            if source_skill and target_skill != source_skill:
                dependencies = string_list(metadata.get(source_skill, {}).get("dependencies"))
                if target_skill not in dependencies:
                    errors.append(f"{path}: 使用未声明依赖 -> {target_skill}")
            continue

        candidates = [path.parent / target]
        relative_parts = path.relative_to(root).parts
        if len(relative_parts) >= 3 and relative_parts[0] == "skills":
            candidates.append(root / "skills" / relative_parts[1] / target)
        if target.startswith(("skills/", "tools/", "docs/", "evals/")):
            candidates.append(root / target)
        if not any(candidate.exists() for candidate in candidates):
            errors.append(f"{path}: 失效引用 -> {target}")
    return checked, skill_uris


def check_component_reference(path: Path, errors: list[str]) -> None:
    text = read_text(path)
    if not COMPONENT_TITLE_RE.search(text):
        return
    for number in range(1, 6):
        if not re.search(rf"^## {number}(?:[.、 ]|$)", text, re.MULTILINE):
            errors.append(f"{path}: 缺少五段式章节 '## {number}'")


def markdown_files(root: Path) -> list[Path]:
    files = []
    for path in root.rglob("*.md"):
        if any(part in SKIP_DIRS for part in path.parts):
            continue
        files.append(path)
    return sorted(files)


def validate(root: Path) -> tuple[list[str], dict[str, int]]:
    root = root.resolve()
    read_text.cache_clear()
    errors: list[str] = []
    skills = discover_skills(root, errors)
    metadata = load_metadata(skills, errors)

    if not (root / "README.md").is_file():
        errors.append(f"{root}: 缺少 README.md")
    else:
        check_skill_shell(root, skills, metadata, errors)

    route_count = check_metadata(root, skills, metadata, errors)
    check_dependency_cycles(metadata, errors)

    markdown = markdown_files(root)
    checked_paths = 0
    skill_uris = 0
    component_references = 0
    for path in markdown:
        path_count, uri_count = check_paths(path, root, metadata, errors)
        checked_paths += path_count
        skill_uris += uri_count
        if COMPONENT_TITLE_RE.search(read_text(path)):
            component_references += 1
            check_component_reference(path, errors)

    registry: dict[str, object] | None = None
    if len(metadata) == len(skills):
        try:
            registry = build_registry(root)
            stale_outputs = check_outputs(root, registry)
        except (KeyError, TypeError, ValueError) as exc:
            errors.append(f"生成 registry 失败 -> {exc}")
        else:
            for path in stale_outputs:
                errors.append(
                    f"{path}: 自动生成文件已过期，运行 python3 tools/skill_registry.py --write"
                )

    routing_cases = 0
    cases_path = root / "evals" / "routing-cases.json"
    if not cases_path.is_file():
        errors.append(f"{cases_path}: 缺少路由评测数据集")
    elif len(metadata) == len(skills):
        try:
            routing_report = evaluate(root, registry)
        except (KeyError, TypeError, ValueError, json.JSONDecodeError) as exc:
            errors.append(f"{cases_path}: 路由评测失败 -> {exc}")
        else:
            summary = routing_report.get("summary")
            if isinstance(summary, dict):
                routing_cases = int(summary.get("cases", 0))
            if not thresholds_pass(routing_report):
                errors.append(f"{cases_path}: 路由评测未达到质量阈值")
            report_file = report_path(root)
            if not report_file.is_file() or read_text(report_file) != report_text(routing_report):
                errors.append(
                    f"{report_file}: "
                    "评测报告已过期，运行 python3 tools/evaluate_routing.py --write-report"
                )

    stats = {
        "skills": len(skills),
        "routes": route_count,
        "markdown": len(markdown),
        "component_references": component_references,
        "paths": checked_paths,
        "skill_uris": skill_uris,
        "routing_cases": routing_cases,
    }
    return errors, stats


def main() -> int:
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent
    errors, stats = validate(root)
    if errors:
        print(f"校验失败，共 {len(errors)} 个问题:")
        for error in errors:
            print(f"  - {error}")
        return 1

    print(
        "校验通过: "
        f"{stats['skills']} 个 Skill, "
        f"{stats['routes']} 条路由, "
        f"{stats['component_references']} 篇器件规范, "
        f"{stats['markdown']} 个 Markdown, "
        f"{stats['paths']} 个路径引用, "
        f"{stats['skill_uris']} 个 Skill URI, "
        f"{stats['routing_cases']} 个路由评测"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
