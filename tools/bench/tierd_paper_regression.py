#!/usr/bin/env python3
"""Freeze and score full Tier D against the preserved paper release.

The paper's 32 GiB campaign and its seven-row 64 GiB checker recheck form one
target.  A candidate must retain every paper solve and verification, preserve
all correctness/resource/cleanup contracts, and stay within ten percent of
both raw-solver and independently-verified PAR-2.
"""

import argparse
from collections import Counter
import csv
import hashlib
import json
import math
from pathlib import Path

from experimental_tierd import check_failure_class
from fixed_runtime import require, sha


ROOT = Path(__file__).resolve().parents[2]
PAPER_COMMIT = "0f7c10174faa13e41e3ab60654bf8fc3322f4041"
PAPER_BINARY = "a7c66babcd568a3497e0b46271b0d25941c59042333b252527f63e40247a4907"
PAPER_BASE = ROOT / "benchmarks/runs/20261010-tierd-fastel"
PAPER_RECHECK = ROOT / "benchmarks/runs/20261011-tierd-fastel-recheck-64g"
BASELINE = ROOT / "benchmarks/baselines/tierd-paper-v1/results.json"
RUNTIME_FACTOR = 1.10


def result_inventory(paths):
    digest = hashlib.sha256()
    for path in sorted(paths):
        digest.update(f"{path.name} {sha(path)}\n".encode())
    return digest.hexdigest()


def read_matrix(campaign):
    with (campaign / "matrix.tsv").open(newline="") as source:
        return list(csv.DictReader(source, delimiter="\t"))


def verify_manifest(campaign):
    manifest_path = campaign / "manifest.json"
    manifest = json.loads(manifest_path.read_text())
    for relative, digest in manifest["pin_sha256"].items():
        require(sha(campaign / relative) == digest,
                f"pin changed: {campaign.name}/{relative}")
    return manifest


def recheck_campaigns(recheck):
    """Normalize one or more direct rechecks in overlay order."""
    if recheck is None:
        return []
    if isinstance(recheck, (str, Path)):
        return [Path(recheck)]
    return list(recheck)


def checked_result(row, result, contract):
    """Return (status, solve seconds, errors) for one immutable result."""
    errors = []
    for key in ("instance", "expected"):
        if result.get(key) != row[key]:
            errors.append("result identity mismatch: " + key)
    if (result.get("error") or result.get("cleanup_pending") or
            result.get("artifact_deleted") is not True or
            result.get("derived_artifacts_deleted") is not True):
        errors.append("infrastructure error or incomplete scratch cleanup")
    solve = result.get("solve") or {}
    seconds = solve.get("wall_seconds")
    if (not isinstance(seconds, (int, float)) or not math.isfinite(seconds) or
            seconds < 0 or seconds > contract["solve_timeout"] + 1):
        errors.append("solve-time contract violated")
        seconds = None
    if solve.get("timed_out"):
        if (result.get("verdict") != "UNKNOWN" or solve.get("exit_code") is not None or
                result.get("model_check") is not None or result.get("proof_check") is not None):
            errors.append("invalid solver-timeout evidence")
        return "solver-timeout", seconds, errors

    verdict = result.get("verdict")
    rss = solve.get("peak_rss_kb")
    if (not isinstance(rss, (int, float)) or not math.isfinite(rss) or
            rss <= 0 or rss > contract["memory_mib"] * 1024):
        errors.append("memory contract violated or measurement missing")
    if (verdict == "UNKNOWN" and solve.get("exit_code") == 137 and
            result.get("model_check") is None and result.get("proof_check") is None):
        return "solver-memout", seconds, errors
    if (verdict not in ("SAT", "UNSAT") or
            solve.get("exit_code") != {"SAT": 10, "UNSAT": 20}.get(verdict)):
        errors.append("solver failure or inconsistent exit/verdict")
        return "solver-failure", seconds, errors
    if row["expected"] not in (verdict, "UNKNOWN") or result.get("expected_match") is not True:
        errors.append("incorrect verdict")
    if verdict == "SAT":
        check = result.get("model_check") or {}
        if (result.get("valid") is not True or check.get("result") != "verified" or
                check.get("exit_code") != 0 or check.get("timed_out")):
            errors.append("SAT model not independently verified")
            return "invalid-SAT", seconds, errors
        return "verified-SAT", seconds, errors

    check = result.get("proof_check") or {}
    check_seconds = check.get("wall_seconds")
    if (not isinstance(check_seconds, (int, float)) or
            not math.isfinite(check_seconds) or check_seconds < 0 or
            check_seconds > contract["checker_timeout"] + 1):
        errors.append("proof-check time contract violated")
    if result.get("valid") is True and check.get("result") == "verified" and not check.get("timed_out"):
        stages = check.get("stages") or []
        if not stages or any(stage.get("result") != "verified" for stage in stages):
            errors.append("verified proof lacks verified stage evidence")
            return "invalid-UNSAT", seconds, errors
        endings = {"dsr-as-drat-drat-trim-cake_lpr": "cake_lpr",
                   "dsr-trim-dsr-compress-trestle": "srcheck",
                   "veripb-cake-pb": "cake-pb-cnf"}
        if stages[-1].get("name") != endings.get(check.get("pipeline")):
            errors.append("verified proof lacks final independent verifier")
            return "invalid-UNSAT", seconds, errors
        return "verified-UNSAT", seconds, errors
    if result.get("valid") is True:
        errors.append("censored proof mislabeled valid")
    failure = check_failure_class(check)
    if failure not in ("timeout", "memout"):
        errors.append("UNSAT proof rejected or checker failed unclearly")
        return "invalid-UNSAT", seconds, errors
    return "censored-UNSAT-" + failure, seconds, errors


def substituted_results(campaign, recheck=None):
    manifest = verify_manifest(campaign)
    require(list(manifest.get("arms", {})) == ["tactix"],
            "campaign must contain exactly the tactix arm")
    arm = manifest["arms"]["tactix"]
    require(arm.get("arguments") == [] and
            arm.get("certificate") == "pbp",
            "candidate whole-run PBP contract changed")
    matrix = read_matrix(campaign)
    require(len(matrix) == 1166, "full Tier D must contain 1,166 rows")
    require([int(row["task"]) for row in matrix] == list(range(1166)),
            "full Tier D task order changed")
    results = {}
    for row in matrix:
        task = int(row["task"])
        path = campaign / "results" / f"{task:03d}.json"
        if path.is_file():
            result = json.loads(path.read_text())
            require(result.get("task") == task and int(result.get("rank")) == int(row["rank"]),
                    "base result task/rank identity mismatch")
            require(result.get("arm") == "tactix" and
                    result.get("binary_sha256") == arm["sha256"],
                    "base result solver identity mismatch")
            results[task] = result
    for recheck in recheck_campaigns(recheck):
        re_manifest = verify_manifest(recheck)
        authority = re_manifest.get("recheck_source") or {}
        require(authority.get("manifest_sha256") == sha(campaign / "manifest.json"),
                "recheck does not bind candidate campaign")
        require(re_manifest["resource_contract"]["memory_mib"] == 65536 and
                re_manifest["resource_contract"]["solve_timeout"] == 5000 and
                re_manifest["resource_contract"]["checker_timeout"] == 28800,
                "recheck resource contract changed")
        require(re_manifest.get("arms") == manifest.get("arms"),
                "recheck solver identity changed")
        re_matrix = read_matrix(recheck)
        for row in re_matrix:
            local = int(row["task"])
            source_task = int(row["source_task"])
            source_path = campaign / "results" / f"{source_task:03d}.json"
            expected_hash = authority.get("selected_result_sha256", {}).get(
                f"results/{source_task:03d}.json")
            require(expected_hash and sha(source_path) == expected_hash,
                    "recheck source result changed")
            path = recheck / "results" / f"{local:03d}.json"
            require(path.is_file(), f"recheck result missing: {recheck.name}/{local:03d}")
            result = json.loads(path.read_text())
            require(result.get("task") == local and
                    result.get("instance") == row["instance"],
                    "recheck result identity mismatch")
            require(result.get("arm") == "tactix" and
                    result.get("binary_sha256") == arm["sha256"],
                    "recheck result solver identity mismatch")
            results[source_task] = result
    return manifest, matrix, results


def collect_paper_target():
    manifest, matrix, results = substituted_results(PAPER_BASE, PAPER_RECHECK)
    arm = next(iter(manifest["arms"].values()))
    require(manifest["source_commit"] == PAPER_COMMIT and arm["sha256"] == PAPER_BINARY,
            "paper release identity changed")
    require(len(results) == 1166, "paper target results incomplete")
    recheck_matrix = read_matrix(PAPER_RECHECK)
    recheck_by_source = {int(row["source_task"]): int(row["task"])
                         for row in recheck_matrix}
    rows, raw, verified = [], [], []
    counts = Counter()
    for row in matrix:
        task = int(row["task"])
        result = results[task]
        contract = (json.loads((PAPER_RECHECK / "manifest.json").read_text())["resource_contract"]
                    if task in recheck_by_source
                    else manifest["resource_contract"])
        status, seconds, errors = checked_result(row, result, contract)
        require(not errors, f"paper target row {task} invalid: {errors}")
        solved = status.startswith(("verified-", "censored-"))
        valid = status.startswith("verified-")
        raw.append(seconds if solved else 10000)
        verified.append(seconds if valid else 10000)
        counts[status] += 1
        source_campaign = PAPER_RECHECK if task in recheck_by_source else PAPER_BASE
        source_task = recheck_by_source.get(task, task)
        source_path = source_campaign / "results" / f"{source_task:03d}.json"
        rows.append({
            "task": task, "rank": int(row["rank"]), "instance": row["instance"],
            "expected": row["expected"], "status": status,
            "solved": solved, "independently_verified": valid,
            "solve_wall_seconds": seconds,
            "source": str(source_path.relative_to(ROOT)),
            "source_sha256": sha(source_path),
        })
    raw_par2 = round(math.fsum(raw), 6)
    verified_par2 = round(math.fsum(verified), 6)
    return {
        "schema": "tactix-tierd-paper-regression-baseline-v1",
        "target_source_commit": PAPER_COMMIT,
        "target_solver_sha256": PAPER_BINARY,
        "runtime_tolerance_fraction": 0.10,
        "source_base_manifest": str((PAPER_BASE / "manifest.json").relative_to(ROOT)),
        "source_base_manifest_sha256": sha(PAPER_BASE / "manifest.json"),
        "source_base_results_inventory_sha256": result_inventory(
            (PAPER_BASE / "results").glob("*.json")),
        "source_recheck_manifest": str((PAPER_RECHECK / "manifest.json").relative_to(ROOT)),
        "source_recheck_manifest_sha256": sha(PAPER_RECHECK / "manifest.json"),
        "source_recheck_results_inventory_sha256": result_inventory(
            (PAPER_RECHECK / "results").glob("*.json")),
        "resource_contract": manifest["resource_contract"],
        "matrix_sha256": sha(PAPER_BASE / "matrix.tsv"),
        "counts": dict(counts),
        "paper_raw_solver_par2": raw_par2,
        "paper_verified_par2": verified_par2,
        "acceptance_raw_solver_par2": round(raw_par2 * RUNTIME_FACTOR, 6),
        "acceptance_verified_par2": round(verified_par2 * RUNTIME_FACTOR, 6),
        "rows": rows,
    }


def load_verified_baseline(path=BASELINE):
    expected = collect_paper_target()
    require(json.loads(path.read_text()) == expected,
            "frozen paper Tier-D target differs from source evidence")
    return expected


def score(campaign, recheck=None, baseline_path=BASELINE):
    baseline = load_verified_baseline(baseline_path)
    rechecks = recheck_campaigns(recheck)
    manifest, matrix, results = substituted_results(campaign, rechecks)
    contract = manifest["resource_contract"]
    for key, value in (("partition", "cpu-q"), ("cpus_per_task", 4),
                       ("memory_mib", 32768), ("solve_timeout", 5000),
                       ("checker_timeout", 28800)):
        require(contract[key] == value, "candidate resource mismatch: " + key)
    require(sha(campaign / "matrix.tsv") == baseline["matrix_sha256"],
            "candidate Tier-D matrix differs from paper target")
    target = {row["task"]: row for row in baseline["rows"]}
    counts, failures, missing, raw, verified = Counter(), [], [], [], []
    recheck_contracts = {}
    for recheck_campaign in rechecks:
        re_contract = json.loads((recheck_campaign / "manifest.json").read_text())["resource_contract"]
        for recheck_row in read_matrix(recheck_campaign):
            recheck_contracts[int(recheck_row["source_task"])] = re_contract
    for row in matrix:
        task = int(row["task"])
        if task not in results:
            missing.append(task)
            raw.append(10000); verified.append(10000)
            continue
        row_contract = recheck_contracts.get(task, contract)
        status, seconds, errors = checked_result(row, results[task], row_contract)
        counts[status] += 1
        solved = status.startswith(("verified-", "censored-"))
        valid = status.startswith("verified-")
        raw.append(seconds if solved else 10000)
        verified.append(seconds if valid else 10000)
        if target[task]["solved"] and not solved:
            errors.append("lost paper solve")
        if target[task]["independently_verified"] and not valid:
            errors.append("lost paper independent verification")
        if errors:
            failures.append({"task": task, "instance": row["instance"],
                             "status": status, "errors": errors})
    raw_par2 = math.fsum(raw)
    verified_par2 = math.fsum(verified)
    if not missing and raw_par2 > baseline["acceptance_raw_solver_par2"]:
        failures.append({"errors": ["raw solver PAR-2 exceeds paper plus ten percent"]})
    if not missing and verified_par2 > baseline["acceptance_verified_par2"]:
        failures.append({"errors": ["verified PAR-2 exceeds paper plus ten percent"]})
    return {
        "schema": "tactix-tierd-paper-regression-score-v1",
        "campaign": str(campaign.relative_to(ROOT)),
        "campaign_manifest_sha256": sha(campaign / "manifest.json"),
        "scorer_sha256": sha(Path(__file__)),
        "recheck_campaign": (str(rechecks[0].relative_to(ROOT))
                             if len(rechecks) == 1 else None),
        "recheck_campaigns": [str(path.relative_to(ROOT)) for path in rechecks],
        "baseline_sha256": sha(baseline_path),
        "target_source_commit": baseline["target_source_commit"],
        "runtime_tolerance_fraction": 0.10,
        "accepted": not missing and not failures,
        "expected_rows": 1166, "published_rows": len(results),
        "missing": missing, "counts": dict(counts), "failures": failures,
        "raw_solver_par2": round(raw_par2, 6),
        "raw_solver_par2_ceiling": baseline["acceptance_raw_solver_par2"],
        "verified_par2": round(verified_par2, 6),
        "verified_par2_ceiling": baseline["acceptance_verified_par2"],
        "paper_raw_solver_par2": baseline["paper_raw_solver_par2"],
        "paper_verified_par2": baseline["paper_verified_par2"],
        "limitation": "The seven paper solver successes without independent proof verification remain explicit censored target rows.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    freeze = commands.add_parser("freeze")
    freeze.add_argument("--output", type=Path, default=BASELINE)
    verify = commands.add_parser("verify")
    verify.add_argument("--baseline", type=Path, default=BASELINE)
    scoring = commands.add_parser("score")
    scoring.add_argument("campaign", type=Path)
    scoring.add_argument("--recheck", type=Path, action="append", default=[],
                         help="repeatable direct recheck; later campaigns override overlapping source tasks")
    scoring.add_argument("--baseline", type=Path, default=BASELINE)
    scoring.add_argument("--output", type=Path)
    args = parser.parse_args()
    if args.command == "freeze":
        require(not args.output.exists(), "baseline output already exists")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(collect_paper_target(), indent=2,
                                          sort_keys=True) + "\n")
        print(args.output)
        return 0
    if args.command == "verify":
        baseline = load_verified_baseline(args.baseline)
        print(f"Verified paper Tier-D target: {len(baseline['rows'])} rows; "
              f"verified PAR-2 {baseline['paper_verified_par2']:.3f}.")
        return 0
    result = score(args.campaign.resolve(),
                   [path.resolve() for path in args.recheck],
                   args.baseline.resolve())
    rendered = json.dumps(result, indent=2) + "\n"
    if args.output:
        require(not args.output.exists(), "score output already exists")
        args.output.write_text(rendered)
    print(rendered, end="")
    return 0 if result["accepted"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
