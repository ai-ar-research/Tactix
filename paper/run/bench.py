#!/usr/bin/env python3
"""Run the paper's experiments, locally or as a SLURM array.

    paper/run/bench.py list
    paper/run/bench.py run EXPERIMENT [--profile P] [selection] [--jobs N] [--out DIR]
    paper/run/bench.py slurm EXPERIMENT [--profile P] [selection] --partition P [--out DIR]
    paper/run/bench.py fetch EXPERIMENT [--profile P] [selection]
    paper/run/bench.py collect [--out DIR]

--profile chooses the instances and resource limits: full (the paper's, the
default), reduced (the artifact-evaluation subsets and limits) or smoke (a few
quick instances); see experiments.py. Selection options narrow it further:
--configurations A,B (names as in `list`), --sets S (interface only),
--subset NAME (paper/benchmarks/subsets/EXPERIMENT-NAME.txt), --limit N (the
first N instances), and --seconds, --check-seconds, --memory-gib,
--check-memory-gib, --cpus
(different limits). --no-check skips certificate checking.

Every run is limited in wall-clock time and in memory: the resident memory of
the solver (or of the whole checking pipeline) and all its child processes is
sampled several times a second, and the run is stopped when it exceeds the
limit (reported as a memory-out).

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
import fcntl
import glob
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
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
# Downloaded instances (TACTIX_INSTANCES overrides; the artifact ships the
# reduced sets there, so they run offline).
CACHE = Path(os.environ.get("TACTIX_INSTANCES", HERE / "instances"))
DEFAULT_OUT = HERE / "results"
GBD_URL = "https://benchmark-database.de/file/{}"
PAGE_KB = os.sysconf("SC_PAGE_SIZE") // 1024


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
                        "file": file_name, "competition": years.get(content_hash, "")})
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
            (CACHE / "archive").mkdir(parents=True, exist_ok=True)
            with (CACHE / "archive" / ".lock").open("w") as lock:
                fcntl.flock(lock, fcntl.LOCK_EX)
                if not target.exists():
                    subprocess.run(["tar", "--no-same-owner", "-xJf", ROOT / item["archive"], "-C", CACHE / "archive"],
                                   check=True)
        return target
    target = CACHE / "gbd" / item["instance"]
    if target.exists():
        return target
    target.parent.mkdir(parents=True, exist_ok=True)
    # One download per instance even when parallel tasks share it.
    with (target.parent / f".{target.name}.lock").open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        if not target.exists():
            partial = target.with_name(f".{target.name}.{os.getpid()}.{threading.get_ident()}.part")
            with urllib.request.urlopen(GBD_URL.format(item["hash"]), timeout=600) as response, \
                    partial.open("wb") as stream:
                shutil.copyfileobj(response, stream)
            subprocess.run(["xz", "-t", partial], check=True)
            os.replace(partial, target)
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

def session_rss_kb(session: int) -> int:
    """Resident memory (KiB) of every process in a session."""
    total = 0
    for entry in os.scandir("/proc"):
        if not entry.name.isdigit():
            continue
        try:
            with open(f"/proc/{entry.name}/stat", "rb") as stream:
                data = stream.read()
        except OSError:
            continue
        fields = data[data.rindex(b")") + 2:].split()  # fields 3.. of proc(5)
        if int(fields[3]) == session:
            total += int(fields[21]) * PAGE_KB
    return total


def measured(command, stdout: Path, stderr: Path, seconds: float, env=None,
             memory_gib: float | None = None, session: bool = True) -> dict:
    """Run under a time and memory limit; wall time, peak RSS (KiB), exit code,
    timeout, memory-out. With session=True the command gets a session of its own
    and the memory limit covers it and all its descendants; otherwise it gets a
    process group (inside a caller that is itself measured)."""
    started = time.monotonic()
    timed_out = memout = False
    limit_kb = memory_gib * 1024 * 1024 if memory_gib else None
    sampled_kb = 0
    next_sample = started
    with stdout.open("wb") as out, stderr.open("wb") as err:
        process = subprocess.Popen([str(c) for c in command], stdout=out, stderr=err, env=env,
                                   **({"start_new_session": True} if session else {"process_group": 0}))
        deadline = started + seconds
        while True:
            pid, status, usage = os.wait4(process.pid, os.WNOHANG)
            if pid:
                break
            now = time.monotonic()
            if session and now >= next_sample:
                next_sample = now + 0.2
                sampled_kb = max(sampled_kb, session_rss_kb(process.pid))
                memout = limit_kb is not None and sampled_kb > limit_kb
            if now > deadline or memout:
                timed_out = not memout
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
    return {"exit_code": None if timed_out or memout else process.returncode,
            "wall_seconds": round(time.monotonic() - started, 3),
            "peak_rss_kb": max(usage.ru_maxrss, sampled_kb), "timed_out": timed_out,
            "memout": memout}


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
            "dsr-compress": "ERROR" not in text and "not supported" not in text}[name]


def cake_heap_mib(memory_gib: float) -> int:
    """CakeML heap for cake_lpr and cake_pb: the 24 GiB the paper's checks used
    (with 64 GB), or three quarters of a smaller memory limit."""
    return int(min(24576, memory_gib * 1024 * 3 // 4))


def check_pipeline(kind: str, cnf: Path, proof: Path, work: Path, seconds: float,
                   memory_gib: float) -> dict:
    """Check an UNSAT certificate; returns {result, pipeline, wall_seconds, stages}.
    Runs inside `bench.py check`, whose memory the caller limits as a whole."""
    started = time.monotonic()
    if kind in ("satsuma", "tactix"):
        # checker_seconds: the time spent in the checkers themselves (every one
        # runs through proof_oracle.run_captured), without the routing around them.
        import proof_oracle  # tools/bench/proof_oracle.py
        spent, run_captured = [], proof_oracle.run_captured
        def timed(*args, **kwargs):
            begun = time.monotonic()
            try:
                return run_captured(*args, **kwargs)
            finally:
                spent.append(time.monotonic() - begun)
        proof_oracle.run_captured = timed
    if kind == "satsuma":  # the verified SR pipeline the paper's runs used for Satsuma
        verified, stage, output = proof_oracle.check_dsr(BIN / "dsr-trim", BIN / "dsr-compress",
                                                         BIN / "srcheck", cnf, proof, work, seconds)
        return {"result": "verified" if verified else "not-verified", "pipeline": "satsuma-dsr",
                "wall_seconds": round(time.monotonic() - started, 3), "checker_seconds": round(sum(spent), 3),
                "failure": None if verified else f"{stage}: {output[-500:]}"}
    if kind == "tactix":
        verified, stage, output, route = proof_oracle.check_routed(
            BIN / "dsr-trim", BIN / "dsr-compress", BIN / "srcheck", BIN / "drat-trim",
            BIN / "cake_lpr", cnf, proof, work, seconds,
            veripb=BIN / "veripb", cake_pb=BIN / "cake_pb_cnf")
        return {"result": "verified" if verified else "not-verified", "pipeline": f"tactix-{route}",
                "wall_seconds": round(time.monotonic() - started, 3), "checker_seconds": round(sum(spent), 3),
                "failure": None if verified else f"{stage}: {output[-500:]}"}
    lrat = work / "proof.lrat"
    if kind == "drat":
        stages = [("drat-trim", [BIN / "drat-trim", cnf, proof, "-L", lrat], None),
                  ("cake_lpr", [BIN / "cake_lpr", cnf, lrat], None)]
    elif kind == "pbp":
        kernel = work / "proof.kernel.pbp"
        stages = [("veripb", [BIN / "veripb", "-u", "--cnf", "--elaborate", kernel, cnf, proof], None),
                  ("cake_pb_cnf", [BIN / "cake_pb_cnf", cnf, kernel], None)]
    else:
        raise ValueError(kind)
    env = dict(os.environ)
    from proof_oracle import CAKE_LPR_ENVIRONMENT
    env.update(CAKE_LPR_ENVIRONMENT)
    log = []
    for number, (name, command, redirect) in enumerate(stages, 1):
        out = redirect or work / f"check-{number}.out"
        err = work / f"check-{number}.err"
        remaining = max(1.0, seconds - (time.monotonic() - started))
        run = measured(command, out, err, remaining, env, session=False)
        text = ("" if redirect else out.read_text(errors="replace")) + err.read_text(errors="replace")
        ok = _stage_ok(name, run["exit_code"], text)
        log.append({"name": name, **run, "result": "verified" if ok else "not-verified"})
        if not ok:
            break
    verified = len(log) == len(stages) and all(s["result"] == "verified" for s in log)
    return {"result": "verified" if verified else "not-verified", "pipeline": kind,
            "wall_seconds": round(time.monotonic() - started, 3),
            "checker_seconds": round(sum(s["wall_seconds"] for s in log), 3),
            "timed_out": any(s["timed_out"] for s in log), "stages": log}


def checked(kind: str, cnf: Path, proof: Path, work: Path, limits: dict) -> dict:
    """Run check_pipeline in a child process under the memory limit."""
    check_work = work / "check"
    check_work.mkdir()
    memory = limits.get("check_memory_gib", limits["memory_gib"])
    env = dict(os.environ, TACTIX_CAKE_HEAP_MIB=str(cake_heap_mib(memory)))
    run = measured([sys.executable, Path(__file__).resolve(), "check", kind, cnf, proof, check_work,
                    str(limits["check_seconds"]), str(memory)],
                   work / "check.json", work / "check.err", limits["check_seconds"] + 60, env, memory)
    try:
        result = json.loads((work / "check.json").read_text())
    except ValueError:
        failure = ("memory-out" if run["memout"] else "timeout" if run["timed_out"]
                   else (work / "check.err").read_text(errors="replace")[-500:])
        result = {"result": "not-verified", "pipeline": kind, "failure": failure}
    result.update(wall_seconds=run["wall_seconds"], peak_rss_kb=run["peak_rss_kb"],
                  memout=run["memout"], timed_out=run["timed_out"] or result.get("timed_out", False))
    return result


# --- one task -------------------------------------------------------------------

def _subset(items: list[dict], path: Path) -> list[dict]:
    keep = [line.split()[0] for line in path.read_text().splitlines()
            if line.strip() and not line.startswith("#")]
    wanted = set(keep)
    chosen = [i for i in items if i["instance"] in wanted or Path(i["instance"]).name in wanted
              or i.get("file") in wanted]
    found = {key for i in chosen for key in (i["instance"], Path(i["instance"]).name, i.get("file"))}
    missing = wanted - found
    if missing:
        sys.exit(f"{path}: not in the experiment: {', '.join(sorted(missing)[:5])}")
    return chosen


def plan(name: str, args) -> list[dict]:
    experiment = EXPERIMENTS[name]
    items = instances(name, experiment["instances"])
    profile_name = getattr(args, "profile", None) or "full"
    if profile_name != "full" and profile_name not in experiment.get("profiles", {}):
        sys.exit(f"experiment {name} has no profile {profile_name}")
    profile = experiment.get("profiles", {}).get(profile_name, {})
    if "subset" in profile:
        items = _subset(items, ROOT / profile["subset"])
    if getattr(args, "sets", None):
        wanted = set(args.sets.split(","))
        items = [i for i in items if i["set"] in wanted]
    if getattr(args, "subset", None):
        items = _subset(items, ROOT / "paper/benchmarks/subsets" / f"{name}-{args.subset}.txt")
    if getattr(args, "limit", None):
        items = items[:args.limit]
    configurations = list(profile.get("configurations", experiment["configurations"]))
    if getattr(args, "configurations", None):
        configurations = args.configurations.split(",")
        unknown = set(configurations) - set(experiment["configurations"])
        if unknown:
            sys.exit(f"unknown configuration(s) for {name}: {', '.join(sorted(unknown))}")
    tasks = []
    for item in items:  # instance-major, so both arms of a pair run close together
        for configuration in configurations:
            if "limits" in profile:
                limits = dict(profile["limits"], **profile.get("sets", {}).get(item["set"], {}))
            else:
                limits = dict(experiment["limits"], **item.get("limits", {}))
            for option, key in (("seconds", "seconds"), ("check_seconds", "check_seconds"),
                                ("memory_gib", "memory_gib"), ("check_memory_gib", "check_memory_gib"),
                                ("cpus", "cpus")):
                if getattr(args, option, None):
                    limits[key] = getattr(args, option)
            task = {"experiment": name, "configuration": configuration, **item,
                    "limits": limits,
                    "check": not getattr(args, "no_check", False) and profile.get("check", True),
                    "profile": profile_name}
            tasks.append(task)
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
        stdout, stderr = work / "solver.out", work / "solver.err"
        solve = measured(command, stdout, stderr, limits["seconds"], None, limits["memory_gib"])
        verdict = answer(stdout)
        certificate = proof if "{proof}" in spec["command"] else proof_dir / "proof.out"
        record = {**{k: task[k] for k in ("experiment", "configuration", "set", "instance")},
                  "profile": task.get("profile", "full"),
                  "competition": task.get("competition", ""), "verdict": verdict, "solve": solve,
                  "limits": limits, "proof_kind": spec["proof"], "certificate_bytes": None,
                  "check": None, "model_ok": None, "build": spec.get("build"),
                  "host": os.uname().nodename,
                  "solver_stdout_tail": stdout.read_text(errors="replace").splitlines()[-12:]}
        if verdict == "SAT":
            model = measured([BIN / "checkmodel", cnf, stdout], work / "model.out", work / "model.err", 600,
                             memory_gib=limits["memory_gib"])
            # Counted only if the assignment satisfies the formula and is printed
            # in the competition format (one final 0), as in the paper.
            record["model_satisfies"] = model["exit_code"] == 0
            record["model_format_ok"] = sat_protocol_ok(stdout)
            record["model_ok"] = record["model_satisfies"] and record["model_format_ok"]
        elif verdict == "UNSAT" and spec["proof"] != "none":
            if certificate.is_file():
                record["certificate_bytes"] = certificate.stat().st_size
                if task["check"]:
                    record["check"] = checked(spec["proof"], cnf, certificate, work, limits)
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
        status = "model-nonstandard-output" if record.get("model_format_ok") is False else "model-rejected"
    elif verdict == "UNSAT":
        status = ("unsat-unchecked" if proof_free or not check else
                  "check-memory-out" if check.get("memout") else "unverified")
    elif solve["timed_out"]:
        status = "timeout"
    elif solve.get("memout"):
        status = "memory-out"
    else:
        status = f"no-answer (exit {solve['exit_code']})"
    wall = solve["wall_seconds"]
    return {"set": record.get("set") or "", "configuration": record["configuration"],
            "instance": record["instance"], "competition": record.get("competition", ""),
            "commit": record.get("commit") or "", "verdict": verdict, "solve_seconds": wall,
            "timed_out": solve["timed_out"], "status": status, "verified": verified,
            "counted": counted, "proof_mode": "none" if proof_free else "proof",
            "proof_check_result": check.get("result") or "", "check_seconds": check.get("wall_seconds", ""),
            "checker_seconds": check.get("checker_seconds", ""),
            "proof_bytes": record.get("certificate_bytes") or "",
            "par2_seconds": (round(wall, 3) if (counted if proof_free else verified)
                             else 2 * record["limits"]["seconds"]),
            "profile": record.get("profile", "full"),
            "solve_limit_seconds": record["limits"]["seconds"],
            "check_limit_seconds": record["limits"]["check_seconds"],
            "memory_limit_gib": record["limits"]["memory_gib"],
            "kissat_screen_seconds": record.get("kissat_screen_seconds", "")}


def collect(out: Path) -> None:
    for directory in sorted(p for p in out.iterdir() if (p / "records").is_dir()):
        records = [json.loads(p.read_text()) for p in sorted((directory / "records").glob("*.json"))]
        if directory.name == "chvatal":
            selection = json.loads((ROOT / "paper/logs/chvatal/paper-hard.json").read_text())
            screen = {Path(e["file"]).name: e["kissat_seconds"] for e in selection["selected"]}
            for record in records:
                record["kissat_screen_seconds"] = screen.get(Path(record["instance"]).name.removesuffix(".xz"), "")
        for record in records:
            build = record.get("build")
            if build and (TOOLS / "tactix" / build / "COMMIT").exists():
                record["commit"] = (TOOLS / "tactix" / build / "COMMIT").read_text().strip()[:9]
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
    parser.add_argument("experiments", nargs="+", choices=sorted(EXPERIMENTS), metavar="EXPERIMENT")
    parser.add_argument("--profile", choices=("full", "reduced", "smoke"), default="full")
    parser.add_argument("--configurations")
    parser.add_argument("--sets")
    parser.add_argument("--subset")
    parser.add_argument("--limit", type=int)
    parser.add_argument("--seconds", type=float, help="solving limit per run (s)")
    parser.add_argument("--check-seconds", type=float, help="checking limit per run (s)")
    parser.add_argument("--memory-gib", type=float, help="memory limit per run (GiB)")
    parser.add_argument("--check-memory-gib", type=float,
                        help="memory limit of the checking pipeline (GiB; default: --memory-gib)")
    parser.add_argument("--cpus", type=int, help="cores reserved per run (SLURM -c)")
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
    slurm.add_argument("--sbatch", default="", help="extra sbatch options, e.g. an account")
    slurm.add_argument("--concurrency", type=int, help="at most this many array tasks at a time")
    slurm.add_argument("--dry-run", action="store_true")
    task = commands.add_parser("task")
    task.add_argument("plan", type=Path)
    task.add_argument("index", type=int)
    prefetch = commands.add_parser("fetch", help="download the selected instances for offline runs")
    _selection(prefetch)
    check = commands.add_parser("check", help="(internal) check one certificate, print JSON")
    for name in ("kind", "cnf", "proof", "work", "seconds", "memory_gib"):
        check.add_argument(name)
    gather = commands.add_parser("collect")
    gather.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = parser.parse_args()

    if args.command == "list":
        for name, experiment in EXPERIMENTS.items():
            print(f"{name:11} {experiment['description']}")
            items = instances(name, experiment["instances"])
            limits = experiment["limits"]
            print(f"{'':11} full:    {len(items)} instances, {limits['seconds']:g} s, "
                  f"{limits['memory_gib']} GiB, {limits['cpus']} CPUs; "
                  f"configurations: {', '.join(experiment['configurations'])}")
            for profile, spec in experiment.get("profiles", {}).items():
                tasks = plan(name, argparse.Namespace(profile=profile))
                runs = len(tasks)
                limits = spec["limits"]
                print(f"{'':11} {profile + ':':8} {len({t['instance'] for t in tasks})} instances, "
                      f"{runs} runs, {limits['seconds']:g} s solve, {limits['check_seconds']:g} s check, "
                      f"{limits['memory_gib']} GiB")
        return
    if args.command == "collect":
        collect(args.out)
        return
    if args.command == "check":
        result = check_pipeline(args.kind, Path(args.cnf), Path(args.proof), Path(args.work),
                                float(args.seconds), float(args.memory_gib))
        print(json.dumps(result))
        return
    if args.command == "task":
        tasks = json.loads(args.plan.read_text())
        run_task(tasks[args.index], args.plan.parent.parent, args.index)
        return

    if args.command == "fetch":
        for name in args.experiments:
            seen = set()
            for task in plan(name, args):
                if task["instance"] not in seen:
                    seen.add(task["instance"])
                    fetch(task)
            print(f"{name}: {len(seen)} instances in {CACHE}")
        return
    plans = {}
    for name in args.experiments:
        tasks = plan(name, args)
        directory = args.out / name
        directory.mkdir(parents=True, exist_ok=True)
        plan_path = directory / "plan.json"
        # Records are indexed by position, so a resumed run must select the same
        # runs in the same order with the same limits.
        identity = lambda ts: [(t["configuration"], t["set"], t["instance"], t["limits"], t["check"]) for t in ts]
        if plan_path.exists() and identity(json.loads(plan_path.read_text())) != identity(tasks):
            sys.exit(f"{plan_path} holds a different selection; use another --out or remove it")
        plan_path.write_text(json.dumps(tasks, indent=1) + "\n")
        plans[name] = (tasks, plan_path)
        print(f"{name}: {len(tasks)} tasks -> {directory}")
    if args.command == "run":
        # One pool for all selected experiments; their longest runs start first
        # (case studies, then tierd, ...), so that short runs fill the tail.
        order = {name: rank for rank, name in enumerate(("chvatal", "cruxmiter", "tierd", "milestones",
                                                          "interface", "smoke"))}
        work = [(task, index) for name in sorted(plans, key=order.get)
                for index, task in enumerate(plans[name][0])]
        failed = 0
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            futures = {pool.submit(run_task, t, args.out, i): t for t, i in work}
            for done, future in enumerate(concurrent.futures.as_completed(futures), 1):
                task = futures[future]
                name = Path(task["instance"]).name[:44]
                label = f"{task['experiment']}/{task['configuration']}"
                try:
                    record = future.result()
                except Exception as error:  # reported; not recorded, so a rerun retries it
                    failed += 1
                    print(f"[{done}/{len(work)}] {label:36} {name:44} ERROR   {error}", flush=True)
                    continue
                check = (record.get("check") or {}).get("result", "")
                if record["verdict"] == "SAT":
                    check = ("model ok" if record.get("model_ok") else
                             "model not ok"
                             if record.get("model_format_ok") is False else "MODEL REJECTED")
                elif record["solve"].get("memout"):
                    check = "memory-out"
                elif (record.get("check") or {}).get("memout"):
                    check = "check memory-out"
                print(f"[{done}/{len(work)}] {label:36} {name:44} "
                      f"{record['verdict']:7} {record['solve']['wall_seconds']:9.2f} s {check}", flush=True)
        collect(args.out)
        if failed:
            sys.exit(f"{failed} task(s) failed to run; rerun the same command to retry them")
        return
    for name, (tasks, plan_path) in plans.items():
        submit(args, name, tasks, plan_path)


def submit(args, name: str, tasks: list[dict], plan_path: Path) -> None:
    """One SLURM array per experiment: one array task per run, with the
    experiment's cores, memory and time limit (as the paper's runs). The job
    gets 1 GB more than the run's limit, for the runner itself; the runner
    enforces the limit on the solver and checkers."""
    directory = plan_path.parent
    limits = tasks[0]["limits"]
    longest = max(t["limits"]["seconds"] + (t["limits"]["check_seconds"] if t["check"] else 0) for t in tasks)
    hours = int(longest // 3600) + 1
    script = directory / "array.sbatch"
    script.write_text(f"""#!/usr/bin/env bash
#SBATCH -J tactix-{name}
#SBATCH -p {args.partition}
#SBATCH -c {limits['cpus']}
#SBATCH --mem={max(limits['memory_gib'], limits.get('check_memory_gib', 0)) + 1}G
#SBATCH -t {hours}:00:00
#SBATCH -a 0-{len(tasks) - 1}{f"%{args.concurrency}" if args.concurrency else ""}
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
