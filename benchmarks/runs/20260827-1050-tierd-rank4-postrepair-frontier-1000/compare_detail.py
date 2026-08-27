#!/usr/bin/env python3
import importlib.util
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "tools/bench/compare_event_trace.py"
SPEC = importlib.util.spec_from_file_location("event_compare", SOURCE)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)
campaign = Path(__file__).resolve().parent
left = MODULE.load(campaign / "traces/tactix.tsv")["rows"]
right = MODULE.load(campaign / "traces/kissat.tsv")["rows"]
left = [row for row in left if row["event"] in MODULE.DETAIL_EVENTS]
right = [row for row in right if row["event"] in MODULE.DETAIL_EVENTS]


def payload(row):
    return re.sub(
        r"[|](?:id|ref|glue|used|redundant)=[^|]*", "", row["payload"]
    )


for index, (a, b) in enumerate(zip(left, right)):
    fields = [
        field
        for field in ("event", "conflicts", "decisions", "level")
        if a[field] != b[field]
    ]
    if fields or payload(a) != payload(b):
        print(f"detail_index={index}")
        print(
            f"tactix seq={a['seq']} event={a['event']} conflicts={a['conflicts']} "
            f"decisions={a['decisions']} level={a['level']}"
        )
        print(
            f"kissat seq={b['seq']} event={b['event']} conflicts={b['conflicts']} "
            f"decisions={b['decisions']} level={b['level']}"
        )
        print(f"different_fields={','.join(fields) or 'payload'}")
        print(f"tactix_payload={payload(a)}")
        print(f"kissat_payload={payload(b)}")
        break
else:
    print(f"common_detail_records={min(len(left), len(right))}")
    print(f"detail_lengths={len(left)},{len(right)}")
