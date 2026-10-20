#!/usr/bin/env python3
"""Build the representation-first CG collection from the full CMS census."""

import csv
import datetime as dt
import hashlib
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / "tools/bench"))

from cms_tierd import family as lexical_family  # noqa: E402
from cms_tierd_cpuq import independently_checked, verified_saved_arms  # noqa: E402
from frozen_baseline import sha256  # noqa: E402

SCREENING = ROOT / "benchmarks/runs/20261018-cms-tierd-proof-free-4core-5000s"
WALL_REPAIR = ROOT / "benchmarks/runs/20261018-cms-tierd-proof-free-4core-5000s-wall-repair"
VERIFICATION_CAMPAIGNS = [
    ROOT / "benchmarks/runs/20261017-cms-tierd-cpuq-anon",
    ROOT / "benchmarks/runs/20261017-cms-tierd-cpuq-workspace-repair",
    ROOT / "benchmarks/runs/20261018-cms-tierd-cpuq-workspace-repair-v2",
]
SAVED = ROOT / "benchmarks/reports/2026-10-11-tierd-per-instance/results.csv"
CACHE_HASHES = ROOT / "benchmarks/baselines/sc24-26-32g-5000s/cache.sha256"

# Twenty-five is a soft budget, not a quota. These 23 rows maximize marginal
# family/mechanism coverage while keeping at most three rows from a family.
# The three MDP rows deliberately cover the 28, 32 and 36 size regimes.
SELECTED = [
    1107,                         # dltm
    550, 103, 974,               # folkman
    83, 609,                     # lightsout: random and corner
    284, 330, 123,               # mdp: sizes 28, 32 and 36
    898,                         # mp1; task 419 is a near-duplicate
    649,                         # nb13t165
    478, 162, 62,                # par32
    378,                         # vdw alias normalized to vanderwaerden
    525,                         # x9
    1122, 723, 353,              # ptn, bp4, crusti
    234, 256, 482, 625,          # st, pcmax, stable, p160
]
UNSAT = {83, 609, 284, 330, 123}
NORMALIZED_FAMILY = {"vdw": "vanderwaerden"}
MECHANISM = {
    "dltm": "social-network/graph SAT search",
    "folkman": "Folkman graph coloring",
    "lightsout": "GF(2) linear-system UNSAT",
    "mdp": "GF(2) matrix UNSAT",
    "mp1": "propagation/watch-pose-sensitive SAT",
    "nb13t165": "family-distinct structured SAT",
    "par32": "parity-heavy SAT",
    "vanderwaerden": "Van der Waerden coloring",
    "x9": "sub-second structured SAT sentinel",
    "ptn": "family-distinct structured SAT",
    "bp4": "family-distinct combinatorial SAT",
    "crusti": "family-distinct structured SAT",
    "st": "phase/local-search-sensitive SAT",
    "pcmax": "scheduling/cardinality SAT",
    "stable": "stable-set graph SAT",
    "p160": "dense p160-family SAT",
}
RATIONALE = {
    83: "lightsout random instance",
    609: "lightsout corner instance; exclusive solve",
    284: "mdp size-28 representative",
    330: "mdp size-32 representative; exclusive solve",
    123: "mdp size-36 representative; exclusive solve",
    478: "par32 shuffled-c variant",
    162: "par32 shuffled variant",
    62: "par32 SAT03-shuffled variant",
    378: "stronger full-census replacement for the old Van der Waerden row",
    234: "new family; phase/local-search-sensitive witness",
    256: "new family; exclusive scheduling instance",
    482: "new family discovered only by the completed wall-time continuation",
    625: "new family; exclusive dense instance",
}

BASELINE_ORDER = [
    "tactix-0f7c10174", "tactix-no-mab", "kissat-4.0.4",
    "kissat-sc2024", "ae-kissat-mab", "satsuma-kissat", "lymphosat",
]


def result_inventory(paths):
    digest = hashlib.sha256()
    for path in sorted(paths):
        digest.update(f"{path.name} {sha256(path)}\n".encode())
    return digest.hexdigest()


def load_cache_hashes():
    rows = {}
    for line in CACHE_HASHES.read_text().splitlines():
        digest, rel = line.split(None, 1)
        rows[rel.strip()] = digest
    return rows


def normalized_family(instance):
    raw = lexical_family(instance)
    return NORMALIZED_FAMILY.get(raw, raw)


def proof_free_inventory():
    screening_manifest = json.loads((SCREENING / "manifest.json").read_text())
    repair_manifest = json.loads((WALL_REPAIR / "manifest.json").read_text())
    binding = repair_manifest["repair_of"]
    screening_paths = sorted((SCREENING / "results").glob("*.json"))
    if (binding["campaign"] != SCREENING.name or
            binding["manifest_sha256"] != sha256(SCREENING / "manifest.json") or
            binding["published_result_count"] != len(screening_paths) or
            binding["published_results_inventory_sha256"] != result_inventory(screening_paths)):
        raise RuntimeError("wall-time repair no longer binds the screening inventory")
    reused = set(binding["reused_source_tasks"])
    rerun = set(binding["rerun_source_tasks"])
    if reused & rerun or reused | rerun != set(range(1166)):
        raise RuntimeError("proof-free census is not an exact Tier-D partition")
    if len(screening_manifest["instances"]) != 1166:
        raise RuntimeError("screening manifest is not full Tier D")

    inventory = {}
    for source_task in reused:
        inventory[source_task] = (SCREENING, screening_manifest,
                                  SCREENING / "results" / f"{source_task:04d}.json")
    if len(repair_manifest["instances"]) != len(rerun):
        raise RuntimeError("wall-time repair row count changed")
    for local_task, row in enumerate(repair_manifest["instances"]):
        source_task = row["source_task"]
        if source_task not in rerun:
            raise RuntimeError("repair result lies outside its declared partition")
        inventory[source_task] = (WALL_REPAIR, repair_manifest,
                                  WALL_REPAIR / "results" / f"{local_task:04d}.json")
    if set(inventory) != set(range(1166)) or any(not item[2].is_file()
                                                 for item in inventory.values()):
        raise RuntimeError("proof-free census is incomplete")

    counts = Counter()
    source_lines = []
    for source_task, (_campaign, _manifest, path) in sorted(inventory.items()):
        result = json.loads(path.read_text())
        expected = screening_manifest["instances"][source_task]
        actual_source = result.get("source_task", result.get("task"))
        if (actual_source != source_task or result.get("instance") != expected["rel"] or
                result.get("expected") != expected["expected"] or
                result.get("schema") != "cms-proof-free-tierd-result-v1" or
                result.get("proof_production") is not False or
                result.get("proof_path") is not None or result.get("error") or
                result.get("cleanup_pending") or result.get("cnf_deleted") is not True or
                result.get("scratch_deleted") is not True):
            raise RuntimeError(f"invalid proof-free result contract: {path}")
        verdict = result.get("verdict")
        solve = result.get("solve") or {}
        if verdict == "SAT":
            check = result.get("model_check") or {}
            if not (result.get("valid") and result.get("expected_match") and
                    result.get("verification") == "model-checked" and
                    check.get("verified") and check.get("exit_code") == 0 and
                    not check.get("timed_out") and not solve.get("timed_out")):
                raise RuntimeError(f"invalid model-checked SAT row: {path}")
            counts["model-checked-SAT"] += 1
        elif verdict == "UNSAT":
            if result.get("valid") is not False or result.get("verification") != "unchecked-proof-free":
                raise RuntimeError(f"proof-free UNSAT was mislabeled: {path}")
            counts["unchecked-UNSAT"] += 1
        elif verdict == "UNKNOWN":
            if result.get("valid") is not False:
                raise RuntimeError(f"UNKNOWN row was marked valid: {path}")
            if solve.get("timed_out"):
                counts["wall-timeout"] += 1
            elif solve.get("exit_code") == -9:
                counts["solver-killed"] += 1
            else:
                counts["solver-crash-or-unknown"] += 1
        else:
            raise RuntimeError(f"unexpected proof-free verdict: {path}")
        source_lines.append(f"{source_task} {path.relative_to(ROOT)} {sha256(path)}\n")
    expected_counts = {"model-checked-SAT": 395, "unchecked-UNSAT": 366,
                       "wall-timeout": 383, "solver-killed": 21,
                       "solver-crash-or-unknown": 1}
    if counts != expected_counts:
        raise RuntimeError(f"unexpected terminal census counts: {counts}")
    digest = hashlib.sha256("".join(source_lines).encode()).hexdigest()
    return inventory, screening_manifest, counts, digest


def checked_unsat_inventory():
    inventory = {}
    for campaign in VERIFICATION_CAMPAIGNS:
        manifest = json.loads((campaign / "manifest.json").read_text())
        for path in sorted((campaign / "results").glob("*.json")):
            task = int(path.stem)
            if task in inventory:
                raise RuntimeError(f"duplicate checked CMS result for Tier-D ID {task}")
            inventory[task] = (campaign, manifest, path, json.loads(path.read_text()))
    return inventory


def cell(row):
    seconds = float(row["solve_wall_s"])
    if row["verified"] == "True":
        return f"{seconds:.3f} {row['verdict'][0]}✓"
    if row["solve_timed_out"] == "True" or row["status"] == "timeout":
        return f"{seconds:.0f} TO"
    if row["verdict"] in {"SAT", "UNSAT"}:
        return f"{seconds:.3f} {row['verdict'][0]}?"
    return f"{seconds:.3f} {row['status']}"


def main():
    if len(SELECTED) != 23 or len(set(SELECTED)) != len(SELECTED):
        raise RuntimeError("CG selection must contain 23 distinct Tier-D IDs")
    proof_free, tierd_manifest, census_counts, census_inventory = proof_free_inventory()
    checked_unsat = checked_unsat_inventory()
    cache_hashes = load_cache_hashes()
    saved = defaultdict(list)
    with SAVED.open(newline="") as stream:
        for row in csv.DictReader(stream):
            saved[row["instance"]].append(row)

    entries = []
    runtime_rows = []
    for task in SELECTED:
        campaign, _manifest, result_path = proof_free[task]
        result = json.loads(result_path.read_text())
        expected = tierd_manifest["instances"][task]
        if result["verdict"] not in {"SAT", "UNSAT"} or result["expected_match"] is not True:
            raise RuntimeError(f"selected row {task} is not a completed expected solve")
        if (task in UNSAT) != (result["verdict"] == "UNSAT"):
            raise RuntimeError(f"selected outcome declaration changed for {task}")

        verification_campaign = campaign
        verification_path = result_path
        verification = result
        certificate_sha256 = None
        independent_check = "checkmodel"
        if task in UNSAT:
            if task not in checked_unsat:
                raise RuntimeError(f"selected UNSAT row {task} lacks proof-producing evidence")
            verification_campaign, verification_manifest, verification_path, verification = checked_unsat[task]
            if not independently_checked(verification, verification_manifest):
                raise RuntimeError(f"selected UNSAT row {task} lacks an independent proof check")
            if verification["instance"] != result["instance"] or verification["verdict"] != "UNSAT":
                raise RuntimeError(f"UNSAT evidence identity mismatch for {task}")
            certificate_sha256 = verification["certificate_sha256"]
            independent_check = "frat-xor then cake-xlrup"

        seconds = float(result["solve"]["wall_seconds"])
        if seconds > 600:
            raise RuntimeError(f"selected row {task} exceeds the 600-second target ceiling")
        old = verified_saved_arms(result, saved[result["instance"]])
        fastest = min((float(row["solve_wall_s"]) for row in old), default=None)
        speedup = fastest / max(seconds, 0.001) if fastest is not None else None
        kind = ("exclusive" if fastest is None else
                (">=10x" if speedup >= 10 else "diversity"))
        fam = normalized_family(result["instance"])
        source_fam = lexical_family(result["instance"])
        input_path = ROOT / "benchmarks/cache" / result["instance"]
        entry = {
            "tier_d_id": task,
            "family": fam,
            "source_family": source_fam,
            "mechanism_stratum": MECHANISM[fam],
            "selection_rationale": RATIONALE.get(task, "retained distinct family or capped family representative"),
            "instance": result["instance"],
            "expected": expected["expected"],
            "verdict": result["verdict"],
            "cms_seconds": seconds,
            "class": kind,
            "speedup_over_fastest_verified_saved_arm": round(speedup, 6) if speedup is not None else None,
            "cms_campaign": campaign.name,
            "cms_result": result_path.relative_to(ROOT).as_posix(),
            "cms_result_sha256": sha256(result_path),
            "cms_proof_production": False,
            "verification_campaign": verification_campaign.name,
            "verification_result": verification_path.relative_to(ROOT).as_posix(),
            "verification_result_sha256": sha256(verification_path),
            "certificate_sha256": certificate_sha256,
            "independent_check": independent_check,
            "generated_artifacts_deleted": True,
            "compressed_input": f"benchmarks/cache/{result['instance']}",
            "compressed_input_sha256": cache_hashes[result["instance"]],
            "compressed_input_present": input_path.is_file(),
        }
        entries.append(entry)

        runtime_rows.append({
            "tier_d_id": task, "family": fam, "instance": result["instance"],
            "solver": "cryptominisat-5.14.7", "source_campaign": campaign.name,
            "status": "verified", "verdict": result["verdict"], "verified": "True",
            "solve_wall_s": f"{seconds:.3f}", "solve_timed_out": "False",
            "comparison_note": ("full proof-free 4-core wall-time census; model checked" if
                                result["verdict"] == "SAT" else
                                "full proof-free 4-core wall-time census; UNSAT independently proved in bound verification result"),
        })
        old_by_solver = {row["solver"]: row for row in saved[result["instance"]]}
        if set(old_by_solver) != set(BASELINE_ORDER):
            raise RuntimeError(f"Tier-D ID {task} does not have seven saved arms")
        for solver in BASELINE_ORDER:
            row = old_by_solver[solver]
            runtime_rows.append({
                "tier_d_id": task, "family": fam, "instance": result["instance"],
                "solver": solver, "source_campaign": row["source_campaign"],
                "status": row["status"], "verdict": row["verdict"],
                "verified": row["verified"], "solve_wall_s": row["solve_wall_s"],
                "solve_timed_out": row["solve_timed_out"],
                "comparison_note": "saved historical arm; solve wall time; resource/proof configuration may differ",
            })

    families = Counter(entry["family"] for entry in entries)
    if max(families.values()) > 3 or len(families) != 16:
        raise RuntimeError(f"CG representation contract changed: {families}")
    if Counter(entry["verdict"] for entry in entries) != {"SAT": 18, "UNSAT": 5}:
        raise RuntimeError("CG outcome coverage changed")

    prior = HERE / "manifest.json"
    old = json.loads(prior.read_text()) if prior.is_file() else {}
    refreshed_at = old.get("refreshed_at", dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds"))
    manifest = {
        "schema": "tactix-cg-v2",
        "name": "CG",
        "refreshed_at": refreshed_at,
        "count": len(entries),
        "soft_budget": 25,
        "purpose": "representation-first checked CryptoMiniSat strengths for Tactix DefaultPolicy development",
        "selection": {
            "source": "complete pinned proof-free four-core 5000-wall-second Tier-D census",
            "published_rows_screened": 1166,
            "tier_d_rows": 1166,
            "solve_ceiling_seconds": 600,
            "rule": "maximize normalized family and mechanism breadth, checked outcome and runtime strata; cap families at three; omit near-duplicates",
            "census_complete": True,
            "family_cap": 3,
            "normalized_aliases": NORMALIZED_FAMILY,
            "excluded_near_duplicate": {"419": "second mp1 row; task 898 is the stronger family representative"},
            "removed_from_v1": {
                "240": "case11 no longer a CMS-over-Tactix development gap in the complete census",
                "799": "1x1x13 timed out in the corrected wall-time census",
                "114,253,183,159,388": "redundant mdp rows; retain one representative each at sizes 28, 32 and 36",
                "352": "replaced by faster major-win Van der Waerden task 378",
            },
        },
        "census_counts": dict(census_counts),
        "resource_contract": {"partition": "cpu-q", "cpus": 4, "memory_gib": 32,
                              "solve_wall_seconds": 5000, "proof_production": False},
        "pins": {
            "proof_free_campaign_manifests": {
                SCREENING.name: sha256(SCREENING / "manifest.json"),
                WALL_REPAIR.name: sha256(WALL_REPAIR / "manifest.json"),
            },
            "proof_free_combined_inventory_sha256": census_inventory,
            "unsat_verification_campaign_manifests": {
                name: sha256(ROOT / "benchmarks/runs" / name / "manifest.json")
                for name in sorted({entry["verification_campaign"] for entry in entries
                                    if entry["verdict"] == "UNSAT"})
            },
            "saved_seven_arm_comparison": {
                "path": SAVED.relative_to(ROOT).as_posix(), "sha256": sha256(SAVED)
            },
            "cache_manifest": {
                "path": CACHE_HASHES.relative_to(ROOT).as_posix(), "sha256": sha256(CACHE_HASHES)
            },
        },
        "entries": entries,
    }
    (HERE / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")

    fields = ["tier_d_id", "family", "instance", "solver", "source_campaign",
              "status", "verdict", "verified", "solve_wall_s", "solve_timed_out",
              "comparison_note"]
    with (HERE / "baseline-runtimes.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(runtime_rows)
    with (HERE / "instances.list").open("w") as stream:
        stream.write("# Tier-D-ID expected compressed-input-sha256 instance\n")
        for entry in entries:
            stream.write(f"{entry['tier_d_id']} {entry['expected']} "
                         f"{entry['compressed_input_sha256']} {entry['instance']}\n")

    by_instance = defaultdict(dict)
    for row in runtime_rows:
        by_instance[row["instance"]][row["solver"]] = row
    lines = [
        "# CG: CryptoMiniSat-guided benchmark collection", "",
        "CG contains 23 independently checked Tier-D instances selected from the complete pinned CryptoMiniSat 5.14.7 four-core, 5,000-wall-second census. It is a representation-first development target for bringing one instance-independent Tactix DefaultPolicy within 2× the checked CMS time.", "",
        "Twenty-five is a soft budget, not a quota. The refresh removes redundant rows and keeps 23 instances spanning 16 normalized families. No family contributes more than three rows; the three `mdp` rows represent sizes 28, 32, and 36. The set covers 18 model-checked SAT and five independently proved UNSAT instances, major/exclusive wins and useful diversity cases, and sub-second through 100+ second CMS targets.", "",
        "All performance rows come from the proof-free census. SAT models were checked in that run. Proof-free UNSAT reports were admitted only where a separately hash-bound proof-producing CMS result independently verified the same compressed input. The qualitative mechanism labels are selection strata, not claims about CryptoMiniSat's internal causal owner.", "",
        "No CNF is duplicated here. `instances.list` and `manifest.json` reference compressed files in `benchmarks/cache/tierD24-26`; generated proofs, models, and expanded CNFs were deleted after checking.", "",
        "## Family coverage", "", "| Family | Rows |", "| --- | ---: |",
    ]
    for name, count in sorted(families.items(), key=lambda pair: (-pair[1], pair[0])):
        lines.append(f"| `{name}` | {count} |")
    lines += ["", "## Representation strata", "",
              "| ID | Family | Outcome | Stratum | Selection reason |",
              "| ---: | --- | --- | --- | --- |"]
    for entry in entries:
        lines.append(f"| {entry['tier_d_id']} | `{entry['family']}` | {entry['verdict']} | {entry['mechanism_stratum']} | {entry['selection_rationale']} |")
    lines += ["", "## Solve-wall-time comparison", "",
              "Times are seconds. `S✓` and `U✓` are independently checked SAT and UNSAT. CMS used four cores, 32 GiB, no proof production, and a true 5,000-second wall limit. The seven saved arms have differing placement/proof details, so small differences are not causal comparisons.", "",
              "| ID | Family | CMS | Tactix | Tactix no-MAB | Kissat 4 | Kissat SC24 | AE-Kissat | Satsuma | Lymphosat |",
              "| ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |"]
    for entry in entries:
        rows = by_instance[entry["instance"]]
        values = [cell(rows[solver]) for solver in BASELINE_ORDER]
        lines.append(f"| {entry['tier_d_id']} | `{entry['family']}` | {entry['cms_seconds']:.3f} {entry['verdict'][0]}✓ | " + " | ".join(values) + " |")
    classes = Counter(entry["class"] for entry in entries)
    lines += ["", "## Pattern", "",
              f"The collection has {classes['exclusive']} CMS-exclusive checked solves, {classes['>=10x']} at-least-10× wins, and {classes['diversity']} diversity rows. The completed census adds `p160`, `pcmax`, `st`, and `stable`; normalizes `vdw` into `vanderwaerden`; and excludes a second `mp1` row as a near-duplicate.", "",
              "## Reproduce and verify", "", "```bash",
              "python3 benchmarks/collections/CG/build.py",
              "python3 benchmarks/collections/CG/verify.py --hash-inputs", "```", "",
              f"`baseline-runtimes.csv` contains all {len(runtime_rows)} solver-instance rows and their provenance caveat.", ""]
    (HERE / "README.md").write_text("\n".join(lines))
    print(f"wrote CG with {len(entries)} instances, {len(families)} families, "
          f"and {len(runtime_rows)} runtime rows from the complete census")


if __name__ == "__main__":
    main()
