#!/usr/bin/env python3
"""Pin a checked Rank 1-40 Tactix/Kissat comparison; submit array.sbatch separately."""

import argparse
import csv
import datetime as dt
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for block in iter(lambda: source.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("campaign", type=Path)
    ap.add_argument("--tactix", required=True, type=Path)
    ap.add_argument("--kissat", required=True, type=Path)
    ap.add_argument("--checkers", required=True, type=Path)
    ap.add_argument("--matrix", required=True, type=Path)
    ap.add_argument("--source-patch", type=Path,
                    help="exact git diff HEAD -- src include CMakeLists.txt for an uncommitted candidate")
    args = ap.parse_args()
    campaign = args.campaign.resolve()
    if campaign.parent != ROOT / "benchmarks/runs":
        ap.error("campaign must be directly under benchmarks/runs")
    source_diff = subprocess.check_output(
        ["git", "-C", str(ROOT), "diff", "HEAD", "--", "src", "include", "CMakeLists.txt"])
    if args.source_patch:
        if not source_diff or args.source_patch.read_bytes() != source_diff:
            ap.error("source patch must match the current production-source diff exactly")
    elif source_diff:
        ap.error("dirty production source requires an exact --source-patch")
    subprocess.run(["git", "-C", str(ROOT / "kissat"), "diff", "--exit-code", "HEAD",
                    "--", "src", "include", "CMakeLists.txt"], check=True)
    with open(args.matrix, newline="") as source:
        instances = {}
        for row in csv.DictReader(source, delimiter="\t"):
            rank = int(row["rank"])
            if rank in instances:
                assert all(row[k] == instances[rank][k]
                           for k in ("instance", "expected"))
            instances[rank] = row
    if sorted(instances) != list(range(1, 41)):
        ap.error("matrix must contain exactly ranks 1-40")
    campaign.mkdir(exist_ok=False)
    for name in ("pins", "claims", "results", "provisional", "logs"):
        (campaign / name).mkdir()
    pins = {}

    def pin(source, name):
        target = campaign / "pins" / name
        shutil.copy2(source, target)
        target.chmod(target.stat().st_mode & ~0o222)
        rel = str(target.relative_to(campaign))
        pins[rel] = sha256(target)
        return rel

    arms = {}
    for name, binary, arguments, tree in (
        ("tactix", args.tactix, [], ROOT),
        ("kissat", args.kissat, ["-v"], ROOT / "kissat"),
    ):
        rel = pin(binary, name)
        commit = subprocess.check_output(
            ["git", "-C", str(tree), "rev-parse", "HEAD"], text=True).strip()
        arms[name] = dict(path=rel, sha256=pins[rel], source_commit=commit,
                          arguments=arguments)
    if args.source_patch:
        rel = pin(args.source_patch, "candidate-source.patch")
        arms["tactix"].update(source_patch=rel, source_patch_sha256=pins[rel])
    for name in ("drat-trim", "checkmodel", "modelcheck.py"):
        pin(args.checkers / name, name)
    runner = pin(Path(__file__).with_name("runtime_task.py"), "runtime_task.py")
    rows, hashes = [], {}
    for rank, row in sorted(instances.items()):
        hashes[row["instance"]] = sha256(ROOT / "benchmarks/cache" / row["instance"])
        for arm in arms:
            rows.append(dict(task=len(rows), rank=rank, arm=arm, tier="D",
                             instance=row["instance"], expected=row["expected"]))
    with open(campaign / "matrix.tsv", "x", newline="") as target:
        writer = csv.DictWriter(target, fieldnames=rows[0], delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)
    pins["matrix.tsv"] = sha256(campaign / "matrix.tsv")
    manifest = dict(
        schema="tactix-kissat-checked-runtime-v1",
        created_at=dt.datetime.now(dt.timezone.utc).isoformat(), arms=arms,
        input_sha256=hashes, pin_sha256=pins,
        resource_contract=dict(partition="cpu-dense-hwulab-q", cpus_per_task=12,
                               memory_mib=48000, array="0-79", throttle=None,
                               node_constraint=None, proof_mode="DRAT on both arms",
                               one_solver_per_task=True, solve_timeout=1200,
                               checker_timeout=16000),
        purpose="Replace unmatched 5291 timing; classify work with counters, never runtime bands.",
        validation="Both arms require proof/model checking, expected exit, and zero scratch residue.")
    (campaign / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    batch = f'''#!/usr/bin/env bash
#SBATCH --job-name=runtime-checked
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=48000M
#SBATCH --time=05:00:00
#SBATCH --array=0-79
#SBATCH --no-requeue
#SBATCH --output={campaign}/logs/%A_%a.log
set -euo pipefail
exec python3 {shlex.quote(str(campaign / runner))} {shlex.quote(str(campaign))} "$SLURM_ARRAY_TASK_ID"
'''
    (campaign / "array.sbatch").write_text(batch)
    print(campaign)


if __name__ == "__main__":
    main()
