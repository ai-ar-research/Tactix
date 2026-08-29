#!/usr/bin/env python3
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
fused_micro = {"analyze", "backjump"}
bad = []

for path in sorted((root / "src/rules").glob("*/*")):
    if path.suffix not in {".cpp", ".hpp"} or path.parent.name in fused_micro:
        continue
    text = path.read_text()
    if re.search(r'^\s*#\s*include\s*[<"]policies/', text, re.MULTILINE):
        bad.append(f"{path.relative_to(root)} imports a policy header")
    if re.search(r'\bheuristics_[A-Za-z0-9_]+\s*\(', text):
        bad.append(f"{path.relative_to(root)} calls a heuristic operation")

# Decide and Restart have complete explicit-instance surfaces. Their canonical
# order and mechanics must remain semantic; DefaultPolicy owns ranked picks,
# trail-reuse scoring, and phase snapshots.  Keep this explicit check separate
# from the general loop so adding another fused micro rule cannot accidentally
# widen their exception.
for rule in ("decide", "restart"):
    for path in sorted((root / "src/rules" / rule).glob("*")):
        if path.suffix not in {".cpp", ".hpp"}:
            continue
        text = path.read_text()
        if re.search(r'^\s*#\s*include\s*[<"]policies/', text, re.MULTILINE):
            bad.append(f"{path.relative_to(root)} imports a policy header")
        if re.search(r'\bheuristics_[A-Za-z0-9_]+\s*\(', text):
            bad.append(f"{path.relative_to(root)} calls a heuristic operation")

# ConflictAnalysis is one atomic micro rule.  Its two source files may use only
# the exact heuristic observations required at internal learn/backtrack
# boundaries; treating the whole directories as exempt would let arbitrary
# strategy ranking creep into the rule unnoticed.
fused_allowed = {
    "analyze": {
        "heuristics_queue_bump",
        "heuristics_score_bump",
        "heuristics_scores_decay",
        "heuristics_update_target_phases",
        "heuristics_var_active",
    },
    "backjump": {
        "heuristics_deactivate_var",
        "heuristics_update_target_phases",
    },
}
for rule, allowed in fused_allowed.items():
    for path in sorted((root / "src/rules" / rule).glob("*")):
        if path.suffix not in {".cpp", ".hpp"}:
            continue
        text = path.read_text()
        calls = set(re.findall(r'\b(heuristics_[A-Za-z0-9_]+)\s*\(', text))
        for call in sorted(calls - allowed):
            bad.append(
                f"{path.relative_to(root)} calls non-allowlisted heuristic "
                f"operation `{call}`"
            )
        policy_includes = set(
            re.findall(
                r'^\s*#\s*include\s*[<"]policies/([^>"]+)[>"]',
                text,
                re.MULTILINE,
            )
        )
        for include in sorted(policy_includes - {"event_trace.hpp", "heuristics.hpp"}):
            bad.append(
                f"{path.relative_to(root)} imports non-allowlisted policy "
                f"header `policies/{include}`"
            )

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

# A proof rule may invoke the kernel's representation-only replacement after it
# has proved and emitted its own large-to-large rewrite. Keep every rule-side
# owner explicit: Substitute consumes proved equivalences, Strengthen owns its
# self-subsuming-resolution proof, and Vivify owns its probing-derived RUP
# rewrite. Strategy code and unrelated rules must not use the primitive as a
# semantic shortcut.
in_place_primitive = "kernel_replace_large_clause_in_place"
allowed_in_place_owners = {
    Path("src/kernel/kernel.hpp"),
    Path("src/kernel/kernel.cpp"),
    Path("src/rules/strengthen/strengthen.cpp"),
    Path("src/rules/substitute/substitute.cpp"),
    Path("src/rules/vivify/vivify.cpp"),
}
for path in sorted((root / "src").glob("**/*")):
    if path.suffix not in {".cpp", ".hpp"}:
        continue
    if in_place_primitive in path.read_text():
        relative = path.relative_to(root)
        if relative not in allowed_in_place_owners:
            bad.append(
                f"{relative} uses `{in_place_primitive}` without explicit "
                "proof-rule ownership"
            )

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
