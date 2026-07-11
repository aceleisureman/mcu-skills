from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import bump_version
import extract_changelog
import skill_registry


class VersionTests(unittest.TestCase):
    def test_parse_version(self) -> None:
        self.assertEqual((1, 2, 3), bump_version.parse_version("1.2.3"))

    def test_parse_version_rejects_invalid(self) -> None:
        for value in ("1.2", "v1.2.3", "1.2.3-rc1", "abc"):
            with self.assertRaises(ValueError):
                bump_version.parse_version(value)

    def test_bump(self) -> None:
        self.assertEqual("2.0.0", bump_version.bump("1.2.3", "major"))
        self.assertEqual("1.3.0", bump_version.bump("1.2.3", "minor"))
        self.assertEqual("1.2.4", bump_version.bump("1.2.3", "patch"))


class ChangelogTests(unittest.TestCase):
    TEXT = (
        "# Changelog\n\n"
        "## v2.1.0 - 2026-07-11\n\n- 新功能 A\n- 修复 B\n\n"
        "## v2.0.0 - 2026-07-01\n\n- 初始版本\n"
    )

    def test_extract_section(self) -> None:
        body = extract_changelog.extract_section(self.TEXT, "2.1.0")
        self.assertEqual("- 新功能 A\n- 修复 B", body)

    def test_extract_last_section(self) -> None:
        self.assertEqual("- 初始版本", extract_changelog.extract_section(self.TEXT, "2.0.0"))

    def test_extract_missing_version(self) -> None:
        self.assertEqual("", extract_changelog.extract_section(self.TEXT, "9.9.9"))

    def test_extract_does_not_match_prefix(self) -> None:
        text = "## v2.1.0-beta\n\n- beta\n"
        self.assertEqual("", extract_changelog.extract_section(text, "2.1.0"))

    def test_update_changelog_inserts_entry(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            skills_dir = Path(temp)
            (skills_dir / "alpha").mkdir()
            path = skills_dir / "alpha" / "CHANGELOG.md"
            path.write_text("# Changelog\n\n## v1.0.0 - 2026-07-01\n\n- 初始\n", encoding="utf-8")
            original = bump_version.SKILLS_DIR
            bump_version.SKILLS_DIR = skills_dir
            try:
                bump_version.update_changelog("alpha", "1.1.0", ["改进"])
            finally:
                bump_version.SKILLS_DIR = original
            text = path.read_text(encoding="utf-8")
            self.assertIn("## v1.1.0", text)
            self.assertIn("- 改进", text)
            self.assertLess(text.index("## v1.1.0"), text.index("## v1.0.0"))


class SkillUriTests(unittest.TestCase):
    def test_parse_valid_uri(self) -> None:
        self.assertEqual(
            ("mcu-sensors", "references/temperature.md"),
            skill_registry.parse_skill_uri("skill://mcu-sensors/references/temperature.md"),
        )

    def test_fragment_is_stripped(self) -> None:
        self.assertEqual(
            ("mcu-driver-core", "SKILL.md"),
            skill_registry.parse_skill_uri("skill://mcu-driver-core/SKILL.md#anchor"),
        )

    def test_invalid_uris_are_rejected(self) -> None:
        for uri in (
            "skill://",
            "skill://Bad-Name/SKILL.md",
            "skill://mcu-sensors/../escape.md",
            "references/entry.md",
        ):
            with self.assertRaises(ValueError):
                skill_registry.parse_skill_uri(uri)


class GeneratedBlockTests(unittest.TestCase):
    def test_replace_generated_block(self) -> None:
        text = "before\n<!-- S -->\nold\n<!-- E -->\nafter\n"
        result = skill_registry.replace_generated_block(text, "<!-- S -->", "<!-- E -->", "new")
        self.assertIn("<!-- S -->\nnew\n<!-- E -->", result)
        self.assertNotIn("old", result)

    def test_missing_markers_raise(self) -> None:
        with self.assertRaises(ValueError):
            skill_registry.replace_generated_block("no markers", "<!-- S -->", "<!-- E -->", "x")

    def test_render_route_table(self) -> None:
        item = {
            "routes": [
                {
                    "id": "entry",
                    "intent": "示例意图",
                    "keywords": ["关键词一", "关键词二"],
                    "target": "references/entry.md",
                }
            ]
        }
        table = skill_registry.render_route_table(item)
        self.assertIn("| 示例意图 | 关键词一、关键词二 | `references/entry.md` |", table)


class ReleaseFilesTests(unittest.TestCase):
    def test_release_files_are_scoped(self) -> None:
        files = bump_version.release_files(["mcu-sensors"])
        self.assertIn("skills/mcu-sensors/skill.json", files)
        self.assertIn("skills/mcu-sensors/CHANGELOG.md", files)
        self.assertIn("skills/registry.json", files)
        self.assertIn("docs/routing-evaluation.json", files)
        self.assertNotIn("skills/mcu-power/skill.json", files)


if __name__ == "__main__":
    unittest.main()
