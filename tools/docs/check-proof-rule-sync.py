#!/usr/bin/env python3
"""Keep the manually maintained proof-rule guide's inventory in sync."""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
CALCULUS = ROOT / "docs/dev/calculus.md"
GUIDE = ROOT / "docs/dev/proof-rules.html"


def duplicates(items):
    return sorted({item for item in items if items.count(item) > 1})


def main():
    calculus = CALCULUS.read_text(encoding="utf-8")
    guide = GUIDE.read_text(encoding="utf-8")
    abstract = re.findall(r"^### ([A-Z]\d+[a-z]?) ·", calculus, re.M)
    cards = re.findall(r'<details class="rule-card" id="([A-Z]\d+[a-z]?)"', guide)
    expected = abstract + ["V6a"]  # documented internal channel inside V6

    errors = []
    if duplicates(abstract):
        errors.append(f"duplicate calculus ids: {', '.join(duplicates(abstract))}")
    if duplicates(cards):
        errors.append(f"duplicate guide cards: {', '.join(duplicates(cards))}")
    missing = sorted(set(expected) - set(cards))
    extra = sorted(set(cards) - set(expected))
    if missing:
        errors.append(f"guide is missing cards: {', '.join(missing)}")
    if extra:
        errors.append(f"guide has cards absent from calculus: {', '.join(extra)}")

    for error in errors:
        print(f"  FAIL  {error}")
    print(f"check-proof-rule-sync: {len(errors)} error(s), "
          f"{len(abstract)} calculus rule(s), {len(cards)} guide card(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
