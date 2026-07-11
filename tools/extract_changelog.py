#!/usr/bin/env python3
"""Extract a single version's section from a skill CHANGELOG for release notes."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


def extract_section(text: str, version: str) -> str:
    heading_re = re.compile(rf"^## v{re.escape(version)}(?:[ \t][^\n]*)?$\n?", re.MULTILINE)
    match = heading_re.search(text)
    if not match:
        return ""
    rest = text[match.end() :]
    next_heading = re.search(r"^## ", rest, re.MULTILINE)
    body = rest[: next_heading.start()] if next_heading else rest
    return body.strip()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("changelog", type=Path)
    parser.add_argument("version")
    args = parser.parse_args()

    if not args.changelog.is_file():
        return 1
    body = extract_section(args.changelog.read_text(encoding="utf-8"), args.version)
    if not body:
        return 1
    print(body)
    return 0


if __name__ == "__main__":
    sys.exit(main())
