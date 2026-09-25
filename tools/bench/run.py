#!/usr/bin/env python3
"""tools/bench/run.py — tiered benchmark runner (plan §8.1/§8.3).

Usage:
  run.py --tier A --solver ./build/tactix [--baseline kissat] [--jobs 8]
  run.py --tier C --solver ./build/tactix --baseline kissat --slurm
  run.py --aggregate benchmarks/runs/<dir>      # re-merge a --slurm run

- SLURM mode (see the slurm-jobs skill): submits one array task per
  solver x instance to cpu-dense-hwulab-q (8 GiB per physical core, with two
  requested logical CPUs per physical core on the current SMT-2 nodes;
  normally 8 logical CPUs / 32 GiB, while the 16-GiB debug contract uses 4),
  plus a dependent aggregate job that merges the
  per-task JSONs into the identical results.json a local run writes. The
  allocation shape (partition, cpus/mem per task, throttle) is recorded in
  place of the local jobs count; numbers are comparable only across identical
  shapes.
  Arrays run at full width by default. --throttle is exceptional: use it only
  on an explicit user request or for a documented correctness/resource limit;
  --dry-run writes manifest + sbatch scripts without submitting.

- Tier lists are frozen files `benchmarks/tiers/<tier>.list`; each
  non-comment line is `<relpath> <SAT|UNSAT|UNKNOWN> <ref_seconds>` with
  paths relative to `benchmarks/cache/`.
- For the solver under test, every UNSAT answer's proof is checked; a rejected
  (or missing) proof counts as UNSOLVED, competition-style. The arm's own
  command picks the requested format: Tactix automatic output is routed by
  certificate content (startup PBP, witnessed DSR, or witness-free DSR),
  `--lrat` selects native LRAT, and `--drat` selects compatibility DRAT.
  Other solver commands default to DRAT.
  A DRAT check that outruns its budget is not final: the arm is re-solved with
  `--lrat` and cake_lpr verifies that instead (§8.1, `lrat_rescue`). A checker
  that rejects still scores the instance unsolved — that is an N2 failure.
  Verdicts are also checked against the expected status in the list — any
  mismatch is a correctness bug (stop and use the fuzz skill).
- Every arm writes proofs, baselines included (plan §8.1): timing a
  proof-writing solver against proof-free references compares two different
  workloads. A baseline's proof is still checked, but a failed check is
  recorded, never scored against it (--no-baseline-proofs restores the old,
  non-comparable behaviour). Checker budget: max(3600 s, 4 x solve timeout).
- Score: PAR-2 = mean(runtime if solved else 2*timeout), reported with
  #solved and SAT/UNSAT split, proof-check time, and proof size.
- Oracle (always on, 2+ scored arms): the per-instance best over the scored
  arms (baselines are references, not candidate policies), plus each arm's gap
  to it — also in results.json under "oracle". It is not a runnable policy; it
  is the ceiling a per-instance admission mechanism could reach, so a gate is
  worth shipping only if it beats the trivial always-on arm and takes a fair
  share of that gap. See docs/design/2026-08-04-scheduling-sophistication-
  diagnosis.md §4.
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
from pathlib import Path
import re
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
MANIFESTS = os.path.join(HERE, "manifests")
RUNS = os.path.join(ROOT, "benchmarks", "runs")
DRAT_TRIM = os.path.join(ROOT, "tools", "checkers", "bin", "drat-trim")
DSR_TRIM = os.path.join(ROOT, "tools", "checkers", "bin", "dsr-trim")
SR_CHECK = os.path.join(ROOT, "tools", "checkers", "bin", "srcheck")
DSR_COMPRESS = os.path.join(ROOT, "tools", "checkers", "bin", "dsr-compress")
CAKE_LPR = os.path.join(ROOT, "tools", "checkers", "bin", "cake_lpr")
PB_PINS = os.path.join(ROOT, "benchmarks", "baselines",
                       "sc24-26-32g-5000s", "pins")
VERIPB = os.path.join(PB_PINS, "veripb")
CAKE_PB_CNF = os.path.join(PB_PINS, "cake-pb-cnf")
sys.path.insert(0, HERE)
from proof_oracle import (bridge_trivial_input_binary_lsr,  # noqa: E402
                          bridge_trivial_input_lrat, bridge_trivial_input_lsr,
                          certificate_route, DSRFormatError,
                          drat_stages, dsr_compress_status,
                          dsr_proof_as_drat, pb_stages,
                          reference_dsr_stages, canonical_checker_cnf,
                          proof_check_status, drat_command_with_timeout,
                          restate_duplicate_input_lrat, run_captured,
                          stage_environment)
# Oracle 1 of the fuzz harness (§8.2), shared here since 2026-08-08: a SAT
# verdict's v-lines are checked against the ORIGINAL formula. The wrong-SAT
# exhibit was caught only by the expected-verdict column; an instance without
# a recorded verdict would have scored a bogus model as a solve.
CHECKMODEL = os.path.join(ROOT, "tools", "fuzz", "checkmodel")
MODEL_CHECK_BUDGET = 600  # linear scan; Tier-scale instances finish in seconds

# The one message `check_proof` emits when the checker outran its budget rather
# than rejecting. Single-sourced because it is a control signal, not prose: it,
# and only it, may trigger the §8.1 native-LRAT re-check (`lrat_rescue`).
CHECKER_TIMEOUT_PREFIX = "checker exceeded its "

# The `kind` field classifies the checker's verdict. A last-output-line archive
# cannot: drat-trim always prints `c verification time: …` last, collapsing an
# invalid certificate (an N2 breach) and a checker that ran out of room (a
# scoring artefact) into one string. Only the classified kind may gate a rescue.
# Exhibit: benchmarks/reports/2026-08-05-sweep-proof-rejection.md.
CHK_VERIFIED = "verified"        # `s VERIFIED` / `s VERIFIED UNSAT`
CHK_REJECTED = "rejected"        # `s NOT VERIFIED` — N2 failure, never rescued
CHK_DERIVATION = "derivation"    # `s DERIVATION` — not a refutation
CHK_TIMEOUT = "checker-timeout"  # budget outrun — the only rescuable outcome
CHK_RESOURCE = "checker-oom"     # `s MEMORY ALLOCATION ERROR` / killed
CHK_ERROR = "checker-error"      # no recognisable verdict at all
# Outcomes that mean the certificate is bad, not that the checker gave up.
CHK_OUR_FAULT = (CHK_REJECTED, CHK_DERIVATION)

# Lines worth keeping verbatim: they name the offending lemma and proof line,
# which is the entire starting point of a shrink.
_CHK_DIAG = ("RAT check on proof pivot failed", "failed at proof line",
             "RAT check failed on all possible pivots", "Checking failed at",
             "c ERROR", "no empty clause")


# One source of truth for the reach patterns: tools/fuzz/fuzz.py owns them and a
# conformance test pins every registry entry to a row there. A second copy here
# would drift, and a drifted reach table reads as "the rule did not fire".
def _load_reach_specs():
    import importlib.util
    fp = os.path.join(HERE, "..", "fuzz", "fuzz.py")
    spec = importlib.util.spec_from_file_location("_tactix_fuzz", fp)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return [(lbl, re.compile(p, re.M)) for lbl, p in mod.REACH_SPECS]


try:
    _REACH_SPECS = _load_reach_specs()
except Exception as _e:    # fuzz.py absent or unimportable
    # Warn loudly: a silently empty table makes every tier run record "no rule
    # fired", which reads as a finding rather than as a broken import.
    sys.stderr.write(f"run.py: WARNING reach counters DISABLED ({_e!r}); "
                     f"tier results will record no `reached` data\n")
    _REACH_SPECS = []


def classify_checker_output(stdout, stderr, returncode, checker='drat-trim',
                            *, input_has_empty_clause=None):
    """(kind, msg, detail) for one checker invocation.

    Split lines recognize drat-trim's leading carriage return without accepting
    embedded or wrong-checker tokens. The shared oracle owns exact successful
    statuses and exits; diagnostic classification must never override it."""
    both = stdout + "\n" + stderr
    lines = [line.strip() for line in both.splitlines()]
    status = proof_check_status(checker, returncode, stdout, stderr,
                                input_has_empty_clause=input_has_empty_clause)
    if status == 'rejected':
        kind = CHK_REJECTED
    elif "s MEMORY ALLOCATION ERROR" in lines:
        kind = CHK_RESOURCE
    elif "s DERIVATION" in lines:
        kind = CHK_DERIVATION
    elif status == 'verified':
        kind = CHK_VERIFIED
    elif returncode is not None and returncode < 0:
        kind = CHK_RESOURCE                 # killed by a signal (OOM killer)
    else:
        kind = CHK_ERROR
    detail = [l.strip() for l in both.splitlines()
              if any(d in l for d in _CHK_DIAG)][:4]
    if kind == CHK_VERIFIED:
        msg = "VERIFIED"
    elif detail:
        msg = f"{kind}: {detail[0][:160]}"
    else:
        tail = (stderr.strip() or stdout.strip()).splitlines()
        msg = f"{kind}: " + (tail[-1][:160] if tail else f"exit {returncode}")
    return kind, msg, detail

TIER_TIMEOUT = {
    "A": 30.0, "B": 300.0, "C": 1200.0, "D": 5000.0,
    "P4A": 30.0, "P4B": 300.0,
}
# All maintained tiers use the current 32-GiB contract. Under the cluster's
# 8-GiB/physical-core and SMT-2 rule this allocates eight logical CPUs; the
# extra CPUs buy memory capacity, not solver parallelism.
TIER_MEM_GB = {tier: 32 for tier in ("A", "B", "C", "D", "P4A", "P4B")}
BASELINES = {
    "kissat": os.path.join(ROOT, "tools", "solvers", "bin", "kissat"),
    "cadical": os.path.join(ROOT, "tools", "solvers", "bin", "cadical"),
    "minisat": os.path.join(ROOT, "tools", "solvers", "bin", "minisat"),
    "glucose": os.path.join(ROOT, "tools", "solvers", "bin", "glucose"),
}


def read_tier_list(tier):
    directory = MANIFESTS if tier.startswith("P4") else TIERS
    path = os.path.join(directory, f"{tier}.list")
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

    Tactix defaults to DSR. Explicit flags select compatibility formats;
    external solvers retain their usual DRAT default."""
    if "--lrat" in cmd:
        return "lrat"
    if "--dsr" in cmd:
        return "dsr"
    if "--drat" in cmd:
        return "drat"
    return "dsr" if os.path.basename(cmd[0]) == "tactix" else "drat"


def checker_budget(timeout):
    """Plan §8.1: max(3600 s, 4 x the tier's solve timeout).

    A tighter budget scores instances unsolved for a checker-budget reason the
    competition would not have applied — measured, `2 x timeout` did exactly
    that on a real UNSAT."""
    return max(3600.0, 4 * timeout)


def check_proof(fmt, cnf, proof, budget, *, allow_pbp=True, route=None):
    """Verify one certificate. Returns (ok, message, kind, detail).

    The KIND matters and used to be lost: a checker that OOMs or times out is
    otherwise INDISTINGUISHABLE from one that REJECTS. cake_lpr needs several GB
    on a gigabyte-scale LRAT, so an under-provisioned array task dies at malloc
    and looks exactly like a rejected proof -- and the converse misreading, a
    genuine `s NOT VERIFIED` taken for an OOM, is just as easy. See
    classify_checker_output."""
    if fmt == "dsr":
        if route is None:
            try:
                route = certificate_route(proof)
            except DSRFormatError as error:
                return False, f"rejected: {error}", CHK_REJECTED, []
        if route == "invalid-dsr":
            return False, "rejected: malformed DSR", CHK_REJECTED, []
        if route == "pbp":
            if not allow_pbp:
                return (False, "rejected: explicit DSR emitted PBP",
                        CHK_REJECTED, [])
            return check_pbp_proof(cnf, proof, budget)
        if route == "drat":
            return check_witness_free_dsr(cnf, proof, budget)
        started = time.monotonic()

        def remaining():
            return max(0.001, budget - (time.monotonic() - started))

        with tempfile.TemporaryDirectory(prefix="tactix-dsr-check-") as scratch:
            checker_cnf = os.path.join(scratch, "checker.cnf")
            try:
                canonical_checker_cnf(cnf, checker_cnf)
            except (OSError, ValueError) as error:
                return False, f"checker-error: invalid original DIMACS: {error}", CHK_ERROR, []
            binary_lsr = os.path.join(scratch, "proof.binary.lsr")
            text_lsr = os.path.join(scratch, "proof.lsr")
            # proof_oracle.reference_dsr_stages owns the pipeline definition.
            stages = [
                (name, argv, argv[-1] if name != "srcheck" else None)
                for name, argv in reference_dsr_stages(
                    DSR_TRIM, DSR_COMPRESS, SR_CHECK, cnf, proof, binary_lsr,
                    text_lsr, checker_cnf=checker_cnf)]
            details = []
            for index, (name, argv, produced) in enumerate(stages):
                if index == 1:
                    made = stages[0][2]
                    (bridge_trivial_input_binary_lsr if made == binary_lsr
                     else bridge_trivial_input_lsr)(cnf, made)
                try:
                    checked = run_captured(argv, timeout=remaining())
                except subprocess.TimeoutExpired:
                    return (False,
                            f"{CHECKER_TIMEOUT_PREFIX}{budget:g}s budget",
                            CHK_TIMEOUT, details)
                if name == "dsr-compress":
                    # A format conversion, not an acceptance oracle.
                    converted = dsr_compress_status(
                        checked.returncode,
                        checked.stdout + "\n" + checked.stderr) == "verified"
                    kind, msg, detail = (
                        (CHK_VERIFIED, "VERIFIED", []) if converted else
                        (CHK_ERROR, "checker-error: dsr-compress failed", []))
                else:
                    kind, msg, detail = classify_checker_output(
                        checked.stdout, checked.stderr, checked.returncode, name)
                details.extend(detail)
                if (kind != CHK_VERIFIED or
                        (produced is not None and
                         (not os.path.isfile(produced) or
                          os.path.getsize(produced) == 0))):
                    if kind == CHK_VERIFIED:
                        kind, msg = CHK_ERROR, "checker-error: empty DSR stage output"
                    return False, msg, kind, details
            return True, "VERIFIED", CHK_VERIFIED, details

    with tempfile.TemporaryDirectory(prefix="tactix-compat-check-") as scratch:
        checker_cnf = os.path.join(scratch, "checker.cnf")
        try:
            spelling = canonical_checker_cnf(cnf, checker_cnf)
        except (OSError, ValueError) as error:
            return False, f"checker-error: invalid original DIMACS: {error}", CHK_ERROR, []
        argv = ([CAKE_LPR, checker_cnf, proof] if fmt == "lrat"
                else drat_command_with_timeout([DRAT_TRIM, checker_cnf, proof], budget))
        try:
            c = run_captured(argv, timeout=budget)
        except subprocess.TimeoutExpired:
            return (False, f"{CHECKER_TIMEOUT_PREFIX}{budget:g}s budget",
                    CHK_TIMEOUT, [])
    kind, msg, detail = classify_checker_output(c.stdout, c.stderr,
                                                c.returncode,
                                                'cake_lpr' if fmt == 'lrat' else 'drat-trim',
                                                input_has_empty_clause=
                                                spelling['first_empty_clause'] is not None)
    return kind == CHK_VERIFIED, msg, kind, detail


def check_pbp_proof(cnf, proof, budget):
    """Check automatic startup PBP through both pinned PB authorities."""
    started = time.monotonic()
    with tempfile.TemporaryDirectory(prefix="tactix-pbp-check-") as scratch:
        checker_cnf = os.path.join(scratch, "checker.cnf")
        kernel = os.path.join(scratch, "proof.kernel.pbp")
        try:
            canonical_checker_cnf(cnf, checker_cnf)
        except (OSError, ValueError) as error:
            return False, f"checker-error: invalid original DIMACS: {error}", CHK_ERROR, []
        details = []
        for name, argv in pb_stages(VERIPB, CAKE_PB_CNF, cnf, proof, kernel,
                                    checker_cnf=checker_cnf):
            remaining = budget - (time.monotonic() - started)
            if remaining <= 0:
                return (False, f"{CHECKER_TIMEOUT_PREFIX}{budget:g}s budget",
                        CHK_TIMEOUT, details)
            try:
                checked = run_captured([str(part) for part in argv],
                                       timeout=remaining,
                                       env=stage_environment(name))
            except subprocess.TimeoutExpired:
                return (False, f"{CHECKER_TIMEOUT_PREFIX}{budget:g}s budget",
                        CHK_TIMEOUT, details)
            except OSError as error:
                return False, f"checker-error: {name}: {error}", CHK_ERROR, details
            kind, msg, detail = classify_checker_output(
                checked.stdout, checked.stderr, checked.returncode, name)
            details.extend(detail)
            if kind != CHK_VERIFIED:
                return False, msg, kind, details
            if name == "veripb" and (not os.path.isfile(kernel) or
                                      os.path.getsize(kernel) == 0):
                return False, "checker-error: no kernel PBP produced", CHK_ERROR, details
        return True, "VERIFIED", CHK_VERIFIED, details


def check_witness_free_dsr(cnf, proof, budget):
    """Use the DRAT owner's complete route for a DSR without witnesses."""
    started = time.monotonic()
    with tempfile.TemporaryDirectory(prefix="tactix-dsr-drat-check-") as scratch:
        checker_cnf = Path(scratch) / "checker.cnf"
        drat, lrat = Path(scratch) / "proof.drat", Path(scratch) / "proof.lrat"
        try:
            spelling = canonical_checker_cnf(cnf, checker_cnf)
        except (OSError, ValueError) as error:
            return False, f"checker-error: invalid original DIMACS: {error}", CHK_ERROR, []
        if not dsr_proof_as_drat(proof, drat):
            return False, "rejected: witnessed line met during rewrite", CHK_REJECTED, []
        checked_lrat = lrat
        details = []
        for name, command in drat_stages(DRAT_TRIM, CAKE_LPR, cnf, drat, lrat,
                                         checker_cnf=checker_cnf):
            remaining = budget - (time.monotonic() - started)
            if remaining <= 0:
                return (False, f"{CHECKER_TIMEOUT_PREFIX}{budget:g}s budget",
                        CHK_TIMEOUT, details)
            if name == "drat-trim":
                command = drat_command_with_timeout(command, remaining)
            else:
                command = [*command[:-1], checked_lrat]
            try:
                checked = run_captured([str(part) for part in command],
                                       timeout=remaining,
                                       env=stage_environment(name))
            except subprocess.TimeoutExpired:
                return (False, f"{CHECKER_TIMEOUT_PREFIX}{budget:g}s budget",
                        CHK_TIMEOUT, details)
            except OSError as error:
                return False, f"checker-error: {name}: {error}", CHK_ERROR, details
            kind, msg, detail = classify_checker_output(
                checked.stdout, checked.stderr, checked.returncode, name,
                input_has_empty_clause=spelling['first_empty_clause'] is not None)
            details.extend(detail)
            if kind != CHK_VERIFIED:
                return False, msg, kind, details
            if name == "drat-trim":
                bridge_trivial_input_lrat(spelling, lrat, checked.returncode,
                                          checked.stdout, checked.stderr)
                if not lrat.is_file() or lrat.stat().st_size == 0:
                    return False, "checker-error: no LRAT produced", CHK_ERROR, details
                try:
                    checked_lrat, _ = restate_duplicate_input_lrat(
                        checker_cnf, spelling, lrat,
                        Path(scratch) / "proof.original-input.lrat")
                except ValueError as error:
                    return False, f"rejected: {error}", CHK_REJECTED, details
        return True, "VERIFIED", CHK_VERIFIED, details


def solve_once(argv, timeout):
    """One solver invocation under the wall timeout + CPU backstop.

    Returns (verdict, elapsed, reached). Shared by the timed run and the §8.1 LRAT
    re-solve. Memory is enforced by the enclosing SLURM task cgroup; an
    RLIMIT_AS address-space cap is deliberately not installed because virtual
    reservations are not resident memory."""
    def limits():
        # kernel-enforced CPU backstop: subprocess.run(timeout=) only kills the
        # child on the normal timeout path; if the parent dies abruptly the child
        # is reparented to init and spins forever. RLIMIT_CPU makes the kernel
        # stop it regardless of parent state (slack keeps wall-timeout primary).
        cap = int(timeout) + 5
        resource.setrlimit(resource.RLIMIT_CPU, (cap, cap + 2))

    t0 = time.monotonic()
    try:
        r = subprocess.run(argv, capture_output=True, text=True,
                           errors="replace", timeout=timeout,
                           preexec_fn=limits)
        return (parse_verdict(r.returncode, r.stdout), time.monotonic() - t0,
                reach_counters(r.stdout), r.stdout)
    except subprocess.TimeoutExpired:
        return "UNKNOWN", timeout, {}, ""


# Which registry entries the run reached, read from the solver's own `c `
# counters and recorded per task: reach evidence is a phase-3 exit condition
# (plan §9). Empty on a timeout, since a killed run prints no stats — a tally
# that cannot tell "did not fire" from "was not measured" is vacuous.
def reach_counters(stdout):
    out = {}
    for label, pat in _REACH_SPECS:
        m = pat.search(stdout)
        if m:
            out[label] = int(m.group(1))
    return out


def lrat_rescue(res, cmd, cnf, proof, timeout):
    """Plan §8.1: "a proof that outruns even this [budget] is re-checked as
    native LRAT before the instance is called unsolved".

    It must be a RE-SOLVE, not a conversion: producing LRAT from DRAT needs
    exactly drat-trim's backward search, the search that just timed out. So the
    arm is re-run with `--lrat` appended and cake_lpr verifies what it writes.
    Measured, cake_lpr can be an order of magnitude faster than drat-trim on
    exactly the proofs that overrun the budget.

    Why this exists: without it a checker-budget overrun DISCARDS A CORRECT
    SOLVE, which has silently changed tier verdicts more than once.

    Sets res["proof_rescue"] to "lrat" when a certificate verified, else to a
    "failed: <reason>" string, and returns whether the instance is proof-clean.
    A re-solve that answers DIFFERENTLY is never rescued: that is a genuine
    anomaly and it must stay visible as an unsolved instance.

    RESCUABILITY IS A PRECONDITION, NOT A HINT. Only CHK_TIMEOUT reaches here,
    and the assert says so structurally: a REJECTED certificate must never be
    re-scored as a budget overrun, which is precisely what this path would do
    if the caller's guard ever loosened."""
    assert res.get("proof_check_kind") == CHK_TIMEOUT, (
        "lrat_rescue is only for a checker BUDGET overrun; got "
        f"{res.get('proof_check_kind')!r} on {res.get('instance')!r}")
    # The DRAT proof has already been sized and judged, and the LRAT one is
    # bigger; drop it before writing a second multi-GB file into the same
    # (node-shared) scratch.
    if os.path.exists(proof):
        os.unlink(proof)
    lproof = proof + ".rescue.lrat"
    t0 = time.monotonic()
    try:
        verdict, solve_s, _, _ = solve_once(cmd + ["--lrat", cnf, lproof],
                                            timeout)
        if verdict != res["verdict"]:
            res["proof_rescue"] = (f"failed: --lrat re-solve answered "
                                   f"{verdict}, not {res['verdict']}")
        elif not os.path.exists(lproof):
            res["proof_rescue"] = "failed: --lrat re-solve wrote no proof"
        else:
            lbytes = os.path.getsize(lproof)
            t1 = time.monotonic()
            ok, msg, rkind, _ = check_proof("lrat", cnf, lproof,
                                            checker_budget(timeout))
            res["proof_rescue_kind"] = rkind
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
                 score_proofs=True, verify_proofs=True):
    """`check_proofs` = hand the solver a proof path, i.e. make it write one.
    `verify_proofs` = actually run the checker over what it wrote.
    `score_proofs` = let a failed check count the instance unsolved.

    The three are separate because they answer separate questions.

    Writing is a timing requirement and applies to every arm (plan §8.1):
    proof generation costs 4.4-6.9 % and the tax is solver-specific, so a
    proof-free baseline number is not comparable.

    Verifying a baseline's proof cannot change any verdict — `score_proofs` is
    already False for a baseline, so its failed checks are recorded and never
    held against it — while costing the majority of a tier run's checking
    wall-clock. Hence `verify_proofs` defaults to False for a baseline
    (--verify-baseline-proofs opts back in for a submission-grade claim).
    `proof_bytes` is still recorded from a stat, so proof size stays a tracked
    metric for every arm.

    `score_proofs` False without `verify_proofs` False = verify, record, never
    score. A baseline's certificate must not be scored against this runner's
    checker budget: measured, that alone costs the references several instances
    and reads as a win that was not earned."""
    fmt = proof_format(cmd)
    res = {"instance": rel, "expected": expected, "verdict": "UNKNOWN",
           "time": timeout, "solved": False, "mismatch": False,
           "proof_ok": None, "proof_bytes": None, "proof_check_time": None,
           "proof_format": fmt if check_proofs else None,
           "proof_route": None,
           "proof_scored": bool(check_proofs and verify_proofs and score_proofs),
           "proof_msg": None,
           # The classified checker outcome (CHK_*) and the checker's own
           # diagnostic lines. `proof_msg` is prose and may be truncated;
           # `proof_check_kind` is the machine-readable fact separating an
           # invalid certificate from a checker that gave up.
           "proof_check_kind": None, "proof_check_detail": None,
           # §8.1 native-LRAT re-check: "lrat" when it saved the instance, a
           # "failed: ..." reason when it was tried and did not, None when the
           # DRAT check never ran out of time. Always present so the schema is
           # stable across runs and the count is aggregatable.
           "proof_rescue": None, "proof_rescue_time": None,
           "proof_rescue_kind": None,
           # §8.2 oracle 1 on the bench path (2026-08-08): SAT verdicts carry
           # a checkmodel result. None = not run (baseline / timeout / UNSAT).
           "model_ok": None, "model_msg": None}
    if cnf is None or not os.path.exists(cnf):
        res["verdict"] = "MISSING"
        return res
    proof = os.path.join(workdir, rel.replace("/", "_") + "." + fmt)
    argv = cmd + [cnf] + ([proof] if check_proofs else [])

    verdict, elapsed, reached, stdout = solve_once(argv, timeout)
    res["reached"] = reached
    res["time"] = round(elapsed, 3)
    res["verdict"] = verdict

    if verdict in ("SAT", "UNSAT"):
        res["solved"] = True
        if expected in ("SAT", "UNSAT") and verdict != expected:
            res["mismatch"] = True   # correctness bug — counts as unsolved
            res["solved"] = False
        elif verdict == "SAT":
            # Same gating as proofs: a baseline records nothing (a reference
            # solver's model never changes a scored number); a scored arm's
            # failed model check counts the instance unsolved and marks the
            # window not clean (the wrong-SAT class no proof checker sees).
            if not verify_proofs:
                res["model_msg"] = "not checked (§8.1: baseline)"
            else:
                mfile = os.path.join(workdir, rel.replace("/", "_") + ".model")
                with open(mfile, "w") as f:
                    f.write(stdout)
                try:
                    mr = subprocess.run([CHECKMODEL, cnf, mfile],
                                        capture_output=True, text=True,
                                        errors="replace",
                                        timeout=MODEL_CHECK_BUDGET)
                    res["model_ok"] = mr.returncode == 0
                    res["model_msg"] = (mr.stdout or mr.stderr).strip()[:200] \
                        or ("OK" if mr.returncode == 0 else "FAILED")
                    if not res["model_ok"] and score_proofs:
                        res["solved"] = False
                except subprocess.TimeoutExpired:
                    res["model_msg"] = "checkmodel outran its budget"
                finally:
                    os.unlink(mfile)
        elif verdict == "UNSAT" and check_proofs:
            if not os.path.exists(proof):
                res["proof_ok"] = False
                res["proof_msg"] = "no proof file written"
                if score_proofs:
                    res["solved"] = False
            else:
                # size is free from a stat and is tracked for every arm
                res["proof_bytes"] = os.path.getsize(proof)
                if fmt == "dsr":
                    try:
                        res["proof_route"] = certificate_route(proof)
                    except DSRFormatError:
                        res["proof_route"] = "invalid-dsr"
                else:
                    res["proof_route"] = fmt
                if not verify_proofs:
                    res["proof_msg"] = "written, not verified (§8.1: baseline)"
                else:
                    t1 = time.monotonic()
                    ok, msg, kind, detail = check_proof(
                        fmt, cnf, proof, checker_budget(timeout),
                        allow_pbp="--dsr" not in cmd,
                        route=res["proof_route"])
                    res["proof_check_time"] = round(time.monotonic() - t1, 3)
                    res["proof_ok"] = ok
                    res["proof_msg"] = msg
                    res["proof_check_kind"] = kind
                    res["proof_check_detail"] = detail or None
                    # §8.1 second chance, deliberately narrow, and gated on the
                    # classified kind rather than a prose prefix. Only a budget
                    # outrun qualifies — a rejection is a real N2 failure, and
                    # silencing it would be a correctness hole. Only a scored
                    # arm (a baseline's check cannot change a verdict and its
                    # CLI need not take `--lrat`), and only a DRAT arm (an
                    # `--lrat` arm has nothing left to re-check).
                    if (not ok and score_proofs and fmt == "drat"
                            and kind == CHK_TIMEOUT):
                        ok = lrat_rescue(res, cmd, cnf, proof, timeout)
                    if not ok and score_proofs:  # rejected proof = unsolved (§2)
                        res["solved"] = False
    if check_proofs and os.path.exists(proof):
        os.unlink(proof)
    return res


def inst_cost(r, timeout):
    """One instance's PAR-2 contribution: its runtime if solved, else 2x the
    timeout. The 2x is the whole point of PAR-2 and is easy to get wrong by
    hand — charging 1x has inverted a conclusion — so every PAR-2-shaped
    number in this file goes through here."""
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
    measurement noises, so the oracle is OPTIMISTICALLY BIASED and the bias
    grows with K. Measured, adding an arm that on any given instance simply IS
    one of the arms already present still moves the oracle slightly, and all of
    that movement is noise. To ask "how good could this gate get", read the
    oracle over the policies the gate chooses BETWEEN (`oracle()` takes any arm
    subset); the all-arms number printed here is the slightly looser ceiling
    for the window as a whole.

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
    # Scored arm: a failed check is a correctness stop. Unscored arm (a
    # baseline): recorded with the checker's own message, never held against
    # it — plan §8.1.
    scored_rej = [r for r in prej if r.get("proof_scored")]
    noted_rej = [r for r in prej if not r.get("proof_scored")]
    if scored_rej:
        # Split by classified kind: an invalid certificate (N2) and a checker
        # that gave up (resource/budget) are different incidents and must not
        # share a headline. Old results.json have no kind — they read as
        # "unclassified" rather than being silently counted as either.
        bad = [r for r in scored_rej
               if r.get("proof_check_kind") in CHK_OUR_FAULT]
        gave_up = [r for r in scored_rej
                   if r.get("proof_check_kind") in (CHK_TIMEOUT, CHK_RESOURCE,
                                                    CHK_ERROR)]
        rest = [r for r in scored_rej if r not in bad and r not in gave_up]
        if bad:
            lines.append(f"  !! INVALID CERTIFICATES — N2 ({len(bad)}): "
                         + ", ".join(f"{r['instance']} "
                                     f"[{(r.get('proof_check_detail') or [r.get('proof_msg')])[0]}]"
                                     for r in bad[:5]))
        if gave_up:
            lines.append(f"  !! CHECKER GAVE UP, scored unsolved ({len(gave_up)}): "
                         + ", ".join(f"{r['instance']} [{r.get('proof_msg')}]"
                                     for r in gave_up[:5]))
        if rest:
            lines.append(f"  !! PROOFS REJECTED/MISSING, unclassified "
                         f"({len(rest)}): "
                         + ", ".join(f"{r['instance']} [{r.get('proof_msg')}]"
                                     for r in rest[:5]))
    if noted_rej:
        lines.append(f"  (baseline proofs not verified, NOT scored against it, "
                     f"{len(noted_rej)}): "
                     + ", ".join(f"{r['instance']} [{r.get('proof_msg')}]"
                                 for r in noted_rej[:5]))
    return "\n".join(lines), bool(mism or scored_rej)


CORRECTNESS_SENTINEL = "CORRECTNESS-BUG"


def correctness_record(all_results, solver_meta):
    """The window's correctness state as DATA, not as a line of prose.

    WHY (2026-08-05). `aggregate.log` ended with "CORRECTNESS BUG(S) FOUND —
    stop performance work", the window shipped a rejected Sweep certificate,
    and the run was written up as a scheduling result anyway: the banner was
    advisory and nothing downstream could see it. Every consumer of a run reads
    results.json, so the state lives THERE, and a `CORRECTNESS-BUG` file is
    dropped next to it so even `ls` shows it.

    `unverified_scored_arms` is not a bug — it is the SHAPE that would let one
    hide. An arm that is scored but whose certificates are never checked (an
    `--no-check` ablation array) can answer UNSAT with an invalid proof and
    still be counted solved. Recording it makes such a window self-describing
    instead of something a later audit has to reconstruct from manifests."""
    mism, bad, gave_up = [], [], []
    unverified = []
    badmodel = []
    for name, results in all_results.items():
        meta = solver_meta.get(name, {})
        if meta.get("score_proofs", True) and not (
                meta.get("check_proofs", True) and meta.get("verify_proofs", True)):
            unverified.append(name)
        for r in results:
            if r.get("mismatch"):
                mism.append({"arm": name, "instance": r["instance"],
                             "expected": r.get("expected"),
                             "answered": r.get("verdict")})
            if r.get("model_ok") is False:
                badmodel.append({"arm": name, "instance": r["instance"],
                                 "msg": r.get("model_msg")})
            if r.get("proof_ok") is False and r.get("proof_scored"):
                row = {"arm": name, "instance": r["instance"],
                       "kind": r.get("proof_check_kind"),
                       "detail": r.get("proof_check_detail"),
                       "msg": r.get("proof_msg")}
                (bad if r.get("proof_check_kind") in CHK_OUR_FAULT
                 or r.get("proof_check_kind") is None else gave_up).append(row)
    return {
        "clean": not (mism or badmodel or bad or gave_up),
        "verdict_mismatches": mism,
        "invalid_models": badmodel,       # SAT answer whose model checkmodel rejects
        "invalid_certificates": bad,      # N2: the proof was rejected
        "checker_gave_up": gave_up,       # budget/resource, a scoring artefact
        "unverified_scored_arms": sorted(set(unverified)),
    }


def write_correctness_sentinel(outdir, rec):
    """Drop (or clear) the `CORRECTNESS-BUG` marker beside results.json."""
    path = os.path.join(outdir, CORRECTNESS_SENTINEL)
    if rec["clean"]:
        if os.path.exists(path):
            os.unlink(path)
        return
    with open(path, "w") as f:
        f.write("This window is NOT clean. Do not quote its numbers as a "
                "performance result until this is resolved.\n\n")
        for k in ("verdict_mismatches", "invalid_models",
                  "invalid_certificates", "checker_gave_up"):
            for row in rec.get(k, ()):
                f.write(f"{k}: {json.dumps(row)}\n")


def correctness_banner(rec):
    """The stop notice, printed FIRST as well as last."""
    if rec["clean"]:
        return None
    n_m = len(rec["verdict_mismatches"])
    n_v = len(rec.get("invalid_models", ()))
    n_b = len(rec["invalid_certificates"])
    n_g = len(rec["checker_gave_up"])
    parts = []
    if n_m:
        parts.append(f"{n_m} verdict mismatch(es)")
    if n_v:
        parts.append(f"{n_v} INVALID MODEL(S) — wrong SAT")
    if n_b:
        parts.append(f"{n_b} INVALID CERTIFICATE(S) — N2")
    if n_g:
        parts.append(f"{n_g} checker resource failure(s)")
    return ("!" * 72 + "\nCORRECTNESS BUG(S) FOUND: " + "; ".join(parts)
            + "\nStop performance work; follow the fuzz skill's failure "
              "procedure.\nThis window's numbers are NOT a performance result. "
              f"Marker file: {CORRECTNESS_SENTINEL}\n" + "!" * 72)


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


BUILDLOCK = os.path.join(HERE, "buildlock.py")


def build_dirs_of(solvers):
    """The build directories an array's arms will execute out of.

    A pin directory is identified structurally — a path token under `build/`
    that names an existing file — so `env VAR=1 build/pin-x/tactix` and a bare
    absolute path both resolve. Reference solvers live outside `build/` and are
    left alone: they are not ours to lock."""
    dirs = set()
    for s in solvers:
        cmd = s[1] if isinstance(s, (list, tuple)) else s.get("cmd", [])
        for tok in cmd:
            if "/" not in tok or not os.path.exists(tok):
                continue
            p = os.path.dirname(os.path.abspath(tok))
            if os.path.abspath(os.path.join(ROOT, "build")) == os.path.dirname(p):
                dirs.add(p)
    return sorted(dirs)


def _buildlock(*argv):
    """Run buildlock.py. NEVER fatal: the lock is a safety net, and a safety
    net that can abort a submission is a worse bug than the one it prevents."""
    try:
        r = subprocess.run([sys.executable, BUILDLOCK, *argv],
                           capture_output=True, text=True, timeout=300)
        for line in (r.stdout + r.stderr).splitlines():
            if line.strip():
                print(line, flush=True)
        return r.returncode
    except Exception as e:                                  # pragma: no cover
        print(f"buildlock: skipped ({e})", file=sys.stderr)
        return 0


def buildlock_gc():
    _buildlock("--gc")


def buildlock_acquire(dirs, jobid, note):
    if dirs:
        _buildlock("--acquire", *dirs, "--job", str(jobid), "--note", note)


def buildlock_release(dirs):
    if dirs:
        _buildlock("--release", *dirs)


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
                           s.get("score_proofs", True),
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
    # The aggregate job is dependency-chained afterany: the array, so reaching
    # here means the array is over and its build directories are free. This is
    # the normal release path — nobody has to remember to run --release.
    buildlock_release(build_dirs_of(man["solvers"]))
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
                 "proof_format": None, "proof_route": None,
                 "proof_scored": False,
                 "proof_msg": None, "proof_rescue": None,
                 "proof_rescue_time": None,
                 "model_ok": None, "model_msg": None}
        all_results[man["solvers"][t["solver"]]["name"]].append(r)
    correctness = correctness_record(
        all_results, {s["name"]: s for s in man["solvers"]})
    banner = correctness_banner(correctness)
    if banner:                       # FIRST, not only last — see the note below
        print(banner, flush=True)
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
    # Single-instance sensitivity for each scored arm against the first scored
    # arm (the baseline) — a headline alone can hide a one-instance verdict.
    # See leave_one_out().
    scored = [s2["name"] for s2 in man["solvers"] if s2.get("score_proofs", True)]
    fragility = []
    for other in scored[1:]:
        loo = leave_one_out(scored[0], all_results[scored[0]],
                            other, all_results[other], man["timeout"])
        if loo:
            print(loo[0], flush=True)
            fragility.append(loo[1])
    meta = {"tier": man["tier"], "timeout": man["timeout"], "jobs": None,
            "slurm": man["slurm"], "git": man["git"], "date": man["date"],
            "scheduler_memory_gib": man.get(
                "scheduler_memory_gib", man.get("mem_gb")),
            "memory_enforcement": man.get(
                "memory_enforcement", "legacy RLIMIT_AS plus SLURM cgroup"),
            "solvers": {s["name"]: " ".join(s["cmd"])
                        for s in man["solvers"]},
            "oracle": orc,
            # Persisted, not stdout-only: the fragility reading decides whether
            # a delta is a verdict or one coin flip, so it belongs next to the
            # numbers it judges.
            "fragility": fragility,
            "correctness": correctness,
            "results": all_results}
    with open(os.path.join(outdir, "results.json"), "w") as f:
        json.dump(meta, f, indent=2)
    write_correctness_sentinel(outdir, correctness)
    print(f"results: {outdir}/results.json")
    if missing:
        print(f"!! {len(missing)} task result(s) MISSING (infrastructure, "
              f"scored as unsolved): "
              + ", ".join(str(i) for i in missing[:10])
              + (" ..." if len(missing) > 10 else "")
              + "\n   resubmit those indices, then re-run --aggregate")
        return 2  # staged/ kept so the resubmission can run
    shutil.rmtree(os.path.join(outdir, "staged"), ignore_errors=True)
    if any_bug or banner:
        # Repeated at the end as well as the start: an aggregate.log is read
        # from both ends, and this banner must be visible from either.
        print(banner or "CORRECTNESS BUG(S) FOUND — stop performance work; "
                        "follow the fuzz skill's failure procedure.")
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
    data = {"a": a_name, "b": b_name, "delta_par2": total / n,
            "delta_seconds": total, "instances": n,
            "largest_mover": os.path.basename(top),
            "largest_mover_seconds": d[top],
            "min_flip_k": k if flipped else None,
            "flip_set": [os.path.basename(x) for x in dropped] if flipped else [],
            "verdict": ("robust" if not flipped
                        else "undecided" if k == 1
                        else "fragile" if k <= max(2, n // 10)
                        else "robust-ish")}
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
    return "\n".join(out), data


def slurm_submit(args, entries, solvers, paths, timeout, mem_gb, outdir,
                 stamp, sha, name):
    # Instance-major, deliberately. Solver-major ordering gives arm 0 the task
    # indices 0..N-1, so SLURM packs that arm onto whichever node it fills first
    # and spills later arms elsewhere, correlating arm identity with node
    # identity — a confound the runner would manufacture itself, and one that
    # measurably flips the sign of a per-instance delta. Interleaving spreads
    # each arm over all nodes and puts one instance's arms adjacent in dispatch
    # order, so they contend alike. Safe to reorder: the aggregator resolves the
    # arm through `man["solvers"][t["solver"]]`, never from index arithmetic.
    tasks = []
    for rel, status, _ in entries:
        for si in range(len(solvers)):
            tasks.append({"solver": si, "rel": rel, "expected": status,
                          "staged": paths[rel]})
    physical_cores = max(1, -(-(mem_gb or 8) // 8))
    cpus = 2 * physical_cores  # SMT-2: two logical CPUs per physical core
    shape = (f"slurm partition={args.partition} physical-cores={physical_cores} "
             f"cpus/task={cpus} mem={mem_gb}G"
             + (f" throttle=%{args.throttle}" if args.throttle else ""))
    man = {"tier": args.tier, "timeout": timeout,
           "scheduler_memory_gib": mem_gb,
           "memory_enforcement": "slurm_task_cgroup",
           "git": sha, "date": stamp, "shape": shape,
           "slurm": {"partition": args.partition, "cpus_per_task": cpus,
                     "physical_cores": physical_cores,
                     "memory_gib": mem_gb, "throttle": args.throttle},
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
    # Per-task wall limit: twice (solve + the plan §8.1 checker budget), plus
    # slack. The second copy pays for `lrat_rescue` — a re-solve plus a cake_lpr
    # run. A limit that cannot fit the runner's own worst case is a trap: SLURM
    # kills the task mid-rescue and the instance comes back missing.
    tlimit = fmt_time(2 * (timeout + checker_budget(timeout)) + 120)
    apath = os.path.join(outdir, "array.sbatch")
    with open(apath, "w") as f:
        f.write(f"""#!/usr/bin/env bash
#SBATCH --partition={args.partition}
#SBATCH --job-name=bench-{args.tier}-{name}
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task={cpus}
#SBATCH --mem={mem_gb}G
#SBATCH --time={tlimit}
#SBATCH --array={array}
#SBATCH --output={outdir}/logs/task-%a.log
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
    buildlock_gc()            # reclaim anything a dead job left locked
    jobid = sbatch(apath)
    # The array now owns every build directory its arms point at. Locking runs
    # after submission (the job id is needed) and is best-effort: a failure here
    # must never cost a submission that already happened.
    owned = build_dirs_of(solvers)
    buildlock_acquire(owned, jobid, f"bench {args.tier} {name} {stamp}")
    aggid = sbatch(gpath, f"--dependency=afterany:{jobid}")
    print(f"submitted: array {jobid}, aggregate {aggid} (runs when the "
          f"array finishes)")
    if owned:
        print("owned build dirs (read-only until the array finishes): "
              + ", ".join(os.path.relpath(d, ROOT) for d in owned))
    print(f"watch:     squeue -j {jobid}")
    print(f"summary:   {outdir}/logs/aggregate.log  (re-merge any time: "
          f"{me} --aggregate {outdir})")
    if args.wait:
        wait = os.path.join(ROOT, "tools", "slurm", "wait.py")
        return subprocess.call([sys.executable, wait, "--job", aggid,
                                "--log", os.path.join(outdir, "logs",
                                                       "aggregate.log")])
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--tier", choices=("A", "B", "C", "D", "P4A", "P4B"))
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
    ap.add_argument("--wait", action="store_true",
                    help="after submission, wait for the aggregate job and "
                         "print its final state (launch this command in the "
                         "background to keep working)")
    ap.add_argument("--partition", default="cpu-dense-hwulab-q")
    ap.add_argument("--throttle", type=int, default=None,
                    help="exceptionally cap concurrent array tasks; default "
                         "is full width, and any use needs an explicit user "
                         "request or documented correctness/resource reason")
    ap.add_argument("--no-check", action="store_true",
                    help="do not write/check proofs for the solver under "
                         "test (baseline-ablation arrays: every arm is a "
                         "reference solver, timed the way baselines are)")
    ap.add_argument("--verify-baseline-proofs", action="store_true",
                    help="run the checker over BASELINE proofs too. Off by "
                         "default: a baseline's check is "
                         "recorded and never scored, so it cannot change a "
                         "verdict, and it is the majority of all checking "
                         "wall-clock. Turn on for a submission-grade claim.")
    ap.add_argument("--no-baseline-proofs", action="store_true",
                    help="time baselines WITHOUT proof logging; the numbers "
                         "are then NOT comparable to ours and must not be "
                         "reported as a ratio")
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
    # (name, cmd, check_proofs, verify_proofs, score_proofs). Plan §8.1: every
    # arm writes proofs, since the write tax is solver-specific and timing a
    # proof-writing solver against a proof-free one compares two workloads.
    # Only the solver under test is verified and scored by default; a baseline's
    # check cannot change a verdict, so it is skipped unless
    # --verify-baseline-proofs asks for it. See run_instance's docstring.
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
                                       score_proofs, verify_proofs),
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

    correctness = correctness_record(
        all_results,
        {n: {"check_proofs": ch, "verify_proofs": ve, "score_proofs": sc}
         for n, _, ch, ve, sc in solvers})
    banner = correctness_banner(correctness)
    fragility = []
    scored_names = [n for n, _, _, _, sc in solvers if sc]
    for other in scored_names[1:]:
        loo = leave_one_out(scored_names[0], all_results[scored_names[0]],
                            other, all_results[other], timeout)
        if loo:
            print(loo[0], flush=True)
            fragility.append(loo[1])
    meta = {"tier": args.tier, "timeout": timeout, "jobs": args.jobs,
            "git": sha, "date": stamp,
            "scheduler_memory_gib": None,
            "memory_enforcement": "none (local diagnostic only)",
            "solvers": {n: " ".join(c) for n, c, *_ in solvers},
            "oracle": orc,
            # Persisted next to the numbers it judges; see the aggregate path.
            "fragility": fragility,
            "correctness": correctness,
            "results": all_results}
    shutil.rmtree(stage, ignore_errors=True)
    with open(os.path.join(outdir, "results.json"), "w") as f:
        json.dump(meta, f, indent=2)
    write_correctness_sentinel(outdir, correctness)
    print(f"results: {outdir}/results.json")
    if any_bug or banner:
        print(banner or "CORRECTNESS BUG(S) FOUND — stop performance work; "
                        "follow the fuzz skill's failure procedure.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
