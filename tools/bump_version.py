#!/usr/bin/env python3
"""一键升级 Skill 版本号，自动同步 CHANGELOG 和 registry。

用法:
  python3 tools/bump_version.py mcu-sensors          # patch: 2.1.0 → 2.1.1
  python3 tools/bump_version.py mcu-sensors --minor  # minor: 2.1.0 → 2.2.0
  python3 tools/bump_version.py mcu-sensors --major  # major: 2.1.0 → 3.0.0
  python3 tools/bump_version.py mcu-sensors --set 2.2.0  # 指定版本
  python3 tools/bump_version.py --all                 # 全部 Skill patch+1
  python3 tools/bump_version.py --all --minor         # 全部 Skill minor+1
  python3 tools/bump_version.py mcu-sensors -m "新增 GP2Y1014AU"  # 带变更说明

执行后自动:
  1. 更新 skill.json 中的 version
  2. 在 CHANGELOG.md 顶部插入新版本条目
  3. 运行 skill_registry.py --write 同步 registry.json 和生成文档
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from datetime import date
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SKILLS_DIR = ROOT / "skills"
SEMVER_RE = re.compile(r"(\d+)\.(\d+)\.(\d+)")


def parse_version(v: str) -> tuple[int, int, int]:
    m = SEMVER_RE.fullmatch(v)
    if not m:
        raise ValueError(f"无效版本号: {v}")
    return int(m[1]), int(m[2]), int(m[3])


def bump(ver: str, kind: str) -> str:
    major, minor, patch = parse_version(ver)
    if kind == "major":
        return f"{major + 1}.0.0"
    if kind == "minor":
        return f"{major}.{minor + 1}.0"
    return f"{major}.{minor}.{patch + 1}"


def list_skills() -> list[str]:
    return sorted(
        p.name for p in SKILLS_DIR.iterdir()
        if p.is_dir() and (p / "skill.json").is_file()
    )


def update_skill_json(skill: str, new_version: str) -> None:
    path = SKILLS_DIR / skill / "skill.json"
    data = json.loads(path.read_text(encoding="utf-8"))
    old = data.get("version", "?")
    data["version"] = new_version
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"  {skill}/skill.json: {old} → {new_version}")


def update_changelog(skill: str, new_version: str, messages: list[str]) -> None:
    path = SKILLS_DIR / skill / "CHANGELOG.md"
    today = date.today().isoformat()
    old_text = path.read_text(encoding="utf-8")

    # 检查是否已存在该版本
    if f"## v{new_version}" in old_text:
        print(f"  {skill}/CHANGELOG.md: v{new_version} 已存在，跳过")
        return

    lines = [f"## v{new_version} - {today}", ""]
    if messages:
        for msg in messages:
            lines.append(f"- {msg}")
    else:
        lines.append("- 版本更新")
    lines.append("")

    # 在第一个 ## 之前插入
    parts = re.split(r"(^## )", old_text, maxsplit=1, flags=re.MULTILINE)
    if len(parts) == 3:
        new_text = parts[0] + "\n".join(lines) + parts[1] + parts[2]
    else:
        new_text = old_text.rstrip() + "\n\n" + "\n".join(lines)

    path.write_text(new_text, encoding="utf-8")
    print(f"  {skill}/CHANGELOG.md: 新增 v{new_version} 条目")


def run_registry_write() -> None:
    print("  同步 registry.json 和生成文档...")
    result = subprocess.run(
        [sys.executable, str(ROOT / "tools" / "skill_registry.py"), "--write"],
        capture_output=True, text=True, cwd=str(ROOT),
    )
    if result.returncode != 0:
        print(f"  [警告] skill_registry.py 失败: {result.stderr}", file=sys.stderr)
    else:
        for line in result.stdout.strip().splitlines():
            print(f"    {line}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="一键升级 Skill 版本号",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument("skill", nargs="?", help="目标 Skill 名称")
    target.add_argument("--all", action="store_true", help="升级所有 Skill")

    level = parser.add_mutually_exclusive_group()
    level.add_argument("--minor", action="store_true", help="minor 升级 (x.Y.0)")
    level.add_argument("--major", action="store_true", help="major 升级 (X.0.0)")
    level.add_argument("--set", metavar="VER", help="指定版本号")

    parser.add_argument("-m", "--message", action="append", default=[], help="变更说明（可多次）")

    args = parser.parse_args()
    kind = "minor" if args.minor else "major" if args.major else "patch"

    if args.all:
        targets = list_skills()
    else:
        if not (SKILLS_DIR / args.skill / "skill.json").is_file():
            print(f"错误: 未找到 Skill '{args.skill}'", file=sys.stderr)
            print(f"可用: {', '.join(list_skills())}", file=sys.stderr)
            return 1
        targets = [args.skill]

    for skill in targets:
        path = SKILLS_DIR / skill / "skill.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        old_version = data["version"]

        if args.set:
            new_version = args.set
            parse_version(new_version)  # 校验格式
        else:
            new_version = bump(old_version, kind)

        print(f"\n{'='*50}")
        print(f"升级 {skill}: {old_version} → {new_version}")
        print(f"{'='*50}")

        update_skill_json(skill, new_version)
        update_changelog(skill, new_version, args.message)

    print(f"\n{'='*50}")
    run_registry_write()
    print(f"\n完成！共升级 {len(targets)} 个 Skill。")
    print(f"下一步: python3 tools/validate.py  验证")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
