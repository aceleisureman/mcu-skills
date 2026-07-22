#!/usr/bin/env python3
"""将 Skill 打成可被平台 ZIP 导入的压缩包。

GitHub 源码 ZIP 的路径是 mcu-skills-*/skills/<name>/SKILL.md，导入器通常只在
ZIP 根或一层子目录查找 SKILL.md，因此整仓下载会失败。

本工具输出：
  1. 单 Skill 包：dist/<name>-v<ver>.zip，SKILL.md 位于 ZIP 根目录
  2. 全量包：dist/mcu-skills-bundle.zip，结构为 <name>/SKILL.md（一层子目录）

用法:
  python3 tools/package_skills.py                 # 全部 Skill + 全量包
  python3 tools/package_skills.py mcu-sensors     # 单个 Skill
  python3 tools/package_skills.py --out dist      # 指定输出目录
  python3 tools/package_skills.py --list          # 仅列出将生成的文件
"""

from __future__ import annotations

import argparse
import json
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SKILLS_DIR = ROOT / "skills"
DEFAULT_OUT = ROOT / "dist"

SKIP_DIR_NAMES = {".git", "__pycache__", ".pytest_cache", "node_modules"}
SKIP_FILE_SUFFIXES = {".pyc", ".pyo"}


def list_skills() -> list[str]:
    return sorted(p.name for p in SKILLS_DIR.iterdir() if p.is_dir() and (p / "SKILL.md").is_file())


def skill_version(name: str) -> str:
    meta = SKILLS_DIR / name / "skill.json"
    if meta.is_file():
        data = json.loads(meta.read_text(encoding="utf-8"))
        version = data.get("version")
        if isinstance(version, str) and version:
            return version
    return "0.0.0"


def should_skip(path: Path, root: Path) -> bool:
    try:
        rel = path.relative_to(root)
    except ValueError:
        return True
    if any(part in SKIP_DIR_NAMES for part in rel.parts):
        return True
    if path.is_file() and path.suffix in SKIP_FILE_SUFFIXES:
        return True
    return False


def iter_skill_files(skill_dir: Path) -> list[Path]:
    files: list[Path] = []
    for path in sorted(skill_dir.rglob("*")):
        if not path.is_file():
            continue
        if should_skip(path, skill_dir):
            continue
        files.append(path)
    return files


def write_single_zip(skill: str, out_dir: Path) -> Path:
    skill_dir = SKILLS_DIR / skill
    if not (skill_dir / "SKILL.md").is_file():
        raise FileNotFoundError(f"{skill}: 缺少 SKILL.md")

    version = skill_version(skill)
    out_path = out_dir / f"{skill}-v{version}.zip"
    files = iter_skill_files(skill_dir)
    if not files:
        raise RuntimeError(f"{skill}: 无可打包文件")

    out_dir.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(out_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for path in files:
            arcname = path.relative_to(skill_dir).as_posix()
            zf.write(path, arcname)
        # 导入器兼容：确保根目录存在 SKILL.md
        names = set(zf.namelist())
        if "SKILL.md" not in names:
            raise RuntimeError(f"{skill}: 打包后缺少根级 SKILL.md")

    return out_path


def write_bundle_zip(skills: list[str], out_dir: Path) -> Path:
    out_path = out_dir / "mcu-skills-bundle.zip"
    out_dir.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(out_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for skill in skills:
            skill_dir = SKILLS_DIR / skill
            if not (skill_dir / "SKILL.md").is_file():
                raise FileNotFoundError(f"{skill}: 缺少 SKILL.md")
            for path in iter_skill_files(skill_dir):
                rel = path.relative_to(skill_dir).as_posix()
                arcname = f"{skill}/{rel}"
                zf.write(path, arcname)
        names = zf.namelist()
        skill_md_count = sum(
            1 for name in names if name.endswith("/SKILL.md") or name == "SKILL.md"
        )
        if skill_md_count < len(skills):
            raise RuntimeError(
                f"全量包 SKILL.md 数量不足: 期望 {len(skills)}，实际 {skill_md_count}"
            )
    return out_path


def main() -> int:
    parser = argparse.ArgumentParser(description="打包可导入的 Skill ZIP")
    parser.add_argument("skills", nargs="*", help="Skill 名称；省略则打包全部")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT, help="输出目录，默认 dist/")
    parser.add_argument("--no-bundle", action="store_true", help="不生成全量 mcu-skills-bundle.zip")
    parser.add_argument("--list", action="store_true", help="只列出将生成的文件，不写入")
    args = parser.parse_args()

    targets = args.skills or list_skills()
    unknown = [name for name in targets if not (SKILLS_DIR / name / "SKILL.md").is_file()]
    if unknown:
        print(f"错误: 未找到 Skill: {', '.join(unknown)}", file=sys.stderr)
        print(f"可用: {', '.join(list_skills())}", file=sys.stderr)
        return 1

    planned: list[str] = [f"{name}-v{skill_version(name)}.zip" for name in targets]
    if not args.no_bundle and len(targets) > 1:
        planned.append("mcu-skills-bundle.zip")

    if args.list:
        for name in planned:
            print((args.out / name).as_posix())
        return 0

    written: list[Path] = []
    for skill in targets:
        path = write_single_zip(skill, args.out)
        written.append(path)
        print(f"wrote {path.relative_to(ROOT) if path.is_relative_to(ROOT) else path}")

    if not args.no_bundle and len(targets) > 1:
        bundle = write_bundle_zip(targets, args.out)
        written.append(bundle)
        print(f"wrote {bundle.relative_to(ROOT) if bundle.is_relative_to(ROOT) else bundle}")

    print(f"完成：{len(written)} 个 ZIP → {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
