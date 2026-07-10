#!/usr/bin/env python3
"""Skills 知识库校验脚本。

用法: python3 tools/validate.py [仓库根目录]
检查项:
  1. 每个 skill 的 SKILL.md 存在且含 name/description frontmatter
  2. mcu-components references 文档遵循五段式结构
  3. 全部 Markdown 内部链接/路径引用有效
  4. SKILL.md 目录树中列出的文件真实存在
退出码: 0 通过, 1 存在错误。
"""
import os
import re
import sys

errors = []


def err(msg: str) -> None:
    errors.append(msg)


def check_frontmatter(skill_md: str) -> None:
    text = open(skill_md, encoding="utf-8").read()
    m = re.match(r"---\n(.*?)\n---\n", text, re.S)
    if not m:
        err(f"{skill_md}: 缺少 YAML frontmatter")
        return
    for key in ("name:", "description:"):
        if key not in m.group(1):
            err(f"{skill_md}: frontmatter 缺少 {key}")


SECTION_PAT = [r"## 1", r"## 2", r"## 3", r"## 4", r"## 5"]


def check_reference_structure(path: str) -> None:
    text = open(path, encoding="utf-8").read()
    for pat in SECTION_PAT:
        if not re.search(rf"^{pat}[\. ]", text, re.M):
            err(f"{path}: 缺少五段式章节 '{pat}'")


LINK_RE = re.compile(r"\]\(([^)#\s]+)(?:#[^)]*)?\)|(?:^|\s)`((?:\.\./)*(?:references|templates|guides|examples|scripts|tools)/[^`]+)`", re.M)


def check_links(path: str, root: str, skill_dirs: list) -> None:
    text = open(path, encoding="utf-8").read()
    base = os.path.dirname(path)
    for m in LINK_RE.finditer(text):
        target = m.group(1) or m.group(2)
        if not target or target.startswith(("http://", "https://", "mailto:")):
            continue
        if "<" in target:  # 占位符路径, 跳过
            continue
        bases = [base, root] + [os.path.join(root, s) for s in skill_dirs]
        if not any(os.path.exists(os.path.normpath(os.path.join(b, target))) for b in bases):
            err(f"{path}: 失效引用 -> {target}")


def check_tree_consistency(skill_md: str) -> None:
    """SKILL.md 目录树中列出的 .md/.c 文件必须真实存在。"""
    text = open(skill_md, encoding="utf-8").read()
    base = os.path.dirname(skill_md)
    tree = re.search(r"```\n" + re.escape(os.path.basename(base)) + r"/\n(.*?)```", text, re.S)
    if not tree:
        return
    stack = {}
    for line in tree.group(1).split("\n"):
        m = re.match(r"([│ ]*)(?:├──|└──) ([\w.\-]+/?)", line)
        if not m:
            continue
        depth = len(m.group(1)) // 4
        name = m.group(2)
        stack[depth] = name.rstrip("/")
        if name.endswith("/"):
            continue
        parts = [stack[d] for d in range(depth)] + [name]
        rel = os.path.join(*parts)
        if not os.path.exists(os.path.join(base, rel)):
            err(f"{skill_md}: 目录树中列出但不存在 -> {rel}")


def main() -> int:
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    skills = [d for d in sorted(os.listdir(root))
              if os.path.isfile(os.path.join(root, d, "SKILL.md"))]
    if not skills:
        err(f"{root}: 未找到任何 skill")
    for s in skills:
        skill_md = os.path.join(root, s, "SKILL.md")
        check_frontmatter(skill_md)
        check_tree_consistency(skill_md)
    refs_dir = os.path.join(root, "mcu-components", "references")
    if os.path.isdir(refs_dir):
        for dirpath, _, files in os.walk(refs_dir):
            for f in sorted(files):
                if f.endswith(".md"):
                    check_reference_structure(os.path.join(dirpath, f))
    for dirpath, dirnames, files in os.walk(root):
        dirnames[:] = [d for d in dirnames if not d.startswith(".")]
        for f in sorted(files):
            if f.endswith(".md"):
                check_links(os.path.join(dirpath, f), root, skills)

    if errors:
        print(f"校验失败，共 {len(errors)} 个问题:")
        for e in errors:
            print(f"  - {e}")
        return 1
    print(f"校验通过: {len(skills)} 个 skill")
    return 0


if __name__ == "__main__":
    sys.exit(main())
