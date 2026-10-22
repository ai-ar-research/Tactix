#!/usr/bin/env python3
"""Campaign requirements are graded after deterministic shard aggregation."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
AGGREGATE = ROOT / "tools" / "fuzz" / "aggregate.py"


def run(root, shards, total, *extra):
    return subprocess.run(
        [sys.executable, str(AGGREGATE), "--root", str(root),
         "--prefix", "leg-shard-",
         "--shards", str(shards), "--total", str(total), *extra],
        cwd=ROOT,
        check=False,
        text=True,
        capture_output=True,
    )


def write(root, shard, summary):
    directory = root / f"leg-shard-{shard}"
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "summary.json").write_text(
        json.dumps(summary), encoding="utf-8")


with tempfile.TemporaryDirectory(prefix="tactix-fuzz-aggregate-") as tmp:
    campaign = Path(tmp)
    requirements = {
        "require_reached": ["BCE"],
        "require_policy_applied": ["Lucky"],
        "require_policy_effective": ["Lucky"],
        "require_policy_counter": ["full-restarts"],
        "max_nonterminating_frac": 0.25,
        "minimum_verdicts": 4,
    }
    for shard, offset, count in ((0, 0, 2), (1, 2, 3)):
        write(campaign, shard, {
            "schema": 1,
            "runner": "seeded",
            "generator": {"seed": 20260709},
            "range": {"offset": offset, "count": count},
            "counts": {
                "failures": 0, "cases_run": count, "nonterminating": 0,
                "effective": count, "stats_cases": count,
            },
            "reach_rules": {"BCE": int(shard == 0)},
            "reach_spec_seen": {"BCE": count},
            "policy": {
                "rule": {"Lucky": {"sum": {
                    "applications": int(shard == 0),
                    "effective": int(shard == 1),
                }}},
                "loop": {"sum": {"full-restarts": int(shard == 1)}},
            },
            "requirements": requirements,
        })
    result = run(campaign, 2, 5)
    assert result.returncode == 0, (result.stdout, result.stderr)
    assert "PASS: seeded, 5 deterministic cases, 2 shards" in result.stdout

    summary_path = campaign / "leg-shard-1" / "summary.json"
    summary = json.loads(summary_path.read_text(encoding="utf-8"))
    summary["requirements"]["minimum_verdicts"] = 6
    summary_path.write_text(json.dumps(summary), encoding="utf-8")
    result = run(campaign, 2, 5)
    assert result.returncode == 1
    assert "different qualification requirements" in result.stdout

    # Regrading must not conceal inconsistent shard contracts.
    override = ("--minimum-verdicts", "4", "--max-nonterminating-frac",
                "0.25", "--regrade-reason", "Correct submission thresholds")
    assert run(campaign, 2, 5, *override).returncode == 1
    first_path = campaign / "leg-shard-0" / "summary.json"
    first = json.loads(first_path.read_text())
    first["requirements"]["minimum_verdicts"] = 6
    write(campaign, 0, first)
    assert run(campaign, 2, 5).returncode == 1
    before = [path.read_bytes() for path in (first_path, summary_path)]
    result = run(campaign, 2, 5, *override)
    assert result.returncode == 0, result.stdout
    assert "REGRADE" in result.stdout and '"minimum_verdicts": 6' in result.stdout
    assert before == [path.read_bytes() for path in (first_path, summary_path)]
    assert run(campaign, 2, 5, "--minimum-verdicts", "4").returncode == 2
    first["counts"]["failures"] = 1
    write(campaign, 0, first)
    assert run(campaign, 2, 5, *override).returncode == 1
    first["counts"]["failures"] = 0
    first["counts"]["nonterminating"] = 2
    first["counts"]["effective"] = 0
    write(campaign, 0, first)
    result = run(campaign, 2, 5, *override)
    assert result.returncode == 1
    assert "non-terminating fraction exceeds maximum" in result.stdout
    assert "verdict minimum not met" in result.stdout

print("fuzz shard aggregate qualification: PASS")
