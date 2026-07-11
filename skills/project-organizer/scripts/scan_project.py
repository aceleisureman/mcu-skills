#!/usr/bin/env python3
"""项目整理扫描脚本（只读）。

用法: python3 scan_project.py <项目路径> [--json]
按 SKILL.md 的 /organize-scan 维度输出问题清单（含失效 Markdown 链接检查），不做任何修改。
--json: 以 JSON 格式输出，便于工具链消费。
"""
import hashlib
import json
import os
import re
import sys
from collections import defaultdict
from urllib.parse import unquote

JUNK_FILES = {".DS_Store", "Thumbs.db", "desktop.ini"}
JUNK_SUFFIXES = (".tmp", ".bak", ".swp", ".swo", ".orig", ".rej", ".pyc", "~")
BUILD_DIRS = {"node_modules", "dist", "build", "__pycache__", ".cache"}
SKIP_DIRS = {".git", ".svn", ".hg", ".codebuddy"}
BIG_FILE_MB = 10
BAD_NAME = re.compile(r"[ ()（）]|副本|新建|final", re.IGNORECASE)
VERSION_COPY = re.compile(r"(?:^|[-_])v\d+(?=[-_.]|$)", re.IGNORECASE)
VERSIONED_SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp", ".py"}
VERSIONED_DOC_NAMES = re.compile(r"^migration-v\d+\.md$", re.IGNORECASE)
# 以下链接/路径检查正则与 tools/validate.py 中的同名逻辑保持同步
#（本 Skill 需独立分发，故有意复制而非导入）。
MD_LINK = re.compile(r"\]\(([^)]+)\)")
FULL_MD_LINK = re.compile(r"\[[^\]]*\]\([^)]+\)")
INLINE_CODE = re.compile(r"`([^`\n]+)`")
FENCED_CODE = re.compile(r"```.*?```", re.DOTALL)
PATH_PREFIXES = (
    "./", "../", "skills/", "references/", "templates/",
    "guides/", "examples/", "scripts/", "tools/", "docs/",
)
URL_PREFIXES = ("http://", "https://", "mailto:", "tel:", "data:")
GENERIC_DIR_TARGETS = {
    "references/", "templates/", "guides/", "examples/", "scripts/", "tools/",
}


def human(size: int) -> str:
    return f"{size / 1024 / 1024:.1f}MB" if size >= 1024 * 1024 else f"{size / 1024:.1f}KB"


def md5(path: str, limit: int = 4 * 1024 * 1024) -> str:
    h = hashlib.md5()
    with open(path, "rb") as f:
        h.update(f.read(limit))
    return h.hexdigest()


def broken_md_links(path: str, root: str) -> list:
    out = []
    try:
        text = open(path, encoding="utf-8").read()
    except (UnicodeDecodeError, OSError):
        return out
    base = os.path.dirname(path)
    targets = {
        (m.group(1).strip().split(maxsplit=1)[0], False)
        for m in MD_LINK.finditer(text)
    }
    inline_text = FULL_MD_LINK.sub("", FENCED_CODE.sub("", text))
    targets.update(
        (m.group(1).strip(), True) for m in INLINE_CODE.finditer(inline_text)
        if m.group(1).strip().startswith(PATH_PREFIXES)
    )
    for raw_target, is_inline in targets:
        target = unquote(raw_target.strip("<>").split("#", maxsplit=1)[0])
        if not target or target.startswith("#") or target.startswith(URL_PREFIXES):
            continue
        if is_inline and (target.startswith("docs/") or target in GENERIC_DIR_TARGETS):
            continue
        if any(
            token in target for token in ("<", ">", "*", "{", "}")
        ):
            continue
        candidates = [os.path.normpath(os.path.join(base, target))]
        rel_parts = os.path.relpath(path, root).split(os.sep)
        if len(rel_parts) >= 3 and rel_parts[0] == "skills":
            candidates.append(os.path.normpath(
                os.path.join(root, "skills", rel_parts[1], target)
            ))
        if target.startswith(("skills/", "tools/", "docs/")):
            candidates.append(os.path.normpath(os.path.join(root, target)))
        if not any(os.path.exists(candidate) for candidate in candidates):
            out.append(f"{os.path.relpath(path, root)} -> {target}")
    return out


def scan(root: str, as_json: bool = False) -> None:
    junk, build, big, badname, empty_files, empty_dirs = [], [], [], [], [], []
    broken_links = []
    by_hash = defaultdict(list)

    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
        rel_dir = os.path.relpath(dirpath, root)
        for d in list(dirnames):
            if d in BUILD_DIRS:
                build.append(os.path.join(rel_dir, d))
                dirnames.remove(d)
        if not dirnames and not filenames and rel_dir != ".":
            empty_dirs.append(rel_dir)
        for name in filenames:
            path = os.path.join(dirpath, name)
            rel = os.path.relpath(path, root)
            size = os.path.getsize(path)
            if name in JUNK_FILES or name.endswith(JUNK_SUFFIXES):
                junk.append(rel)
                continue
            if size == 0:
                empty_files.append(rel)
            if size > BIG_FILE_MB * 1024 * 1024:
                big.append(f"{rel} ({human(size)})")
            suffix = os.path.splitext(name)[1].lower()
            if BAD_NAME.search(name) or (
                VERSION_COPY.search(name) and suffix not in VERSIONED_SOURCE_SUFFIXES
                and not VERSIONED_DOC_NAMES.fullmatch(name)
            ):
                badname.append(rel)
            if 0 < size < 4 * 1024 * 1024:
                by_hash[(size, md5(path))].append(rel)
            if name.endswith(".md"):
                broken_links.extend(broken_md_links(path, root))

    dupes = [v for v in by_hash.values() if len(v) > 1]

    sections = [
        ("垃圾文件（可安全删除）", junk),
        ("构建产物目录（应加入 .gitignore）", build),
        ("空文件", empty_files),
        ("空目录", empty_dirs),
        (f"大文件（> {BIG_FILE_MB}MB）", big),
        ("命名不规范（空格/副本/final/非源码版本后缀等）", badname),
        ("内容重复文件组", ["  <==>  ".join(g) for g in dupes]),
        ("失效 Markdown 链接", broken_links),
    ]
    if as_json:
        print(json.dumps({title: sorted(items) for title, items in sections},
                         ensure_ascii=False, indent=2))
        return
    total = 0
    for title, items in sections:
        print(f"\n## {title}: {len(items)}")
        for it in sorted(items):
            print(f"  - {it}")
        total += len(items)
    print(f"\n共发现 {total} 项待整理问题。")


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a != "--json"]
    if len(args) != 1 or not os.path.isdir(args[0]):
        print(__doc__)
        sys.exit(1)
    scan(args[0], as_json="--json" in sys.argv)
