from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import skill_registry
import evaluate_routing
import validate


class ValidateTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.root = Path(self.temp_dir.name)
        self.write(
            "README.md",
            "# Test\n\n<!-- GENERATED:SKILL_TABLE:START -->\n<!-- GENERATED:SKILL_TABLE:END -->\n",
        )
        self.write(
            "skills/README.md",
            "# Skills\n\n"
            "<!-- GENERATED:DEPENDENCY_TABLE:START -->\n"
            "<!-- GENERATED:DEPENDENCY_TABLE:END -->\n",
        )
        self.write_skill("alpha", "foundation")
        self.write(
            "evals/routing-cases.json",
            json.dumps(
                {
                    "schema_version": 1,
                    "cases": [
                        {
                            "id": "alpha",
                            "query": "alpha route",
                            "expected_skills": ["alpha"],
                            "expected_targets": ["skill://alpha/references/entry.md"],
                        }
                    ],
                },
                ensure_ascii=False,
                indent=2,
            )
            + "\n",
        )
        self.generate()

    def tearDown(self) -> None:
        self.temp_dir.cleanup()

    def write(self, relative_path: str, content: str) -> None:
        path = self.root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")

    def write_metadata(
        self,
        name: str,
        layer: str,
        dependencies: list[str] | None = None,
        trigger: str | None = None,
    ) -> None:
        data = {
            "schema_version": 1,
            "name": name,
            "version": "1.0.0",
            "title": name.title(),
            "summary": f"{name} test skill",
            "layer": layer,
            "dependencies": dependencies or [],
            "triggers": [trigger or f"{name} trigger"],
            "routes": [
                {
                    "id": "entry",
                    "intent": f"{name} entry",
                    "keywords": [f"{name} route"],
                    "target": "references/entry.md",
                }
            ],
        }
        self.write(
            f"skills/{name}/skill.json",
            json.dumps(data, ensure_ascii=False, indent=2) + "\n",
        )

    def write_skill(
        self,
        name: str,
        layer: str,
        dependencies: list[str] | None = None,
        frontmatter_name: str | None = None,
        trigger: str | None = None,
    ) -> None:
        self.write(
            f"skills/{name}/SKILL.md",
            "---\n"
            f"name: {frontmatter_name or name}\n"
            "description: 用于测试校验器的有效 Skill。\n"
            "---\n\n"
            f"# {name.title()}\n\n"
            "`references/entry.md`\n",
        )
        self.write(
            f"skills/{name}/CHANGELOG.md",
            "# Changelog\n\n## v1.0.0 - 2026-07-10\n",
        )
        self.write(f"skills/{name}/references/entry.md", "# Entry\n")
        self.write_metadata(name, layer, dependencies, trigger)

    def metadata(self, name: str) -> dict[str, object]:
        path = self.root / "skills" / name / "skill.json"
        value = json.loads(path.read_text(encoding="utf-8"))
        self.assertIsInstance(value, dict)
        return value

    def save_metadata(self, name: str, value: dict[str, object]) -> None:
        self.write(
            f"skills/{name}/skill.json",
            json.dumps(value, ensure_ascii=False, indent=2) + "\n",
        )

    def generate(self) -> None:
        skill_registry.write_outputs(self.root, verbose=False)
        evaluate_routing.write_report(self.root, verbose=False)

    def errors(self) -> list[str]:
        errors, _ = validate.validate(self.root)
        return errors

    def test_valid_repository(self) -> None:
        self.assertEqual([], self.errors())

    def test_frontmatter_name_must_match_directory(self) -> None:
        self.write_skill("beta", "foundation", frontmatter_name="alpha")
        self.generate()
        self.assertTrue(any("与目录名" in error for error in self.errors()))

    def test_inline_code_path_is_checked_after_chinese_punctuation(self) -> None:
        self.write("docs/example.md", "说明：`references/missing.md`\n")
        self.assertTrue(any("references/missing.md" in error for error in self.errors()))

    def test_component_reference_requires_five_sections(self) -> None:
        self.write(
            "skills/alpha/references/device.md",
            "# 测试器件开发规范\n\n## 1. 选型\n\n## 2. 硬件\n\n## 3. 驱动\n\n## 5. 避坑\n",
        )
        self.generate()
        self.assertTrue(any("## 4" in error for error in self.errors()))

    def test_duplicate_skill_names_are_rejected(self) -> None:
        self.write_skill("beta", "foundation", frontmatter_name="alpha")
        self.generate()
        self.assertTrue(any("重复" in error for error in self.errors()))

    def test_changelog_is_required(self) -> None:
        (self.root / "skills/alpha/CHANGELOG.md").unlink()
        self.assertTrue(any("缺少 CHANGELOG.md" in error for error in self.errors()))

    def test_skill_metadata_is_required(self) -> None:
        (self.root / "skills/alpha/skill.json").unlink()
        self.assertTrue(any("缺少 skill.json" in error for error in self.errors()))

    def test_unknown_dependency_is_rejected(self) -> None:
        data = self.metadata("alpha")
        data["layer"] = "domain"
        data["dependencies"] = ["missing"]
        self.save_metadata("alpha", data)
        self.generate()
        self.assertTrue(any("未知依赖" in error for error in self.errors()))

    def test_dependency_cycle_is_rejected(self) -> None:
        self.write_skill("beta", "orchestrator", ["alpha"])
        alpha = self.metadata("alpha")
        alpha["layer"] = "orchestrator"
        alpha["dependencies"] = ["beta"]
        self.save_metadata("alpha", alpha)
        self.generate()
        self.assertTrue(any("依赖存在循环" in error for error in self.errors()))

    def test_orphan_resource_is_rejected(self) -> None:
        self.write("skills/alpha/references/orphan.md", "# Orphan\n")
        self.generate()
        self.assertTrue(any("孤儿资源" in error for error in self.errors()))

    def test_duplicate_routing_term_is_rejected(self) -> None:
        self.write_skill("beta", "foundation", trigger="alpha trigger")
        self.generate()
        self.assertTrue(any("触发词" in error and "重复" in error for error in self.errors()))

    def test_undeclared_skill_uri_is_rejected(self) -> None:
        self.write_skill("beta", "foundation")
        self.write(
            "skills/alpha/SKILL.md",
            "---\n"
            "name: alpha\n"
            "description: 用于测试未声明依赖。\n"
            "---\n\n"
            "`references/entry.md`\n"
            "`skill://beta/SKILL.md`\n",
        )
        self.generate()
        self.assertTrue(any("使用未声明依赖" in error for error in self.errors()))

    def test_invalid_output_uri_is_rejected(self) -> None:
        self.write("skills/alpha/notes.md", "`output://../escape.md`\n")
        self.assertTrue(any("无效 output:// URI" in error for error in self.errors()))

    def test_generated_output_drift_is_rejected(self) -> None:
        with (self.root / "skills/registry.json").open("a", encoding="utf-8") as file:
            file.write("\n")
        self.assertTrue(any("自动生成文件已过期" in error for error in self.errors()))


if __name__ == "__main__":
    unittest.main()
