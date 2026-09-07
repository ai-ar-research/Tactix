#!/usr/bin/env python3
"""Reproduce the first seed-1 comparison; never merge repeated environments."""
import json
from pathlib import Path

C = Path(__file__).resolve().parent


def first_environment(arm):
    result = {}
    complete = set()
    for line in (C / "traces" / (arm + ".tsv")).open():
        if line.startswith("#"):
            assert "truncated" not in line
            continue
        row = line.rstrip().split("\t")
        if int(row[3]) != 26913:
            continue
        event = row[2]
        payload = dict(part.split("=", 1) for part in row[-1].split("|")
                       if "=" in part)
        if payload.get("seed") != "1" or "ordinal" not in payload:
            continue
        records = result.setdefault(event, [])
        if records and payload["ordinal"] == "0":
            complete.add(event)
        if event not in complete:
            records.append(payload)
    return result


t, k = (first_environment(arm) for arm in ("tactix", "kissat"))
tc, kc = (r["sweep_environment_clause"] for r in (t, k))
assert len(tc) == len(kc) == 1025
assert [r["ordinal"] for r in tc] == [str(i) for i in range(1025)]
assert [r["ordinal"] for r in kc] == [str(i) for i in range(1025)]
ordered_mismatches, set_mismatches = [], []
for i, (a, b) in enumerate(zip(tc, kc)):
    if a["order"] != b["order"]:
        ordered_mismatches.append(dict(ordinal=i, tactix=a["order"], kissat=b["order"]))
    if sorted(map(int, a["order"].split(","))) != sorted(map(int, b["order"].split(","))):
        set_mismatches.append(i)
tv, kv = (r["sweep_environment_var"] for r in (t, k))
assert len(tv) == len(kv) == 243
assert [r["external"] for r in tv] == [r["external"] for r in kv]
tq, kq = (r["sweep_query"] for r in (t, k))
keys = ("ordinal", "kind", "a", "b", "result")
first_query = next((i for i, (a, b) in enumerate(zip(tq, kq))
                    if any(a[key] != b[key] for key in keys)), None)
if first_query is None and len(tq) != len(kq):
    first_query = min(len(tq), len(kq))
tm, km = ({int(p["ordinal"]): p["model"] for p in r["sweep_query_model"]}
          for r in (t, k))
first_model = next((i for i in sorted(tm.keys() | km.keys())
                    if tm.get(i) != km.get(i)), None)
print(json.dumps(dict(clauses=1025, variables=243, ordered_mismatches=ordered_mismatches,
                     set_mismatches=set_mismatches, queries=[len(tq), len(kq)],
                     first_query_mismatch=first_query, first_model_mismatch=first_model,
                     initial_queries=dict(tactix=tq[:5], kissat=kq[:5])), indent=2))
