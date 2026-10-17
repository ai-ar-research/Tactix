#!/usr/bin/env python3
"""Run one correctness-checked causal-convergence fixture on one SLURM node.

Without ``--tier`` this is the frozen P4A/P4B gate runner.  ``--tier`` admits
one named fixture from a frozen benchmark tier for a P4-CVG-style diagnostic;
it does not add that fixture to the gate manifests.
"""
import argparse
import datetime
import hashlib
import json
import lzma
import os
import pathlib
import shlex
import shutil
import subprocess
import sys
import tempfile
import time


ROOT = pathlib.Path(__file__).resolve().parents[2]
MANIFESTS = pathlib.Path(__file__).resolve().parent / "manifests"
TIERS = ROOT / "tools/bench/tiers"
CACHE = ROOT / "benchmarks/cache"
CHECKMODEL = ROOT / "tools/fuzz/checkmodel"
DRAT = ROOT / "tools/checkers/bin/drat-trim"


def read_fixtures(tier=None):
    result = []
    groups = (tier,) if tier else ("P4A", "P4B")
    directory = TIERS if tier else MANIFESTS
    for group in groups:
        for line in (directory / f"{group}.list").read_text().splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            rel, expected, reference = line.split()
            result.append({"group": group, "relpath": rel,
                           "expected": expected,
                           "reference_seconds": float(reference)})
    return result


def resolve_fixture(name, tier=None):
    matches = []
    for row in read_fixtures(tier):
        path = pathlib.Path(row["relpath"])
        candidates = {row["relpath"], path.name, path.stem}
        if path.suffix == ".xz":
            candidates.add(path.with_suffix("").stem)
            candidates.add(path.name[:-3])
        if name in candidates:
            matches.append(row)
    if len(matches) != 1:
        scope = f"Tier {tier}" if tier else "P4"
        raise ValueError(
            f"fixture {name!r} resolves to {len(matches)} {scope} rows")
    return matches[0]


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as src:
        for block in iter(lambda: src.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def run(cmd, env, timeout, log):
    start = time.monotonic()
    try:
        result = subprocess.run(cmd, env=env, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True,
                                errors="replace", timeout=timeout)
        code, output = result.returncode, result.stdout
    except subprocess.TimeoutExpired as error:
        code = None
        output = error.stdout or ""
        if isinstance(output, bytes):
            output = output.decode(errors="replace")
    log.write_text(output)
    return code, time.monotonic() - start


def checker_budget(timeout):
    """Plan §8.1 budget for validating a solver result."""
    return max(3600.0, 4 * timeout)


def verify(expected, cnf, proof, log, out, timeout):
    if expected == "SAT":
        cmd = [str(CHECKMODEL), str(cnf), str(log)]
    else:
        cmd = [str(DRAT), str(cnf), str(proof)]
    budget = checker_budget(timeout)
    try:
        checked = subprocess.run(cmd, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, text=True,
                                 errors="replace", timeout=budget)
    except subprocess.TimeoutExpired as error:
        output = error.stdout or ""
        if isinstance(output, bytes):
            output = output.decode(errors="replace")
        out.write_text(output + f"\nCHECKER TIMEOUT after {budget:g}s\n")
        return False, "timeout"
    out.write_text(checked.stdout)
    if expected == "SAT":
        ok = checked.returncode == 0
    else:
        ok = checked.returncode == 0 and "s VERIFIED" in checked.stdout
    return ok, "verified" if ok else "rejected"


def write_metadata(out, metadata):
    (out / "metadata.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n")


def stage(row, directory):
    source = CACHE / row["relpath"]
    if not source.is_file():
        raise FileNotFoundError(source)
    target = directory / (source.stem if source.suffix == ".xz" else source.name)
    if source.suffix == ".xz":
        with lzma.open(source, "rb") as src, open(target, "wb") as dst:
            shutil.copyfileobj(src, dst)
    else:
        shutil.copy2(source, target)
    return source, target


def worker(args, row):
    if "SLURM_JOB_ID" not in os.environ:
        raise SystemExit("P4 real-instance workers must run under SLURM")
    out = pathlib.Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    tactix, kissat = pathlib.Path(args.tactix), pathlib.Path(args.kissat)
    metadata = {"schema": "p4-cvg-run-v1", "fixture": row,
                "diagnostic_tier": args.tier,
                "slurm_job_id": os.environ["SLURM_JOB_ID"],
                "tactix": {"path": str(tactix.resolve()),
                            "sha256": sha256(tactix)},
                "kissat": {"path": str(kissat.resolve()),
                            "sha256": sha256(kissat)},
                "timeout": args.timeout, "memory_gb": args.memory_gb,
                "repeats": args.repeats,
                "conflict_window": [args.conflict_from, args.conflict_to],
                "started_utc": datetime.datetime.now(
                    datetime.timezone.utc).isoformat(), "runs": []}
    expected_code = 10 if row["expected"] == "SAT" else 20
    scratch_root = pathlib.Path(os.environ.get("SLURM_TMPDIR", "/tmp"))
    with tempfile.TemporaryDirectory(prefix="tactix-p4-", dir=scratch_root) as d:
        source, cnf = stage(row, pathlib.Path(d))
        metadata["input"] = {"path": str(source), "sha256": sha256(cnf)}
        for repeat in range(args.repeats):
            rd = out / f"repeat-{repeat + 1}"
            rd.mkdir(exist_ok=True)
            record = {"repeat": repeat + 1, "solvers": {}}
            metadata["runs"].append(record)
            write_metadata(out, metadata)
            solvers = []
            if not args.kissat_only:
                solvers.append(("tactix", tactix, "TACTIX"))
            if not args.tactix_only:
                solvers.append(("kissat", kissat, "KISSAT"))
            for name, binary, prefix in solvers:
                log, proof = rd / f"{name}.log", rd / f"{name}.drat"
                event = rd / f"{name}-event.tsv"
                reduce = rd / f"{name}-reduce.tsv"
                env = os.environ.copy()
                if not args.no_trace:
                    env[f"{prefix}_EVENT_TRACE"] = str(event)
                    env[f"{prefix}_EVENT_TRACE_LIMIT"] = str(args.trace_limit)
                    if args.mode_clock:
                        env[f"{prefix}_EVENT_TRACE_MODE_CLOCK"] = "1"
                    env[f"{prefix}_REDUCE_TRACE"] = str(reduce)
                    if args.conflict_from is not None:
                        env[f"{prefix}_EVENT_TRACE_CONFLICT_FROM"] = str(
                            args.conflict_from)
                        env[f"{prefix}_REDUCE_TRACE_CONFLICT_FROM"] = str(
                            args.conflict_from)
                    if args.conflict_to is not None:
                        env[f"{prefix}_EVENT_TRACE_CONFLICT_TO"] = str(
                            args.conflict_to)
                        env[f"{prefix}_REDUCE_TRACE_CONFLICT_TO"] = str(
                            args.conflict_to)
                if name == "tactix":
                    env["TACTIX_STRATEGY_CONFIG"] = "1"
                    if args.tactix_bve_backward is not None:
                        env["TACTIX_BVE_BACKWARD"] = args.tactix_bve_backward
                    if args.tactix_slot_enable:
                        env["TACTIX_SLOT_ENABLE"] = args.tactix_slot_enable
                    if args.tactix_slot_disable:
                        env["TACTIX_SLOT_DISABLE"] = args.tactix_slot_disable
                    if args.tactix_slot_permille:
                        env["TACTIX_SLOT_PERMILLE"] = args.tactix_slot_permille
                command = [str(binary), str(cnf), str(proof)]
                code, elapsed = run(command, env, args.timeout, log)
                record["solvers"][name] = {
                    "command": command, "exit_code": code,
                    "seconds": elapsed, "verified": False,
                    "check_status": "pending",
                    "event_trace": None if args.no_trace else str(event),
                    "reduce_trace": None if args.no_trace else str(reduce)}
                # Preserve the child status before a potentially long or
                # failing checker and before starting the reference solver.
                write_metadata(out, metadata)
                if code == expected_code:
                    ok, check_status = verify(
                        row["expected"], cnf, proof, log,
                        rd / f"{name}-check.log", args.timeout)
                else:
                    ok, check_status = False, "not-run: unexpected solver exit"
                record["solvers"][name]["verified"] = ok
                record["solvers"][name]["check_status"] = check_status
                write_metadata(out, metadata)
                if not ok and not args.tier:
                    raise SystemExit(f"{name} failed verdict/proof/model validation")
            event_json = rd / "event-comparison.json"
            if (rd / "tactix-event.tsv").exists() and (
                    rd / "kissat-event.tsv").exists():
                subprocess.run(
                    [sys.executable,
                     str(ROOT / "tools/bench/compare_event_trace.py"),
                     str(rd / "tactix-event.tsv"),
                     str(rd / "kissat-event.tsv"), "--mode", "semantic",
                     "--fixture", pathlib.Path(row["relpath"]).name,
                     "--json", str(event_json)], check=False)
            if (rd / "tactix-reduce.tsv").exists() and (
                    rd / "kissat-reduce.tsv").exists():
                reduce_json = rd / "reduce-comparison.json"
                subprocess.run(
                    [sys.executable,
                     str(ROOT / "tools/bench/compare_reduce_trace.py"),
                     str(rd / "tactix-reduce.tsv"),
                     str(rd / "kissat-reduce.tsv"), "--json", str(reduce_json)],
                    check=False)
    metadata["finished_utc"] = datetime.datetime.now(
        datetime.timezone.utc).isoformat()
    write_metadata(out, metadata)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fixture", required=True)
    ap.add_argument("--tier", choices=("A", "B", "C", "D"),
                    help="use a frozen tier fixture diagnostically; omit for "
                         "the fixed P4A/P4B gate manifests")
    ap.add_argument("--tactix", required=True)
    ap.add_argument("--kissat", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--timeout", type=float)
    ap.add_argument("--memory-gb", type=int,
                    help="diagnostic allocation override; requires --tier "
                         "and preserves 8 GiB per physical core on SMT-2")
    ap.add_argument("--repeats", type=int, default=1)
    ap.add_argument("--trace-limit", type=int, default=1000000)
    ap.add_argument("--conflict-from", type=int)
    ap.add_argument("--conflict-to", type=int)
    ap.add_argument("--mode-clock", action="store_true")
    ap.add_argument("--tactix-only", action="store_true",
                    help="run only Tactix in --tier diagnostic mode; reuse "
                         "stored reference results")
    ap.add_argument("--kissat-only", action="store_true",
                    help="run only diagnostic Kissat in --tier mode; never "
                         "use this result as a performance baseline")
    ap.add_argument("--tactix-slot-enable",
                    help="diagnostic comma-separated Tactix slot override")
    ap.add_argument("--tactix-slot-disable",
                    help="diagnostic comma-separated Tactix slot override")
    ap.add_argument("--tactix-slot-permille",
                    help="diagnostic Rule:permille comma-separated override")
    ap.add_argument("--tactix-bve-backward", choices=("0", "1"),
                    help="diagnostic BVE backward-subsumption override")
    ap.add_argument("--no-trace", action="store_true",
                    help="disable semantic/reduction traces in --tier "
                         "diagnostic mode")
    ap.add_argument("--worker", action="store_true", help=argparse.SUPPRESS)
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()
    try:
        row = resolve_fixture(args.fixture, args.tier)
    except ValueError as error:
        ap.error(str(error))
    if args.timeout is None:
        args.timeout = {"P4A": 30.0, "P4B": 300.0, "A": 30.0,
                        "B": 300.0, "C": 1200.0,
                        "D": 5000.0}[row["group"]]
    if args.repeats < 1:
        ap.error("--repeats must be positive")
    if args.memory_gb is None:
        args.memory_gb = 32
    if args.memory_gb <= 0 or args.memory_gb % 8:
        ap.error("--memory-gb must be a positive multiple of 8")
    if args.memory_gb != 32 and not args.tier:
        ap.error("a non-default --memory-gb requires --tier")
    if args.tactix_only and not args.tier:
        ap.error("--tactix-only requires --tier")
    if args.kissat_only and not args.tier:
        ap.error("--kissat-only requires --tier")
    if args.tactix_only and args.kissat_only:
        ap.error("--tactix-only and --kissat-only are mutually exclusive")
    if args.no_trace and not args.tier:
        ap.error("--no-trace requires --tier")
    if args.worker:
        worker(args, row)
        return
    command = [sys.executable, str(pathlib.Path(__file__).resolve()),
               "--fixture", args.fixture, "--tactix", args.tactix,
               "--kissat", args.kissat, "--out", args.out,
               "--timeout", str(args.timeout), "--repeats", str(args.repeats),
               "--memory-gb", str(args.memory_gb),
               "--trace-limit", str(args.trace_limit), "--worker"]
    if args.tier:
        command += ["--tier", args.tier]
    if args.conflict_from is not None:
        command += ["--conflict-from", str(args.conflict_from)]
    if args.conflict_to is not None:
        command += ["--conflict-to", str(args.conflict_to)]
    if args.mode_clock:
        command += ["--mode-clock"]
    if args.tactix_only:
        command += ["--tactix-only"]
    if args.kissat_only:
        command += ["--kissat-only"]
    if args.tactix_slot_enable:
        command += ["--tactix-slot-enable", args.tactix_slot_enable]
    if args.tactix_slot_disable:
        command += ["--tactix-slot-disable", args.tactix_slot_disable]
    if args.tactix_slot_permille:
        command += ["--tactix-slot-permille", args.tactix_slot_permille]
    if args.tactix_bve_backward is not None:
        command += ["--tactix-bve-backward", args.tactix_bve_backward]
    if args.no_trace:
        command += ["--no-trace"]
    solver_count = 1 if args.tactix_only or args.kissat_only else 2
    per_solver_wall = args.timeout + checker_budget(args.timeout)
    wall_minutes = max(
        10, int((solver_count * per_solver_wall * args.repeats) / 60) + 10)
    memory_gb = args.memory_gb
    physical_cores = max(1, -(-memory_gb // 8))
    cpus = 2 * physical_cores
    sbatch = ["sbatch", "--parsable", "--partition=cpu-dense-hwulab-q",
              f"--cpus-per-task={cpus}", f"--mem={memory_gb}G",
              f"--time={wall_minutes}",
              f"--output={args.out}/slurm-%j.log", "--wrap",
              shlex.join(command)]
    if args.dry_run:
        print(shlex.join(sbatch))
        return
    pathlib.Path(args.out).mkdir(parents=True, exist_ok=True)
    result = subprocess.run(sbatch, check=True, text=True,
                            stdout=subprocess.PIPE)
    print(result.stdout.strip())


if __name__ == "__main__":
    main()
