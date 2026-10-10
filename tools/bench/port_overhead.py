#!/usr/bin/env python3
"""Plan §8.4 dynamic-macro/static-micro overhead: StaticPort vs HardcodedPort.

`tactix` (DefaultPolicy over the production StaticPort) and `tactix-hardcoded`
(the same build with default_policy.cpp over HardcodedPort, registry dispatch
without its accounting; src/strategy/action_port.hpp) run the same instances
through tools/bench/runtime_task.py with perf counters (collect_runtime_context)
and checked certificates. One array task runs both arms of one instance
sequentially in one allocation, on the same CPU (runtime_task's taskset),
alternating the arm order by instance parity.

  prepare CAMPAIGN --build DIR --source-commit SHA [--max-seconds S]
      Rows: the Rank1-50 and B28+C5 instances the reference regression
      campaigns solved within S seconds (default 300; the larger rows dominate
      cluster time without changing a per-instance ratio). Pins hard-link the
      reference campaign's runner and checkers.
  summarize CAMPAIGN
      Per instance and aggregate static/hardcoded ratios of user instructions,
      cycles and solve wall; certificate and verdict identity between arms.
      The bound is satisfied when the aggregate cycle ratio is at most 1.05.
"""

import argparse
import csv
import datetime as dt
import hashlib
import json
import math
import os
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REFERENCES = {
    "rank1-50": "benchmarks/runs/20261009-c2.4-regression-rank1-50",
    "b28c5": "benchmarks/runs/20261009-c2.4-regression-b28c5",
}
TEMPLATE = "benchmarks/runs/20261009-c2.4-regression-rank1-50"
CHECKER_PINS = ["anonymous_scratch.py", "cake-pb-cnf", "cake_lpr", "checkmodel",
                "drat-trim", "dsr-compress", "dsr-trim", "modelcheck.py",
                "proof_oracle.py", "runtime_task.py", "srcheck", "veripb"]
ARMS = ("static", "hardcoded")
BOUND = 1.05


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for block in iter(lambda: source.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def reference_rows(max_seconds):
    rows = []
    for name, rel in REFERENCES.items():
        campaign = ROOT / rel
        manifest = json.loads((campaign / "manifest.json").read_text())
        with open(campaign / "matrix.tsv", newline="") as source:
            matrix = list(csv.DictReader(source, delimiter="\t"))
        for row in matrix:
            result = campaign / "results" / f"{int(row['task']):03d}.json"
            if not result.exists():
                continue
            data = json.loads(result.read_text())
            wall = (data.get("solve") or {}).get("wall_seconds")
            if not data.get("valid") or data.get("verdict") not in ("SAT", "UNSAT"):
                continue
            if wall is None or wall > max_seconds:
                continue
            rows.append({"set": name, "rank": row["rank"], "tier": row["tier"],
                         "instance": row["instance"], "expected": row["expected"],
                         "reference_seconds": wall,
                         "input_sha256": manifest["input_sha256"][row["instance"]]})
    return rows


def prepare(args):
    campaign = (ROOT / args.campaign).resolve()
    if campaign.exists():
        raise SystemExit(f"{campaign} exists; campaigns are immutable")
    build = Path(args.build).resolve()
    binaries = {"static": build / "tactix", "hardcoded": build / "tactix-hardcoded"}
    for path in binaries.values():
        if not path.is_file():
            raise SystemExit(f"missing {path}")
    rows = reference_rows(args.max_seconds)
    if not rows:
        raise SystemExit("no reference rows")
    for sub in ("pins", "results", "claims", "provisional", "logs"):
        (campaign / sub).mkdir(parents=True)
    template = ROOT / TEMPLATE / "pins"
    pin_sha = {}
    for name in CHECKER_PINS:
        os.link(template / name, campaign / "pins" / name)
        pin_sha[f"pins/{name}"] = sha256(campaign / "pins" / name)
    arms = {}
    for arm, path in binaries.items():
        target = campaign / "pins" / f"tactix-{arm}"
        shutil.copyfile(path, target)
        target.chmod(0o555)
        pin_sha[f"pins/tactix-{arm}"] = sha256(target)
        arms[arm] = {"path": f"pins/tactix-{arm}", "sha256": pin_sha[f"pins/tactix-{arm}"],
                     "source_commit": args.source_commit, "source_build": str(build),
                     "arguments": ["--auto-proof"], "certificate": "auto"}
    matrix = []
    for index, row in enumerate(rows):
        order = ARMS if index % 2 == 0 else ARMS[::-1]
        for arm in order:
            matrix.append({"task": len(matrix), "rank": row["rank"], "arm": arm,
                           "tier": row["tier"], "instance": row["instance"],
                           "expected": row["expected"]})
    with open(campaign / "matrix.tsv", "w", newline="") as out:
        writer = csv.DictWriter(out, ["task", "rank", "arm", "tier", "instance", "expected"],
                                delimiter="\t")
        writer.writeheader()
        writer.writerows(matrix)
    manifest = {
        "schema": "tactix-port-overhead-v1",
        "created_at": dt.datetime.now(dt.timezone.utc).isoformat(),
        "purpose": "plan §8.4 dynamic-macro/static-micro overhead: StaticPort vs HardcodedPort",
        "arms": arms,
        "resource_contract": {
            "partition": "cpu-dense-hwulab-q", "cpus_per_task": args.cpus,
            "memory_mib": args.memory_mib, "solve_timeout": args.solve_timeout,
            "checker_timeout": 1800, "checker_timeout_overrides": {},
            "one_solver_per_task": True, "sequential_same_allocation": True,
            "node_constraint": None, "throttle": None,
            "proof_mode": "automatic PBP/DSR to anonymous scratch"},
        "collect_runtime_context": True,
        "scratch_policy": "anonymous-local-files-v1",
        "reference_campaigns": REFERENCES,
        "max_reference_seconds": args.max_seconds,
        "rows": rows,
        "input_sha256": {row["instance"]: row["input_sha256"] for row in rows},
        "pin_sha256": pin_sha,
    }
    (campaign / "manifest.json").write_text(json.dumps(manifest, indent=1) + "\n")
    count = len(rows)
    (campaign / "run.sbatch").write_text(f"""#!/bin/bash
#SBATCH --job-name={campaign.name}
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task={args.cpus}
#SBATCH --mem={args.memory_mib}M
#SBATCH --time={args.wall}
#SBATCH --array=0-{count - 1}
#SBATCH --no-requeue
#SBATCH --output={campaign}/logs/%A_%a.log
# Both arms of one instance, sequentially in one allocation (same CPU).
set -uo pipefail
ulimit -c 0
export PYTHONDONTWRITEBYTECODE=1
rc=0
for j in 0 1; do t=$((2*SLURM_ARRAY_TASK_ID+j)); python3 {campaign}/pins/runtime_task.py {campaign} $t; r=$?; echo "task$t=$r"; [ $r -gt $rc ] && rc=$r; done
exit $rc
""")
    print(f"prepared {campaign} with {count} instances ({2 * count} rows); submit: sbatch {campaign}/run.sbatch")


def perf_value(result, event):
    for line in (result.get("runtime_context") or {}).get("perf_statistics") or []:
        fields = line.split("\t")
        if len(fields) > 2 and fields[2].startswith(event):
            try:
                return float(fields[0])
            except ValueError:
                return None
    return None


def summarize(args):
    campaign = (ROOT / args.campaign).resolve()
    with open(campaign / "matrix.tsv", newline="") as source:
        matrix = list(csv.DictReader(source, delimiter="\t"))
    by_instance = {}
    for row in matrix:
        path = campaign / "results" / f"{int(row['task']):03d}.json"
        if path.exists():
            by_instance.setdefault(row["instance"], {})[row["arm"]] = json.loads(path.read_text())
    lines, ratios = [], {"instructions": [], "cycles": [], "wall": []}
    totals = {arm: {"instructions": 0.0, "cycles": 0.0, "wall": 0.0} for arm in ARMS}
    identical = compared = 0
    for instance, arms in sorted(by_instance.items()):
        if set(arms) != set(ARMS):
            lines.append(f"{instance}: incomplete")
            continue
        s, h = arms["static"], arms["hardcoded"]
        same = (s.get("verdict") == h.get("verdict") and
                s.get("certificate_sha256") == h.get("certificate_sha256"))
        valid = s.get("valid") and h.get("valid")
        compared += 1
        identical += same
        values = {}
        for arm, result in arms.items():
            values[arm] = {"instructions": perf_value(result, "instructions"),
                           "cycles": perf_value(result, "cycles"),
                           "wall": (result.get("solve") or {}).get("wall_seconds")}
        row = []
        for key in ("instructions", "cycles", "wall"):
            a, b = values["static"][key], values["hardcoded"][key]
            if a and b and valid and same:
                ratios[key].append(a / b)
                totals["static"][key] += a
                totals["hardcoded"][key] += b
                row.append(f"{key}={a / b:.4f}")
            else:
                row.append(f"{key}=n/a")
        lines.append(f"{Path(instance).name}: verdict={s.get('verdict')} "
                     f"identical={'yes' if same else 'NO'} valid={bool(valid)} "
                     f"wall={values['static']['wall']}/{values['hardcoded']['wall']} " + " ".join(row))
    print("\n".join(lines))
    summary = {"instances": compared, "identical": identical}
    for key, values in ratios.items():
        if values:
            summary[f"{key}_geomean"] = math.exp(sum(map(math.log, values)) / len(values))
            summary[f"{key}_max"] = max(values)
            summary[f"{key}_aggregate"] = totals["static"][key] / totals["hardcoded"][key]
    summary["bound"] = BOUND
    summary["within_bound"] = bool(ratios["cycles"]) and summary["cycles_aggregate"] <= BOUND
    print(json.dumps(summary, indent=1))
    return 0 if summary["within_bound"] and identical == compared else 1


def main():
    global ROOT
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", default=str(ROOT), help="repository checkout")
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("prepare")
    p.add_argument("campaign")
    p.add_argument("--build", required=True, help="Release build with tactix and tactix-hardcoded")
    p.add_argument("--source-commit", required=True)
    p.add_argument("--max-seconds", type=float, default=300.0)
    p.add_argument("--solve-timeout", type=int, default=1200)
    p.add_argument("--cpus", type=int, default=8)
    p.add_argument("--memory-mib", type=int, default=32768)
    p.add_argument("--wall", default="02:30:00")
    s = sub.add_parser("summarize")
    s.add_argument("campaign")
    args = parser.parse_args()
    ROOT = Path(args.root).resolve()
    if args.command == "prepare":
        prepare(args)
        return 0
    return summarize(args)


if __name__ == "__main__":
    sys.exit(main())
