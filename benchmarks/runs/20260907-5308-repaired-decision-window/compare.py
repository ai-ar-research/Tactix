#!/usr/bin/env python3
"""Compare ordinary decisions separately from rephase/warm-up events."""
import hashlib
import json
from pathlib import Path

C = Path(__file__).resolve().parent


def extract(arm):
    path = C / "traces" / (arm + ".tsv")
    digest = hashlib.sha256()
    decisions = []
    warming = False
    last_conflict = 0
    with path.open("rb") as source:
        for raw in source:
            digest.update(raw)
            if raw.startswith(b"#"):
                continue
            fields = raw.decode().rstrip("\n").split("\t")
            event, conflict = fields[2], int(fields[3])
            last_conflict = max(last_conflict, conflict)
            if event == "rephase_begin":
                warming = True
            elif event == "rephase_end":
                warming = False
            elif event == "decision" and 50000 <= conflict <= 75451:
                decisions.append(dict(conflict=conflict, counter=int(fields[4]),
                                      level=int(fields[6]), lit=int(fields[-1]),
                                      warming=warming))
    assert last_conflict >= 75452, (arm, last_conflict)
    return decisions, digest.hexdigest()


t, th = extract("tactix")
k, kh = extract("kissat")
ordinary_t = [r for r in t if not r["warming"]]
ordinary_k = [r for r in k if not r["warming"]]
first = next((dict(index=i, tactix=a, kissat=b)
              for i, (a, b) in enumerate(zip(t, k)) if a != b), None)
print(json.dumps(dict(ordinary_counts=[len(ordinary_t), len(ordinary_k)],
                     ordinary_equal=ordinary_t == ordinary_k,
                     combined_counts=[len(t), len(k)], first_mismatch=first,
                     trace_sha256=dict(tactix=th, kissat=kh)), indent=2))
