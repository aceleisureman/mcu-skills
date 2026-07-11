#!/usr/bin/env python3
"""一键升级 Skill 版本号，自动同步 CHANGELOG 和 registry，可选发布 GitHub Release。

用法:
  python3 tools/bump_version.py mcu-sensors          # patch: 2.1.0 → 2.1.1
  python3 tools/bump_version.py mcu-sensors --minor  # minor: 2.1.0 → 2.2.0
  python3 tools/bump_version.py mcu-sensors --major  # major: 2.1.0 → 3.0.0
  python3 tools/bump_version.py mcu-sensors --set 2.2.0  # 指定版本
  python3 tools/bump_version.py --all                 # 全部 Skill patch+1
  python3 tools/bump_version.py --all --minor         # 全部 Skill minor+1
  python3 tools/bump_version.py mcu-sensors -m "新增 GP2Y1014AU"  # 带变更说明

发布:
  python3 tools/bump_version.py mcu-sensors --minor --release  # 升级 + git commit + tag + push
  python3 tools/bump_version.py --all --minor --release         # 全部升级 + 发布
  python3 tools/bump_version.py mcu-sensors --release --dry-run # 预览发布操作

执行后自动:
  1. 更新 skill.json 中的 version
  2. 在 CHANGELOG.md 顶部插入新版本条目
  3. 运行 skill_registry.py --write 同步 registry.json 和生成文档
  4. 运行 validate.py 验证
  5. (--release) git commit + 创建 tag + push（触发 GitHub Actions 自动发布 Release）
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

    parts = re.split(r"(^## )", old_text, maxsplit=1, flags=re.MULTILINE)
    if len(parts) == 3:
        new_text = parts[0] + "\n".join(lines) + parts[1] + parts[2]
    else:
        new_text = old_text.rstrip() + "\n\n" + "\n".join(lines)

    path.write_text(new_text, encoding="utf-8")
    print(f"  {skill}/CHANGELOG.md: 新增 v{new_version} 条目")


def run_cmd(cmd: list[str], cwd: Path = ROOT, check: bool = True) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, capture_output=True, text=True, cwd=str(cwd), check=check)


def run_registry_write() -> None:
    print("  同步 registry.json 和生成文档...")
    result = run_cmd([sys.executable, str(ROOT / "tools" / "skill_registry.py"), "--write"])
    for line in result.stdout.strip().splitlines():
        print(f"    {line}")


def run_validate() -> bool:
    print("  验证...")
    result = run_cmd(
        [sys.executable, str(ROOT / "tools" / "validate.py")], check=False
    )
    if result.returncode != 0:
        print(f"  [错误] 验证失败:\n{result.stdout}", file=sys.stderr)
        return False
    print(f"    {result.stdout.strip()}")
    return True


def git_release(skill: str, new_version: str, messages: list[str], dry_run: bool) -> bool:
    """执行 git commit + tag + push，触发 GitHub Actions 自动创建 Release。"""
    tag = f"{skill}-v{new_version}"
    print(f"\n{'='*50}")
    print(f"发布 {tag}")
    print(f"{'='*50}")

    # 检查工作区是否有未提交的改动（应该有刚改的文件）
    status = run_cmd(["git", "status", "--porcelain"])
    if not status.stdout.strip():
        print("  [警告] 工作区无变更，跳过发布")
        return False

    # commit
    commit_msg = f"release: {skill} v{new_version}"
    if messages:
        commit_msg += "\n\n" + "\n".join(f"- {m}" for m in messages)

    if dry_run:
        print(f"  [dry-run] git add -A")
        print(f"  [dry-run] git commit -m \"{commit_msg}\"")
        print(f"  [dry-run] git tag {tag}")
        print(f"  [dry-run] git push origin main --tags")
        return True

    print("  git add -A ...")
    run_cmd(["git", "add", "-A"])

    print(f"  git commit ...")
    run_cmd(["git", "commit", "-m", commit_msg])

    print(f"  git tag {tag} ...")
    # 如果 tag 已存在先删除
    run_cmd(["git", "tag", "-d", tag], check=False)
    run_cmd(["git", "tag", tag])

    print(f"  git push ...")
    push_result = run_cmd(
        ["git", "push", "origin", "main", "--tags"], check=False
    )
    if push_result.returncode != 0:
        print(f"  [错误] push 失败:\n{push_result.stderr}", file=sys.stderr)
        return False

    print(f"\n  已推送 tag {tag}")
    print(f"  GitHub Actions 将自动创建 Release")
    return True


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
    parser.add_argument("--release", action="store_true", help="升级后自动 git commit + tag + push，触发 GitHub Release")
    parser.add_argument("--dry-run", action="store_true", help="只预览不执行发布操作")

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

    bumped: list[tuple[str, str]] = []  # (skill, new_version)

    for skill in targets:
        path = SKILLS_DIR / skill / "skill.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        old_version = data["version"]

        if args.set:
            new_version = args.set
            parse_version(new_version)
        else:
            new_version = bump(old_version, kind)

        print(f"\n{'='*50}")
        print(f"升级 {skill}: {old_version} → {new_version}")
        print(f"{'='*50}")

        update_skill_json(skill, new_version)
        update_changelog(skill, new_version, args.message)
        bumped.append((skill, new_version))

    print(f"\n{'='*50}")
    run_registry_write()

    if not run_validate():
        print("\n验证未通过，请修复后再发布", file=sys.stderr)
        return 1

    if args.release:
        if args.all and len(bumped) > 1:
            # 多 Skill 一起发布，用一个汇总 tag
            tag = f"v{date.today().isoformat()}"
            print(f"\n{'='*50}")
            print(f"发布 {tag}（{len(bumped)} 个 Skill）")
            print(f"{'='*50}")

            commit_parts = [f"release: {len(bumped)} skills bump"]
            for skill, ver in bumped:
                commit_parts.append(f"  {skill} → v{ver}")
            if args.message:
                commit_parts.append("")
                commit_parts.extend(f"- {m}" for m in args.message)
            commit_msg = "\n".join(commit_parts)

            # 生成 Release notes
            release_notes = "\n".join(
                f"### {skill} v{ver}\n" + "\n".join(f"- {m}" for m in args.message)
                if args.message else f"### {skill} v{ver}\n- 版本更新"
                for skill, ver in bumped
            )

            if args.dry_run:
                print(f"  [dry-run] git add -A")
                print(f"  [dry-run] git commit -m \"{commit_msg}\"")
                print(f"  [dry-run] git tag {tag}")
                print(f"  [dry-run] git push origin main --tags")
                return 0

            run_cmd(["git", "add", "-A"])
            run_cmd(["git", "commit", "-m", commit_msg])
            run_cmd(["git", "tag", "-d", tag], check=False)
            run_cmd(["git", "tag", tag])
            push = run_cmd(["git", "push", "origin", "main", "--tags"], check=False)
            if push.returncode != 0:
                print(f"  [错误] push 失败:\n{push.stderr}", file=sys.stderr)
                return 1
            print(f"\n  已推送 tag {tag}")
        else:
            # 单 Skill 发布
            for skill, new_version in bumped:
                if not git_release(skill, new_version, args.message, args.dry_run):
                    return 1

        print(f"\n  GitHub Actions 将自动创建 Release")
        print(f"  查看进度: GitHub repo → Actions")
    else:
        print(f"\n完成！共升级 {len(bumped)} 个 Skill。")
        print(f"下一步:")
        print(f"  python3 tools/validate.py          # 验证")
        print(f"  python3 tools/bump_version.py {targets[0]} --release  # 发布")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
