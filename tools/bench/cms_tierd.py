#!/usr/bin/env python3
"""Pinned, independently checked CryptoMiniSat Tier-D discovery campaign.

One Slurm array task owns one frozen instance.  Solver and proof scratch lives
on the compute node; only bounded classification records return to the campaign.
"""

import argparse
import csv
import datetime as dt
import json
import lzma
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from collections import Counter
from pathlib import Path

from frozen_baseline import (atomic_json, measured, sat_output_protocol_ok,
                             sha256, verdict)

ROOT = Path(__file__).resolve().parents[2]
LIST = ROOT / "benchmarks/baselines/sc24-26-32g-5000s/instances.list"
CACHE = ROOT / "benchmarks/cache/tierD24-26"
SAVED = ROOT / "benchmarks/reports/2026-10-11-tierd-per-instance/results.csv"
SOLVE_SECONDS = 5000
CHECK_SECONDS = 45000


def rows():
    data = []
    for line in LIST.read_text().splitlines():
        if line and not line.startswith("#"):
            relative, expected, _ = line.split()
            data.append({"instance": relative, "expected": expected})
    if len(data) != 1166 or len({r["instance"] for r in data}) != 1166:
        raise RuntimeError("frozen Tier-D union is not 1,166 unique rows")
    return data


def pin(source, destination):
    source = source.resolve(strict=True)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    destination.chmod(0o555)
    return {"source": str(source), "path": str(destination.name),
            "sha256": sha256(destination)}


def prepare(campaign, args):
    if campaign.exists():
        raise RuntimeError(f"campaign already exists: {campaign}")
    entries = rows()
    missing = [r["instance"] for r in entries
               if not (CACHE / Path(r["instance"]).name).is_file()]
    if missing:
        raise RuntimeError(f"frozen cache is missing {len(missing)} instances")
    campaign.mkdir(parents=True)
    for directory in ("pins", "results", "claims", "logs"):
        (campaign / directory).mkdir()
    sources = {"solver": args.solver, "frat_xor": args.frat_xor,
               "cake_xlrup": args.cake_xlrup,
               "checkmodel": ROOT / "tools/fuzz/checkmodel",
               "modelcheck.py": ROOT / "tools/fuzz/modelcheck.py"}
    pinned = {name: pin(Path(source), campaign / "pins" / name)
              for name, source in sources.items()}
    pinned["libraries"] = [pin(Path(source), campaign / "pins" / Path(source).name)
                           for source in args.library]
    manifest = {
        "kind": "cryptominisat-tierd-discovery-v1",
        "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "tier_list": str(LIST.relative_to(ROOT)), "tier_list_sha256": sha256(LIST),
        "frozen_cache_manifest_sha256": sha256(
            ROOT / "benchmarks/baselines/sc24-26-32g-5000s/cache.sha256"),
        "source_commit": "3c8e228e8a48e41276e8ab039f763daa08d61161",
        "frat_xor_commit": "855f3d0ae45fe37c3ad29e4a8ef56e62e1b5e4ad",
        "limits": {"solver_seconds": SOLVE_SECONDS, "checker_seconds": CHECK_SECONDS,
                   "task_memory_gib": 32, "task_cpus": 4},
        "solver_arguments": ["--zero-exit-status", "--verb", "0",
                             "{cnf}", "{frat}"],
        "pins": pinned, "instances": entries,
        "saved_comparison": str(SAVED.relative_to(ROOT)),
    }
    atomic_json(campaign / "manifest.json", manifest)
    batch = f"""#!/usr/bin/env bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --job-name=cms-tierd-discovery
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=4
#SBATCH --mem=32G
#SBATCH --time=14:15:00
#SBATCH --array=0-1165
#SBATCH --no-requeue
#SBATCH --output={campaign}/logs/task-%a.log
exec python3 {ROOT}/tools/bench/cms_tierd.py --campaign {campaign} --task "$SLURM_ARRAY_TASK_ID"
"""
    (campaign / "array.sbatch").write_text(batch)
    print(campaign)


def verify_sat(campaign, cnf, solver_stdout, work, remaining):
    check = measured([campaign / "pins/checkmodel", cnf, solver_stdout],
                     work / "model.stdout", work / "model.stderr",
                     remaining, work / "model.time")
    check["verified"] = (check["exit_code"] == 0 and not check["timed_out"])
    return check


def verify_unsat(campaign, cnf, frat, work):
    clean, xlrup = work / "proof.clean.frat", work / "proof.xlrup"
    with frat.open("rb") as source, clean.open("wb") as target:
        for line in source:
            if not line.startswith(b"c"):
                target.write(line)
    stages = []
    started = time.monotonic()
    for name, command in (
        ("frat-xor", [campaign / "pins/frat_xor", "elab", clean, cnf, xlrup]),
        ("cake_xlrup", [campaign / "pins/cake_xlrup", cnf, xlrup]),
    ):
        remaining = max(1, CHECK_SECONDS - int(time.monotonic() - started))
        stage_env = os.environ.copy()
        if name == "cake_xlrup":
            stage_env["CML_HEAP_SIZE"] = "24576"
        stage = measured(command, work / f"{name}.stdout",
                         work / f"{name}.stderr", remaining,
                         work / f"{name}.time", stage_env)
        output = (work / f"{name}.stdout").read_text(errors="replace")
        stage["name"] = name
        stage["verified"] = (stage["exit_code"] == 0 and not stage["timed_out"]
                             and (name != "cake_xlrup" or "s VERIFIED" in output))
        stage["stdout_tail"] = output[-1000:]
        stage["stderr_tail"] = (work / f"{name}.stderr").read_text(
            errors="replace")[-1000:]
        stages.append(stage)
        if name == "frat-xor":
            clean.unlink(missing_ok=True)
        if not stage["verified"]:
            break
    return {"verified": len(stages) == 2 and all(s["verified"] for s in stages),
            "wall_seconds": round(time.monotonic() - started, 3),
            "stages": stages,
            "xlrup_bytes": xlrup.stat().st_size if xlrup.exists() else None}


def task(campaign, index):
    manifest = json.loads((campaign / "manifest.json").read_text())
    if index < 0 or index >= len(manifest["instances"]):
        raise RuntimeError("array index out of range")
    entry = manifest["instances"][index]
    result_path = campaign / "results" / f"{index:04d}.json"
    atomic_json(campaign / "claims" / f"{index:04d}.json", {
        "index": index, "host": os.uname().nodename,
        "started_utc": dt.datetime.now(dt.timezone.utc).isoformat()}, exclusive=True)
    if result_path.exists():
        raise RuntimeError(f"result already exists: {result_path}")
    scratch_root = Path(os.environ.get("SLURM_TMPDIR", "/tmp"))
    with tempfile.TemporaryDirectory(prefix=f"cms-tierd-{index:04d}-",
                                     dir=scratch_root) as directory:
        work = Path(directory)
        cnf, frat = work / "instance.cnf", work / "proof.frat"
        source = CACHE / Path(entry["instance"]).name
        with lzma.open(source, "rb") as compressed, cnf.open("wb") as output:
            shutil.copyfileobj(compressed, output)
        env = os.environ.copy()
        env["LD_LIBRARY_PATH"] = str(campaign / "pins") + ":" + env.get(
            "LD_LIBRARY_PATH", "")
        solve = measured([campaign / "pins/solver", "--zero-exit-status",
                          "--verb", "0", cnf, frat],
                         work / "solver.stdout", work / "solver.stderr",
                         SOLVE_SECONDS, work / "solver.time", env)
        answer = verdict(solve["exit_code"], work / "solver.stdout")
        result = {"index": index, **entry, "solve": solve, "verdict": answer,
                  "proof_bytes": frat.stat().st_size if frat.exists() else None,
                  "proof_sha256": sha256(frat) if frat.exists() else None,
                  "solver_stdout_tail": (work / "solver.stdout").read_text(
                      errors="replace")[-1000:],
                  "solver_stderr_tail": (work / "solver.stderr").read_text(
                      errors="replace")[-1000:]}
        if solve["timed_out"]:
            result["status"] = "timeout"
        elif answer == "SAT":
            result["model_check"] = verify_sat(campaign, cnf,
                work / "solver.stdout", work, 600)
            result["status"] = ("verified" if result["model_check"]["verified"]
                                and sat_output_protocol_ok(work / "solver.stdout")
                                else "unverified")
        elif answer == "UNSAT" and frat.exists():
            result["proof_check"] = verify_unsat(campaign, cnf, frat, work)
            result["status"] = ("verified" if result["proof_check"]["verified"]
                                else "unverified")
        else:
            result["status"] = "no-answer"
        atomic_json(result_path, result)
    return 0


def summary(campaign):
    entries = rows()
    found = sorted((campaign / "results").glob("*.json"))
    print(f"completed {len(found)}/{len(entries)}")
    if len(found) != len(entries):
        return 1
    by_instance = {}
    with SAVED.open(newline="") as stream:
        for row in csv.DictReader(stream):
            by_instance.setdefault(row["instance"], []).append(row)
    if any(len(by_instance.get(entry["instance"], [])) != 7 for entry in entries):
        raise RuntimeError("saved comparison does not have seven arms per row")
    exclusive, large, ordinary = [], [], []
    statuses = Counter()
    for index, path in enumerate(found):
        result = json.loads(path.read_text())
        if result["index"] != index or result["instance"] != entries[index]["instance"]:
            raise RuntimeError(f"wrong or duplicate campaign row at {path}")
        statuses[result["status"]] += 1
        if result["status"] != "verified":
            continue
        old = [r for r in by_instance[result["instance"]]
               if r["verified"] == "True"]
        candidate = {
            "instance": result["instance"],
            "expected": result["expected"],
            "verdict": result["verdict"],
            "cms_seconds": result["solve"]["wall_seconds"],
            "family": family(result["instance"]),
            "fastest_saved_seconds": min((float(r["solve_wall_s"]) for r in old),
                                         default=None),
        }
        if not old:
            candidate["class"] = "exclusive"
            exclusive.append(candidate)
        else:
            candidate["speedup"] = (candidate["fastest_saved_seconds"] /
                                    max(candidate["cms_seconds"], 0.001))
            if candidate["speedup"] >= 10:
                candidate["class"] = "major-speed-win"
                large.append(candidate)
            else:
                candidate["class"] = "other"
                ordinary.append(candidate)
    eligible = [candidate for candidate in exclusive + large
                if candidate["cms_seconds"] <= 600]
    selected = select_cg(eligible, 30)
    analysis = {
        "campaign_manifest_sha256": sha256(campaign / "manifest.json"),
        "saved_comparison_sha256": sha256(SAVED),
        "status_counts": dict(statuses),
        "verified_exclusive": sorted(exclusive, key=lambda x: x["instance"]),
        "verified_major_speed_wins": sorted(large, key=lambda x: x["instance"]),
        "verified_other": len(ordinary),
        "cms_sub600_exclusive_or_major": len(eligible),
        "cg_selection_rule": "exclusive then >=10x, round-robin lexical families; "
                             "exclusive ascending CMS time, major descending speedup",
        "cg": selected,
        "cg_families": dict(Counter(row["family"] for row in selected)),
    }
    atomic_json(campaign / "analysis.json", analysis)
    cg_lines = ["# CG: verified pinned CryptoMiniSat Tier-D rerun, <=600 s; "
                "exclusive or >=10x vs fastest verified saved arm.",
                f"# Campaign: {campaign.relative_to(ROOT)}; manifest sha256 "
                f"{analysis['campaign_manifest_sha256']}",
                "# Target: one Tactix DefaultPolicy <=2x CMS per row; N1-N4 remain mandatory."]
    cg_lines += [f"{row['instance']} {row['verdict']} {row['cms_seconds']:.3f}"
                 for row in selected]
    (campaign / "CG.list").write_text("\n".join(cg_lines) + "\n")
    (ROOT / "benchmarks/tiers/CG.list").write_text("\n".join(cg_lines) + "\n")
    print(f"verified exclusive: {len(exclusive)}; >=10x wins: {len(large)}")
    print(f"CG: {len(selected)} verified rerun solves <=600s; families: "
          f"{analysis['cg_families']}")
    return 0


def family(instance):
    name = Path(instance).name.split("-", 1)[-1]
    return re.split(r"[-_.]", name, maxsplit=1)[0].lower()


def select_cg(candidates, limit):
    """Select only verified rerun wins, balancing lexical benchmark families."""
    buckets = {}
    for candidate in candidates:
        buckets.setdefault((candidate["class"], candidate["family"]), []).append(candidate)
    for (kind, _), bucket in buckets.items():
        bucket.sort(key=(lambda r: (r["cms_seconds"], r["instance"])) if
                    kind == "exclusive" else
                    (lambda r: (-r["speedup"], r["cms_seconds"], r["instance"])))
    selected = []
    for kind in ("exclusive", "major-speed-win"):
        families = sorted(family for cls, family in buckets if cls == kind)
        while len(selected) < limit and any(buckets[(kind, f)] for f in families):
            for name in families:
                if buckets[(kind, name)] and len(selected) < limit:
                    selected.append(buckets[(kind, name)].pop(0))
    return selected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--campaign", type=Path, required=True)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--prepare", action="store_true")
    mode.add_argument("--submit", action="store_true")
    mode.add_argument("--task", type=int)
    mode.add_argument("--summary", action="store_true")
    parser.add_argument("--solver", type=Path)
    parser.add_argument("--frat-xor", type=Path)
    parser.add_argument("--cake-xlrup", type=Path)
    parser.add_argument("--library", action="append", type=Path, default=[])
    args = parser.parse_args()
    campaign = args.campaign.resolve()
    if args.prepare:
        if not all((args.solver, args.frat_xor, args.cake_xlrup)):
            parser.error("--prepare requires --solver, --frat-xor and --cake-xlrup")
        prepare(campaign, args)
    elif args.submit:
        output = subprocess.check_output(["sbatch", "--parsable",
                                          str(campaign / "array.sbatch")], text=True)
        atomic_json(campaign / "submission.json", {
            "job_id": output.strip(),
            "submitted_utc": dt.datetime.now(dt.timezone.utc).isoformat()})
        print(output.strip())
    elif args.task is not None:
        return task(campaign, args.task)
    else:
        return summary(campaign)
    return 0


if __name__ == "__main__":
    sys.exit(main())
