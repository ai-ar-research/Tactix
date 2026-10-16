#!/usr/bin/env python3
"""Run the paper's experiments, locally or as a SLURM array.

    paper/run/bench.py list
    paper/run/bench.py run EXPERIMENT [selection] [--jobs N] [--out DIR]
    paper/run/bench.py slurm EXPERIMENT [selection] --partition P [--out DIR]
    paper/run/bench.py fetch EXPERIMENT [selection]
    paper/run/bench.py collect [--out DIR]

Selection options narrow an experiment: --configurations A,B (names as in
`list`), --sets S (interface only), --subset short (the reduced instance set in
paper/benchmarks/subsets/), --limit N (the first N instances), and
--seconds S (a different time limit). --no-check skips certificate checking.

Each run writes one JSON record per (configuration, instance) to
DIR/EXPERIMENT/records/. `collect` turns them into DIR/EXPERIMENT/results.csv,
the same table that paper/scripts/normalize.py builds from the archived logs,
so `make -C paper LOGS=DIR` draws the paper's tables and figures from a rerun.

Build the solvers and checkers first: paper/run/setup.sh all.
"""
from __future__ import annotations

import argparse
import concurrent.futures
import csv
import fnmatch
import glob
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(ROOT / "tools" / "bench"))
from experiments import EXPERIMENTS  # noqa: E402

TOOLS = HERE / "tools"
BIN = TOOLS / "bin"
CACHE = HERE / "instances"
DEFAULT_OUT = HERE / "results"
GBD_URL = "https://benchmark-database.de/file/{}"
PENALTY = 10000


# --- instances ----------------------------------------------------------------

def _read_list(path: Path) -> list[list[str]]:
    return [line.split() for line in (ROOT / path).read_text().splitlines()
            if line.strip() and not line.startswith("#")]


def instances(name: str, spec: dict, set_name: str | None = None) -> list[dict]:
    """Every instance of an experiment as {set, instance, source, ...}."""
    kind = spec["kind"]
    if kind == "sets":
        return [item for label, sub in spec["sets"].items() for item in instances(name, sub, label)]
    out = []
    if kind == "gbd":
        years = {}
        if "years" in spec:
            with (ROOT / spec["years"]).open() as stream:
                for row in csv.DictReader(stream, delimiter="\t"):
                    years[row["hash"]] = "+".join(y for y in ("SC2024", "SC2025", "SC2026") if row[y] == "1")
        for fields in _read_list(Path(spec["list"])):
            content_hash, file_name = fields[0], fields[1]
            instance = file_name if file_name.startswith(content_hash) else f"{content_hash}-{file_name}"
            out.append({"set": set_name, "instance": instance, "source": "gbd", "hash": content_hash,
                        "competition": years.get(content_hash, "")})
    elif kind == "archive":
        for fields in _read_list(Path(spec["list"])):
            out.append({"set": set_name, "instance": fields[0], "source": "archive",
                        "archive": spec["archive"]})
    elif kind == "files":
        for path in sorted(glob.glob(str(ROOT / spec["glob"]))):
            out.append({"set": set_name, "instance": str(Path(path).relative_to(ROOT)), "source": "file"})
    if set_name and "limits" in spec:
        for item in out:
            item["limits"] = spec["limits"]
    return out


def fetch(item: dict) -> Path:
    """The instance as a (possibly compressed) local file."""
    if item["source"] == "file":
        return ROOT / item["instance"]
    if item["source"] == "archive":
        target = CACHE / "archive" / item["instance"]
        if not target.exists():
            target.parent.parent.mkdir(parents=True, exist_ok=True)
            subprocess.run(["tar", "-xJf", ROOT / item["archive"], "-C", target.parent.parent],
                           check=True)
        return target
    target = CACHE / "gbd" / item["instance"]
    if not target.exists():
        target.parent.mkdir(parents=True, exist_ok=True)
        partial = target.with_suffix(target.suffix + ".part")
        with urllib.request.urlopen(GBD_URL.format(item["hash"]), timeout=600) as response, \
                partial.open("wb") as stream:
            shutil.copyfileobj(response, stream)
        partial.rename(target)
    return target


def decompress(source: Path, target: Path) -> None:
    opener = {".xz": ["xz", "-dc"], ".gz": ["gzip", "-dc"], ".bz2": ["bzip2", "-dc"],
              ".lzma": ["xz", "-dc"]}.get(source.suffix)
    if opener is None:
        shutil.copyfile(source, target)
        return
    with target.open("wb") as stream:
        subprocess.run([*opener, source], stdout=stream, check=True)


# --- measured execution -------------------------------------------------------

def measured(command, stdout: Path, stderr: Path, seconds: float, env=None) -> dict:
    """Run in a new process group; wall time, peak RSS (KiB), exit code, timeout."""
    started = time.monotonic()
    timed_out = False
    with stdout.open("wb") as out, stderr.open("wb") as err:
        process = subprocess.Popen([str(c) for c in command], stdout=out, stderr=err,
                                   start_new_session=True, env=env)
        deadline = started + seconds
        while True:
            pid, status, usage = os.wait4(process.pid, os.WNOHANG)
            if pid:
                break
            if time.monotonic() > deadline:
                timed_out = True
                os.killpg(process.pid, signal.SIGTERM)
                for _ in range(100):
                    pid, status, usage = os.wait4(process.pid, os.WNOHANG)
                    if pid:
                        break
                    time.sleep(0.1)
                else:
                    os.killpg(process.pid, signal.SIGKILL)
                    pid, status, usage = os.wait4(process.pid, 0)
                break
            time.sleep(0.05)
    process.returncode = os.waitstatus_to_exitcode(status)
    try:
        os.killpg(process.pid, signal.SIGKILL)  # stray descendants
    except ProcessLookupError:
        pass
    return {"exit_code": None if timed_out else process.returncode,
            "wall_seconds": round(time.monotonic() - started, 3),
            "peak_rss_kb": usage.ru_maxrss, "timed_out": timed_out}


def answer(stdout: Path) -> str:
    states = re.findall(r"^s\s+(SATISFIABLE|UNSATISFIABLE)\s*$",
                        stdout.read_text(errors="replace"), re.M)
    if len(states) == 1:
        return "SAT" if states[0] == "SATISFIABLE" else "UNSAT"
    return "UNKNOWN"


def sat_protocol_ok(stdout: Path) -> bool:
    lines = [line for line in stdout.read_text(errors="replace").splitlines() if line.startswith("v ")]
    try:
        literals = [int(token) for line in lines for token in line[2:].split()]
    except ValueError:
        return False
    if not literals or literals[-1] != 0 or 0 in literals[:-1] or any(len(line) > 4096 for line in lines):
        return False
    assigned = set(literals[:-1])
    return not any(-literal in assigned for literal in assigned)


# --- certificate checking -----------------------------------------------------

def _stage_ok(name: str, code, text: str) -> bool:
    if code != 0:
        return False
    return {"drat-trim": "s VERIFIED" in text and "s NOT VERIFIED" not in text,
            "cake_lpr": "s VERIFIED UNSAT" in text,
            "veripb": "s VERIFIED UNSATISFIABLE" in text,
            "cake_pb_cnf": "VERIFIED" in text,
            "dsr-compress": "ERROR" not in text and "not supported" not in text,
            "sr2drat": True}[name]


def check_pipeline(kind: str, cnf: Path, proof: Path, work: Path, seconds: float,
                   memory_gib: float) -> dict:
    """Check an UNSAT certificate; returns {result, pipeline, wall_seconds, stages}."""
    started = time.monotonic()
    if kind == "tactix":
        from proof_oracle import check_routed  # tools/bench/proof_oracle.py
        verified, stage, output, route = check_routed(
            BIN / "dsr-trim", BIN / "dsr-compress", BIN / "srcheck", BIN / "drat-trim",
            BIN / "cake_lpr", cnf, proof, work, seconds,
            veripb=BIN / "veripb", cake_pb=BIN / "cake_pb_cnf")
        return {"result": "verified" if verified else "not-verified", "pipeline": f"tactix-{route}",
                "wall_seconds": round(time.monotonic() - started, 3),
                "failure": None if verified else f"{stage}: {output[-500:]}"}
    lrat, text_dsr, drat = work / "proof.lrat", work / "proof.text.dsr", work / "proof.drat"
    if kind == "drat":
        stages = [("drat-trim", [BIN / "drat-trim", cnf, proof, "-L", lrat], None),
                  ("cake_lpr", [BIN / "cake_lpr", cnf, lrat], None)]
    elif kind == "satsuma":
        stages = [("dsr-compress", [BIN / "satsuma-dsr-compress", "-d", proof, text_dsr], None),
                  ("sr2drat", [BIN / "sr2drat", cnf, text_dsr], drat),
                  ("drat-trim", [BIN / "drat-trim", cnf, drat, "-L", lrat], None),
                  ("cake_lpr", [BIN / "cake_lpr", cnf, lrat], None)]
    elif kind == "pbp":
        kernel = work / "proof.kernel.pbp"
        stages = [("veripb", [BIN / "veripb", "-u", "--cnf", "--elaborate", kernel, cnf, proof], None),
                  ("cake_pb_cnf", [BIN / "cake_pb_cnf", cnf, kernel], None)]
    else:
        raise ValueError(kind)
    env = dict(os.environ)
    if memory_gib >= 32:  # CakeML's default 4 GiB heap rejects large valid proofs
        env["CML_HEAP_SIZE"] = "24576"
    log = []
    for number, (name, command, redirect) in enumerate(stages, 1):
        out = redirect or work / f"check-{number}.out"
        err = work / f"check-{number}.err"
        remaining = max(1.0, seconds - (time.monotonic() - started))
        run = measured(command, out, err, remaining, env)
        text = ("" if redirect else out.read_text(errors="replace")) + err.read_text(errors="replace")
        ok = _stage_ok(name, run["exit_code"], text) and (not redirect or out.stat().st_size > 0)
        log.append({"name": name, **run, "result": "verified" if ok else "not-verified"})
        if not ok:
            break
    verified = len(log) == len(stages) and all(s["result"] == "verified" for s in log)
    return {"result": "verified" if verified else "not-verified", "pipeline": kind,
            "wall_seconds": round(time.monotonic() - started, 3),
            "timed_out": any(s["timed_out"] for s in log), "stages": log}


# --- one task -------------------------------------------------------------------

def plan(name: str, args) -> list[dict]:
    experiment = EXPERIMENTS[name]
    items = instances(name, experiment["instances"])
    if getattr(args, "sets", None):
        wanted = set(args.sets.split(","))
        items = [i for i in items if i["set"] in wanted]
    if getattr(args, "subset", None):
        subset = ROOT / "paper/benchmarks/subsets" / f"{name}-{args.subset}.txt"
        keep = {line.strip() for line in subset.read_text().splitlines()
                if line.strip() and not line.startswith("#")}
        items = [i for i in items if i["instance"] in keep or Path(i["instance"]).name in keep]
    if getattr(args, "limit", None):
        items = items[:args.limit]
    configurations = list(experiment["configurations"])
    if getattr(args, "configurations", None):
        configurations = args.configurations.split(",")
        unknown = set(configurations) - set(experiment["configurations"])
        if unknown:
            sys.exit(f"unknown configuration(s) for {name}: {', '.join(sorted(unknown))}")
    tasks = []
    for item in items:  # instance-major, so both arms of a pair run close together
        for configuration in configurations:
            limits = dict(experiment["limits"], **item.get("limits", {}))
            if getattr(args, "seconds", None):
                limits["seconds"] = args.seconds
            tasks.append({"experiment": name, "configuration": configuration, **item,
                          "limits": limits, "check": not getattr(args, "no_check", False)})
    return tasks


def run_task(task: dict, out: Path, index: int) -> dict:
    record_path = out / task["experiment"] / "records" / f"{index:06d}.json"
    if record_path.exists():
        return json.loads(record_path.read_text())
    spec = EXPERIMENTS[task["experiment"]]["configurations"][task["configuration"]]
    limits = task["limits"]
    work = Path(tempfile.mkdtemp(prefix="tactix-bench-"))
    try:
        cnf, proof, proof_dir = work / "formula.cnf", work / "proof", work / "proof-dir"
        decompress(fetch(task), cnf)
        proof_dir.mkdir()
        substitutions = {"{cnf}": str(cnf), "{proof}": str(proof), "{proof_dir}": str(proof_dir)}
        command = [substitutions.get(part, part) for part in spec["command"]]
        command[0] = str(HERE / command[0]) if not Path(command[0]).is_absolute() else command[0]
        env = dict(os.environ, **spec.get("environment", {}))
        stdout, stderr = work / "solver.out", work / "solver.err"
        solve = measured(command, stdout, stderr, limits["seconds"], env)
        verdict = answer(stdout)
        certificate = proof if "{proof}" in spec["command"] else proof_dir / "proof.out"
        record = {**{k: task[k] for k in ("experiment", "configuration", "set", "instance")},
                  "competition": task.get("competition", ""), "verdict": verdict, "solve": solve,
                  "limits": limits, "proof_kind": spec["proof"], "certificate_bytes": None,
                  "check": None, "model_ok": None, "commit_tag": spec.get("commit_tag"),
                  "host": os.uname().nodename,
                  "solver_stdout_tail": stdout.read_text(errors="replace").splitlines()[-12:]}
        if verdict == "SAT":
            model = measured([BIN / "checkmodel", cnf, stdout], work / "model.out", work / "model.err", 600)
            record["model_ok"] = model["exit_code"] == 0 and sat_protocol_ok(stdout)
        elif verdict == "UNSAT" and spec["proof"] != "none":
            if certificate.is_file():
                record["certificate_bytes"] = certificate.stat().st_size
                if task["check"]:
                    check_work = work / "check"
                    check_work.mkdir()
                    record["check"] = check_pipeline(spec["proof"], cnf, certificate, check_work,
                                                     limits["check_seconds"], limits["memory_gib"])
            else:
                record["check"] = {"result": "not-verified", "failure": "no certificate written"}
    finally:
        shutil.rmtree(work, ignore_errors=True)
    record_path.parent.mkdir(parents=True, exist_ok=True)
    partial = record_path.with_suffix(".tmp")
    partial.write_text(json.dumps(record, indent=1) + "\n")
    partial.rename(record_path)
    return record


# --- collect: records -> results.csv ---------------------------------------------

def _row(record: dict) -> dict:
    solve = record["solve"]
    check = record.get("check") or {}
    verdict = record["verdict"]
    answered = verdict in ("SAT", "UNSAT")
    proof_free = record["proof_kind"] == "none"
    if verdict == "SAT":
        verified = bool(record["model_ok"])
    elif verdict == "UNSAT":
        verified = check.get("result") == "verified"
    else:
        verified = False
    counted = answered and (verified or (verdict == "UNSAT" and proof_free))
    if verified:
        status = "verified"
    elif verdict == "SAT":
        status = "model-rejected"
    elif verdict == "UNSAT":
        status = "unsat-unchecked" if proof_free or not check else "unverified"
    elif solve["timed_out"]:
        status = "timeout"
    else:
        status = f"no-answer (exit {solve['exit_code']})"
    wall = solve["wall_seconds"]
    return {"set": record.get("set") or "", "configuration": record["configuration"],
            "instance": record["instance"], "competition": record.get("competition", ""),
            "commit": record.get("commit") or "", "verdict": verdict, "solve_seconds": wall,
            "timed_out": solve["timed_out"], "status": status, "verified": verified,
            "counted": counted, "proof_mode": "none" if proof_free else "proof",
            "proof_check_result": check.get("result") or "", "check_seconds": check.get("wall_seconds", ""),
            "proof_bytes": record.get("certificate_bytes") or "",
            "par2_seconds": round(wall, 3) if (counted if proof_free else verified) else PENALTY,
            "solved_checked": answered and verified, "kissat_screen_seconds": record.get("kissat_screen_seconds", ""),
            "rechecked": False}


def collect(out: Path) -> None:
    for directory in sorted(p for p in out.iterdir() if (p / "records").is_dir()):
        records = [json.loads(p.read_text()) for p in sorted((directory / "records").glob("*.json"))]
        if directory.name == "chvatal":
            selection = json.loads((ROOT / "paper/logs/chvatal/paper-hard.json").read_text())
            screen = {Path(e["file"]).name: e["kissat_seconds"] for e in selection["selected"]}
            for record in records:
                record["kissat_screen_seconds"] = screen.get(Path(record["instance"]).name.removesuffix(".xz"), "")
        for record in records:
            tag = record.get("commit_tag")
            if tag and (TOOLS / "tactix" / tag / "COMMIT").exists():
                record["commit"] = (TOOLS / "tactix" / tag / "COMMIT").read_text().strip()[:9]
        rows = [_row(r) for r in records]
        if not rows:
            continue
        with (directory / "results.csv").open("w", newline="") as stream:
            writer = csv.DictWriter(stream, list(rows[0]), lineterminator="\n")
            writer.writeheader()
            writer.writerows(rows)
        done = sum(r["verdict"] in ("SAT", "UNSAT") for r in rows)
        print(f"{directory.name}: {len(rows)} records, {done} answered, "
              f"{sum(r['verified'] for r in rows)} verified -> {directory / 'results.csv'}")


# --- command line -----------------------------------------------------------------

def _selection(parser):
    parser.add_argument("experiment", choices=sorted(EXPERIMENTS))
    parser.add_argument("--configurations")
    parser.add_argument("--sets")
    parser.add_argument("--subset")
    parser.add_argument("--limit", type=int)
    parser.add_argument("--seconds", type=float)
    parser.add_argument("--no-check", action="store_true")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("list")
    run = commands.add_parser("run")
    _selection(run)
    run.add_argument("--jobs", type=int, default=1)
    slurm = commands.add_parser("slurm")
    _selection(slurm)
    slurm.add_argument("--partition", required=True)
    slurm.add_argument("--sbatch", default="", help="extra sbatch options")
    slurm.add_argument("--dry-run", action="store_true")
    task = commands.add_parser("task")
    task.add_argument("plan", type=Path)
    task.add_argument("index", type=int)
    prefetch = commands.add_parser("fetch", help="download the selected instances for offline runs")
    _selection(prefetch)
    gather = commands.add_parser("collect")
    gather.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = parser.parse_args()

    if args.command == "list":
        for name, experiment in EXPERIMENTS.items():
            print(f"{name:11} {experiment['description']}")
            items = instances(name, experiment["instances"])
            limits = experiment["limits"]
            print(f"{'':11} {len(items)} instances, {limits['seconds']:g} s, {limits['memory_gib']} GiB, "
                  f"{limits['cpus']} CPUs; configurations: {', '.join(experiment['configurations'])}")
        return
    if args.command == "collect":
        collect(args.out)
        return
    if args.command == "task":
        tasks = json.loads(args.plan.read_text())
        run_task(tasks[args.index], args.plan.parent.parent, args.index)
        return

    tasks = plan(args.experiment, args)
    if args.command == "fetch":
        seen = set()
        for task in tasks:
            if task["instance"] not in seen:
                seen.add(task["instance"])
                fetch(task)
        print(f"{args.experiment}: {len(seen)} instances in {CACHE}")
        return
    directory = args.out / args.experiment
    directory.mkdir(parents=True, exist_ok=True)
    plan_path = directory / "plan.json"
    if plan_path.exists() and json.loads(plan_path.read_text()) != tasks:
        sys.exit(f"{plan_path} holds a different selection; use another --out or remove it")
    plan_path.write_text(json.dumps(tasks, indent=1) + "\n")
    print(f"{args.experiment}: {len(tasks)} tasks -> {directory}")
    if args.command == "run":
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            futures = {pool.submit(run_task, t, args.out, i): t for i, t in enumerate(tasks)}
            for done, future in enumerate(concurrent.futures.as_completed(futures), 1):
                record = future.result()
                check = (record.get("check") or {}).get("result", "")
                print(f"[{done}/{len(tasks)}] {record['configuration']:24} {Path(record['instance']).name[:48]:48} "
                      f"{record['verdict']:7} {record['solve']['wall_seconds']:9.2f} s {check}", flush=True)
        collect(args.out)
        return
    # slurm: one array task per (configuration, instance); full width
    limits = tasks[0]["limits"]
    longest = max(t["limits"]["seconds"] + (t["limits"]["check_seconds"] if t["check"] else 0) for t in tasks)
    hours = int(longest // 3600) + 1
    script = directory / "array.sbatch"
    script.write_text(f"""#!/usr/bin/env bash
#SBATCH -J tactix-{args.experiment}
#SBATCH -p {args.partition}
#SBATCH -c {limits['cpus']}
#SBATCH --mem={limits['memory_gib']}G
#SBATCH -t {hours}:00:00
#SBATCH -a 0-{len(tasks) - 1}
#SBATCH -o {directory}/logs/%A_%a.log
{args.sbatch and '#SBATCH ' + args.sbatch}
exec python3 {Path(__file__).resolve()} task {plan_path} "$SLURM_ARRAY_TASK_ID"
""")
    (directory / "logs").mkdir(exist_ok=True)
    if args.dry_run:
        print(f"wrote {script}")
        return
    job = subprocess.run(["sbatch", "--parsable", script], check=True, capture_output=True, text=True)
    print(f"submitted job {job.stdout.strip()}; when it finishes: {Path(__file__).name} collect --out {args.out}")


if __name__ == "__main__":
    main()
