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


def cmd_run(args):
    solver = shlex.split(args.solver)
    oracle = shlex.split(args.oracle) if args.oracle else None
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
    skips_count = {}  # oracle -> cases where it was deliberately not applied
    done = 0
    lock = threading.Lock()  # guards the counters + stdout
    stop = threading.Event()  # set once max_failures is reached

    def one_case(i):
        # case i is fully determined by (seed, i) regardless of which
        # worker runs it or in what order — results are the same at any
        # --jobs level, only wall-clock changes (the work is subprocess-
        # bound, so threads parallelize it fine)
        nonlocal nfail, done
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
        restated = input_restated(outcome.solver_out)
        with lock:
            verdicts[outcome.verdict] = verdicts.get(outcome.verdict, 0) + 1
            if restated:
                reach["restated_cases"] += 1
                if outcome.verdict == "UNSAT":
                    reach["restated_unsat"] += 1
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
    print(f"fuzz: {args.n} cases, {nfail} failures"
          + (f" {kinds_count}" if kinds_count else "")
          + f" | verdicts: {vsummary}"
          + f" | reached: input-restated {reach['restated_cases']} cases"
            f" ({reach['restated_unsat']} UNSAT)"
          + (f" | oracles skipped: {skips_count}" if skips_count else ""))
    return 1 if nfail else 0


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
    print(f"corpus: {len(entries)} entries, {nfail} failures")
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
    p.set_defaults(func=cmd_corpus)

    args = ap.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
