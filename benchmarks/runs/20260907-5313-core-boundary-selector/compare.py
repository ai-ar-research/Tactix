#!/usr/bin/env python3
"""Pin the original-core unit missing from the host's derived-step-only walk."""
import hashlib
import json
from pathlib import Path
import re

C = Path(__file__).resolve().parent


def unit(row):
    values = [(int(a), int(b)) for a, b in re.findall(r"(-?\d+):(-?\d+),", row)]
    if any(v > 0 for _, v in values):
        return None
    live = [lit for lit, value in values if value == 0]
    return live[0] if len(live) == 1 else None


blocks = {}
ordinal = None
for row in (C / "logs/tactix.out").read_text().splitlines():
    if row.startswith("OBS_QUERY "):
        ordinal = int(re.search(r"ordinal=(\d+)", row)[1])
    elif row.startswith("OBS_"):
        blocks.setdefault(ordinal, []).append(row)
krows = [r for r in (C / "logs/kissat.out").read_text().splitlines()
         if r.startswith("OBS_")]
q = 20
original = next(r for r in blocks[q] if r.startswith("OBS_ORIGINAL id=44 "))
assert unit(original) == -970
tunits = [unit(r) for r in blocks[q] if r.startswith("OBS_STEP ") and unit(r)]
assert -970 not in tunits and -693 in tunits
kunit = next(r for r in krows if r.startswith("OBS_ADD query=20 ") and unit(r) == -970)
ksource = next(r for r in krows if r.startswith("OBS_SAVE query=20 learned=0 ")
               and unit(r) == -970)
kunits = [unit(r) for r in krows if r.startswith("OBS_ADD query=20 ") and unit(r)]
hashes = {}
for arm in ("tactix", "kissat"):
    current = (C / "traces" / (arm + ".tsv")).read_bytes()
    earlier = (C.parent / "20260907-5311-seed563-boundary/traces" / (arm + ".tsv")).read_bytes()
    assert current == earlier, arm
    hashes[arm] = hashlib.sha256(current).hexdigest()
print(json.dumps(dict(query=q, tactix_original_core_clause=original,
                     tactix_derived_step_units=tunits, kissat_original_core_clause=ksource,
                     kissat_unit_commit_input=kunit, kissat_add_core_units=kunits,
                     event_trace_byte_identical_to_5311=True, event_trace_sha256=hashes,
                     interpretation="The original unit is present in Tactix's core but absent from its derived-step-only unit capture; no new solver repair is promoted by this observer."), indent=2))
