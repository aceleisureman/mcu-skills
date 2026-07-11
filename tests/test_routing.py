from __future__ import annotations

import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import evaluate_routing
import skill_registry


class RoutingEvaluationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.report = evaluate_routing.evaluate(ROOT)

    def test_dataset_has_broad_coverage(self) -> None:
        summary = self.report["summary"]
        self.assertGreaterEqual(summary["cases"], 50)

    def test_quality_thresholds_pass(self) -> None:
        self.assertTrue(evaluate_routing.thresholds_pass(self.report))

    def test_all_curated_cases_match_exactly(self) -> None:
        failed = [result["id"] for result in self.report["results"] if not result["passed"]]
        self.assertEqual([], failed)

    def test_router_is_case_insensitive(self) -> None:
        registry = skill_registry.build_registry(ROOT)
        skills, targets, _ = evaluate_routing.route_query(
            registry,
            "esp8266 wifi at 断线重连",
        )
        self.assertEqual(["mcu-communication"], skills)
        self.assertIn("skill://mcu-communication/references/wifi.md", targets)

    def test_checked_report_is_current(self) -> None:
        self.assertTrue(evaluate_routing.check_report(ROOT))


if __name__ == "__main__":
    unittest.main()
