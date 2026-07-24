"""Unit tests that do not require MySQL or network."""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

SERVER_ROOT = Path(__file__).resolve().parents[1]
REPO_ROOT = SERVER_ROOT.parent
sys.path.insert(0, str(SERVER_ROOT))
sys.path.insert(0, str(REPO_ROOT / "tools"))


def test_hash_and_parse_token():
    from app.auth.deps import generate_api_key, hash_secret, parse_token

    key_id, secret, token = generate_api_key()
    assert parse_token(token) == (key_id, secret)
    assert hash_secret(secret) != secret
    assert hash_secret(secret) == hash_secret(secret)
    assert parse_token("noperiod") is None


def test_skill_catalog_list_and_route():
    from app.services.skill_catalog import SkillCatalog

    catalog = SkillCatalog(REPO_ROOT)
    skills = catalog.list_skills()
    names = {s["name"] for s in skills}
    assert "mcu-sensors" in names
    assert "mcu-driver-core" in names

    routed = catalog.route_skills("I2C 驱动模板")
    assert "mcu-driver-core" in routed["skills"]
    assert any(t.startswith("skill://mcu-driver-core/") for t in routed["targets"])


def test_get_resource_skill_uri():
    from app.services.skill_catalog import SkillCatalog

    catalog = SkillCatalog(REPO_ROOT)
    data = catalog.get_resource(uri="skill://mcu-sensors/references/temperature.md")
    assert data["type"] == "file"
    assert "温度" in data["content"] or "DS18B20" in data["content"] or len(data["content"]) > 50


def test_chunk_markdown_and_bm25():
    from app.services.knowledge import KnowledgeService, chunk_markdown

    chunks = chunk_markdown(
        "# 标题\n\nDS18B20 温度传感器\n\n## 驱动\n\n读取温度寄存器\n",
        doc_id="t1",
        source="upload",
        uri="upload://t1/a.md",
    )
    assert len(chunks) >= 1
    svc = KnowledgeService(REPO_ROOT, REPO_ROOT / "data" / "test-index")
    svc.build_from_chunks(chunks)
    hits = svc.search("DS18B20 温度", top_k=3)
    assert hits
    assert hits[0]["score"] > 0


def test_organize_allowlist():
    from app.services.organize import OrganizeService

    org = OrganizeService(REPO_ROOT, [REPO_ROOT])
    result = org.scan(str(REPO_ROOT))
    assert isinstance(result, dict)
    with pytest.raises(PermissionError):
        OrganizeService(REPO_ROOT, [REPO_ROOT / "skills"]).scan("/tmp")


def test_sanitize_filename():
    from app.services.storage import sanitize_filename

    assert ".." not in sanitize_filename("../etc/passwd")
    assert "/" not in sanitize_filename("a/b.md")
