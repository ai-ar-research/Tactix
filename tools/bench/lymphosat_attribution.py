#!/usr/bin/env python3
"""Rerun the LymphoSAT arm on Tier D with per-instance module attribution.

The frozen SC24-26 baseline campaign deleted each task's solver stderr, so the
composite dispatcher's trace of which nested solver answered is unrecoverable
from it.  This runner reruns `tools/solvers/variants/lymphosat-modlog` (the
frozen LymphoSAT pin with an attribution-logging dispatcher; all nested solver
binaries byte-identical) on the same 1,166-instance Tier D set under the same
5,000 s / 32 GiB contract, parses the dispatcher trace, and records the
responsible module per instance.  Attribution-only: proofs and models are not
re-checked; the aggregate instead cross-checks verdicts against the frozen
campaign, which owns validity.
"""

import argparse
import datetime as dt
import hashlib
import json
import os
import re
import resource
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CACHE = ROOT / "benchmarks/cache/tierD24-26"
TIME = Path("/usr/bin/time")
SOLVE_LIMIT = 5000
MEM_GIB = 32
PARTITION = "cpu-dense-hwulab-q"
BASE_CAMPAIGN = ROOT / "benchmarks/baselines/sc24-26-32g-5000s"
VARIANT = ROOT / "tools/solvers/variants/lymphosat-modlog"

COMPOSITE_LINE = re.compile(r"^c composite: (.*)$")
ACCEPTED = re.compile(r"^matcher (\S+) accepted formula$")
CONTINUED_UNKNOWN = re.compile(r"^(\S+) returned UNKNOWN; continuing$")
CONTINUED_EXIT = re.compile(r"^(\S+) exited (\d+); continuing$")
THREW = re.compile(r"^matcher (\S+) threw; skipping$")
FALLBACK = re.compile(r"^dispatching fallback (\S+)$")
SOLVED = re.compile(r"^solved by (\S+) \(exit (\d+)\)$")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def atomic_json(path, value, exclusive=False):
    path = Path(path)
    tmp = path.with_name(path.name + f".tmp.{os.getpid()}")
    with open(tmp, "w") as f:
        json.dump(value, f, indent=1, sort_keys=True)
        f.write("\n")
    if exclusive:
        os.link(tmp, path)
        tmp.unlink()
    else:
        os.replace(tmp, path)


def limits():
    os.setsid()
    b = MEM_GIB << 30
    resource.setrlimit(resource.RLIMIT_AS, (b, b))


def parse_rss(path):
    try:
        text = Path(path).read_text(errors="replace")
        m = re.search(r"^max_rss_kb=(\d+)$", text, re.M)
        return int(m.group(1)) if m else None
    except OSError:
        return None


def measured(cmd, stdout, stderr, timeout, metric):
    wrapped = [str(TIME), "-f", "max_rss_kb=%M", "-o", str(metric), "--",
               *map(str, cmd)]
    start = time.monotonic()
    timed_out = False
    with open(stdout, "wb") as out, open(stderr, "wb") as err:
        p = subprocess.Popen(wrapped, stdout=out, stderr=err, preexec_fn=limits)
        try:
            rc = p.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            os.killpg(p.pid, signal.SIGTERM)
            try:
                p.wait(timeout=10)
            except subprocess.TimeoutExpired:
                os.killpg(p.pid, signal.SIGKILL)
                p.wait()
            rc = None
    return {"exit_code": rc, "wall_seconds": round(time.monotonic() - start, 3),
            "peak_rss_kb": parse_rss(metric), "timed_out": timed_out}


def verdict(rc, stdout):
    text = Path(stdout).read_text(errors="replace")
    if rc == 10 and re.search(r"^s SATISFIABLE\s*$", text, re.M):
        return "SAT"
    if rc == 20 and re.search(r"^s UNSATISFIABLE\s*$", text, re.M):
        return "UNSAT"
    return "UNKNOWN"


def tail(path, lines=40):
    try:
        return "\n".join(Path(path).read_text(errors="replace")
                         .splitlines()[-lines:])[-4000:]
    except OSError:
        return ""


def parse_attribution(stderr_path):
    """Turn the dispatcher's `c composite:` stderr trace into structured data."""
    events, attempts, responsible, responsible_exit = [], [], None, None
    try:
        text = Path(stderr_path).read_text(errors="replace")
    except OSError:
        text = ""
    for line in text.splitlines():
        m = COMPOSITE_LINE.match(line)
        if not m:
            continue
        body = m.group(1)
        events.append(body)
        if responsible is not None:
            # Everything after the solved-by line is replayed child output.
            continue
        if a := ACCEPTED.match(body):
            attempts.append({"solver": a.group(1), "via": "matcher",
                             "outcome": "launched"})
        elif a := FALLBACK.match(body):
            attempts.append({"solver": a.group(1), "via": "fallback",
                             "outcome": "launched"})
        elif a := THREW.match(body):
            attempts.append({"solver": a.group(1), "via": "matcher",
                             "outcome": "matcher-threw"})
        elif a := CONTINUED_UNKNOWN.match(body):
            if attempts and attempts[-1]["solver"] == a.group(1):
                attempts[-1]["outcome"] = "returned-unknown"
        elif a := CONTINUED_EXIT.match(body):
            if attempts and attempts[-1]["solver"] == a.group(1):
                attempts[-1]["outcome"] = f"exited-{a.group(2)}"
        elif a := SOLVED.match(body):
            responsible, responsible_exit = a.group(1), int(a.group(2))
            if attempts and attempts[-1]["solver"] == responsible:
                attempts[-1]["outcome"] = "solved"
    return {"responsible": responsible, "responsible_exit": responsible_exit,
            "attempts": attempts, "composite_events": events}


def prepare(campaign):
    campaign = campaign.resolve()
    if (campaign / "manifest.json").exists():
        raise SystemExit("campaign already prepared")
    base_man_path = BASE_CAMPAIGN / "manifest.json"
    base_man = json.loads(base_man_path.read_text())
    campaign.mkdir(parents=True, exist_ok=True)
    for sub in ("results", "claims", "logs", "scratch"):
        (campaign / sub).mkdir(exist_ok=True)
    pin = campaign / "pins/lymphosat-modlog"
    pin.parent.mkdir(exist_ok=True)
    shutil.copytree(VARIANT, pin)
    frozen_pin = BASE_CAMPAIGN / "pins/lymphosat"
    nested_identical, nested_total = 0, 0
    for solver_bin in sorted((frozen_pin / "solver/solvers").glob("*/solver/solver")):
        nested_total += 1
        ours = pin / solver_bin.relative_to(frozen_pin)
        if ours.is_file() and sha256(ours) == sha256(solver_bin):
            nested_identical += 1
    manifest = {
        "schema": 1,
        "campaign": campaign.name,
        "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "purpose": "per-instance module attribution for LymphoSAT on Tier D",
        "base_campaign": str(BASE_CAMPAIGN.relative_to(ROOT)),
        "base_manifest_sha256": sha256(base_man_path),
        "instances": [{"rel": inst["rel"], "expected": inst["expected"]}
                      for inst in base_man["instances"]],
        "solver": {
            "name": "lymphosat-modlog",
            "executable": "pins/lymphosat-modlog/run.sh",
            "kind": "output-directory",
            "variant_source": str(VARIANT.relative_to(ROOT)),
            "composite_sha256": sha256(pin / "solver/solver"),
            "frozen_composite_sha256": sha256(frozen_pin / "solver/solver"),
            "dispatcher_patch": "solver/core/main.cpp attribution logging only",
            "nested_binaries_identical_to_frozen_pin":
                f"{nested_identical}/{nested_total}",
        },
        "checking": ("attribution-only; proofs/models not re-checked; "
                     "verdicts cross-checked against the frozen campaign, "
                     "which owns validity"),
        "limits": {"solve_seconds": SOLVE_LIMIT, "process_memory_gib": MEM_GIB},
        "slurm": {
            "partition": PARTITION,
            "cpus_per_task": 4, "mem_per_cpu": "8G", "time": "02:30:00",
            "array_throttle": 40,
            "array_throttle_reason": (
                "user directive 2026-08-19: cap concurrent tasks at 40 to "
                "leave partition capacity for the concurrent Codex Tactix "
                "experiments"),
        },
    }
    if nested_identical != nested_total:
        raise SystemExit(f"nested binary mismatch vs frozen pin: "
                         f"{nested_identical}/{nested_total}")
    atomic_json(campaign / "manifest.json", manifest, exclusive=True)
    print(json.dumps({k: manifest[k] for k in
                      ("campaign", "solver", "slurm", "checking")}, indent=2))
    return 0


def run_one(campaign, index):
    campaign = campaign.resolve()
    man = json.loads((campaign / "manifest.json").read_text())
    inst = man["instances"][index]
    result_path = campaign / "results" / f"{index:05d}.json"
    claim = campaign / "claims" / f"{index:05d}.json"
    atomic_json(claim, {"task": index, "host": os.uname().nodename,
                        "started_utc": dt.datetime.now(dt.timezone.utc).isoformat()},
                exclusive=True)
    if result_path.exists():
        raise SystemExit("result already exists after exclusive claim; refusing rerun")

    source = CACHE / Path(inst["rel"]).name
    if not source.is_file():
        atomic_json(result_path, {"task": index, "instance": inst["rel"],
                                  "outcome": "MISSING_INPUT"})
        return 2
    work = campaign / "scratch" / f"{index:05d}"
    work.mkdir(parents=True, exist_ok=False)
    local = Path(tempfile.mkdtemp(prefix=f"attrib-{index:05d}-"))
    try:
        cnf = local / "instance.cnf"
        with open(cnf, "wb") as out:
            subprocess.run(["xz", "-dc", source], stdout=out, check=True)
        stdout, stderr = work / "solver.stdout", work / "solver.stderr"
        out_dir = local / "out"
        solve = measured([campaign / man["solver"]["executable"], cnf, out_dir],
                         stdout, stderr, SOLVE_LIMIT, work / "solve.time")
        answer = verdict(solve["exit_code"], stdout)
        attribution = parse_attribution(stderr)
        proof = out_dir / "proof.out"
        result = {
            "task": index, "solver": man["solver"]["name"],
            "instance": inst["rel"], "expected": inst["expected"],
            "verdict": answer,
            "expected_match": inst["expected"] == "UNKNOWN"
                              or answer == inst["expected"],
            "solve": solve,
            "node": os.environ.get("SLURMD_NODENAME", os.uname().nodename),
            "attribution": attribution,
            "proof_bytes": proof.stat().st_size if proof.is_file() else None,
            "stdout_bytes": stdout.stat().st_size,
            "stdout_tail": tail(stdout, 8),
            "stderr_tail": tail(stderr),
        }
        atomic_json(result_path, result)
        shutil.rmtree(work)
        return 0
    finally:
        shutil.rmtree(local, ignore_errors=True)


def aggregate(campaign):
    campaign = campaign.resolve()
    man = json.loads((campaign / "manifest.json").read_text())
    results, missing = [], []
    for i in range(len(man["instances"])):
        path = campaign / "results" / f"{i:05d}.json"
        if path.exists():
            results.append(json.loads(path.read_text()))
        else:
            missing.append(i)

    base = json.loads((BASE_CAMPAIGN / "results.json").read_text())
    frozen = {r["instance"]: r for r in base["results"]
              if r.get("solver") == "lymphosat"}

    modules, verdict_flips = {}, []
    for r in results:
        if r.get("outcome") == "MISSING_INPUT":
            continue
        if r["verdict"] in ("SAT", "UNSAT"):
            who = r["attribution"]["responsible"] or "UNATTRIBUTED"
            m = modules.setdefault(who, {"SAT": 0, "UNSAT": 0})
            m[r["verdict"]] += 1
        old = frozen.get(r["instance"])
        if old is not None and old.get("verdict") != r["verdict"]:
            verdict_flips.append({
                "instance": r["instance"],
                "frozen_verdict": old.get("verdict"),
                "rerun_verdict": r["verdict"],
                "frozen_wall": old.get("solve", {}).get("wall_seconds"),
                "rerun_wall": r["solve"]["wall_seconds"],
                "responsible": r["attribution"]["responsible"],
            })

    unattributed = [r["instance"] for r in results
                    if r.get("verdict") in ("SAT", "UNSAT")
                    and not r.get("attribution", {}).get("responsible")]
    summary = {
        "schema": 1,
        "manifest_sha256": sha256(campaign / "manifest.json"),
        "complete": not missing,
        "task_count": len(man["instances"]),
        "result_count": len(results),
        "missing_tasks": missing,
        "solved": sum(r.get("verdict") in ("SAT", "UNSAT") for r in results),
        "unknown": sum(r.get("verdict") == "UNKNOWN" for r in results),
        "modules": {k: modules[k] for k in
                    sorted(modules, key=lambda k: -sum(modules[k].values()))},
        "verdict_flips_vs_frozen": verdict_flips,
        "unattributed_solved": unattributed,
        "results": results,
    }
    atomic_json(campaign / "results.json", summary)
    print(json.dumps({k: v for k, v in summary.items() if k != "results"},
                     indent=2))
    return 0 if summary["complete"] else 2


def submit(campaign):
    campaign = campaign.resolve()
    man_path, launch_path = campaign / "manifest.json", campaign / "launch.json"
    if not man_path.is_file():
        raise SystemExit("prepare campaign first")
    if launch_path.exists():
        raise SystemExit("campaign was already submitted")
    man = json.loads(man_path.read_text())
    count = len(man["instances"])
    throttle = man["slurm"]["array_throttle"]
    me = Path(__file__).resolve()
    array = campaign / "array.sbatch"
    agg = campaign / "aggregate.sbatch"
    array.write_text(f"""#!/usr/bin/env bash
#SBATCH --partition={PARTITION}
#SBATCH --job-name=lymphosat-modlog-tierd
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=4
#SBATCH --mem-per-cpu=8G
#SBATCH --time={man["slurm"]["time"]}
#SBATCH --array=0-{count - 1}%{throttle}
#SBATCH --no-requeue
#SBATCH --output={campaign}/logs/task-%a.log
exec {sys.executable} {me} --campaign {campaign} --run-one "$SLURM_ARRAY_TASK_ID"
""")
    agg.write_text(f"""#!/usr/bin/env bash
#SBATCH --partition={PARTITION}
#SBATCH --job-name=lymphosat-modlog-aggregate
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=01:00:00
#SBATCH --no-requeue
#SBATCH --output={campaign}/logs/aggregate.log
exec {sys.executable} {me} --campaign {campaign} --aggregate
""")
    a = subprocess.run(["sbatch", "--parsable", array], capture_output=True,
                       text=True, check=True).stdout.strip().split(";")[0]
    g = subprocess.run(["sbatch", "--parsable", f"--dependency=afterany:{a}", agg],
                       capture_output=True, text=True, check=True).stdout.strip().split(";")[0]
    launch = {"array_job_id": a, "aggregate_job_id": g, "tasks": count,
              "array_throttle": throttle,
              "manifest_sha256": sha256(man_path),
              "submitted_utc": dt.datetime.now(dt.timezone.utc).isoformat()}
    atomic_json(launch_path, launch, exclusive=True)
    print(json.dumps(launch, indent=2))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--campaign", required=True, type=Path)
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--prepare", action="store_true")
    g.add_argument("--run-one", type=int, metavar="N")
    g.add_argument("--aggregate", action="store_true")
    g.add_argument("--submit", action="store_true")
    args = ap.parse_args()
    if args.prepare:
        return prepare(args.campaign)
    if args.run_one is not None:
        return run_one(args.campaign, args.run_one)
    if args.aggregate:
        return aggregate(args.campaign)
    return submit(args.campaign)


if __name__ == "__main__":
    sys.exit(main() or 0)
