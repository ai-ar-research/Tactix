#!/usr/bin/env python3
"""tools/bench/run.py — tiered benchmark runner (plan §8.1/§8.3).

Usage:
  run.py --tier A --solver ./build/tactix [--baseline kissat] [--jobs 8]
  run.py --tier C --solver ./build/tactix --baseline kissat --slurm
  run.py --aggregate benchmarks/runs/<dir>      # re-merge a --slurm run

- SLURM mode (cluster protocol, PROGRESS Log 2026-07-23): submits one array
  task per solver x instance to cpu-dense-hwulab-q (8 GB/core rule:
  cpus-per-task = ceil(mem_gb/8), default 1 core / 8 GB), plus a dependent
  aggregate job that merges the per-task JSONs into the identical
  results.json a local run writes. The allocation shape (partition,
  cpus/mem per task, throttle) is recorded in place of the local jobs
  count — compare numbers only across identical shapes (L6 generalized).
  --throttle caps concurrent tasks for lower-contention gate runs;
  --dry-run writes manifest + sbatch scripts without submitting.

- Tier lists are frozen files `benchmarks/tiers/<tier>.list`; each
  non-comment line is `<relpath> <SAT|UNSAT|UNKNOWN> <ref_seconds>` with
  paths relative to `benchmarks/cache/`.
- For the solver under test, every UNSAT answer's proof is checked; a rejected
  (or missing) proof counts as UNSOLVED, competition-style. The arm's own
  command picks the format (fuzz-harness convention): `--lrat` in the command
  => native LRAT verified by cake_lpr, otherwise DRAT verified by drat-trim.
  A DRAT check that OUTRUNS its budget is not final: the arm is re-solved with
  `--lrat` and cake_lpr verifies that instead (§8.1, `lrat_rescue`). A checker
  that REJECTS still scores the instance unsolved — that is an N2 failure.
  Verdicts are also checked against the expected status
  in the list — any mismatch is a correctness bug (stop and use the fuzz
  skill).
- EVERY arm writes proofs, baselines included (plan §8.1 rev 72): timing a
  proof-writing solver against proof-free references compares two different
  workloads. A baseline's proof is still checked, but a failed check is
  RECORDED, never scored against it (--no-baseline-proofs restores the old,
  non-comparable behaviour). Checker budget: max(3600 s, 4 x solve timeout).
- Score: PAR-2 = mean(runtime if solved else 2*timeout), reported with
  #solved and SAT/UNSAT split, proof-check time, and proof size.
- ORACLE (always on, 2+ scored arms): the per-instance best over OUR arms
  (baselines are references, not candidate policies), plus each arm's gap to
  it — also in results.json under "oracle". It is not a runnable policy; it
  is the ceiling a per-instance admission mechanism could reach, so a gate is
  worth shipping only if it beats the trivial always-on arm AND takes a fair
  share of that gap (docs/design/2026-08-04-scheduling-sophistication-
  diagnosis.md §4: at n≈30 the instrument otherwise selects a crude gate).
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
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
CACHE = os.path.join(ROOT, "benchmarks", "cache")
TIERS = os.path.join(ROOT, "benchmarks", "tiers")
RUNS = os.path.join(ROOT, "benchmarks", "runs")
DRAT_TRIM = os.path.join(ROOT, "tools", "checkers", "bin", "drat-trim")
CAKE_LPR = os.path.join(ROOT, "tools", "checkers", "bin", "cake_lpr")

# The one message `check_proof` emits when the checker RAN OUT OF TIME rather
# than REJECTED. Single-sourced because it is a control signal, not prose: it,
# and only it, may trigger the §8.1 native-LRAT re-check (`lrat_rescue`).
CHECKER_TIMEOUT_PREFIX = "checker exceeded its "

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


def proof_format(cmd):
    """Which certificate this arm emits — the solver's own flag decides.

    Same convention as the fuzz harness: an arm whose command carries
    `--lrat` writes native LRAT (checked by the verified cake_lpr), anything
    else writes DRAT (checked by drat-trim). No separate runner switch, so a
    single array can time a DRAT arm and an LRAT arm in the same window."""
    return "lrat" if "--lrat" in cmd else "drat"


def checker_budget(timeout):
    """Plan §8.1 rev 72: max(3600 s, 4 x the tier's solve timeout).

    The old `2 x timeout` scored instances unsolved for a checker-budget reason
    the competition would not have applied (urq45: 1447 s to verify under a
    2400 s budget, and over it one commit earlier)."""
    return max(3600.0, 4 * timeout)


def check_proof(fmt, cnf, proof, budget):
    """Verify one certificate. Returns (ok, message).

    The message matters: a checker that OOMs or times out is otherwise
    indistinguishable from one that REJECTS, and that ambiguity cost a probe on
    2026-07-27 (cake_lpr needs 4.2 GB on a gigabyte-scale LRAT; the array task
    had 8 GB total, so it died at malloc and looked like a rejected proof)."""
    argv = ([CAKE_LPR, cnf, proof] if fmt == "lrat"
            else [DRAT_TRIM, cnf, proof])
    try:
        c = subprocess.run(argv, capture_output=True, text=True,
                           errors="replace", timeout=budget)
    except subprocess.TimeoutExpired:
        return False, f"{CHECKER_TIMEOUT_PREFIX}{budget:g}s budget"
    # drat-trim: "s VERIFIED"; cake_lpr: "s VERIFIED UNSAT"
    if "s VERIFIED" in c.stdout:
        return True, "VERIFIED"
    tail = (c.stderr.strip() or c.stdout.strip()).splitlines()
    return False, (tail[-1][:200] if tail else f"exit {c.returncode}, no output")


def solve_once(argv, timeout, mem_gb):
    """One solver invocation under the wall timeout + kernel backstops.

    Returns (verdict, elapsed). Shared by the timed run and the §8.1 LRAT
    re-solve so the rescue runs under exactly the same limits as the
    measurement it is rescuing."""
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
        return parse_verdict(r.returncode, r.stdout), time.monotonic() - t0
    except subprocess.TimeoutExpired:
        return "UNKNOWN", timeout


def lrat_rescue(res, cmd, cnf, proof, timeout, mem_gb):
    """Plan §8.1: "a proof that outruns even this [budget] is re-checked as
    native LRAT before the instance is called unsolved".

    It must be a RE-SOLVE, not a conversion: producing LRAT from DRAT needs
    exactly drat-trim's backward search, the search that just timed out. So the
    arm is re-run with `--lrat` appended and cake_lpr verifies what it writes —
    measured on `urq45`, cake_lpr took 77 s where drat-trim needed 1447 s
    (probe 1566825).

    Why this exists: without it a checker-budget overrun discards a CORRECT
    solve. It did, three times, all on `urq45` — 2026-07-27 (354.5 s solve,
    2400.1 s check) and 2026-08-03, when it took a scored 619.3 s UNSAT off the
    `broad` arm, one of the 27/30 that held broad Factor admission.

    Sets res["proof_rescue"] to "lrat" when a certificate verified, else to a
    "failed: <reason>" string, and returns whether the instance is proof-clean.
    A re-solve that answers DIFFERENTLY is never rescued: that is a genuine
    anomaly and it must stay visible as an unsolved instance."""
    # The DRAT proof has already been sized and judged, and the LRAT one is
    # bigger; drop it before writing a second multi-GB file into the same
    # (node-shared) scratch.
    if os.path.exists(proof):
        os.unlink(proof)
    lproof = proof + ".rescue.lrat"
    t0 = time.monotonic()
    try:
        verdict, solve_s = solve_once(cmd + ["--lrat", cnf, lproof],
                                      timeout, mem_gb)
        if verdict != res["verdict"]:
            res["proof_rescue"] = (f"failed: --lrat re-solve answered "
                                   f"{verdict}, not {res['verdict']}")
        elif not os.path.exists(lproof):
            res["proof_rescue"] = "failed: --lrat re-solve wrote no proof"
        else:
            lbytes = os.path.getsize(lproof)
            t1 = time.monotonic()
            ok, msg = check_proof("lrat", cnf, lproof, checker_budget(timeout))
            detail = (f"re-solve {solve_s:.1f}s, {lbytes / 1e6:.0f} MB LRAT, "
                      f"cake_lpr {time.monotonic() - t1:.1f}s: {msg}")
            if ok:
                res["proof_rescue"] = "lrat"
                res["proof_ok"] = True
                res["proof_msg"] += f"; RESCUED as native LRAT ({detail})"
                return True
            res["proof_rescue"] = f"failed: {detail}"
    finally:
        if os.path.exists(lproof):
            os.unlink(lproof)
        res["proof_rescue_time"] = round(time.monotonic() - t0, 3)
    res["proof_msg"] += f" [{res['proof_rescue']}]"
    return False


def run_instance(cmd, rel, cnf, expected, timeout, check_proofs, workdir,
                 mem_gb=None, score_proofs=True, verify_proofs=True):
    """`check_proofs` = hand the solver a proof path, i.e. make it WRITE one.
    `verify_proofs` = actually run the checker over what it wrote.
    `score_proofs` = let a failed check count the instance unsolved.

    The three are separate because they answer separate questions.

    WRITING is a TIMING requirement and applies to every arm (plan §8.1 rev 72):
    proof generation costs 4.4-6.9 % and the tax is solver-specific, so a
    proof-free baseline number is not comparable to ours.

    VERIFYING a BASELINE's proof buys nothing and is expensive. `score_proofs`
    is already False for baselines — their failed checks are recorded and never
    held against them — so the check cannot change any verdict. Measured on
    Tier C window 20260803-211105: baseline checking was 6.07 h of 9.08 h, i.e.
    **67 % of all checking wall-clock for zero verdict impact**, and the single
    longest task in the run (61.5 min) was drat-trim on kissat's proof. Hence
    `verify_proofs` defaults to False for baselines (--verify-baseline-proofs
    opts back in for a submission-grade claim). `proof_bytes` is still recorded
    from a stat, so proof size stays a tracked metric for every arm.

    `score_proofs` False without `verify_proofs` False = the pre-2026-08-03
    behaviour: verify, record, never score. Scoring a baseline's certificate
    against OUR checker budget would flatter us — measured, it did: on Tier C
    2026-07-27 kissat lost 2 instances and CaDiCaL 3 to the old budget alone,
    which would have read as us beating kissat by 55 PAR-2 when we are in fact
    1.41x behind it."""
    fmt = proof_format(cmd)
    res = {"instance": rel, "expected": expected, "verdict": "UNKNOWN",
           "time": timeout, "solved": False, "mismatch": False,
           "proof_ok": None, "proof_bytes": None, "proof_check_time": None,
           "proof_format": fmt if check_proofs else None,
           "proof_scored": bool(check_proofs and verify_proofs and score_proofs),
           "proof_msg": None,
           # §8.1 native-LRAT re-check: "lrat" when it saved the instance, a
           # "failed: ..." reason when it was tried and did not, None when the
           # DRAT check never ran out of time. Always present so the schema is
           # stable across runs and the count is aggregatable.
           "proof_rescue": None, "proof_rescue_time": None}
    if cnf is None or not os.path.exists(cnf):
        res["verdict"] = "MISSING"
        return res
    proof = os.path.join(workdir, rel.replace("/", "_") + "." + fmt)
    argv = cmd + [cnf] + ([proof] if check_proofs else [])

    verdict, elapsed = solve_once(argv, timeout, mem_gb)
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
                res["proof_msg"] = "no proof file written"
                if score_proofs:
                    res["solved"] = False
            else:
                # size is free from a stat and is a tracked metric for EVERY arm
                res["proof_bytes"] = os.path.getsize(proof)
                if not verify_proofs:
                    res["proof_msg"] = "written, not verified (§8.1: baseline)"
                else:
                    t1 = time.monotonic()
                    ok, msg = check_proof(fmt, cnf, proof,
                                          checker_budget(timeout))
                    res["proof_check_time"] = round(time.monotonic() - t1, 3)
                    res["proof_ok"] = ok
                    res["proof_msg"] = msg
                    # §8.1 second chance, deliberately narrow. Only a checker
                    # that RAN OUT OF TIME qualifies: a REJECTION is a real N2
                    # failure and silencing it would be a correctness hole.
                    # Only a SCORED arm (ours — a baseline's check cannot change
                    # a verdict and its CLI need not take `--lrat`), and only a
                    # DRAT arm (an `--lrat` arm has nothing left to re-check).
                    if (not ok and score_proofs and fmt == "drat"
                            and msg.startswith(CHECKER_TIMEOUT_PREFIX)):
                        ok = lrat_rescue(res, cmd, cnf, proof, timeout, mem_gb)
                    if not ok and score_proofs:  # rejected proof = unsolved (§2)
                        res["solved"] = False
    if check_proofs and os.path.exists(proof):
        os.unlink(proof)
    return res


def inst_cost(r, timeout):
    """One instance's PAR-2 contribution: its runtime if solved, else 2x the
    timeout. The 2x is the whole point of PAR-2 and is easy to get wrong by
    hand (charging 1x inverted a conclusion on 2026-08-04), so every
    PAR-2-shaped number in this file goes through here."""
    return r["time"] if r["solved"] else 2 * timeout


def par2(results, timeout):
    if not results:
        return 0.0
    return sum(inst_cost(r, timeout) for r in results) / len(results)


def oracle(scored, timeout):
    """Per-instance best over the SCORED arms — the ceiling for admission.

    `scored` is an ordered list of `(name, results)` for the arms with
    `score_proofs` (ours). Baselines are excluded deliberately: they are timed
    as references, not as candidate policies, so "best over the arms" would
    stop meaning anything if a kissat column could win an instance.

    The ORACLE is not a runnable policy — nothing can consult it at solve time.
    It is the score a PERFECT per-instance admission mechanism over exactly
    these arms would get, and therefore the bound any real gate is chasing.
    That makes the useful number the *gap*: an arm's PAR-2 minus the oracle's
    is all the headroom an admission mechanism could possibly recover, and a
    gate is only worth shipping if it beats both the best single arm here and
    a fair share of that gap (plan §1.1 N4, `docs/design/
    2026-08-04-scheduling-sophistication-diagnosis.md` §4: at n≈30 with no
    held-out set, the instrument otherwise selects a crude per-instance
    classifier every time).

    One caveat when reading it: a min over K arms is also a min over K
    measurement noises, so the oracle is optimistically biased and the bias
    grows with K. Measured on `20260804-165514-B-shipped`: over the two
    trivial policies (`facon`/`facoff`) the oracle is 32.25, and adding the
    gated `shipped` arm — which on any given instance IS one of the other two
    — moves it only to 32.14, nearly all of that from sub-3 % margins between
    arms running the same code path (Tier C the same day: 254.55 -> 250.55,
    inside that tier's 6.72 PAR-2 noise floor). To ask "how good could this
    gate get", read the oracle over the policies the gate chooses BETWEEN
    (`oracle()` takes any arm subset); the all-arms number printed here is the
    slightly looser ceiling for the window as a whole.

    Instances are matched by name, in the first arm's order; an arm that is
    missing one simply does not compete for it. Returns None for <2 arms.
    """
    order = []
    seen = set()
    for _, results in scored:
        for r in results:
            if r["instance"] not in seen:
                seen.add(r["instance"])
                order.append(r["instance"])
    if len(scored) < 2 or not order:
        return None
    by_arm = {name: {r["instance"]: r for r in results}
              for name, results in scored}
    best = {}
    total = 0.0
    for rel in order:
        # ties go to the earliest arm listed (the solver under test first)
        cand = [(inst_cost(by_arm[n][rel], timeout), i, n)
                for i, (n, _) in enumerate(scored) if rel in by_arm[n]]
        cost, _, name = min(cand)
        r = by_arm[name][rel]
        total += cost
        best[rel] = {"arm": name if r["solved"] else None,
                     "time": r["time"], "solved": r["solved"],
                     "verdict": r["verdict"] if r["solved"] else "UNKNOWN"}
    n = len(order)
    solved = [b for b in best.values() if b["solved"]]
    return {
        "arms": [name for name, _ in scored],
        "instances": n,
        "par2": round(total / n, 3),
        "solved": len(solved),
        "sat": sum(1 for b in solved if b["verdict"] == "SAT"),
        "unsat": sum(1 for b in solved if b["verdict"] == "UNSAT"),
        # headroom: what a perfect per-instance chooser would take off this arm
        "gap": {name: {
            "par2": round(par2(results, timeout) - total / n, 3),
            "solved": len(solved) - sum(1 for r in results if r["solved"])}
            for name, results in scored},
        "per_instance": best,
    }


def summarize_oracle(orc, shape):
    """The ORACLE block, in the per-arm summary's own format."""
    lines = [
        f"ORACLE (per-instance best of {', '.join(orc['arms'])}): "
        f"PAR-2 {orc['par2']:.2f}  "
        f"solved {orc['solved']}/{orc['instances']} "
        f"(SAT {orc['sat']}, UNSAT {orc['unsat']})  [{shape}]",
        "  gap to oracle: " + ", ".join(
            f"{name} {g['par2']:+.2f} ({g['solved']:+d} solves)"
            for name, g in sorted(orc["gap"].items(),
                                  key=lambda kv: kv[1]["par2"])),
        "  (not a runnable policy: the ceiling for per-instance admission over "
        "these arms — a gate must beat the best arm AND a fair share of its gap)",
    ]
    return "\n".join(lines)


def summarize(name, results, timeout, shape):
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
        f"(SAT {len(sat)}, UNSAT {len(uns)})  [{shape}]"
    ]
    if pbytes:
        fmts = sorted({r.get("proof_format") or "drat" for r in results
                       if r["proof_bytes"]})
        lines.append(f"  proofs ({'/'.join(fmts)}): {len(pbytes)} checked, "
                     f"total {sum(pbytes) / 1e6:.1f} MB, "
                     f"check time {sum(pctime):.1f} s")
    # §8.1: instances whose DRAT check outran the budget and were saved (or not)
    # by a native-LRAT re-solve. Never silent — a rescue means an instance would
    # have been scored unsolved for a checker-budget reason.
    resc = [r for r in results if r.get("proof_rescue") == "lrat"]
    rfail = [r for r in results
             if (r.get("proof_rescue") or "").startswith("failed")]
    if resc:
        lines.append(f"  LRAT rescues ({len(resc)}, DRAT budget outrun -> "
                     f"re-solved with --lrat, cake_lpr VERIFIED): "
                     + ", ".join(f"{r['instance']} (+{r['proof_rescue_time']:.0f} s)"
                                 for r in resc[:5]))
    if rfail:
        lines.append(f"  !! LRAT RESCUES ATTEMPTED AND FAILED ({len(rfail)}): "
                     + ", ".join(f"{r['instance']} [{r['proof_rescue']}]"
                                 for r in rfail[:5]))
    if mism:
        lines.append(f"  !! VERDICT MISMATCHES ({len(mism)}): "
                     + ", ".join(r["instance"] for r in mism[:5]))
    # scored arms (ours): a failed check is a correctness stop. Unscored arms
    # (baselines): recorded with the checker's own message, never held against
    # them — plan §8.1 rev 72.
    scored_rej = [r for r in prej if r.get("proof_scored")]
    noted_rej = [r for r in prej if not r.get("proof_scored")]
    if scored_rej:
        lines.append(f"  !! PROOFS REJECTED/MISSING ({len(scored_rej)}): "
                     + ", ".join(f"{r['instance']} [{r.get('proof_msg')}]"
                                 for r in scored_rej[:5]))
    if noted_rej:
        lines.append(f"  (baseline proofs not verified, NOT scored against it, "
                     f"{len(noted_rej)}): "
                     + ", ".join(f"{r['instance']} [{r.get('proof_msg')}]"
                                 for r in noted_rej[:5]))
    return "\n".join(lines), bool(mism or scored_rej)


def stage_instances(entries, stage):
    """Decompress instances once into `stage`; returns {rel: path|None}.

    Staged as plain DIMACS — some baselines (MiniSat, Glucose) read .gz at
    best, and excluding decompression time from every solver's wall clock
    is fairer anyway.
    """
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
    return paths


def fmt_time(seconds):
    s = int(seconds)
    return f"{s // 3600:02d}:{s % 3600 // 60:02d}:{s % 60:02d}"


def resolve_cmd(cmd):
    # absolutize tokens that name existing files, so array tasks are
    # cwd-independent (also catches `env VAR=1 build/release/tactix`)
    return [os.path.abspath(t) if "/" in t and os.path.exists(t) else t
            for t in cmd]


def sbatch(script, *extra):
    r = subprocess.run(["sbatch", "--parsable", *extra, script],
                       capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit(f"error: sbatch failed: {r.stderr.strip()}")
    return r.stdout.strip().split(";")[0]


def slurm_run_one(outdir, idx):
    """One array task: solve one (solver, instance) pair, write one JSON."""
    with open(os.path.join(outdir, "manifest.json")) as f:
        man = json.load(f)
    t = man["tasks"][idx]
    s = man["solvers"][t["solver"]]
    workdir = tempfile.mkdtemp(prefix="bench-")  # $TMPDIR: node-local
    try:
        res = run_instance(s["cmd"], t["rel"], t["staged"], t["expected"],
                           man["timeout"], s["check_proofs"], workdir,
                           man["mem_gb"], s.get("score_proofs", True),
                           # older manifests predate the split: verifying was
                           # unconditional, so default True for replayability
                           s.get("verify_proofs", True))
    finally:
        shutil.rmtree(workdir, ignore_errors=True)
    res["node"] = os.environ.get("SLURMD_NODENAME")
    resdir = os.path.join(outdir, "results")
    os.makedirs(resdir, exist_ok=True)
    tmp = os.path.join(resdir, f".{idx:05d}.tmp")
    with open(tmp, "w") as f:
        json.dump(res, f, indent=2)
    os.replace(tmp, os.path.join(resdir, f"{idx:05d}.json"))
    print(f"task {idx}: {s['name']} {t['rel']} -> {res['verdict']} "
          f"{res['time']}s solved={res['solved']}")
    return 0


def slurm_aggregate(outdir):
    """Merge per-task JSONs into the same results.json a local run writes."""
    with open(os.path.join(outdir, "manifest.json")) as f:
        man = json.load(f)
    resdir = os.path.join(outdir, "results")
    all_results = {s["name"]: [] for s in man["solvers"]}
    missing = []
    for i, t in enumerate(man["tasks"]):
        p = os.path.join(resdir, f"{i:05d}.json")
        if os.path.exists(p):
            with open(p) as f:
                r = json.load(f)
        else:
            missing.append(i)
            r = {"instance": t["rel"], "expected": t["expected"],
                 "verdict": "MISSING", "time": man["timeout"],
                 "solved": False, "mismatch": False, "proof_ok": None,
                 "proof_bytes": None, "proof_check_time": None,
                 "proof_format": None, "proof_scored": False,
                 "proof_msg": None, "proof_rescue": None,
                 "proof_rescue_time": None}
        all_results[man["solvers"][t["solver"]]["name"]].append(r)
    any_bug = False
    for s in man["solvers"]:
        summary, bug = summarize(s["name"], all_results[s["name"]],
                                 man["timeout"], man["shape"])
        print(summary, flush=True)
        any_bug |= bug
    # older manifests predate the split (everything was scored) — same default
    # as slurm_run_one, so an old run re-aggregates to the same arms
    orc = oracle([(s["name"], all_results[s["name"]]) for s in man["solvers"]
                  if s.get("score_proofs", True)], man["timeout"])
    if orc:
        print(summarize_oracle(orc, man["shape"]), flush=True)
    # single-instance sensitivity for each scored arm against the FIRST scored
    # arm (the baseline). See leave_one_out(): the headline alone has now hidden
    # a one-instance verdict three times running.
    scored = [s2["name"] for s2 in man["solvers"] if s2.get("score_proofs", True)]
    for other in scored[1:]:
        loo = leave_one_out(scored[0], all_results[scored[0]],
                            other, all_results[other], man["timeout"])
        if loo:
            print(loo, flush=True)
    meta = {"tier": man["tier"], "timeout": man["timeout"], "jobs": None,
            "slurm": man["slurm"], "git": man["git"], "date": man["date"],
            "mem_gb": man["mem_gb"],
            "solvers": {s["name"]: " ".join(s["cmd"])
                        for s in man["solvers"]},
            "oracle": orc,
            "results": all_results}
    with open(os.path.join(outdir, "results.json"), "w") as f:
        json.dump(meta, f, indent=2)
    print(f"results: {outdir}/results.json")
    if missing:
        print(f"!! {len(missing)} task result(s) MISSING (infrastructure, "
              f"scored as unsolved): "
              + ", ".join(str(i) for i in missing[:10])
              + (" ..." if len(missing) > 10 else "")
              + "\n   resubmit those indices, then re-run --aggregate")
        return 2  # staged/ kept so the resubmission can run
    shutil.rmtree(os.path.join(outdir, "staged"), ignore_errors=True)
    if any_bug:
        print("CORRECTNESS BUG(S) FOUND — stop performance work; follow the "
              "fuzz skill's failure procedure.")
        return 1  # per-task logs kept: they are the debugging record
    # clean run: the per-task logs have served their purpose — per-instance
    # verdict/time/node live on in results.json. Keep aggregate.log only.
    logdir = os.path.join(outdir, "logs")
    if os.path.isdir(logdir):
        for f in os.listdir(logdir):
            if f.startswith("task-"):
                os.unlink(os.path.join(logdir, f))
    return 0


def leave_one_out(a_name, a_res, b_name, b_res, timeout):
    """Single-instance sensitivity of a paired A/B — printed next to the headline.

    WHY THIS EXISTS. Three gates in a row had their SIGN decided by one
    instance, and every one was caught only by hand-decomposing the run
    afterwards: broad Factor admission (a regression that was +0.65..+1.21 once
    one lottery instance came out), Stage B Tier B (+17.75 -> -2.38 without
    `mod4block`), Stage B Tier C (+46.57 -> -21.15 without `mp1-Nb5T15`). The
    tier headline cannot show this — a real regression and a one-instance
    lottery print identically — so the aggregator computes it rather than
    leaving it to whoever remembers to ask.

    Reports the MINIMUM number of instances whose removal flips the sign. k=1
    means the verdict is one coin flip. That is not a claim the instance is a
    lottery — it is a claim that the run has not answered the question, and the
    next step is paired draws on the named instance, not a decision.
    """
    def par2(rs):
        return {r["instance"]: (r["time"] if r.get("solved") else 2 * timeout)
                for r in rs}
    A, B = par2(a_res), par2(b_res)
    common = sorted(set(A) & set(B))
    if len(common) < 3:
        return None
    d = {i: B[i] - A[i] for i in common}
    n = len(common)
    total = sum(d.values())
    if total == 0:
        return None
    # minimum removals to flip the sign: greedily drop the instances pulling
    # hardest in the total's own direction.
    order = sorted(common, key=lambda i: -d[i] if total > 0 else d[i])
    run, k, dropped = total, 0, []
    for i in order:
        if (run > 0) != (total > 0):
            break
        run -= d[i]
        k += 1
        dropped.append(i)
    flipped = (run > 0) != (total > 0)
    top = order[0]
    out = [f"  A/B {a_name} -> {b_name}: {total / n:+.2f} PAR-2 "
           f"({total:+.1f} s over {n} instances)"]
    out.append(f"    largest single mover: {os.path.basename(top)} "
               f"{d[top]:+.1f} s  ({abs(d[top]) / abs(total) * 100:.0f}% of the total)")
    if not flipped:
        out.append("    sign is robust: no subset removal flips it")
    elif k == 1:
        out.append(f"    ▶ UNDECIDED: removing {os.path.basename(dropped[0])} "
                   f"alone flips the sign to {(total - d[dropped[0]]) / n:+.2f} "
                   f"PAR-2. One instance carries this result — run paired draws "
                   f"on it before reading the headline as a verdict.")
    elif k <= max(2, n // 10):
        out.append(f"    ▶ FRAGILE: {k} instances flip the sign "
                   f"({', '.join(os.path.basename(x) for x in dropped)}) "
                   f"-> {run / n:+.2f} PAR-2")
    else:
        out.append(f"    sign survives removing any {k - 1} instances")
    return "\n".join(out)


def slurm_submit(args, entries, solvers, paths, timeout, mem_gb, outdir,
                 stamp, sha, name):
    # INSTANCE-MAJOR, deliberately. Solver-major ordering (what this was until
    # 2026-08-04) gives arm 0 the task indices 0..N-1, so SLURM packs that whole
    # arm onto whichever node it fills first and spills later arms elsewhere --
    # ARM IDENTITY ENDS UP CORRELATED WITH NODE IDENTITY, which is a confound the
    # runner itself manufactures. Measured on the Stage B gate (window
    # 20260804-221715-B-preB1): `preB1` ran 28/28 on node017 while `stageB` ran
    # 21 there and 7 on node018, and the SIGN of the per-instance delta tracked
    # the node rather than the arm (same-node subset +52.8 PAR-2, cross-node
    # subset -119.6). Interleaving spreads each arm over all nodes evenly and puts
    # one instance's arms adjacent in dispatch order, so they contend alike.
    # Safe to reorder: the aggregator resolves the arm through
    # `man["solvers"][t["solver"]]`, never from index arithmetic.
    tasks = []
    for rel, status, _ in entries:
        for si in range(len(solvers)):
            tasks.append({"solver": si, "rel": rel, "expected": status,
                          "staged": paths[rel]})
    cpus = max(1, -(-(mem_gb or 8) // 8))  # 8 GB/core rule
    shape = (f"slurm partition={args.partition} cpus/task={cpus} mem/cpu=8G"
             + (f" throttle=%{args.throttle}" if args.throttle else ""))
    man = {"tier": args.tier, "timeout": timeout, "mem_gb": mem_gb,
           "git": sha, "date": stamp, "shape": shape,
           "slurm": {"partition": args.partition, "cpus_per_task": cpus,
                     "mem_per_cpu_gb": 8, "throttle": args.throttle},
           "solvers": [{"name": n, "cmd": resolve_cmd(c), "check_proofs": k,
                        "verify_proofs": v,
                        "score_proofs": sc}
                       for n, c, k, v, sc in solvers],
           "tasks": tasks}
    with open(os.path.join(outdir, "manifest.json"), "w") as f:
        json.dump(man, f, indent=2)
    os.makedirs(os.path.join(outdir, "logs"), exist_ok=True)

    me = os.path.abspath(__file__)
    array = f"0-{len(tasks) - 1}" + (f"%{args.throttle}"
                                     if args.throttle else "")
    # Per-task wall limit: solve + the plan §8.1 checker budget, TWICE, + slack.
    # The second copy pays for `lrat_rescue` — a re-solve plus a cake_lpr run.
    # A limit that cannot fit the runner's own worst case is exactly the trap
    # this file just closed: SLURM would SIGKILL the task mid-rescue and the
    # instance would come back MISSING instead of solved.
    tlimit = fmt_time(2 * (timeout + checker_budget(timeout)) + 120)
    # PIN EVERY ARM TO ONE NODE (--nodelist). For an A/B this is not a nicety:
    # the arms must contend alike, and the partition's nodes do NOT (measured
    # 2026-08-04, mid-run: node017 60/132 cores allocated, node018 6/186). An arm
    # that spills onto the quiet node wins instances it did not earn. Pinning also
    # makes a run directly comparable to the recorded 6.72 PAR-2 noise floor, which
    # was itself measured with all three bit-identical arms on node017.
    nodeline = f"#SBATCH --nodelist={args.nodelist}" if args.nodelist else ""
    apath = os.path.join(outdir, "array.sbatch")
    with open(apath, "w") as f:
        f.write(f"""#!/usr/bin/env bash
#SBATCH --partition={args.partition}
#SBATCH --job-name=bench-{args.tier}-{name}
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task={cpus}
#SBATCH --mem-per-cpu=8G
#SBATCH --time={tlimit}
#SBATCH --array={array}
#SBATCH --output={outdir}/logs/task-%a.log
{nodeline}
exec {sys.executable} {me} --run-one {outdir} \
--task-index $SLURM_ARRAY_TASK_ID
""")
    gpath = os.path.join(outdir, "aggregate.sbatch")
    with open(gpath, "w") as f:
        f.write(f"""#!/usr/bin/env bash
#SBATCH --partition={args.partition}
#SBATCH --job-name=bench-{args.tier}-{name}-agg
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=01:00:00
#SBATCH --output={outdir}/logs/aggregate.log
exec {sys.executable} {me} --aggregate {outdir}
""")
    print(f"{len(tasks)} tasks ({len(solvers)} solver(s) x {len(entries)} "
          f"instances), {shape}, per-task limit {tlimit}")
    if args.dry_run:
        print(f"dry run: wrote {apath} and {gpath}, nothing submitted")
        return 0
    jobid = sbatch(apath)
    aggid = sbatch(gpath, f"--dependency=afterany:{jobid}")
    print(f"submitted: array {jobid}, aggregate {aggid} (runs when the "
          f"array finishes)")
    print(f"watch:     squeue -j {jobid}")
    print(f"summary:   {outdir}/logs/aggregate.log  (re-merge any time: "
          f"{me} --aggregate {outdir})")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--tier", choices="ABCD")
    ap.add_argument("--solver", help="solver under test (competition CLI)")
    ap.add_argument("--name", default=None, help="label for the solver")
    ap.add_argument("--baseline", action="append", default=[],
                    help="baseline solver: kissat|cadical|<command>")
    ap.add_argument("--checked-arm", action="append", default=[],
                    help="extra arm timed WITH proof logging and checking, "
                         "`label=<command>` (baselines are timed proof-free) "
                         "— e.g. a DRAT arm vs an `--lrat` arm in one window")
    ap.add_argument("--jobs", type=int, default=8,
                    help="local mode: parallel solver processes (1..8)")
    ap.add_argument("--timeout", type=float, default=None,
                    help="override the tier's per-instance timeout")
    ap.add_argument("--slurm", action="store_true",
                    help="submit as a SLURM job array (one task per "
                         "solver x instance) + a dependent aggregate job")
    ap.add_argument("--partition", default="cpu-dense-hwulab-q")
    ap.add_argument("--nodelist", default=None,
                    help="pin every array task to this node (e.g. node017). "
                         "Use for any A/B: arms on different nodes contend "
                         "differently and the runner will otherwise correlate "
                         "arm with node.")
    ap.add_argument("--throttle", type=int, default=None,
                    help="cap concurrent array tasks (sbatch %%N) for "
                         "lower-contention gate runs")
    ap.add_argument("--no-check", action="store_true",
                    help="do not write/check proofs for the solver under "
                         "test (baseline-ablation arrays: every arm is a "
                         "reference solver, timed the way baselines are)")
    ap.add_argument("--verify-baseline-proofs", action="store_true",
                    help="run the checker over BASELINE proofs too. Off by "
                         "default since 2026-08-03: a baseline's check is "
                         "recorded and never scored, so it cannot change a "
                         "verdict, and it was 67%% of Tier C checking "
                         "wall-clock. Turn on for a submission-grade claim.")
    ap.add_argument("--no-baseline-proofs", action="store_true",
                    help="time baselines WITHOUT proof logging (pre-rev-72 "
                         "behaviour; the numbers are then not comparable to "
                         "ours and must not be reported as a ratio)")
    ap.add_argument("--dry-run", action="store_true",
                    help="with --slurm: write manifest + scripts, do not "
                         "submit")
    ap.add_argument("--run-one", metavar="OUTDIR", help=argparse.SUPPRESS)
    ap.add_argument("--task-index", type=int, help=argparse.SUPPRESS)
    ap.add_argument("--aggregate", metavar="OUTDIR",
                    help="(re)aggregate an existing --slurm run directory")
    args = ap.parse_args()

    if args.run_one is not None:
        return slurm_run_one(args.run_one, args.task_index)
    if args.aggregate is not None:
        return slurm_aggregate(args.aggregate)
    if not (args.tier and args.solver):
        ap.error("--tier and --solver are required")
    if not args.slurm and not 1 <= args.jobs <= 8:
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
    # (name, cmd, check_proofs, verify_proofs, score_proofs). Plan §8.1 rev 72:
    # EVERY arm WRITES proofs (a timing requirement — the tax is
    # solver-specific). Only ours is VERIFIED and SCORED by default; a
    # baseline's check cannot change a verdict, so since 2026-08-03 it is not
    # run at all unless --verify-baseline-proofs asks for it. See
    # run_instance's docstring for the measured cost.
    solvers = [(name, shlex.split(args.solver), not args.no_check,
                not args.no_check, True)]
    for a in args.checked_arm:
        if "=" not in shlex.split(a)[0]:
            sys.exit("error: --checked-arm needs an explicit label=<command>")
        aname, acmd = a.split("=", 1)
        solvers.append((aname, shlex.split(acmd), True, True, True))
    for b in args.baseline:
        # `label=<command>` gives the arm an explicit name — required when
        # several arms share a leading token (`env VAR=x build/release/tactix`
        # would otherwise all be named "env" and collide in results.json).
        bname = None
        if "=" in shlex.split(b)[0]:
            bname, b = b.split("=", 1)
        spec = BASELINES.get(b, b)
        solvers.append((bname or os.path.basename(shlex.split(spec)[0]),
                        shlex.split(spec), not args.no_baseline_proofs,
                        bool(args.verify_baseline_proofs), False))
    if len({s[0] for s in solvers}) != len(solvers):
        sys.exit("error: duplicate solver names; use --baseline label=<cmd>")

    stage = os.path.join(outdir, "staged")
    paths = stage_instances(entries, stage)

    if args.slurm:
        return slurm_submit(args, entries, solvers, paths, timeout, mem_gb,
                            outdir, stamp, sha, name)

    all_results = {}
    any_bug = False
    for sname, cmd, check_proofs, verify_proofs, score_proofs in solvers:
        print(f"== {sname} on Tier {args.tier} "
              f"({len(entries)} instances, timeout {timeout:g}s, "
              f"jobs {args.jobs}) ==", flush=True)
        with concurrent.futures.ThreadPoolExecutor(args.jobs) as ex:
            results = list(ex.map(
                lambda e: run_instance(cmd, e[0], paths[e[0]], e[1],
                                       timeout, check_proofs, outdir,
                                       mem_gb, score_proofs, verify_proofs),
                entries))
        summary, bug = summarize(sname, results, timeout,
                                 f"jobs={args.jobs}")
        print(summary, flush=True)
        all_results[sname] = results
        any_bug |= bug

    orc = oracle([(n, all_results[n]) for n, _, _, _, sc in solvers if sc],
                 timeout)
    if orc:
        print(summarize_oracle(orc, f"jobs={args.jobs}"), flush=True)

    meta = {"tier": args.tier, "timeout": timeout, "jobs": args.jobs,
            "git": sha, "date": stamp, "mem_gb": mem_gb,
            "solvers": {n: " ".join(c) for n, c, *_ in solvers},
            "oracle": orc,
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
