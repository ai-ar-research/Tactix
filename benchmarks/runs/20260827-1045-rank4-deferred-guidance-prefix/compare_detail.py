#!/usr/bin/env python3
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
COMPARE = ROOT / "tools" / "bench" / "compare_event_trace.py"
SPEC = importlib.util.spec_from_file_location("event_compare", COMPARE)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)

campaign = Path(__file__).resolve().parent
candidate = MODULE.load(campaign / "traces" / "tactix.tsv")["rows"]
accepted = MODULE.load(
    ROOT
    / "benchmarks/runs/20260827-1030-tierd-rank4-postrepair-search-frontier/traces/tactix.tsv"
)["rows"]
candidate = [row for row in candidate if row["event"] in MODULE.DETAIL_EVENTS]
accepted = [row for row in accepted if row["event"] in MODULE.DETAIL_EVENTS]

fields = ("event", "conflicts", "decisions", "level", "payload")
for index, (left, right) in enumerate(zip(candidate, accepted)):
    delta = [field for field in fields if left[field] != right[field]]
    if delta:
        print(f"detail_index={index}")
        print(f"different_fields={','.join(delta)}")
        print(f"candidate={left}")
        print(f"accepted={right}")
        raise SystemExit(1)
if len(candidate) != len(accepted):
    print(f"detail_lengths={len(candidate)},{len(accepted)}")
    raise SystemExit(1)
print(f"exact_detail_records={len(candidate)}")
