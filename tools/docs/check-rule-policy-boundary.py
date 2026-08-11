#!/usr/bin/env python3
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
micro = {"analyze", "backjump", "decide", "restart"}
bad = []

for path in sorted((root / "src/rules").glob("*/*")):
    if path.suffix not in {".cpp", ".hpp"} or path.parent.name in micro:
        continue
    text = path.read_text()
    if re.search(r'^\s*#\s*include\s*[<"]policies/', text, re.MULTILINE):
        bad.append(f"{path.relative_to(root)} imports a policy header")
    if re.search(r'\bheuristics_[A-Za-z0-9_]+\s*\(', text):
        bad.append(f"{path.relative_to(root)} calls a heuristic operation")

# Data-flow checks for the two failure shapes found by the post-P4 audit. A
# header-import check cannot see either: both compile through neutral Kernel
# fields or Stats. Caller-owned limits must not be mirrored as Kernel policy
# switches, and one rule must not mutate another rule's scheduling currency.
kernel = (root / "src/kernel/kernel.hpp").read_text()
for field in ("factor_admit", "sweep_resume"):
    if re.search(rf"^\s+(?:bool|uint\w+_t)\s+{field}\b", kernel, re.MULTILINE):
        bad.append(f"src/kernel/kernel.hpp stores policy switch `{field}`")

for path in sorted((root / "src/rules").glob("*/*")):
    if path.suffix not in {".cpp", ".hpp"} or path.parent.name == "sweep":
        continue
    text = path.read_text()
    if re.search(r"\bstats\.sweep_ticks_repr\s*(?:\+\+|--|[+\-*/]?=)", text):
        bad.append(f"{path.relative_to(root)} writes Sweep's effort currency")

factor = (root / "src/rules/factor/factor.cpp").read_text()
if "TACTIX_FACTOR_FORCE" in factor or "factor_admit" in factor:
    bad.append("src/rules/factor/factor.cpp contains a rule-side admission gate")

policy = (root / "src/policies/default_policy.cpp").read_text()
meter = re.search(r"uint64_t meter_sweep\([^}]+\}", policy)
if not meter or "sweep_effort(k)" not in meter.group(0):
    bad.append("DefaultPolicy reconstructs Sweep's private effort currency")

if bad:
    print("check-rule-policy-boundary:")
    print("\n".join(f"  {line}" for line in bad))
    raise SystemExit(1)

print("check-rule-policy-boundary: rule/policy imports and data flows are clean")
