#!/usr/bin/env python3
"""项目整理扫描脚本（只读）。

用法: python3 scan_project.py <项目路径>
按 SKILL.md 的 /organize-scan 维度输出问题清单，不做任何修改。
"""
import os
import re
import sys
import hashlib
from collections import defaultdict

JUNK_FILES = {".DS_Store", "Thumbs.db", "desktop.ini"}
JUNK_SUFFIXES = (".tmp", ".bak", ".swp", ".swo", ".orig", ".rej", ".pyc", "~")
BUILD_DIRS = {"node_modules", "dist", "build", "__pycache__", ".cache"}
SKIP_DIRS = {".git", ".svn", ".hg"}
BIG_FILE_MB = 10
BAD_NAME = re.compile(r"[ ()（）]|副本|新建|final|_v\d+", re.IGNORECASE)


def human(size: int) -> str:
    return f"{size / 1024 / 1024:.1f}MB" if size >= 1024 * 1024 else f"{size / 1024:.1f}KB"


def md5(path: str, limit: int = 4 * 1024 * 1024) -> str:
    h = hashlib.md5()
    with open(path, "rb") as f:
        h.update(f.read(limit))
    return h.hexdigest()


def scan(root: str) -> None:
    junk, build, big, badname, empty_files, empty_dirs = [], [], [], [], [], []
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
            if BAD_NAME.search(name):
                badname.append(rel)
            if 0 < size < 4 * 1024 * 1024:
                by_hash[(size, md5(path))].append(rel)

    dupes = [v for v in by_hash.values() if len(v) > 1]

    sections = [
        ("垃圾文件（可安全删除）", junk),
        ("构建产物目录（应加入 .gitignore）", build),
        ("空文件", empty_files),
        ("空目录", empty_dirs),
        (f"大文件（> {BIG_FILE_MB}MB）", big),
        ("命名不规范（空格/副本/版本后缀等）", badname),
        ("内容重复文件组", ["  <==>  ".join(g) for g in dupes]),
    ]
    total = 0
    for title, items in sections:
        print(f"\n## {title}: {len(items)}")
        for it in sorted(items):
            print(f"  - {it}")
        total += len(items)
    print(f"\n共发现 {total} 项待整理问题。")


if __name__ == "__main__":
    if len(sys.argv) != 2 or not os.path.isdir(sys.argv[1]):
        print(__doc__)
        sys.exit(1)
    scan(sys.argv[1])
