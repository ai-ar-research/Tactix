#!/usr/bin/env python3
"""Public theory target projections must remain exact frozen-WV subsets."""

import csv
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TARGETS = ROOT / "benchmarks/probes/public-theory-targets-v1"


def rows(path):
    with path.open(newline="") as stream:
        lines = (line for line in stream if line.strip() and not line.startswith("#"))
        return list(csv.DictReader(lines, delimiter="\t"))


authorities = {}
for name in ("WV100.tsv", "WV2-89.tsv"):
    for row in rows(ROOT / "benchmarks/tiers" / name):
        key = row["instance"]
        assert key not in authorities
        authorities[key] = row

expected_counts = {
    "gf3-wv100.tsv": 12,
    "tseitin-wv100.tsv": 3,
    "tseitin-wv2.tsv": 6,
    "direct-php-wv2.tsv": 4,
}
seen = set()
for name, expected in expected_counts.items():
    selected = rows(TARGETS / name)
    assert len(selected) == expected
    assert len({row["instance"] for row in selected}) == expected
    for row in selected:
        assert row["instance"] not in seen
        seen.add(row["instance"])
        authority = authorities[row["instance"]]
        for field in ("coverage_rank", "original_tierd_rank", "verdict"):
            assert row[field] == authority[field], (name, field, row, authority)
        assert (ROOT / "benchmarks/cache" / row["instance"]).is_file()

assert len(seen) == sum(expected_counts.values())
print("Public theory target projections: PASS")
