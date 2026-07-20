#!/usr/bin/env python3
"""tools/bench/run.py — tiered benchmark runner (plan §8.1/§8.3).

Usage:
  run.py --tier A --solver ./build/tactix [--baseline kissat] [--jobs 8]

- Tier lists are frozen files `benchmarks/tiers/<tier>.list`; each
  non-comment line is `<relpath> <SAT|UNSAT|UNKNOWN> <ref_seconds>` with
  paths relative to `benchmarks/cache/`.
- For the solver under test, every UNSAT answer's DRAT proof is checked with
  drat-trim; a rejected (or missing) proof counts as UNSOLVED,
  competition-style. Verdicts are also checked against the expected status
  in the list — any mismatch is a correctness bug (stop and use the fuzz
  skill). Baselines are timed without proof logging (performance reference).
- Score: PAR-2 = mean(runtime if solved else 2*timeout), reported with
  #solved and SAT/UNSAT split, proof-check time, and proof size.
- Up to 8 parallel single-threaded solver jobs (plan §8.1); the concurrency
  level is recorded in the results and must accompany any reported number.

Results: JSON + text summary under benchmarks/runs/<stamp>-<tier>-<name>/
(untracked); reports for the repo go to benchmarks/reports/ (see the
benchmarks skill).
"""

import argparse
import concurrent.futures
import datetime
import json
import os
import resource
import shlex
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
CACHE = os.path.join(ROOT, "benchmarks", "cache")
TIERS = os.path.join(ROOT, "benchmarks", "tiers")
RUNS = os.path.join(ROOT, "benchmarks", "runs")
DRAT_TRIM = os.path.join(ROOT, "tools", "checkers", "bin", "drat-trim")

TIER_TIMEOUT = {"A": 30.0, "B": 300.0, "C": 1200.0, "D": 5000.0}
TIER_MEM_GB = {"D": 16}
BASELINES = {
    "kissat": os.path.join(ROOT, "tools", "solvers", "bin", "kissat"),
    "cadical": os.path.join(ROOT, "tools", "solvers", "bin", "cadical"),
    "minisat": os.path.join(ROOT, "tools", "solvers", "bin", "minisat"),
    "glucose": os.path.join(ROOT, "tools", "solvers", "bin", "glucose"),
}


def read_tier_list(tier):
    path = os.path.join(TIERS, f"{tier}.list")
    if not os.path.exists(path):
        sys.exit(f"error: tier list {path} does not exist (build/freeze it "
                 f"first — see the benchmarks skill)")
    entries = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            rel, status, ref = line.split()
            entries.append((rel, status, float(ref)))
    return entries


def parse_verdict(exitcode, stdout):
    sline = next((l[2:].strip() for l in stdout.splitlines()
                  if l.startswith("s ")), None)
    if exitcode == 10 and sline == "SATISFIABLE":
        return "SAT"
    if exitcode == 20 and sline == "UNSATISFIABLE":
        return "UNSAT"
    if sline is None and exitcode in (10, 20):
        # baseline solvers without competition s-lines (MiniSat): trust
        # the conventional exit code
        return "SAT" if exitcode == 10 else "UNSAT"
    return "UNKNOWN"


def run_instance(cmd, rel, cnf, expected, timeout, check_proofs, workdir,
                 mem_gb=None):
    res = {"instance": rel, "expected": expected, "verdict": "UNKNOWN",
           "time": timeout, "solved": False, "mismatch": False,
           "proof_ok": None, "proof_bytes": None, "proof_check_time": None}
    if cnf is None or not os.path.exists(cnf):
        res["verdict"] = "MISSING"
        return res
    proof = os.path.join(workdir, rel.replace("/", "_") + ".drat")
    argv = cmd + [cnf] + ([proof] if check_proofs else [])

    def limits():
        # kernel-enforced CPU backstop: subprocess.run(timeout=) only kills the
        # child on the normal timeout path; if the parent dies abruptly the child
        # is reparented to init and spins forever. RLIMIT_CPU makes the kernel
        # stop it regardless of parent state (slack keeps wall-timeout primary).
        cap = int(timeout) + 5
        resource.setrlimit(resource.RLIMIT_CPU, (cap, cap + 2))
        if mem_gb:
            b = mem_gb << 30
            resource.setrlimit(resource.RLIMIT_AS, (b, b))

    t0 = time.monotonic()
    try:
        r = subprocess.run(argv, capture_output=True, text=True,
                           errors="replace", timeout=timeout,
                           preexec_fn=limits)
        elapsed = time.monotonic() - t0
        verdict = parse_verdict(r.returncode, r.stdout)
    except subprocess.TimeoutExpired:
        elapsed, verdict = timeout, "UNKNOWN"
    res["time"] = round(elapsed, 3)
    res["verdict"] = verdict

    if verdict in ("SAT", "UNSAT"):
        res["solved"] = True
        if expected in ("SAT", "UNSAT") and verdict != expected:
            res["mismatch"] = True   # correctness bug — counts as unsolved
            res["solved"] = False
        elif verdict == "UNSAT" and check_proofs:
            if not os.path.exists(proof):
                res["proof_ok"] = False
                res["solved"] = False
            else:
                res["proof_bytes"] = os.path.getsize(proof)
                t1 = time.monotonic()
                try:
                    c = subprocess.run([DRAT_TRIM, cnf, proof],
                                       capture_output=True, text=True,
                                       errors="replace",
                                       timeout=max(60.0, 2 * timeout))
                    ok = "s VERIFIED" in c.stdout
                except subprocess.TimeoutExpired:
                    ok = False
                res["proof_check_time"] = round(time.monotonic() - t1, 3)
                res["proof_ok"] = ok
                if not ok:          # rejected proof = unsolved (plan §2)
                    res["solved"] = False
    if check_proofs and os.path.exists(proof):
        os.unlink(proof)
    return res


def par2(results, timeout):
    if not results:
        return 0.0
    return sum(r["time"] if r["solved"] else 2 * timeout
               for r in results) / len(results)


def summarize(name, results, timeout, jobs):
    solved = [r for r in results if r["solved"]]
    sat = [r for r in solved if r["verdict"] == "SAT"]
    uns = [r for r in solved if r["verdict"] == "UNSAT"]
    mism = [r for r in results if r["mismatch"]]
    prej = [r for r in results if r["proof_ok"] is False]
    pbytes = [r["proof_bytes"] for r in results if r["proof_bytes"]]
    pctime = [r["proof_check_time"] for r in results if r["proof_check_time"]]
    lines = [
        f"{name}: PAR-2 {par2(results, timeout):.2f}  "
        f"solved {len(solved)}/{len(results)} "
        f"(SAT {len(sat)}, UNSAT {len(uns)})  [jobs={jobs}]"
    ]
    if pbytes:
        lines.append(f"  proofs: {len(pbytes)} checked, "
                     f"total {sum(pbytes) / 1e6:.1f} MB, "
                     f"check time {sum(pctime):.1f} s")
    if mism:
        lines.append(f"  !! VERDICT MISMATCHES ({len(mism)}): "
                     + ", ".join(r["instance"] for r in mism[:5]))
    if prej:
        lines.append(f"  !! PROOFS REJECTED/MISSING ({len(prej)}): "
                     + ", ".join(r["instance"] for r in prej[:5]))
    return "\n".join(lines), bool(mism or prej)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--tier", required=True, choices="ABCD")
    ap.add_argument("--solver", required=True,
                    help="solver under test (competition CLI)")
    ap.add_argument("--name", default=None, help="label for the solver")
    ap.add_argument("--baseline", action="append", default=[],
                    help="baseline solver: kissat|cadical|<command>")
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--timeout", type=float, default=None,
                    help="override the tier's per-instance timeout")
    args = ap.parse_args()
    if not 1 <= args.jobs <= 8:
        sys.exit("error: --jobs must be 1..8 (plan §8.1)")

    entries = read_tier_list(args.tier)
    timeout = args.timeout or TIER_TIMEOUT[args.tier]
    mem_gb = TIER_MEM_GB.get(args.tier)
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    name = args.name or os.path.basename(shlex.split(args.solver)[0])
    outdir = os.path.join(RUNS, f"{stamp}-{args.tier}-{name}")
    os.makedirs(outdir, exist_ok=True)

    sha = subprocess.run(["git", "-C", ROOT, "rev-parse", "--short", "HEAD"],
                         capture_output=True, text=True).stdout.strip()
    solvers = [(name, shlex.split(args.solver), True)]
    for b in args.baseline:
        solvers.append((os.path.basename(shlex.split(BASELINES.get(b, b))[0]),
                        shlex.split(BASELINES.get(b, b)), False))

    # stage instances as plain DIMACS once — some baselines (MiniSat,
    # Glucose) read .gz at best, and excluding decompression time from
    # every solver's wall clock is fairer anyway
    stage = os.path.join(outdir, "staged")
    os.makedirs(stage, exist_ok=True)
    paths = {}
    for rel, _, _ in entries:
        src = os.path.join(CACHE, rel)
        if not os.path.exists(src):
            paths[rel] = None
            continue
        tool = {".xz": "xz", ".gz": "gzip"}.get(os.path.splitext(rel)[1])
        if tool:
            dst = os.path.join(stage, os.path.basename(rel)[:-3])
            if not os.path.exists(dst):
                with open(dst, "wb") as f:
                    subprocess.run([tool, "-dc", src], stdout=f, check=True)
            paths[rel] = dst
        else:
            paths[rel] = src

    all_results = {}
    any_bug = False
    for sname, cmd, check_proofs in solvers:
        print(f"== {sname} on Tier {args.tier} "
              f"({len(entries)} instances, timeout {timeout:g}s, "
              f"jobs {args.jobs}) ==", flush=True)
        with concurrent.futures.ThreadPoolExecutor(args.jobs) as ex:
            results = list(ex.map(
                lambda e: run_instance(cmd, e[0], paths[e[0]], e[1],
                                       timeout, check_proofs, outdir,
                                       mem_gb),
                entries))
        summary, bug = summarize(sname, results, timeout, args.jobs)
        print(summary, flush=True)
        all_results[sname] = results
        any_bug |= bug

    meta = {"tier": args.tier, "timeout": timeout, "jobs": args.jobs,
            "git": sha, "date": stamp, "mem_gb": mem_gb,
            "solvers": {n: " ".join(c) for n, c, _ in solvers},
            "results": all_results}
    shutil.rmtree(stage, ignore_errors=True)
    with open(os.path.join(outdir, "results.json"), "w") as f:
        json.dump(meta, f, indent=2)
    print(f"results: {outdir}/results.json")
    if any_bug:
        print("CORRECTNESS BUG(S) FOUND — stop performance work; follow the "
              "fuzz skill's failure procedure.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
