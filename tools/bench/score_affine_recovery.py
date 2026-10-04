#!/usr/bin/env python3
"""Score the source-pinned affine-opportunity recovery campaigns.

This is intentionally narrower than the four regression-gate scorer.  It
answers two causal questions: did restoring Lucky instance 8 recover every
direct affine SAT row under its original timeout, and did it retain the three
trajectory controls under their original solve caps?  SAT claims count only
after the independent model checker accepts them.  Timing deltas are retained
as diagnostics; the formal solver-performance gate is aggregate, while the
30-percent/20-second row allowance belongs to completed proof checking.  Newer builds
also expose partial-guidance telemetry, which is recorded without treating an
offer or import as a solve claim.
"""

import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import sys


DIRECT_AFFINE = {
    "8ffd718f763ed7a3f691cf46e57f8d98-mdp-32-10-sat.cnf.xz",
    "04ded94454830d4ea960327e8b91f5a3-mdp-28-14-sat.cnf.xz",
    "db15a85651e0d941a11bf8640625edd8-mdp-32-11-sat.cnf.xz",
    "d11944fbca2dd6540f18bd05b6ccda0c-mdp-32-14-sat.cnf.xz",
    "a5dc6226e4c0bebb06926efe55640995-mdp-36-10-sat.cnf.xz",
    "ab3438b504b296fcee78ec9e71969863-mdp-36-14-sat.cnf.xz",
}
EXPECTED_COMMIT = "60446aa0d3e8e36c948577f0083922d35e2adeb6"
AFFINE = re.compile(r"^c affine\s+(\d+) calls,.*?, (\d+) SAT,")
LOCAL_SEARCH = re.compile(
    r"^c localsearch\s+(\d+) hints offered"
    r"(?: \((\d+) phases, (\d+) replacements\))?, (\d+) applied"
    r"(?: / (\d+) target installs from (\d+) attempts"
    r" \((\d+) eligible, (\d+) changed, (\d+) matching, (\d+) replaced\))?$")
LOCAL_PHASE = re.compile(
    r"^c localphase\s+(\d+) decisions \((\d+) matched, (\d+) overridden\),"
    r" (\d+) native / (\d+) rephase / (\d+) walk overwrites$")
WALK_SEED = re.compile(
    r"^c walkseed\s+(\d+) imports(?: / (\d+) phases"
    r" \((\d+) improved, (\d+) exported\))?,")

# Accepted DefaultPolicy references from the frozen 2026-09-28 gate.
TRAJECTORY_REFERENCES = {
    "9541d12f3e3d9201085c13f25145d2f7-cabp-V-nos6.mtx.rnd-k275.sanitized.cnf.xz": {
        "gate": "Rank1-50", "rank": 28, "wall_seconds": 222.228,
    },
    "44c25de7963c45e92a2407ad839f6e8b-Break_20_72.xml.cnf.xz": {
        "gate": "WV100", "rank": 97, "wall_seconds": 134.586,
    },
    "95931bb010dba306e24c79c120044ff9-SC25_Timetable_C_496_E_48_Cl_33_D_7_T_50.normalised.cnf.xz": {
        "gate": "WV2-89", "rank": 168, "wall_seconds": 482.234,
    },
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def load_rows(campaign: Path):
    with (campaign / "matrix.tsv").open(newline="") as source:
        return list(csv.DictReader(source, delimiter="\t"))


def affine_counts(statistics):
    matches = [AFFINE.match(line) for line in statistics]
    matches = [match for match in matches if match]
    if len(matches) != 1:
        return None
    return tuple(map(int, matches[0].groups()))


def guidance_counts(statistics):
    result = {
        "hints_offered": 0,
        "hint_phases": None,
        "hint_replacements": None,
        "hints_applied": 0,
        "target_installs": None,
        "target_attempts": None,
        "target_eligible": None,
        "target_changed": None,
        "target_matching": None,
        "target_replaced": None,
        "target_decisions": None,
        "target_matched_decisions": None,
        "target_overridden_decisions": None,
        "walk_imports": 0,
        "walk_imported_phases": None,
    }
    seen = set()
    for line in statistics:
        match = LOCAL_SEARCH.match(line)
        if match:
            if "localsearch" in seen:
                return None
            seen.add("localsearch")
            values = [None if value is None else int(value)
                      for value in match.groups()]
            keys = (
                "hints_offered", "hint_phases", "hint_replacements",
                "hints_applied", "target_installs", "target_attempts",
                "target_eligible", "target_changed", "target_matching",
                "target_replaced",
            )
            result.update(zip(keys, values))
            continue
        match = LOCAL_PHASE.match(line)
        if match:
            if "localphase" in seen:
                return None
            seen.add("localphase")
            values = list(map(int, match.groups()))
            result.update(zip((
                "target_decisions", "target_matched_decisions",
                "target_overridden_decisions", "native_overwrites",
                "rephase_overwrites", "walk_overwrites",
            ), values))
            continue
        match = WALK_SEED.match(line)
        if match:
            if "walkseed" in seen:
                return None
            seen.add("walkseed")
            values = [None if value is None else int(value)
                      for value in match.groups()]
            result["walk_imports"] = values[0]
            result["walk_imported_phases"] = values[1]
    if "localsearch" not in seen or "walkseed" not in seen:
        return None
    result["observed"] = any((
        result["hints_offered"], result["hints_applied"],
        result["target_installs"] or 0, result["target_decisions"] or 0,
        result["walk_imports"],
    ))
    return result


def performance_reference(instance):
    reference = TRAJECTORY_REFERENCES.get(Path(instance).name)
    if reference is None:
        return None
    reference = dict(reference)
    return reference


def score_campaign(campaign: Path, source_commit: str):
    manifest = json.loads((campaign / "manifest.json").read_text())
    rows = load_rows(campaign)
    errors = []
    arm = manifest.get("arms", {}).get("tactix-auto", {})
    if arm.get("source_commit") != source_commit:
        errors.append("wrong source commit")
    binary = campaign / arm.get("path", "missing")
    if not binary.is_file() or sha256(binary) != arm.get("sha256"):
        errors.append("solver pin missing or hash mismatch")
    expected_array = f"0-{len(rows) - 1}"
    if manifest.get("resource_contract", {}).get("array") != expected_array:
        errors.append("array contract does not match matrix")

    scored = []
    for ordinal, row in enumerate(rows):
        path = campaign / "results" / f"{ordinal:03d}.json"
        item = {
            "task": ordinal,
            "instance": row["instance"],
            "direct_affine": Path(row["instance"]).name in DIRECT_AFFINE,
            "performance_reference": performance_reference(row["instance"]),
            "guidance": None,
        }
        row_errors = []
        if int(row["task"]) != ordinal:
            row_errors.append("non-canonical task order")
        if not path.is_file():
            row_errors.append("missing terminal result")
            item.update(status="pending", errors=row_errors)
            scored.append(item)
            continue
        result = json.loads(path.read_text())
        for field in ("instance", "arm", "expected"):
            if result.get(field) != row[field]:
                row_errors.append(f"{field} mismatch")
        solve = result.get("solve") or {}
        model = result.get("model_check") or {}
        if not (result.get("valid") is True and result.get("verdict") == "SAT"
                and result.get("expected_match") is True
                and solve.get("exit_code") == 10
                and solve.get("timed_out") is False
                and model.get("result") == "verified"):
            row_errors.append("SAT result lacks independent model-checked validity")
        if result.get("binary_sha256") != arm.get("sha256"):
            row_errors.append("result binary hash mismatch")
        counts = affine_counts(result.get("solver_statistics", []))
        guidance = guidance_counts(result.get("solver_statistics", []))
        if item["direct_affine"] and counts != (1, 1):
            row_errors.append(f"direct row affine calls/SAT is {counts!r}, expected (1, 1)")
        reference = item["performance_reference"]
        timing = None
        if reference is not None and solve.get("wall_seconds") is not None:
            baseline = reference["wall_seconds"]
            timing = {
                "delta_seconds": solve["wall_seconds"] - baseline,
                "ratio": solve["wall_seconds"] / baseline,
            }
        if guidance is None:
            row_errors.append("missing or ambiguous guidance telemetry")
        item.update(
            status="pass" if not row_errors else "fail",
            wall_seconds=solve.get("wall_seconds"),
            affine_calls=None if counts is None else counts[0],
            affine_sat=None if counts is None else counts[1],
            timing=timing,
            guidance=guidance,
            errors=row_errors,
        )
        scored.append(item)
    return {
        "campaign": str(campaign),
        "solve_timeout": manifest.get("resource_contract", {}).get("solve_timeout"),
        "rows": scored,
        "errors": errors,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("campaign", nargs="+", type=Path)
    parser.add_argument("--source-commit", default=EXPECTED_COMMIT)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    campaigns = [score_campaign(path.resolve(), args.source_commit)
                 for path in args.campaign]
    rows = [row for campaign in campaigns for row in campaign["rows"]]
    errors = [error for campaign in campaigns for error in campaign["errors"]]
    pending = sum(row["status"] == "pending" for row in rows)
    failed = sum(row["status"] == "fail" for row in rows)
    result = {
        "schema": "tactix-affine-opportunity-recovery-score-v2",
        "source_commit": args.source_commit,
        "campaigns": campaigns,
        "summary": {
            "tasks": len(rows),
            "unique_instances": len({row["instance"] for row in rows}),
            "direct_affine_tasks": sum(row["direct_affine"] for row in rows),
            "direct_affine_unique_instances": len({
                row["instance"] for row in rows if row["direct_affine"]
            }),
            "trajectory_control_tasks": sum(
                row["performance_reference"] is not None for row in rows),
            "guidance_observed_tasks": sum(
                bool((row.get("guidance") or {}).get("observed")) for row in rows),
            "passed": sum(row["status"] == "pass" for row in rows),
            "failed": failed,
            "pending": pending,
            "campaign_errors": errors,
            "accepted": not errors and not failed and not pending,
        },
    }
    text = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(text)
    else:
        sys.stdout.write(text)
    return 0 if result["summary"]["accepted"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
