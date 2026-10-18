#!/usr/bin/env python3
"""Prepare and summarize the pinned cpu-q CryptoMiniSat Tier-D rerun."""

import argparse
import csv
import datetime as dt
import hashlib
import json
import shutil
import subprocess
from collections import Counter
from pathlib import Path

from cms_tierd import family, select_cg
from frozen_baseline import atomic_json, sha256

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "benchmarks/runs/20261017-cms-tierd-discovery/pins"
LIST = ROOT / "benchmarks/baselines/sc24-26-32g-5000s/instances.list"
CACHE_MANIFEST = ROOT / "benchmarks/baselines/sc24-26-32g-5000s/cache.sha256"
SAVED = ROOT / "benchmarks/reports/2026-10-11-tierd-per-instance/results.csv"
PIN_NAMES = ("solver", "frat_xor", "cake_xlrup", "checkmodel", "modelcheck.py",
             "libcadiback.so.0", "libcadical.so.3", "libcryptominisat5.so.5.14")


def instances():
    rows = []
    for line in LIST.read_text().splitlines():
        if line and not line.startswith("#"):
            rel, expected, _ = line.split()
            rows.append({"rel": rel, "expected": expected})
    if len(rows) != 1166 or len({r["rel"] for r in rows}) != 1166:
        raise RuntimeError("expected exactly 1,166 unique frozen Tier-D rows")
    return rows


def prepare(campaign):
    if campaign.exists():
        raise RuntimeError(f"campaign exists: {campaign}")
    rows = instances()
    cache = ROOT / "benchmarks/cache/tierD24-26"
    missing = [r["rel"] for r in rows if not (cache / Path(r["rel"]).name).is_file()]
    if missing:
        raise RuntimeError(f"missing {len(missing)} compressed inputs")
    for name in ("pins", "results", "pending", "claims", "scratch", "logs"):
        (campaign / name).mkdir(parents=True, exist_ok=True)
    provenance = {}
    for name in PIN_NAMES:
        destination = campaign / "pins" / name
        source = (ROOT / "tools/checkers/frat-xor-855f3d0/target/release/frat-xor"
                  if name == "frat_xor" else SOURCE / name)
        shutil.copy2(source, destination)
        provenance[name.replace("_", "-")] = {
            "path": str(destination.relative_to(campaign)), "sha256": sha256(destination)}
    runner = campaign / "pins/frozen_baseline.py"
    shutil.copy2(ROOT / "tools/bench/frozen_baseline.py", runner)
    provenance["frozen-baseline"] = {
        "path": "pins/frozen_baseline.py", "sha256": sha256(runner)}
    scratch_module = campaign / "pins/anonymous_scratch.py"
    shutil.copy2(ROOT / "tools/bench/anonymous_scratch.py", scratch_module)
    provenance["anonymous-scratch"] = {
        "path": "pins/anonymous_scratch.py", "sha256": sha256(scratch_module)}
    checker_source = ROOT / "tools/checkers/frat-xor-855f3d0"
    checker_patch = campaign / "pins/frat_xor_node_local_temp.patch"
    checker_patch.write_bytes(subprocess.run(
        ["git", "-C", str(checker_source), "diff", "--", "src/elab.rs"],
        capture_output=True, check=True).stdout)
    if checker_patch.stat().st_size == 0:
        raise RuntimeError("missing FRAT-XOR anonymous-temp source patch")
    provenance["frat-xor-source-patch"] = {
        "path": "pins/frat_xor_node_local_temp.patch", "sha256": sha256(checker_patch)}
    wrapper = campaign / "pins/cms-wrapper"
    wrapper.write_text(
        "#!/usr/bin/env bash\nset -euo pipefail\n"
        f"export LD_LIBRARY_PATH='{campaign}/pins':${{LD_LIBRARY_PATH:-}}\n"
        f"exec '{campaign}/pins/solver' --zero-exit-status --verb 0 \"$1\" \"$2\"\n")
    wrapper.chmod(0o555)
    provenance["cms-wrapper"] = {"path": "pins/cms-wrapper", "sha256": sha256(wrapper),
                                 "arguments": ["--zero-exit-status", "--verb", "0",
                                               "{cnf}", "{frat}"]}
    manifest = {
        "schema": 1, "kind": "cryptominisat-tierd-cpuq-anonymous-baseline-v2",
        "campaign": campaign.name,
        "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "once_only": True, "requeue": False,
        "certificate_policy": {"retention": "none",
                               "storage": "anonymous-node-local",
                               "result_publication": "pending-journal-then-atomic-promotion",
                               "stage_cleanup": "delete-input-after-consuming-checker-stage",
                               "terminal_cleanup": "close-anonymous-handles-before-result-publication"},
        "limits": {"solve_wall_seconds": 5000, "checker_pipeline_wall_seconds": 45000,
                   "scheduler_memory_gib": 32, "memory_enforcement": "slurm_task_cgroup"},
        "slurm": {"partition": "cpu-q", "cpus_per_task": 4, "memory_gib": 32,
                  "array_throttle": None},
        "source_commits": {"cryptominisat": "3c8e228e8a48e41276e8ab039f763daa08d61161",
                           "frat-xor": "855f3d0ae45fe37c3ad29e4a8ef56e62e1b5e4ad"},
        "source_patches": {"frat-xor-elab-node-local-temp": sha256(
            ROOT / "tools/checkers/frat-xor-855f3d0/src/elab.rs"),
                           "frat-xor-patch": provenance["frat-xor-source-patch"]},
        "instances_list_sha256": sha256(LIST),
        "cache_manifest_sha256": sha256(CACHE_MANIFEST),
        "saved_comparison_sha256": sha256(SAVED),
        "instances": rows,
        "tasks": [{"instance": i, "solver": 0} for i in range(len(rows))],
        "solvers": [{"name": "cryptominisat-5.14.7", "kind": "positional",
                     "certificate": "frat-xor", "executable": "pins/cms-wrapper",
                     "checker_pipeline": "frat-xor-cake-xlrup", "vbs_eligible": "SAT+UNSAT"}],
        "checkers": {"checkmodel": "pins/checkmodel", "modelcheck.py": "pins/modelcheck.py",
                     "frat-xor": "pins/frat_xor", "cake-xlrup": "pins/cake_xlrup"},
        "provenance": {"cryptominisat-5.14.7": provenance["cms-wrapper"],
                       "checkmodel": provenance["checkmodel"],
                       "modelcheck.py": provenance["modelcheck.py"],
                       "frat-xor": provenance["frat-xor"],
                       "cake-xlrup": provenance["cake-xlrup"],
                       "frozen-baseline": provenance["frozen-baseline"],
                       "anonymous-scratch": provenance["anonymous-scratch"],
                       "solver-binary": provenance["solver"],
                       "shared-libraries": [provenance[n] for n in (
                           "libcadiback.so.0", "libcadical.so.3",
                           "libcryptominisat5.so.5.14")]},
    }
    atomic_json(campaign / "manifest.json", manifest, exclusive=True)
    batch = campaign / "array.sbatch"
    batch.write_text(f"""#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --job-name=cms-tierd-cpuq
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=4
#SBATCH --mem=32G
#SBATCH --time=14:15:00
#SBATCH --array=0-1165
#SBATCH --no-requeue
#SBATCH --output={campaign}/logs/task-%a.log
export TACTIX_REPO_ROOT='{ROOT}'
export PYTHONDONTWRITEBYTECODE=1
export TMPDIR=/tmp
exec python3 '{runner}' --campaign '{campaign}' --run-one "$SLURM_ARRAY_TASK_ID"
""")
    print(f"prepared {campaign}: {len(rows)} rows, frozen harness, zero retention")


def independently_checked(result, manifest):
    if not result.get("valid"):
        return False
    if (result.get("expected_match") is not True or
            result.get("solver") != manifest["solvers"][0]["name"] or
            result.get("solve", {}).get("timed_out") or
            result.get("solve", {}).get("exit_code") != 0 or
            not result.get("certificate_bytes") or
            not result.get("certificate_sha256")):
        raise RuntimeError("published valid row lacks a checked solver certificate")
    if result["verdict"] == "SAT":
        check = result.get("model_check") or {}
        if (result.get("model_ok") is not True or
                result.get("model_protocol_ok") is not True or
                result.get("model_checker", {}).get("sha256") !=
                manifest["provenance"]["checkmodel"]["sha256"] or
                check.get("result") != "verified" or
                check.get("exit_code") != 0 or check.get("timed_out")):
            raise RuntimeError("published SAT row lacks independent model check")
    elif result["verdict"] == "UNSAT":
        check = result.get("proof_check") or {}
        stages = check.get("stages") or []
        storage = manifest.get("certificate_policy", {}).get("storage")
        temporary_key = ("temporary_deleted" if storage == "workspace-ephemeral"
                         else "anonymous_temp_deleted")
        if (result.get("proof_ok") is not True or
                check.get("result") != "verified" or
                check.get("pipeline") != "frat-xor-cake-xlrup" or
                len(stages) != 2 or
                [stage.get("name") for stage in stages] !=
                ["frat-xor", "cake-xlrup"] or
                any(stage.get("result") != "verified" or
                    stage.get("exit_code") != 0 or stage.get("timed_out") or
                    stage.get("input_artifact_deleted") is not True or
                    (stage["name"] == "frat-xor" and
                     stage.get(temporary_key) is not True) or
                    stage.get("provenance", {}).get("sha256") !=
                    manifest["provenance"][stage["name"]]["sha256"]
                    for stage in stages) or
                any(artifact.get("deleted") is not True
                    for artifact in check.get("derived_artifacts", []))):
            raise RuntimeError("published UNSAT row lacks independent proof check")
    else:
        raise RuntimeError("published valid row has no SAT/UNSAT verdict")
    return True


def compatible_repair_policy(base, repair):
    """Permit the declared workspace repair without weakening proof retention."""
    base_policy = base.get("certificate_policy", {})
    policy = repair.get("certificate_policy", {})
    if repair.get("kind") not in {
            "cryptominisat-tierd-cpuq-workspace-repair-v1",
            "cryptominisat-tierd-cpuq-workspace-repair-v2"}:
        return policy == base_policy
    common = (base_policy.get("storage") == "anonymous-node-local" and
              policy.get("storage") == "workspace-ephemeral" and
              all(policy.get(key) == base_policy.get(key) for key in
                  ("retention", "result_publication", "stage_cleanup")) and
              policy.get("terminal_cleanup") ==
              "bounded-nfs-retry-in-finally-before-result-publication" and
              repair.get("slurm", {}).get("array_throttle") == 125)
    limits = repair.get("limits", {})
    provenance = repair.get("provenance", {})
    legacy_task_killer = (limits.get("workspace_quota_stop_gib") == 750 and
                          limits.get("workspace_quota_poll_seconds") == 2 and
                          "workspace-quota" in provenance)
    governor_only = ("workspace_quota_stop_gib" not in limits and
                     "workspace_quota_poll_seconds" not in limits and
                     "workspace-quota" not in provenance)
    return common and (legacy_task_killer or governor_only)


def result_inventory(paths):
    """Hash the exact predecessor result set named by a successor repair."""
    digest = hashlib.sha256()
    for path in sorted(paths):
        digest.update(f"{path.name} {sha256(path)}\n".encode())
    return digest.hexdigest()


def verified_saved_arms(result, arms):
    """Compare only independently verified saved arms with a checked CMS row."""
    checked = [arm for arm in arms if arm["verified"] == "True"]
    if any(arm["expected"] != result["expected"] or
           arm["verdict"] != result["verdict"] for arm in checked):
        raise RuntimeError("saved verified verdict disagrees with CMS")
    return checked


def verify_campaign_pins(campaign, manifest):
    """Check the frozen inputs and every file named by a campaign provenance pin."""
    if (manifest.get("campaign") != campaign.name or
            manifest.get("instances_list_sha256") != sha256(LIST) or
            manifest.get("cache_manifest_sha256") != sha256(CACHE_MANIFEST) or
            manifest.get("saved_comparison_sha256") != sha256(SAVED) or
            manifest.get("certificate_policy", {}).get("retention") != "none" or
            manifest.get("slurm", {}).get("partition") != "cpu-q"):
        raise RuntimeError("campaign frozen input or execution contract changed")
    pins = []
    for value in manifest.get("provenance", {}).values():
        pins.extend(value if isinstance(value, list) else [value])
    if not pins:
        raise RuntimeError("campaign has no provenance pins")
    for pin in pins:
        relative = Path(pin["path"])
        if (relative.is_absolute() or ".." in relative.parts or
                not relative.parts or
                sha256(campaign / relative) != pin["sha256"]):
            raise RuntimeError(f"campaign provenance pin changed: {relative}")
    if (manifest["solvers"][0]["executable"] !=
            manifest["provenance"]["cryptominisat-5.14.7"]["path"] or
            any(manifest["checkers"][name] != manifest["provenance"][name]["path"]
                for name in ("checkmodel", "modelcheck.py", "frat-xor", "cake-xlrup"))):
        raise RuntimeError("campaign solver or checker path changed")


def summary(campaign, repair_campaigns=None):
    rows = instances()
    base_manifest = json.loads((campaign / "manifest.json").read_text())
    verify_campaign_pins(campaign, base_manifest)
    if base_manifest["instances"] != rows:
        raise RuntimeError("base campaign no longer matches frozen Tier-D rows")
    paths = {int(path.stem): (path, campaign.name)
             for path in (campaign / "results").glob("*.json")}
    repair_campaigns = ([repair_campaigns] if isinstance(repair_campaigns, Path)
                        else list(repair_campaigns or []))
    repair_manifests = {}
    processed_repairs = []
    for repair_campaign in repair_campaigns:
        repair_campaign = repair_campaign.resolve()
        repair_manifest = json.loads((repair_campaign / "manifest.json").read_text())
        verify_campaign_pins(repair_campaign, repair_manifest)
        declared = repair_manifest.get("repair_of", {})
        missing = set(range(len(rows))) - set(paths)
        if (repair_manifest.get("instances") != rows or
                repair_manifest.get("tasks") != base_manifest.get("tasks") or
                repair_manifest.get("solvers") != base_manifest.get("solvers") or
                repair_manifest.get("checkers") != base_manifest.get("checkers") or
                repair_manifest.get("source_commits") != base_manifest.get("source_commits") or
                not compatible_repair_policy(base_manifest, repair_manifest) or
                repair_manifest.get("limits", {}).get("solve_wall_seconds") !=
                base_manifest.get("limits", {}).get("solve_wall_seconds") or
                repair_manifest.get("limits", {}).get("checker_pipeline_wall_seconds") !=
                base_manifest.get("limits", {}).get("checker_pipeline_wall_seconds") or
                declared.get("manifest_sha256") != sha256(campaign / "manifest.json") or
                declared.get("campaign") != campaign.name or
                set(declared.get("missing_indices", [])) != missing):
            raise RuntimeError("repair campaign does not match the next missing rows")
        predecessors = declared.get("prior_repairs", [])
        expected_predecessors = [{
            "campaign": prior.name,
            "manifest_sha256": sha256(prior / "manifest.json"),
            "published_indices": sorted(int(path.stem) for path in
                                         (prior / "results").glob("*.json")),
            "results_inventory_sha256": result_inventory(
                (prior / "results").glob("*.json")),
        } for prior in processed_repairs]
        if ((repair_manifest.get("kind", "").endswith("-v2") and
             predecessors != expected_predecessors) or
                (not repair_manifest.get("kind", "").endswith("-v2") and
                 predecessors)):
            raise RuntimeError("repair predecessor inventory changed")
        for name in ("solver-binary", "frat-xor", "cake-xlrup", "checkmodel"):
            if (repair_manifest["provenance"][name]["sha256"] !=
                    base_manifest["provenance"][name]["sha256"]):
                raise RuntimeError(f"repair changed checked pin: {name}")
        repair_paths = {int(path.stem): path
                        for path in (repair_campaign / "results").glob("*.json")}
        if set(repair_paths) - missing:
            raise RuntimeError("repair published a nonmissing base row")
        paths.update({index: (path, repair_campaign.name)
                      for index, path in repair_paths.items()})
        repair_manifests[repair_campaign.name] = repair_manifest
        processed_repairs.append(repair_campaign)
    print(f"completed {len(paths)}/{len(rows)}")
    if len(paths) != len(rows):
        return 1
    if set(paths) != set(range(len(rows))):
        raise RuntimeError("campaign result indices are not exactly the Tier-D task IDs")
    by_instance = {}
    with SAVED.open(newline="") as stream:
        for row in csv.DictReader(stream):
            by_instance.setdefault(row["instance"], []).append(row)
    if any(len(by_instance.get(row["rel"], [])) != 7 for row in rows):
        raise RuntimeError("saved comparison must have seven arms per instance")
    exclusive, major, ordinary = [], [], []
    counts = Counter()
    for index in range(len(rows)):
        path, source_campaign = paths[index]
        source_manifest = repair_manifests.get(source_campaign, base_manifest)
        result = json.loads(path.read_text())
        if result.get("task") != index or result.get("instance") != rows[index]["rel"]:
            raise RuntimeError(f"wrong campaign row: {path}")
        if (result.get("cleanup_pending") or
                result.get("scratch_deleted") is not True or
                result.get("artifact_deleted") is not True or
                result.get("derived_artifacts_deleted") is not True or
                result.get("retained_artifacts")):
            raise RuntimeError(f"campaign artifact cleanup incomplete: {path}")
        counts["valid" if result.get("valid") else result.get("verdict", "invalid")] += 1
        if not independently_checked(result, source_manifest):
            continue
        old = verified_saved_arms(result, by_instance[result["instance"]])
        candidate = {"instance": result["instance"], "expected": result["expected"],
                     "verdict": result["verdict"],
                     "cms_seconds": result["solve"]["wall_seconds"],
                     "family": family(result["instance"]),
                     "source_campaign": source_campaign,
                     "fastest_saved_seconds": min((float(r["solve_wall_s"]) for r in old),
                                                  default=None)}
        if not old:
            candidate["class"] = "exclusive"
            exclusive.append(candidate)
        else:
            candidate["speedup"] = candidate["fastest_saved_seconds"] / max(
                candidate["cms_seconds"], 0.001)
            if candidate["speedup"] >= 10:
                candidate["class"] = "major-speed-win"
                major.append(candidate)
            else:
                ordinary.append(candidate)
    eligible = [c for c in exclusive + major if c["cms_seconds"] <= 600]
    selected = select_cg(eligible, 30)
    analysis = {"campaign_manifest_sha256": sha256(campaign / "manifest.json"),
                "repair_manifests": [
                    {"campaign": repair.name,
                     "sha256": sha256(repair / "manifest.json")}
                    for repair in processed_repairs],
                "result_source_counts": dict(Counter(source for _, source in paths.values())),
                "saved_comparison_sha256": sha256(SAVED),
                "status_counts": dict(counts), "verified_exclusive": exclusive,
                "verified_major_speed_wins": major, "verified_other": len(ordinary),
                "cms_sub600_exclusive_or_major": len(eligible), "cg": selected,
                "cg_families": dict(Counter(r["family"] for r in selected))}
    atomic_json(campaign / "analysis.json", analysis, exclusive=True)
    header = ["# CG: verified pinned CryptoMiniSat cpu-q Tier-D rerun, <=600 s; exclusive or >=10x.",
              f"# Campaign: {campaign.relative_to(ROOT)}; manifest sha256 {analysis['campaign_manifest_sha256']}",
              "# Target: one Tactix DefaultPolicy <=2x CMS per row; N1-N4 remain mandatory."]
    for offset, repair in enumerate(processed_repairs, start=2):
        header.insert(offset, f"# Repair: {repair.relative_to(ROOT)}; manifest sha256 "
                      f"{sha256(repair / 'manifest.json')}")
    content = "\n".join(header + [
        f"{r['instance']} {r['verdict']} {r['cms_seconds']:.3f}" for r in selected]) + "\n"
    (campaign / "CG.list").write_text(content)
    (ROOT / "benchmarks/tiers/CG.list").write_text(content)
    print(f"verified exclusive {len(exclusive)}, major wins {len(major)}, CG {len(selected)}")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--campaign", type=Path, required=True)
    parser.add_argument("--repair-campaign", type=Path, action="append", default=[])
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--prepare", action="store_true")
    mode.add_argument("--summary", action="store_true")
    args = parser.parse_args()
    return (prepare(args.campaign.resolve()) if args.prepare else
            summary(args.campaign.resolve(), args.repair_campaign))


if __name__ == "__main__":
    raise SystemExit(main())
