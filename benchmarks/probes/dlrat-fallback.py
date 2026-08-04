#!/usr/bin/env python3
"""D-LRAT-FALLBACK probe — proves tools/bench/run.py's native-LRAT re-check.

PLAN §8.1 requires, of the checker budget: "A proof that outruns even this is
re-checked as native LRAT before the instance is called unsolved (`--lrat` +
cake_lpr verified `urq45` in 77 s where drat-trim needed 1447 s)."  run.py did
not implement it, and the omission threw away CORRECT urq45 solves three times:
2026-07-27 (354.5 s solve, 2400.1 s check), and 2026-08-03, when it took a
scored 619.3 s UNSAT off the `broad` arm — one of the 27/30 that held broad
Factor admission, a shipping decision.

HOW CLOSE THIS RUNS: in the Tier C gate window `20260804-111459-C-shipped`
(array 1581158, task 29, the true 1-core array shape) urq45 solved in 630.7 s
and drat-trim verified in **4491.98 s of the 4800 s budget** — 93.6 % of it.
Six percent of margin is the whole difference between a scored solve and a
discarded one, which is also why the `honest` real-budget leg is NOT part of
this probe: at 4492 s it does not fire, so it would cost 1.5 h to demonstrate
nothing.  `forced` below is the leg that carries the correctness claim.

Every leg runs the REAL `run_instance` an array task runs (imported, not
re-implemented), so a green leg is evidence about the shipped runner.

  forced  urq45 with `checker_budget` shrunk to BUDGET s (default 600).  Real
          solve, real ~900 MB DRAT proof, real drat-trim run to a real
          subprocess.TimeoutExpired, real `--lrat` re-solve, real cake_lpr —
          only the budget NUMBER is shrunk, so the trigger fires
          DETERMINISTICALLY instead of depending on a node's mood.
  fast    four seconds-scale legs that pin the guards:
          clean   — a check that VERIFIES must be untouched: no rescue, no new
                    cost.  This is the no-regression leg for the 29 Tier C
                    instances that never come near the budget.
          reject  — tools/fuzz/broken/truncate-proof.sh (kissat, proof cut to
                    half its lines) is a GENUINE rejection: it must still score
                    UNSOLVED and must NOT rescue.  Silencing a real N2 failure
                    would be a correctness hole, which is why only the timeout
                    message may trigger the fallback.
          anomaly — an arm that answers UNSAT with DRAT but SAT under `--lrat`.
                    The re-solve disagrees with the verdict being rescued, so
                    the rescue must REFUSE and leave the instance unsolved.
          guard   — a BASELINE (score_proofs False) whose check times out must
                    not be re-solved at all: baselines are never scored on
                    proofs and their CLI need not accept `--lrat`.

Each leg asserts its pre-registered outcome and exits nonzero if it fails.
Submit with dlrat-fallback.sh — sbatch only (plan §8.1 rev 65).
"""

import importlib.util
import json
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))

_spec = importlib.util.spec_from_file_location(
    "bench_run", os.path.join(ROOT, "tools", "bench", "run.py"))
bench = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(bench)          # __main__-guarded: import is inert

SHIPPED = os.path.join(ROOT, "build", "pin-ship", "tactix")
KISSAT = os.path.join(ROOT, "tools", "solvers", "bin", "kissat")
TRUNCATE = os.path.join(ROOT, "tools", "fuzz", "broken", "truncate-proof.sh")
URQ45 = "tierC/urq45.cnf.xz"
SMALL = "tierA/tr-n200-s000.cnf"         # UNSAT, sub-second, non-trivial proof

FAILURES = []


def stage(rel, work):
    src = os.path.join(bench.CACHE, rel)
    if not os.path.exists(src):
        sys.exit(f"probe: missing instance {src}")
    if not rel.endswith(".xz"):
        return src
    dst = os.path.join(work, os.path.basename(rel)[:-3])
    with open(dst, "wb") as f:
        subprocess.run(["xz", "-dc", src], stdout=f, check=True)
    return dst


def shrink_budget(seconds):
    """Force the plan §8.1 trigger by shrinking the budget NUMBER only.

    drat-trim, the proof, and the TimeoutExpired are all real; the fallback
    cannot tell this from a 4800 s budget it genuinely outran."""
    bench.checker_budget = lambda _timeout, _b=seconds: float(_b)


def report(label, res, expect):
    """Print the whole result dict, then check the pre-registered facts."""
    print(f"\n=== leg {label} ===")
    print(json.dumps(res, indent=2, sort_keys=True), flush=True)
    for what, ok in expect:
        print(f"  [{'PASS' if ok else 'FAIL'}] {what}", flush=True)
        if not ok:
            FAILURES.append(f"{label}: {what}")


def write_wrapper(path, body):
    with open(path, "w") as f:
        f.write("#!/usr/bin/env bash\n" + body)
    os.chmod(path, 0o755)
    return path


# ------------------------------------------------------- the load-bearing leg

def run_forced(work, budget):
    cnf = stage(URQ45, work)
    shrink_budget(budget)
    eff = bench.checker_budget(1200.0)
    print(f"leg forced: {SHIPPED} on {URQ45}, solve timeout 1200s, "
          f"checker budget {eff:g}s (shrunk; drat-trim needs ~4492 s here)",
          flush=True)
    res = bench.run_instance([SHIPPED], URQ45, cnf, "UNSAT", 1200.0,
                             True, work, None, True, True)
    msg = res.get("proof_msg") or ""
    report("forced", res, [
        ("verdict is UNSAT", res["verdict"] == "UNSAT"),
        (f"DRAT check ran the full {eff:g}s budget",
         (res.get("proof_check_time") or 0) >= eff),
        ("the DRAT check TIMED OUT (it did not reject)",
         msg.startswith(bench.CHECKER_TIMEOUT_PREFIX)),
        ("RESCUE FIRED: proof_rescue == 'lrat'",
         res.get("proof_rescue") == "lrat"),
        ("rescue cost recorded", bool(res.get("proof_rescue_time"))),
        ("proof_ok True (a certificate WAS verified)",
         res.get("proof_ok") is True),
        ("SCORED SOLVED — this is the instance the old runner discarded",
         res["solved"] is True),
        ("proof_bytes kept (the DRAT arm's proof size)",
         bool(res.get("proof_bytes"))),
    ])


# ---------------------------------------------------------------- fast legs

def run_clean(work):
    """A check that VERIFIES must be completely untouched by the new path."""
    cnf = stage(SMALL, work)
    res = bench.run_instance([KISSAT, "-q"], SMALL, cnf, "UNSAT", 30.0,
                             True, work, None, True, True)
    report("clean", res, [
        ("proof VERIFIED by drat-trim", res.get("proof_ok") is True),
        ("NO rescue attempted", res.get("proof_rescue") is None),
        ("no rescue cost charged", res.get("proof_rescue_time") is None),
        ("SCORED SOLVED", res["solved"] is True),
    ])


def run_reject(work):
    """A GENUINE rejection must still cost the instance, and must not rescue."""
    cnf = stage(SMALL, work)
    res = bench.run_instance([TRUNCATE], SMALL, cnf, "UNSAT", 30.0,
                             True, work, None, True, True)
    msg = res.get("proof_msg") or ""
    report("reject", res, [
        ("verdict is UNSAT (the solver was right; the PROOF is broken)",
         res["verdict"] == "UNSAT"),
        ("proof_ok False", res.get("proof_ok") is False),
        ("checker REJECTED, it did not time out",
         not msg.startswith(bench.CHECKER_TIMEOUT_PREFIX)),
        ("NO rescue attempted", res.get("proof_rescue") is None),
        ("still SCORED UNSOLVED", res["solved"] is False),
    ])


def run_anomaly(work):
    """Re-solve disagrees with the original verdict => refuse to rescue."""
    cnf = stage(SMALL, work)
    arm = write_wrapper(os.path.join(work, "flip-under-lrat.sh"), f"""
# Probe-local broken arm: honest UNSAT + DRAT proof, but claims SAT as soon as
# `--lrat` appears — i.e. the re-solve contradicts the run being rescued.
set -u
for a in "$@"; do
  if [ "$a" = "--lrat" ]; then echo "s SATISFIABLE"; echo "v 0"; exit 10; fi
done
exec {KISSAT} -q --no-binary "$@"
""")
    shrink_budget(0.05)          # drat-trim cannot finish; TimeoutExpired
    res = bench.run_instance([arm], SMALL, cnf, "UNSAT", 30.0,
                             True, work, None, True, True)
    resc = res.get("proof_rescue") or ""
    report("anomaly", res, [
        ("verdict is UNSAT", res["verdict"] == "UNSAT"),
        ("the DRAT check TIMED OUT",
         (res.get("proof_msg") or "").startswith(bench.CHECKER_TIMEOUT_PREFIX)),
        ("rescue was ATTEMPTED and REFUSED", resc.startswith("failed")),
        ("the refusal names the disagreement", "SAT" in resc),
        ("SCORED UNSOLVED", res["solved"] is False),
    ])


def run_guard(work):
    """score_proofs False (a baseline) is never re-solved."""
    cnf = stage(SMALL, work)
    shrink_budget(0.05)
    res = bench.run_instance([KISSAT, "-q"], SMALL, cnf, "UNSAT", 30.0,
                             True, work, None, False, True)
    report("guard", res, [
        ("the DRAT check TIMED OUT",
         (res.get("proof_msg") or "").startswith(bench.CHECKER_TIMEOUT_PREFIX)),
        ("NO rescue on an unscored arm", res.get("proof_rescue") is None),
        ("baseline keeps its solve (a failed check is never scored)",
         res["solved"] is True),
    ])


LEGS = ("forced", "fast")


def main():
    leg = sys.argv[1] if len(sys.argv) > 1 else "fast"
    budget = float(sys.argv[2]) if len(sys.argv) > 2 else 600.0
    if leg not in LEGS:
        sys.exit(f"usage: {sys.argv[0]} {{{'|'.join(LEGS)}}} [forced-budget]")
    jid = os.environ.get("SLURM_JOB_ID", str(os.getpid()))
    sha = subprocess.run(["git", "-C", ROOT, "rev-parse", "--short", "HEAD"],
                         capture_output=True, text=True).stdout.strip()
    work = tempfile.mkdtemp(prefix=f"dlrat-{jid}-", dir=os.environ.get("TMPDIR"))
    print(f"host={os.uname().nodename} work={work} leg={leg} git={sha}",
          flush=True)
    try:
        if leg == "forced":
            run_forced(work, budget)
        else:
            run_clean(work)      # order matters: the budget-shrinking legs
            run_reject(work)     # below replace bench.checker_budget for good
            run_anomaly(work)
            run_guard(work)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    if FAILURES:
        print("\nPROBE FAILED:\n  " + "\n  ".join(FAILURES), flush=True)
        return 1
    print("\nPROBE PASSED (all pre-registered checks held)", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
