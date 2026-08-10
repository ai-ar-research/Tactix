#!/usr/bin/env python3
"""Run one correctness-checked P4 convergence fixture on one SLURM node."""
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
CACHE = ROOT / "benchmarks/cache"
CHECKMODEL = ROOT / "tools/fuzz/checkmodel"
DRAT = ROOT / "tools/checkers/bin/drat-trim"


def fixtures():
    result = []
    for group in ("P4A", "P4B"):
        for line in (MANIFESTS / f"{group}.list").read_text().splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            rel, expected, reference = line.split()
            result.append({"group": group, "relpath": rel,
                           "expected": expected,
                           "reference_seconds": float(reference)})
    return result


def resolve_fixture(name):
    matches = []
    for row in fixtures():
        path = pathlib.Path(row["relpath"])
        candidates = {row["relpath"], path.name, path.stem}
        if path.suffix == ".xz":
            candidates.add(path.with_suffix("").stem)
            candidates.add(path.name[:-3])
        if name in candidates:
            matches.append(row)
    if len(matches) != 1:
        raise ValueError(f"fixture {name!r} resolves to {len(matches)} P4 rows")
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


def verify(expected, cnf, proof, log, out, timeout):
    if expected == "SAT":
        cmd = [str(CHECKMODEL), str(cnf), str(log)]
    else:
        cmd = [str(DRAT), str(cnf), str(proof)]
    checked = subprocess.run(cmd, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True,
                             errors="replace", timeout=max(timeout, 300))
    out.write_text(checked.stdout)
    if expected == "SAT":
        return checked.returncode == 0
    return checked.returncode == 0 and "s VERIFIED" in checked.stdout


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
                "slurm_job_id": os.environ["SLURM_JOB_ID"],
                "tactix": {"path": str(tactix.resolve()),
                            "sha256": sha256(tactix)},
                "kissat": {"path": str(kissat.resolve()),
                            "sha256": sha256(kissat)},
                "timeout": args.timeout, "repeats": args.repeats,
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
            for name, binary, prefix in (("tactix", tactix, "TACTIX"),
                                         ("kissat", kissat, "KISSAT")):
                log, proof = rd / f"{name}.log", rd / f"{name}.drat"
                event = rd / f"{name}-event.tsv"
                reduce = rd / f"{name}-reduce.tsv"
                env = os.environ.copy()
                env[f"{prefix}_EVENT_TRACE"] = str(event)
                env[f"{prefix}_EVENT_TRACE_LIMIT"] = str(args.trace_limit)
                env[f"{prefix}_REDUCE_TRACE"] = str(reduce)
                if args.conflict_from is not None:
                    env[f"{prefix}_EVENT_TRACE_CONFLICT_FROM"] = str(
                        args.conflict_from)
                if args.conflict_to is not None:
                    env[f"{prefix}_EVENT_TRACE_CONFLICT_TO"] = str(
                        args.conflict_to)
                if name == "tactix":
                    env["TACTIX_STRATEGY_CONFIG"] = "1"
                command = [str(binary), str(cnf), str(proof)]
                code, elapsed = run(command, env, args.timeout, log)
                ok = code == expected_code and verify(
                    row["expected"], cnf, proof, log,
                    rd / f"{name}-check.log", args.timeout)
                record["solvers"][name] = {
                    "command": command, "exit_code": code,
                    "seconds": elapsed, "verified": ok,
                    "event_trace": str(event), "reduce_trace": str(reduce)}
                if not ok:
                    metadata["runs"].append(record)
                    (out / "metadata.json").write_text(
                        json.dumps(metadata, indent=2, sort_keys=True) + "\n")
                    raise SystemExit(f"{name} failed verdict/proof/model validation")
            event_json = rd / "event-comparison.json"
            subprocess.run(
                [sys.executable, str(ROOT / "tools/bench/compare_event_trace.py"),
                 str(rd / "tactix-event.tsv"), str(rd / "kissat-event.tsv"),
                 "--mode", "semantic", "--fixture", pathlib.Path(
                     row["relpath"]).name, "--json", str(event_json)],
                check=False)
            if (rd / "tactix-reduce.tsv").exists() and (
                    rd / "kissat-reduce.tsv").exists():
                reduce_json = rd / "reduce-comparison.json"
                subprocess.run(
                    [sys.executable,
                     str(ROOT / "tools/bench/compare_reduce_trace.py"),
                     str(rd / "tactix-reduce.tsv"),
                     str(rd / "kissat-reduce.tsv"), "--json", str(reduce_json)],
                    check=False)
            metadata["runs"].append(record)
    metadata["finished_utc"] = datetime.datetime.now(
        datetime.timezone.utc).isoformat()
    (out / "metadata.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fixture", required=True)
    ap.add_argument("--tactix", required=True)
    ap.add_argument("--kissat", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--timeout", type=float)
    ap.add_argument("--repeats", type=int, default=1)
    ap.add_argument("--trace-limit", type=int, default=1000000)
    ap.add_argument("--conflict-from", type=int)
    ap.add_argument("--conflict-to", type=int)
    ap.add_argument("--worker", action="store_true", help=argparse.SUPPRESS)
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()
    try:
        row = resolve_fixture(args.fixture)
    except ValueError as error:
        ap.error(str(error))
    if args.timeout is None:
        args.timeout = 30.0 if row["group"] == "P4A" else 300.0
    if args.repeats < 1:
        ap.error("--repeats must be positive")
    if args.worker:
        worker(args, row)
        return
    command = [sys.executable, str(pathlib.Path(__file__).resolve()),
               "--fixture", args.fixture, "--tactix", args.tactix,
               "--kissat", args.kissat, "--out", args.out,
               "--timeout", str(args.timeout), "--repeats", str(args.repeats),
               "--trace-limit", str(args.trace_limit), "--worker"]
    if args.conflict_from is not None:
        command += ["--conflict-from", str(args.conflict_from)]
    if args.conflict_to is not None:
        command += ["--conflict-to", str(args.conflict_to)]
    wall_minutes = max(10, int((2 * args.timeout * args.repeats) / 60) + 10)
    sbatch = ["sbatch", "--parsable", "--partition=cpu-dense-hwulab-q",
              "--cpus-per-task=1", "--mem=8G", f"--time={wall_minutes}",
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
