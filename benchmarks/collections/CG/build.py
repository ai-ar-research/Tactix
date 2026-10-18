#!/usr/bin/env python3
"""Build the frozen 25-instance CG collection from checked CMS rerun rows."""

import csv
import datetime as dt
import hashlib
import json
import math
import sys
from collections import Counter, defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / "tools/bench"))

from cms_tierd import family  # noqa: E402
from cms_tierd_cpuq import independently_checked, verified_saved_arms  # noqa: E402
from frozen_baseline import sha256  # noqa: E402

CAMPAIGNS = [
    ROOT / "benchmarks/runs/20261017-cms-tierd-cpuq-anon",
    ROOT / "benchmarks/runs/20261017-cms-tierd-cpuq-workspace-repair",
    ROOT / "benchmarks/runs/20261018-cms-tierd-cpuq-workspace-repair-v2",
]
SAVED = ROOT / "benchmarks/reports/2026-10-11-tierd-per-instance/results.csv"
CACHE_HASHES = ROOT / "benchmarks/baselines/sc24-26-32g-5000s/cache.sha256"

# All independently checked exclusive or >=10x wins below 600 seconds, followed
# by the best checked CMS-over-baseline row from four previously absent families.
CORE = [240, 1107, 550, 103, 974, 83, 609, 284, 114, 253, 330, 183, 159,
        388, 898, 649, 478, 162, 62, 352, 525]
DIVERSITY = [799, 1122, 723, 353]
SELECTED = CORE + DIVERSITY

SOLVER_LABELS = {
    "cryptominisat-5.14.7": "CMS",
    "tactix-0f7c10174": "Tactix",
    "tactix-no-mab": "Tactix no-MAB",
    "kissat-4.0.4": "Kissat 4",
    "kissat-sc2024": "Kissat SC24",
    "ae-kissat-mab": "AE-Kissat",
    "satsuma-kissat": "Satsuma",
    "lymphosat": "Lymphosat",
}
BASELINE_ORDER = [
    "tactix-0f7c10174", "tactix-no-mab", "kissat-4.0.4",
    "kissat-sc2024", "ae-kissat-mab", "satsuma-kissat", "lymphosat",
]


def load_cache_hashes():
    rows = {}
    for line in CACHE_HASHES.read_text().splitlines():
        digest, rel = line.split(None, 1)
        rows[rel.strip()] = digest
    return rows


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
    if len(SELECTED) != 25 or len(set(SELECTED)) != 25:
        raise RuntimeError("CG must contain exactly 25 distinct Tier D IDs")

    manifests = {p.name: json.loads((p / "manifest.json").read_text())
                 for p in CAMPAIGNS}
    inventory = {}
    all_results = []
    published = set()
    for campaign in CAMPAIGNS:
        manifest = manifests[campaign.name]
        for path in sorted((campaign / "results").glob("*.json")):
            task = int(path.stem)
            if task in published:
                raise RuntimeError(f"duplicate published Tier D ID {task}")
            published.add(task)
            result = json.loads(path.read_text())
            all_results.append((campaign, manifest, path, result))
            if task in SELECTED:
                inventory[task] = (campaign, manifest, path, result)
    if set(inventory) != set(SELECTED):
        raise RuntimeError(f"missing selected results: {set(SELECTED) - set(inventory)}")

    saved = defaultdict(list)
    with SAVED.open(newline="") as stream:
        for row in csv.DictReader(stream):
            saved[row["instance"]].append(row)
    cache_hashes = load_cache_hashes()

    major_or_exclusive = set()
    best_new_family = {}
    core_families = set()
    for task in CORE:
        core_families.add(family(inventory[task][3]["instance"]))
    for _campaign, source_manifest, path, result in all_results:
        if not independently_checked(result, source_manifest):
            continue
        seconds = float(result["solve"]["wall_seconds"])
        if seconds > 600:
            continue
        old = verified_saved_arms(result, saved[result["instance"]])
        fastest = min((float(row["solve_wall_s"]) for row in old), default=None)
        speedup = fastest / max(seconds, 0.001) if fastest is not None else None
        task = int(path.stem)
        if fastest is None or speedup >= 10:
            major_or_exclusive.add(task)
        name = family(result["instance"])
        if name not in core_families and speedup is not None and speedup > 1:
            prior = best_new_family.get(name)
            candidate = (speedup, task)
            if prior is None or candidate > prior:
                best_new_family[name] = candidate
    if major_or_exclusive != set(CORE):
        raise RuntimeError("the frozen core no longer equals all checked exclusive/major wins")
    expected_diversity = {task for _speedup, task in
                          sorted(best_new_family.values(), reverse=True)[:4]}
    if expected_diversity != set(DIVERSITY):
        raise RuntimeError("the diversity rows are no longer the top four new-family wins")

    entries = []
    runtime_rows = []
    classifications = {}
    for task in SELECTED:
        campaign, source_manifest, result_path, result = inventory[task]
        expected = source_manifest["instances"][task]
        if (result.get("task") != task or result.get("instance") != expected["rel"] or
                result.get("expected") != expected["expected"]):
            raise RuntimeError(f"identity mismatch for Tier D ID {task}")
        if not independently_checked(result, source_manifest):
            raise RuntimeError(f"Tier D ID {task} lacks an independent check")
        if (result.get("cleanup_pending") or result.get("scratch_deleted") is not True or
                result.get("artifact_deleted") is not True or
                result.get("derived_artifacts_deleted") is not True or
                result.get("retained_artifacts")):
            raise RuntimeError(f"Tier D ID {task} retained generated artifacts")
        seconds = float(result["solve"]["wall_seconds"])
        if seconds > 600:
            raise RuntimeError(f"Tier D ID {task} exceeds the 600-second ceiling")
        old = verified_saved_arms(result, saved[result["instance"]])
        fastest = min((float(row["solve_wall_s"]) for row in old), default=None)
        speedup = fastest / max(seconds, 0.001) if fastest is not None else None
        kind = "exclusive" if fastest is None else (">=10x" if speedup >= 10 else "diversity")
        classifications[task] = (kind, speedup)
        if task in CORE and kind not in {"exclusive", ">=10x"}:
            raise RuntimeError(f"core row {task} is no longer an exclusive/major win")
        if task in DIVERSITY and (kind != "diversity" or speedup <= 1):
            raise RuntimeError(f"diversity row {task} no longer beats the saved baseline")

        rel = result["instance"]
        source_rel = result_path.relative_to(ROOT).as_posix()
        input_path = ROOT / "benchmarks/cache" / Path(rel)
        entry = {
            "tier_d_id": task,
            "family": family(rel),
            "instance": rel,
            "expected": expected["expected"],
            "verdict": result["verdict"],
            "cms_seconds": seconds,
            "class": kind,
            "speedup_over_fastest_verified_saved_arm": (
                round(speedup, 6) if speedup is not None else None),
            "cms_campaign": campaign.name,
            "cms_result": source_rel,
            "cms_result_sha256": sha256(result_path),
            "certificate_sha256": result["certificate_sha256"],
            "independent_check": (
                "checkmodel" if result["verdict"] == "SAT" else
                "frat-xor then cake-xlrup"),
            "generated_artifacts_deleted": True,
            "compressed_input": f"benchmarks/cache/{rel}",
            "compressed_input_sha256": cache_hashes[rel],
            "compressed_input_present": input_path.is_file(),
        }
        entries.append(entry)

        runtime_rows.append({
            "tier_d_id": task, "family": family(rel), "instance": rel,
            "solver": "cryptominisat-5.14.7", "source_campaign": campaign.name,
            "status": "verified", "verdict": result["verdict"], "verified": "True",
            "solve_wall_s": f"{seconds:.3f}", "solve_timed_out": "False",
            "comparison_note": "pinned CG rerun; 32 GiB; 5000 s cap",
        })
        old_by_solver = {row["solver"]: row for row in saved[rel]}
        if set(old_by_solver) != set(BASELINE_ORDER):
            raise RuntimeError(f"Tier D ID {task} does not have seven saved arms")
        for solver in BASELINE_ORDER:
            row = old_by_solver[solver]
            runtime_rows.append({
                "tier_d_id": task, "family": family(rel), "instance": rel,
                "solver": solver, "source_campaign": row["source_campaign"],
                "status": row["status"], "verdict": row["verdict"],
                "verified": row["verified"], "solve_wall_s": row["solve_wall_s"],
                "solve_timed_out": row["solve_timed_out"],
                "comparison_note": "saved historical arm; solve wall time; resource/proof configuration may differ",
            })

    if Counter(e["family"] for e in entries).keys() != {
            "case11", "dltm", "folkman", "lightsout", "mdp", "mp1",
            "nb13t165", "par32", "vanderwaerden", "x9", "1x1x13", "ptn",
            "bp4", "crusti"}:
        raise RuntimeError("unexpected CG family coverage")

    prior = HERE / "manifest.json"
    frozen_at = (json.loads(prior.read_text())["frozen_at"] if prior.is_file()
                 else dt.datetime.now(dt.timezone.utc).isoformat(timespec="seconds"))
    manifest = {
        "schema": "tactix-cg-v1",
        "name": "CG",
        "frozen_at": frozen_at,
        "count": 25,
        "purpose": "representative checked CryptoMiniSat strengths for Tactix DefaultPolicy development",
        "selection": {
            "source": "canceled partial pinned Tier D rerun",
            "published_rows_screened": len(published),
            "tier_d_rows": 1166,
            "solve_ceiling_seconds": 600,
            "rule": "all 21 checked exclusive or >=10x wins, plus the best checked CMS-over-baseline row from four previously unrepresented families",
            "census_complete": False,
            "accepted_limitation": "The user accepted representative pattern evidence; unrun Tier D rows were not screened.",
        },
        "resource_contract": {"partition": "cpu-q", "memory_gib": 32,
                              "solve_limit_seconds": 5000,
                              "proof_retention": "none"},
        "pins": {
            "campaign_manifests": {
                p.name: sha256(p / "manifest.json") for p in CAMPAIGNS
            },
            "saved_seven_arm_comparison": {
                "path": SAVED.relative_to(ROOT).as_posix(), "sha256": sha256(SAVED)
            },
            "cache_manifest": {
                "path": CACHE_HASHES.relative_to(ROOT).as_posix(),
                "sha256": sha256(CACHE_HASHES),
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
    counts = Counter(entry["family"] for entry in entries)
    lines = [
        "# CG: CryptoMiniSat-guided benchmark collection",
        "",
        "CG contains 25 independently checked Tier D instances selected from the pinned "
        "CryptoMiniSat 5.14.7 rerun. It is a development target for bringing one "
        "instance-independent Tactix DefaultPolicy within 2× the checked CMS solve time.",
        "",
        f"The rerun was stopped after {len(published)} of 1,166 rows because the user accepted a "
        "representative collection. The set contains every checked exclusive or at-least-10× "
        "win below 600 seconds in that snapshot (21 rows), then adds the best CMS-over-baseline "
        "row from four previously unrepresented families. This is a frozen representative "
        "sample, not a complete Tier D census. N1–N4 remain required for solver promotion.",
        "",
        "No CNF is duplicated here. `instances.list` and `manifest.json` reference the "
        "compressed files in `benchmarks/cache/tierD24-26`; all generated proofs and expanded "
        "CNFs were deleted after checking.",
        "",
        "## Family coverage",
        "",
        "| Family | Rows |", "| --- | ---: |",
    ]
    for name, count in sorted(counts.items(), key=lambda pair: (-pair[1], pair[0])):
        lines.append(f"| `{name}` | {count} |")
    lines += [
        "", "## Solve-wall-time comparison", "",
        "Times are seconds. `S✓` and `U✓` are independently verified SAT and UNSAT; "
        "`S?`/`U?` are unverified answers; `TO` is a timeout. CMS used the pinned `cpu-q` "
        "campaign with 32 GiB and a 5,000-second solve cap. The other seven columns are saved "
        "historical arms and are included for the requested general pattern; their placement, "
        "proof configuration, and some resource details differ, so small timing differences "
        "are not causal comparisons.", "",
        "| ID | Family | CMS | Tactix | Tactix no-MAB | Kissat 4 | Kissat SC24 | AE-Kissat | Satsuma | Lymphosat |",
        "| ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for entry in entries:
        rows = by_instance[entry["instance"]]
        values = [cell(rows[solver]) for solver in BASELINE_ORDER]
        lines.append(f"| {entry['tier_d_id']} | `{entry['family']}` | "
                     f"{entry['cms_seconds']:.3f} {entry['verdict'][0]}✓ | " +
                     " | ".join(values) + " |")
    exclusive = sum(e["class"] == "exclusive" for e in entries)
    major = sum(e["class"] == ">=10x" for e in entries)
    diversity = sum(e["class"] == "diversity" for e in entries)
    lines += [
        "", "## Pattern", "",
        f"The collection has {exclusive} CMS-exclusive checked solves, {major} rows where CMS "
        "is at least 10× faster than the fastest verified saved arm, and "
        f"{diversity} diversity rows where CMS is 4.26×–8.48× faster. The strongest clusters "
        "are `mdp`, `par32`, `lightsout`, and `folkman`; the remaining families test whether "
        "the policy generalizes beyond those clusters.",
        "", "## Reproduce and verify", "",
        "```bash",
        "python3 benchmarks/collections/CG/build.py",
        "python3 benchmarks/collections/CG/verify.py --hash-inputs",
        "```",
        "",
        "`baseline-runtimes.csv` contains the exact long-form values, statuses, provenance "
        "campaigns, and comparison caveat for all 200 solver-instance rows.", "",
    ]
    (HERE / "README.md").write_text("\n".join(lines))
    print(f"wrote CG with {len(entries)} instances, {len(counts)} families, "
          f"and {len(runtime_rows)} runtime rows from {len(published)} published CMS rows")


if __name__ == "__main__":
    main()
