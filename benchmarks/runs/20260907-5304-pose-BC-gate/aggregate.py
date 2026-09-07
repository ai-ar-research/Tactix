#!/usr/bin/env python3
"""Require all 66 checked, clean records before reporting the B28/C5 guard."""
import json
from pathlib import Path

C = Path(__file__).resolve().parent
m = json.loads((C / "manifest.json").read_text())
rows = [json.loads((C / "results" / f"{i:03d}.json").read_text()) for i in range(66)]
for i, row in enumerate(rows):
    assert row["task"] == i
    assert row["valid"] and row["artifact_deleted"] and not row["cleanup_pending"]
    assert row["binary_sha256"] == m["arms"][row["arm"]]["sha256"]
    assert row["solve"]["exit_code"] == (10 if row["verdict"] == "SAT" else 20)
    checked = row["model_check"] if row["verdict"] == "SAT" else row["proof_check"]
    assert checked["result"] == "verified" and checked["exit_code"] == 0
totals = {arm: sum(r["solve"]["wall_seconds"] for r in rows if r["arm"] == arm)
          for arm in ("shipped", "candidate")}
print(json.dumps(dict(valid_checked_clean=66, pairs=33, seconds=totals,
                     candidate_over_shipped=totals["candidate"] / totals["shipped"],
                     limitation="B28/C5 cumulative guard, not a full Tier-D or three-wave targeted timing result."), indent=2))
