#!/usr/bin/env python3
"""Evaluate deterministic Skill routing against a curated query dataset."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

from skill_registry import build_registry


NORMALIZE_RE = re.compile(r"[\W_]+", re.UNICODE)


def normalize(value: str) -> str:
    return NORMALIZE_RE.sub("", value.casefold())


def string_list(value: object) -> list[str]:
    if not isinstance(value, list):
        return []
    return [item for item in value if isinstance(item, str)]


def object_list(value: object) -> list[dict[str, object]]:
    if not isinstance(value, list):
        return []
    return [item for item in value if isinstance(item, dict)]


def canonical_target(skill_name: str, target: str) -> str:
    if target.startswith("skill://"):
        return target
    return f"skill://{skill_name}/{target}"


def route_query(
    registry: dict[str, object],
    query: str,
) -> tuple[list[str], list[str], dict[str, list[str]]]:
    normalized_query = normalize(query)
    skill_scores: dict[str, int] = {}
    route_scores: dict[str, int] = {}
    matches: dict[str, list[str]] = {}

    for skill in object_list(registry.get("skills")):
        name = str(skill["name"])
        score = 0
        matched_terms: list[str] = []
        for trigger in string_list(skill.get("triggers")):
            normalized = normalize(trigger)
            if normalized and normalized in normalized_query:
                score += len(normalized)
                matched_terms.append(trigger)

        for route in object_list(skill.get("routes")):
            route_score = 0
            for keyword in string_list(route.get("keywords")):
                normalized = normalize(keyword)
                if normalized and normalized in normalized_query:
                    route_score += len(normalized) * 2
                    matched_terms.append(keyword)
            if route_score:
                target = canonical_target(name, str(route["target"]))
                route_scores[target] = route_scores.get(target, 0) + route_score
                score += route_score

        if score:
            skill_scores[name] = score
            matches[name] = sorted(set(matched_terms))

    predicted_skills = [
        name for name, _ in sorted(skill_scores.items(), key=lambda item: (-item[1], item[0]))
    ]
    predicted_targets = [
        target for target, _ in sorted(route_scores.items(), key=lambda item: (-item[1], item[0]))
    ]
    return predicted_skills, predicted_targets, matches


def load_cases(path: Path) -> list[dict[str, object]]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict) or not isinstance(value.get("cases"), list):
        raise ValueError(f"{path}: 顶层必须包含 cases 数组")
    cases = object_list(value["cases"])
    if len(cases) != len(value["cases"]):
        raise ValueError(f"{path}: cases 必须全部为 object")
    return cases


def safe_ratio(numerator: int, denominator: int) -> float:
    return numerator / denominator if denominator else 1.0


def evaluate(root: Path) -> dict[str, object]:
    registry = build_registry(root)
    cases = load_cases(root / "evals" / "routing-cases.json")
    true_positive = 0
    predicted_total = 0
    expected_total = 0
    target_true_positive = 0
    target_predicted_total = 0
    target_expected_total = 0
    top1_hits = 0
    results: list[dict[str, object]] = []

    for case in cases:
        case_id = str(case["id"])
        query = str(case["query"])
        expected_skills = string_list(case.get("expected_skills"))
        expected_targets = string_list(case.get("expected_targets"))
        predicted_skills, predicted_targets, matches = route_query(registry, query)

        expected_skill_set = set(expected_skills)
        predicted_skill_set = set(predicted_skills)
        expected_target_set = set(expected_targets)
        predicted_target_set = set(predicted_targets)
        skill_hits = expected_skill_set & predicted_skill_set
        target_hits = expected_target_set & predicted_target_set

        true_positive += len(skill_hits)
        predicted_total += len(predicted_skill_set)
        expected_total += len(expected_skill_set)
        target_true_positive += len(target_hits)
        target_predicted_total += len(predicted_target_set)
        target_expected_total += len(expected_target_set)
        if predicted_skills and predicted_skills[0] in expected_skill_set:
            top1_hits += 1

        results.append({
            "id": case_id,
            "query": query,
            "expected_skills": expected_skills,
            "predicted_skills": predicted_skills,
            "expected_targets": expected_targets,
            "predicted_targets": predicted_targets,
            "matches": matches,
            "passed": (
                predicted_skill_set == expected_skill_set
                and expected_target_set <= predicted_target_set
            ),
        })

    precision = safe_ratio(true_positive, predicted_total)
    recall = safe_ratio(true_positive, expected_total)
    f1 = safe_ratio(2 * true_positive, predicted_total + expected_total)
    target_precision = safe_ratio(target_true_positive, target_predicted_total)
    target_recall = safe_ratio(target_true_positive, target_expected_total)
    exact_match = safe_ratio(
        sum(1 for result in results if result["passed"]),
        len(results),
    )
    return {
        "schema_version": 1,
        "generated_by": "tools/evaluate_routing.py",
        "summary": {
            "cases": len(results),
            "skill_precision": round(precision, 4),
            "skill_recall": round(recall, 4),
            "skill_f1": round(f1, 4),
            "top1_accuracy": round(safe_ratio(top1_hits, len(results)), 4),
            "target_precision": round(target_precision, 4),
            "target_recall": round(target_recall, 4),
            "exact_match": round(exact_match, 4),
        },
        "results": results,
    }


def report_text(report: dict[str, object]) -> str:
    return json.dumps(report, ensure_ascii=False, indent=2) + "\n"


def report_path(root: Path) -> Path:
    return root / "docs" / "routing-evaluation.json"


def write_report(root: Path, verbose: bool = True) -> None:
    path = report_path(root)
    path.write_text(report_text(evaluate(root)), encoding="utf-8")
    if verbose:
        print(f"updated {path.relative_to(root)}")


def check_report(root: Path) -> bool:
    path = report_path(root)
    return path.is_file() and path.read_text(encoding="utf-8") == report_text(evaluate(root))


def thresholds_pass(report: dict[str, object]) -> bool:
    summary_object = report.get("summary")
    if not isinstance(summary_object, dict):
        return False
    return (
        float(summary_object.get("skill_precision", 0.0)) >= 0.95
        and float(summary_object.get("skill_recall", 0.0)) >= 0.95
        and float(summary_object.get("top1_accuracy", 0.0)) >= 0.95
        and float(summary_object.get("target_recall", 0.0)) >= 0.90
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    action = parser.add_mutually_exclusive_group()
    action.add_argument("--write-report", action="store_true")
    action.add_argument("--check-report", action="store_true")
    action.add_argument("--json", action="store_true")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    args = parser.parse_args()
    root = args.root.resolve()

    if args.write_report:
        write_report(root)
        return 0
    if args.check_report:
        if check_report(root):
            print("路由评测报告为最新")
            return 0
        print("路由评测报告已过期，运行 python3 tools/evaluate_routing.py --write-report")
        return 1

    report = evaluate(root)
    if args.json:
        print(report_text(report), end="")
    else:
        summary = report["summary"]
        print(
            "路由评测: "
            f"{summary['cases']} cases, "
            f"precision={summary['skill_precision']:.2%}, "
            f"recall={summary['skill_recall']:.2%}, "
            f"top1={summary['top1_accuracy']:.2%}, "
            f"target_recall={summary['target_recall']:.2%}, "
            f"exact={summary['exact_match']:.2%}"
        )
    return 0 if thresholds_pass(report) else 1


if __name__ == "__main__":
    raise SystemExit(main())
