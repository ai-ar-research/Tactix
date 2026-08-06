#!/usr/bin/env python3
"""fuzz.py — soundness fuzzing runner (plan §8.2; see tools/fuzz/run.sh).

Oracles enforced on every case:
  1. model     — SAT verdicts: `checkmodel` verifies the v-line assignment
  2. proof     — UNSAT verdicts: DRAT accepted by drat-trim ('s VERIFIED');
                 with --lrat also drat-trim -L -> lrat-check
  3. differential — verdict must agree with the oracle solver
  4. crash/io  — clean exit (10/20), s-line consistent with exit code,
                 no sanitizer reports on stderr

Subcommands:
  run        fuzz N cases; write failure bundles; auto-shrink via ddmin
  check-one  re-run the oracles on a single CNF (ddmin predicate / repro)
  corpus     replay corpus/regressions/ — every entry must pass

A leg's strength is the number of cases an oracle actually ran on, which is
NOT --n. Two readouts on the summary line keep the difference visible:
  coverage:  cases that reached a verdict. A case that times out (or answers
             UNKNOWN) exercises no model/proof/differential oracle at all, so
             it contributes nothing; the non-terminating population is printed
             on its own line, saved under <outdir>/nonterminating/, and can be
             made fatal with --max-nonterminating-frac.
  reached:   per-registry-entry fire counts, read from the solver's own
             counters. A rule with 0 is a rule this leg says nothing about;
             --require-reached makes that fatal.
"""

import argparse
import concurrent.futures
import json
import os
import re
import resource
import shlex
import shutil
import subprocess
import sys
import tempfile
import threading

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from generators import generate, parse_dimacs, write_dimacs  # noqa: E402
from modelcheck import check_files  # noqa: E402
from shrink import minimize  # noqa: E402

DRAT_TRIM = os.path.join(ROOT, "tools", "checkers", "bin", "drat-trim")
LRAT_CHECK = os.path.join(ROOT, "tools", "checkers", "bin", "lrat-check")


class CaseOutcome:
    def __init__(self):
        self.verdict = "UNKNOWN"
        self.oracle_verdict = None
        self.failures = []  # list of (kind, detail)
        # oracles deliberately NOT applied to this case, with the reason. Never
        # let an oracle drop out silently: a skip that is not counted is
        # indistinguishable from a pass ([[ab-arm-identical-artefacts]]).
        self.skips = []  # list of (oracle, reason)
        self.solver_out = ""
        self.solver_err = ""

    def fail(self, kind, detail):
        self.failures.append((kind, detail))

    def skip(self, oracle, reason):
        self.skips.append((oracle, reason))


def _cpu_backstop(timeout):
    """preexec_fn: kernel-enforced CPU cap on the child. subprocess.run(timeout=)
    only kills the child on the NORMAL timeout path; if the PARENT dies abruptly
    (session/nightly killed mid-run) the child is reparented to init and spins
    forever — the 13.5 h / 6-core orphan we hit. RLIMIT_CPU makes the kernel
    SIGXCPU (then SIGKILL) the child regardless of parent state, so no solver can
    ever outlive its budget. Slack keeps the wall-timeout the primary mechanism."""
    cap = int(timeout) + 5

    def _pre():
        resource.setrlimit(resource.RLIMIT_CPU, (cap, cap + 2))
    return _pre


def run_cmd(cmd, timeout):
    """Returns (exitcode, stdout, stderr); exitcode None on timeout."""
    try:
        r = subprocess.run(cmd, capture_output=True, text=True,
                           errors="replace", timeout=timeout,
                           preexec_fn=_cpu_backstop(timeout))
        return r.returncode, r.stdout, r.stderr
    except subprocess.TimeoutExpired as e:
        def _txt(b):
            return b.decode(errors="replace") if isinstance(b, bytes) else (b or "")
        return None, _txt(e.stdout), _txt(e.stderr)


def parse_verdict(exitcode, stdout):
    sline = None
    for line in stdout.splitlines():
        if line.startswith("s "):
            sline = line[2:].strip()
            break
    if exitcode is None:
        return "TIMEOUT", sline
    if exitcode == 10:
        return "SAT" if sline == "SATISFIABLE" else "IO-MISMATCH", sline
    if exitcode == 20:
        return "UNSAT" if sline == "UNSATISFIABLE" else "IO-MISMATCH", sline
    if sline == "UNKNOWN":
        return "UNKNOWN", sline
    return "CRASH", sline


def lrat_verified(code, stdout):
    """Did lrat-check ACCEPT the proof?

    It prints `c VERIFIED` on success and `c NOT VERIFIED` on rejection, so a
    substring test for "VERIF"/"VERIFIED" matches BOTH — which is what the two
    native-LRAT oracles here used to do. Every LRAT rejection in the project's
    fuzz history was therefore recorded as a pass; only crashes and timeouts
    (`code is None`) ever failed a native-LRAT leg. Found 2026-08-05 while
    closing the V5 Sweep N2 coverage gap: a 200-case sample rejected 3 proofs
    that the harness called green. Test for the rejection FIRST.
    (drat-trim's own `s NOT VERIFIED` does not contain `s VERIFIED`, so the
    DRAT branches were never affected.)"""
    if code is None:
        return False
    up = stdout.upper()
    return "NOT VERIFIED" not in up and "VERIFIED" in up


def conversion_crosscheck_unsound(path):
    """Is `drat-trim -L` -> lrat-check a VALID oracle for this CNF?

    Returns a reason string when it is not, else None. Both classes below are
    disagreements BETWEEN THE TWO CHECKERS, not anything a solver does: with
    each of them, KISSAT's and CADICAL's own DRAT proofs are rejected exactly
    as ours are (measured 2026-08-05; three-solver table and a five-clause
    reproducer in
    benchmarks/reports/2026-08-05-drat-trim-conversion-crosscheck.md).
    Before `lrat_verified()` landed the same day, this oracle accepted
    `c NOT VERIFIED`, so the whole class was invisible — including in this
    harness's own selftest, where `expect_clean kissat` passed over two of them.

    1. A REPEATED LITERAL in some clause. drat-trim's parser sorts and DELETES
       duplicate literals as it reads the CNF (drat-trim.c parse(), the
       `buffer[i] == buffer[i+1]` loop), so it holds `80 80` as the unit `80`
       and its own RUP check passes; `-L` then cites input clauses BY ID; and
       lrat-check re-reads the ORIGINAL file without deduping, so that id names
       a clause it cannot propagate — `FAILED: multiple literals unassigned in
       hint 161: 80 80`.
    2. THE EMPTY CLAUSE in the input. drat-trim short-circuits (the formula is
       refuted at parse) and `-L` writes a ZERO-BYTE proof, so lrat-check never
       sees an empty-clause derivation: `Verification incomplete` ->
       `NOT VERIFIED`.

    Tautologies, duplicate clauses and plain UNSAT are all fine — measured, not
    assumed; do not widen this predicate without the same evidence, because
    every widening is an oracle you stop running.

    This gates ONLY the conversion cross-check. The NATIVE-LRAT oracle (solver
    run with --lrat) is deliberately untouched: it checks ids our own emitter
    wrote, it is the oracle that covers the frontend's input restatement, and
    it passes on both classes."""
    dup = False
    empty = False
    try:
        with open(path) as f:
            for line in f:
                line = line.strip()
                if not line or line[0] in "cp%":
                    continue
                lits = line.split()
                if lits and lits[-1] == "0":
                    lits = lits[:-1]
                if not lits:
                    empty = True
                elif len(set(lits)) != len(lits):
                    dup = True
    except OSError:
        return None
    if dup and empty:
        return "repeated input literal + empty input clause"
    if dup:
        return "drat-trim -L dedupes the CNF, lrat-check does not"
    if empty:
        return "empty input clause: drat-trim -L emits a zero-byte proof"
    return None


_RESTATED_RE = re.compile(r"^c input\s+(\d+) clauses restated", re.M)


def input_restated(stdout):
    """How many input clauses the solver had to restate (repeated literals).

    Reads the solver's own `c input` counter, so this measures the path at its
    SOURCE rather than re-deriving it from the CNF. 0 when the leg runs the
    solver with -q (no stats printed) — the standard legs do not."""
    m = _RESTATED_RE.search(stdout or "")
    return int(m.group(1)) if m else 0


# --- per-rule reachability, counted at the SOURCE -------------------------
# One row per registry entry (rule or policy op): the label, and a regex whose
# group(1) is the counter that is non-zero iff that entry's apply actually ran
# on the case. A leg that never fires a rule proves nothing about it, and the
# only way to know is to read the solver's own counters
# ([[ab-arm-identical-artefacts]]).
#
# The regexes are matched against the solver's stats block, so a reworded print
# line would silently zero a row. That failure mode is checked, not trusted:
# `spec_miss` counts rows whose LINE never appeared in a case that printed
# stats at all, and a leg with any spec miss FAILS — an instrument that cannot
# fail is worth nothing.
REACH_SPECS = [
    ("Propagate", r"^c propagations\s+(\d+)"),
    ("Decide", r"^c decisions\s+(\d+)"),
    ("ConflictAnalysis", r"^c conflicts\s+(\d+)"),
    ("Restart", r"^c restarts\s+(\d+)"),
    ("Reduce", r"^c reductions\s+(\d+)"),
    ("Lucky", r"^c lucky\s+(\d+) probes"),
    ("Vivify", r"^c vivify\s+(\d+) rounds,"),
    ("Probe", r"^c probe\s+(\d+) rounds,"),
    ("Backbone", r"^c backbone\s+(\d+) rounds,"),
    ("TransitiveReduce", r"^c transitive\s+(\d+) rounds,"),
    ("Substitute", r"^c substitute\s+(\d+) rounds,"),
    ("BCE", r"^c bce\s+(\d+) rounds,"),
    ("CCE", r"^c cce\s+(\d+) rounds,"),
    ("Instantiate", r"^c instantiate\s+(\d+) rounds,"),
    ("Sweep", r"^c sweep\s+(\d+) rounds,"),
    ("HyperTernaryRes", r"^c ternary\s+(\d+) rounds,"),
    ("Congruence", r"^c congruence\s+(\d+) rounds,"),
    ("Factor", r"^c factor\s+(\d+) factorings,"),
    ("BVE", r"^c bve\s+(\d+) rounds,"),
    ("SubsumeForward", r"^c subsume\s+(\d+) rounds \("),
    ("Strengthen", r"^c strengthen\s+(\d+) rounds \("),
    ("Walk", r"^c walk\s+(\d+) walks"),
    ("Rephase", r"^c rephased\s+(\d+)"),
    ("Reorder", r"^c reordered\s+(\d+) decision"),
    ("SwitchMode", r"^c mode\s+\S+ after (\d+) switches"),
]
_REACH_RES = [(label, re.compile(pat, re.M)) for label, pat in REACH_SPECS]
_STATS_BANNER_RE = re.compile(r"^c tactix ", re.M)


def rule_reachability(stdout):
    """(stats_present, {label: count}, {label: line_present}) for one case.

    `count` is the entry's fired-counter value; `line_present` says the print
    line existed at all, which separates "the rule did not fire" from "the
    harness is looking for a line the solver no longer prints"."""
    text = stdout or ""
    present = bool(_STATS_BANNER_RE.search(text))
    counts, seen = {}, {}
    if not present:
        return False, counts, seen
    for label, rx in _REACH_RES:
        m = rx.search(text)
        seen[label] = m is not None
        counts[label] = int(m.group(1)) if m else 0
    return True, counts, seen


def run_case(cnf_path, solver, oracle, timeout, lrat, workdir):
    """Run the solver under test on cnf_path and apply all oracles.

    Native-LRAT mode: when the solver command itself contains --lrat, the
    proof is the solver's own LRAT and is checked directly with
    lrat-check (no drat-trim conversion)."""
    out = CaseOutcome()
    native_lrat = "--lrat" in solver
    proof = os.path.join(workdir,
                         "proof.lrat" if native_lrat else "proof.drat")
    if os.path.exists(proof):
        os.unlink(proof)

    code, stdout, stderr = run_cmd(solver + [cnf_path, proof], timeout)
    out.solver_out, out.solver_err = stdout, stderr
    verdict, sline = parse_verdict(code, stdout)
    out.verdict = verdict

    if "Sanitizer" in stderr or "runtime error" in stderr:
        out.fail("crash", "sanitizer report on stderr")
    if verdict == "CRASH":
        out.fail("crash", f"exit code {code}, s-line {sline!r}")
    elif verdict == "IO-MISMATCH":
        out.fail("io", f"exit code {code} inconsistent with s-line {sline!r}")

    if verdict == "SAT":
        out_file = os.path.join(workdir, "solver.out")
        with open(out_file, "w") as f:
            f.write(stdout)
        ok, msg = check_files(cnf_path, out_file)
        if not ok:
            out.fail("model", msg)

    if verdict == "UNSAT":
        if not os.path.exists(proof) or os.path.getsize(proof) == 0:
            # empty proof file is legal iff the input itself contains the
            # empty clause; drat-trim handles that, so only flag absence
            if not os.path.exists(proof):
                out.fail("proof", "no proof file produced")
        if os.path.exists(proof) and native_lrat:
            c, o, e = run_cmd([LRAT_CHECK, cnf_path, proof], 120)
            if not lrat_verified(c, o):
                out.fail("proof",
                         f"lrat-check rejected the native LRAT (exit {c})")
        elif os.path.exists(proof):
            c, o, e = run_cmd([DRAT_TRIM, cnf_path, proof], 120)
            if c is None or "s VERIFIED" not in o:
                out.fail("proof", f"drat-trim did not verify (exit {c})")
            elif lrat:
                # On some degenerate-but-legal CNFs drat-trim and lrat-check
                # disagree about the FILE, so the conversion cross-check is not
                # an oracle — for ANY solver. See conversion_crosscheck_unsound.
                why = conversion_crosscheck_unsound(cnf_path)
                if why:
                    out.skip("lrat-convert", why)
                else:
                    lrat_file = os.path.join(workdir, "proof.lrat")
                    c, o, e = run_cmd(
                        [DRAT_TRIM, cnf_path, proof, "-L", lrat_file], 120)
                    if c is None or "s VERIFIED" not in o:
                        out.fail("proof", "drat-trim -L (LRAT emission) failed")
                    else:
                        c, o, e = run_cmd([LRAT_CHECK, cnf_path, lrat_file],
                                          120)
                        if not lrat_verified(c, o):
                            out.fail("proof",
                                     "lrat-check rejected the LRAT proof")

    if oracle and verdict in ("SAT", "UNSAT"):
        code_o, stdout_o, _ = run_cmd(oracle + [cnf_path], timeout)
        overdict, _ = parse_verdict(code_o, stdout_o)
        out.oracle_verdict = overdict
        if overdict in ("SAT", "UNSAT") and overdict != verdict:
            out.fail("differential",
                     f"solver says {verdict}, oracle says {overdict}")
    return out


def save_bundle(outdir, idx, seed, name, cnf_path, outcome, solver, oracle,
                timeout, lrat, shrink):
    kinds = "+".join(sorted({k for k, _ in outcome.failures}))
    bundle = os.path.join(outdir, f"{idx:06d}-{kinds}-seed{seed}")
    os.makedirs(bundle, exist_ok=True)
    shutil.copy(cnf_path, os.path.join(bundle, "case.cnf"))
    with open(os.path.join(bundle, "solver.out"), "w") as f:
        f.write(outcome.solver_out)
    with open(os.path.join(bundle, "solver.err"), "w") as f:
        f.write(outcome.solver_err)
    meta = {
        "seed": seed,
        "index": idx,
        "generator": name,
        "solver": solver,
        "oracle": oracle,
        "verdict": outcome.verdict,
        "oracle_verdict": outcome.oracle_verdict,
        "failures": outcome.failures,
        "timeout": timeout,
    }
    with open(os.path.join(bundle, "meta.json"), "w") as f:
        json.dump(meta, f, indent=2)
    fail_kind = outcome.failures[0][0]
    check_cmd = [os.path.join(HERE, "fuzz.py"), "check-one",
                 "--solver", shlex.join(solver),
                 "--fail-kind", fail_kind,
                 "--timeout", str(timeout)]
    # the oracle run doubles predicate cost; only shrink with it when the
    # failure being preserved is the differential one
    if oracle and fail_kind == "differential":
        check_cmd += ["--oracle", shlex.join(oracle)]
    if lrat:
        check_cmd += ["--lrat"]
    with open(os.path.join(bundle, "repro.sh"), "w") as f:
        f.write("#!/usr/bin/env bash\n# exit 0 iff the recorded failure "
                "kind reproduces on $1 (default: case.cnf)\n"
                f"cd \"$(dirname \"$0\")\"\n"
                f"{shlex.join(check_cmd)} \"${{1:-case.cnf}}\"\n")
    os.chmod(os.path.join(bundle, "repro.sh"), 0o755)
    if shrink:
        # in-process shrinking: one solver run per candidate, no
        # interpreter startups (see shrink.py)
        shrink_oracle = oracle if fail_kind == "differential" else None
        with tempfile.TemporaryDirectory(prefix="fuzz-shrink-") as sdir:
            def pred(nv, cls):
                cand = os.path.join(sdir, "cand.cnf")
                write_dimacs(cand, nv, cls)
                o = run_case(cand, solver, shrink_oracle, timeout, False,
                             sdir)
                return fail_kind in {k for k, _ in o.failures}

            nvars, clauses = parse_dimacs(cnf_path)
            before = len(clauses)
            nv, cls, evals = minimize(pred, nvars, clauses, max_seconds=120)
            write_dimacs(os.path.join(bundle, "min.cnf"), nv, cls)
        with open(os.path.join(bundle, "ddmin.log"), "w") as f:
            f.write(f"shrunk {before} -> {len(cls)} clauses "
                    f"({evals} evals, kind={fail_kind})\n")
    return bundle


def save_nonterminating(outdir, idx, seed, name, cnf_path, outcome, solver,
                        timeout):
    """Keep a non-terminating case so the loss is DIAGNOSABLE, not just counted.

    Not a failure bundle: nothing is wrong with the verdict, the case simply
    never produced one, so no oracle ran on it. The CNF plus `repro.sh` is what
    turns "a double-digit percentage of the leg was lost" into a livelock to
    shrink (task #39 / RV-S31), and it is also how a longer-timeout re-run
    separates merely slow from non-terminating."""
    d = os.path.join(outdir, "nonterminating", f"{idx:06d}-seed{seed}")
    os.makedirs(d, exist_ok=True)
    shutil.copy(cnf_path, os.path.join(d, "case.cnf"))
    with open(os.path.join(d, "meta.json"), "w") as f:
        json.dump({"seed": seed, "index": idx, "generator": name,
                   "solver": solver, "verdict": outcome.verdict,
                   "timeout": timeout}, f, indent=2)
    cmd = [os.path.join(HERE, "fuzz.py"), "check-one",
           "--solver", shlex.join(solver)]
    with open(os.path.join(d, "repro.sh"), "w") as f:
        # cd to the REPO ROOT, not to the bundle: a repo-relative --solver path
        # (`build/…/tactix`, which every leg uses) does not resolve anywhere
        # else, and the CNF is passed absolute so the cd cannot break it either.
        f.write("#!/usr/bin/env bash\n# re-run this case; pass a timeout in "
                "seconds as $1 to test whether it EVER terminates\n"
                f'cd {shlex.quote(ROOT)}\n'
                f"{shlex.join(cmd)}"
                ' --timeout "${1:-' f"{timeout}" '}" '
                f"{shlex.quote(os.path.abspath(os.path.join(d, 'case.cnf')))}\n")
    os.chmod(os.path.join(d, "repro.sh"), 0o755)
    return d


def cmd_run(args):
    solver = shlex.split(args.solver)
    oracle = shlex.split(args.oracle) if args.oracle else None
    # A misspelled --require-reached label would be a requirement that can
    # never fire and can never fail: reject it here rather than report green.
    known = {label for label, _ in REACH_SPECS}
    bad = [x for x in (args.require_reached or []) if x not in known]
    if bad:
        print(f"fuzz: unknown --require-reached entries {bad}; "
              f"known: {sorted(known)}")
        return 2
    outdir = args.outdir
    os.makedirs(outdir, exist_ok=True)
    nfail = 0
    kinds_count = {}
    verdicts = {}
    # REACHABILITY, counted at the source. A green leg proves nothing about a
    # path no case reached ([[ab-arm-identical-artefacts]]); the solver prints
    # `c input <n> clauses restated` for the frontend's input-normalization
    # path, so tally how many cases actually had a degenerate input clause and
    # report it next to the pass/fail counts. Add a line here for any other
    # path whose coverage a leg is supposed to demonstrate.
    reach = {"restated_cases": 0, "restated_unsat": 0}
    # per-rule reachability (the same principle, generalised): cases in which
    # each registry entry actually fired, plus the spec-miss guard.
    reach_rules = {label: 0 for label, _ in REACH_SPECS}
    reach_spec_seen = {label: 0 for label, _ in REACH_SPECS}
    stats_cases = 0
    # NON-TERMINATION = LOST COVERAGE, and it must be as visible as a failure.
    # A case that does not reach a verdict exercises no semantic oracle
    # (model/proof/differential all need SAT or UNSAT), so "N cases, 0
    # failures" is really "N minus the non-terminating population". Weak-solver
    # legs (--random-policy above all) lose a double-digit percentage this way;
    # counted and printed here, and reproducible from the recorded seeds.
    nonterm = []  # [{index, seed, generator, nvars, nclauses, verdict}]
    saved_nonterm = [0]  # CNFs written to outdir/nonterminating/ (capped)
    skips_count = {}  # oracle -> cases where it was deliberately not applied
    done = 0
    lock = threading.Lock()  # guards the counters + stdout
    stop = threading.Event()  # set once max_failures is reached

    def one_case(i):
        # case i is fully determined by (seed, i) regardless of which
        # worker runs it or in what order — results are the same at any
        # --jobs level, only wall-clock changes (the work is subprocess-
        # bound, so threads parallelize it fine)
        nonlocal nfail, done, stats_cases
        if stop.is_set():
            return
        seed = args.seed + i
        name, nvars, clauses = generate(seed, i)
        # size cap (weak-solver legs, e.g. --random-policy: a random
        # schedule cannot finish big instances within the timeout, and
        # the semantic oracles only fire on a verdict): deterministically
        # re-roll oversized cases. The fixed edge degeneracies (name prefix
        # "edge-", e.g. edge-max-var-index = 500000 vars) are exempt so the
        # "all 10 edge cases run every batch" guarantee holds — they resolve
        # instantly via their unit clauses, so they're harmless even oversized.
        attempt = 0
        while (args.max_vars and nvars > args.max_vars
               and not name.startswith("edge-") and attempt < 1000):
            attempt += 1
            name, nvars, clauses = generate(seed + 7919 * attempt, i)
        with tempfile.TemporaryDirectory(prefix="fuzz-case-") as workdir:
            cnf = os.path.join(workdir, "case.cnf")
            write_dimacs(cnf, nvars, clauses)
            outcome = run_case(cnf, solver, oracle, args.timeout,
                               args.lrat, workdir)
            bundle = None
            if outcome.failures:
                # bundles (incl. shrinking) happen in the worker while the
                # case dir is alive; bundle names carry the case index, so
                # completion order does not matter
                bundle = save_bundle(outdir, i, seed, name, cnf, outcome,
                                     solver, oracle, args.timeout, args.lrat,
                                     not args.no_shrink)
            if outcome.verdict in ("TIMEOUT", "UNKNOWN"):
                with lock:
                    take = saved_nonterm[0] < args.max_nonterminating_saved
                    if take:
                        saved_nonterm[0] += 1
                if take:
                    save_nonterminating(outdir, i, seed, name, cnf, outcome,
                                        solver, args.timeout)
        restated = input_restated(outcome.solver_out)
        stats_present, rcounts, rseen = rule_reachability(outcome.solver_out)
        with lock:
            verdicts[outcome.verdict] = verdicts.get(outcome.verdict, 0) + 1
            if outcome.verdict in ("TIMEOUT", "UNKNOWN"):
                nonterm.append({"index": i, "seed": seed, "generator": name,
                                "nvars": nvars, "nclauses": len(clauses),
                                "verdict": outcome.verdict,
                                "timeout": args.timeout})
            if restated:
                reach["restated_cases"] += 1
                if outcome.verdict == "UNSAT":
                    reach["restated_unsat"] += 1
            if stats_present:
                stats_cases += 1
                for label in reach_rules:
                    if rcounts.get(label, 0) > 0:
                        reach_rules[label] += 1
                    if rseen.get(label):
                        reach_spec_seen[label] += 1
            for o, _ in outcome.skips:
                skips_count[o] = skips_count.get(o, 0) + 1
            if outcome.failures:
                nfail += 1
                for k, _ in outcome.failures:
                    kinds_count[k] = kinds_count.get(k, 0) + 1
                print(f"FAIL case {i} seed {seed} ({name}): "
                      f"{outcome.failures} -> {bundle}", flush=True)
                if nfail >= args.max_failures and not stop.is_set():
                    print(f"stopping after {nfail} failures", flush=True)
                    stop.set()
            done += 1
            if done % 100 == 0:
                print(f"... {done}/{args.n} cases, {nfail} failures",
                      flush=True)

    jobs = max(1, args.jobs)
    if jobs == 1:
        for i in range(args.n):
            if stop.is_set():
                break
            one_case(i)
    else:
        with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as ex:
            list(ex.map(one_case, range(args.n)))
    # verdict distribution = oracle coverage: only SAT/UNSAT cases exercise
    # the differential/model/proof oracles — a TIMEOUT is sanitizer-only
    vsummary = " ".join(f"{k}={verdicts[k]}" for k in sorted(verdicts))
    ran = done  # cases actually executed (a --max-failures stop cuts the leg)
    nnon = len(nonterm)
    effective = ran - nnon
    print(f"fuzz: {args.n} cases, {nfail} failures"
          + (f" {kinds_count}" if kinds_count else "")
          + f" | verdicts: {vsummary}"
          + f" | coverage: {effective}/{ran} cases reached a verdict"
          + f" | reached: input-restated {reach['restated_cases']} cases"
            f" ({reach['restated_unsat']} UNSAT)"
          + (f" | oracles skipped: {skips_count}" if skips_count else ""))
    if nnon:
        pct = 100.0 * nnon / ran if ran else 0.0
        # Spelled out, because the headline number was being quoted without it:
        # these cases ran NO semantic oracle, so the leg's strength is
        # `effective`, not `args.n`.
        print(f"fuzz: NON-TERMINATING {nnon} of {ran} cases ({pct:.1f}%) at "
              f"--timeout {args.timeout}s: no model/proof/differential oracle "
              f"ran on them, so this leg is {effective} cases, not {ran}. "
              f"{saved_nonterm[0]} kept under "
              f"{os.path.join(outdir, 'nonterminating')} "
              f"(seeds in nonterminating.json).")
        with open(os.path.join(outdir, "nonterminating.json"), "w") as f:
            json.dump({"solver": solver, "timeout": args.timeout,
                       "cases_run": ran, "nonterminating": nnon,
                       "detail": sorted(nonterm, key=lambda c: c["index"])},
                      f, indent=2)
    # per-rule reachability table (RV-S23): which registry entries this leg
    # actually exercised. Zero means every claim the leg makes about that rule
    # is vacuous.
    if stats_cases:
        cols = " ".join(
            f"{label}={reach_rules[label]}"
            + ("!" if reach_rules[label] == 0 else "")
            + ("?" if reach_spec_seen[label] != stats_cases else "")
            for label, _ in REACH_SPECS)
        print(f"fuzz: reached (cases firing each rule, of {stats_cases} with "
              f"stats; ! = never fired, ? = counter line missing): {cols}")
    missing_spec = [label for label, _ in REACH_SPECS
                    if stats_cases and reach_spec_seen[label] == 0]
    if missing_spec:
        print("fuzz: REACH-SPEC MISS (the harness looked for a counter line "
              "the solver never printed; the readout for these rules is "
              f"meaningless until the spec is fixed): {missing_spec}")
    unreached = [label for label in (args.require_reached or [])
                 if reach_rules.get(label, 0) == 0]
    if unreached:
        print(f"fuzz: REQUIRED RULES NEVER FIRED: {unreached}")
    over_budget = (args.max_nonterminating_frac is not None and ran
                   and nnon / ran > args.max_nonterminating_frac)
    if over_budget:
        print(f"fuzz: non-terminating fraction {nnon / ran:.3f} exceeds "
              f"--max-nonterminating-frac {args.max_nonterminating_frac}")
    return 1 if (nfail or missing_spec or unreached or over_budget) else 0


def cmd_check_one(args):
    solver = shlex.split(args.solver)
    oracle = shlex.split(args.oracle) if args.oracle else None
    with tempfile.TemporaryDirectory(prefix="fuzz-check-") as workdir:
        outcome = run_case(args.cnf, solver, oracle, args.timeout,
                           args.lrat, workdir)
    kinds = {k for k, _ in outcome.failures}
    print(f"verdict={outcome.verdict} oracle={outcome.oracle_verdict} "
          f"failures={outcome.failures}"
          + (f" skipped={outcome.skips}" if outcome.skips else ""))
    if args.fail_kind:
        return 0 if args.fail_kind in kinds else 1
    return 0 if kinds else 1


def cmd_corpus(args):
    corpus = os.path.join(HERE, "corpus", "regressions")
    solver = shlex.split(args.solver)
    oracle = shlex.split(args.oracle) if args.oracle else None
    entries = sorted(
        d for d in (os.listdir(corpus) if os.path.isdir(corpus) else [])
        if os.path.isdir(os.path.join(corpus, d)))
    nfail = 0
    nonterm = 0
    with tempfile.TemporaryDirectory(prefix="fuzz-corpus-") as workdir:
        for entry in entries:
            for cnf_name in ("min.cnf", "case.cnf"):
                cnf = os.path.join(corpus, entry, cnf_name)
                if os.path.exists(cnf):
                    break
            else:
                continue
            # A case only reproduces under the SCHEDULE that produced it, and
            # some of them need a non-default one. meta.json has carried
            # `solver_args` / `solver_env` since entry 011 and this replay
            # ignored them, so such an entry passed for the wrong reason -- a
            # regression test that cannot fail. Both are now applied on top of
            # the caller's solver command (env restored afterwards; the loop is
            # sequential, so this is safe).
            meta = {}
            meta_path = os.path.join(corpus, entry, "meta.json")
            if os.path.exists(meta_path):
                with open(meta_path) as f:
                    meta = json.load(f)
            entry_solver = solver + list(meta.get("solver_args", []))
            entry_env = meta.get("solver_env", {})
            saved = {k: os.environ.get(k) for k in entry_env}
            os.environ.update({k: str(v) for k, v in entry_env.items()})
            try:
                outcome = run_case(cnf, entry_solver, oracle, args.timeout,
                                   args.lrat, workdir)
            finally:
                for k, v in saved.items():
                    if v is None:
                        os.environ.pop(k, None)
                    else:
                        os.environ[k] = v
            if outcome.failures:
                nfail += 1
                print(f"CORPUS FAIL {entry}: {outcome.failures}")
            # A replay that never reaches a verdict runs no SEMANTIC oracle
            # (model / proof / differential all need SAT or UNSAT), so for an
            # entry whose recorded failure was one of those, a timeout means the
            # entry is present and testing nothing — the same class as the
            # ignored `solver_args` above, and fatal for the same reason.
            #
            # It is NOT vacuous for a `crash`/`io` entry: those reproduce as an
            # assert/sanitizer abort, and the fixed solver's expected behaviour
            # is exactly "runs past the budget without aborting" (entry 006's own
            # meta says so). The crash oracle ran; only the semantic ones did
            # not. So report it and move on, and say which oracle stood.
            if outcome.verdict in ("TIMEOUT", "UNKNOWN"):
                # entries record their kind as `failure` (a string) or
                # `failure_kind` (a list); crash-only means EVERY recorded kind
                # is one the crash oracle covers
                kinds = meta.get("failure_kind") or [meta.get("failure", "")]
                kind = ",".join(k for k in kinds if k)
                crash_only = bool(kind) and all(k in ("crash", "io")
                                                for k in kinds)
                if crash_only:
                    print(f"CORPUS crash-oracle only {entry}: no verdict at "
                          f"--timeout {args.timeout}s, and no abort — which is "
                          f"the recorded pass condition for a `{kind}` entry")
                else:
                    nonterm += 1
                    print(f"CORPUS NON-TERMINATING {entry}: verdict "
                          f"{outcome.verdict} at --timeout {args.timeout}s — "
                          f"the `{kind or 'semantic'}` oracle this entry exists "
                          f"for did not run")
    print(f"corpus: {len(entries)} entries, {nfail} failures"
          + (f", {nonterm} non-terminating (no oracle ran)" if nonterm else ""))
    # Non-termination is fatal by default: an entry that cannot finish is not
    # replaying its regression. A weak policy (a stress policy driving whole
    # rounds to a fixpoint, with per-step invariant checks) genuinely cannot
    # finish the larger entries, so a leg may pre-register a CAP — a number in
    # the leg's own script, never a silent default.
    if nonterm > args.allow_nonterminating:
        print(f"corpus: {nonterm} non-terminating exceeds "
              f"--allow-nonterminating {args.allow_nonterminating}")
        return 1
    return 1 if nfail else 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    sub = ap.add_subparsers(dest="cmd", required=True)

    def common(p):
        p.add_argument("--solver", required=True,
                       help="solver under test (competition CLI: "
                            "solver CNF [PROOF]; exit 10/20)")
        p.add_argument("--oracle", default=None,
                       help="oracle solver for differential testing")
        p.add_argument("--timeout", type=float, default=5.0)
        p.add_argument("--lrat", action="store_true",
                       help="also exercise drat-trim -L -> lrat-check")

    p = sub.add_parser("run")
    common(p)
    p.add_argument("--n", type=int, default=1000)
    p.add_argument("--seed", type=int, default=20260709)
    p.add_argument("--outdir", default=os.path.join(HERE, "failures"))
    p.add_argument("--max-failures", type=int, default=20)
    p.add_argument("--no-shrink", action="store_true")
    p.add_argument("--max-vars", type=int, default=None,
                   help="re-roll cases above this size (weak-solver legs "
                        "need verdicts for the semantic oracles to fire)")
    p.add_argument("--require-reached", default=None,
                   type=lambda s: [x for x in s.split(",") if x],
                   help="comma-separated registry entries this leg must fire "
                        "(else it exits nonzero): "
                        + ",".join(label for label, _ in REACH_SPECS))
    p.add_argument("--max-nonterminating-frac", type=float, default=None,
                   help="fail the leg when more than this FRACTION of cases "
                        "never reached a verdict (default: report only). Such "
                        "cases run no semantic oracle, so they are lost "
                        "coverage, not passes")
    p.add_argument("--max-nonterminating-saved", type=int, default=25,
                   help="keep at most this many non-terminating cases (CNF + "
                        "repro) under <outdir>/nonterminating/")
    p.add_argument("--jobs", type=int,
                   default=int(os.environ.get("SLURM_CPUS_PER_TASK", 8)),
                   help="parallel fuzz workers (correctness testing, not "
                        "timing — contention is harmless; results are "
                        "independent of the job count); defaults to the "
                        "SLURM allocation when run inside a job")
    p.set_defaults(func=cmd_run)

    p = sub.add_parser("check-one")
    common(p)
    p.add_argument("cnf")
    p.add_argument("--fail-kind", default=None,
                   choices=["crash", "io", "model", "proof", "differential"])
    p.set_defaults(func=cmd_check_one)

    p = sub.add_parser("corpus")
    common(p)
    p.add_argument("--allow-nonterminating", type=int, default=0,
                   help="entries allowed to not reach a verdict (they replay "
                        "no oracle); default 0 = any is fatal")
    p.set_defaults(func=cmd_corpus)

    args = ap.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
