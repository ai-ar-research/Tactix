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
import json
import os
import shlex
import shutil
import subprocess
import sys
import tempfile

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
        self.solver_out = ""
        self.solver_err = ""

    def fail(self, kind, detail):
        self.failures.append((kind, detail))


def run_cmd(cmd, timeout):
    """Returns (exitcode, stdout, stderr); exitcode None on timeout."""
    try:
        r = subprocess.run(cmd, capture_output=True, text=True,
                           errors="replace", timeout=timeout)
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


def run_case(cnf_path, solver, oracle, timeout, lrat, workdir):
    """Run the solver under test on cnf_path and apply all oracles."""
    out = CaseOutcome()
    proof = os.path.join(workdir, "proof.drat")
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
        if os.path.exists(proof):
            c, o, e = run_cmd([DRAT_TRIM, cnf_path, proof], 120)
            if c is None or "s VERIFIED" not in o:
                out.fail("proof", f"drat-trim did not verify (exit {c})")
            elif lrat:
                lrat_file = os.path.join(workdir, "proof.lrat")
                c, o, e = run_cmd(
                    [DRAT_TRIM, cnf_path, proof, "-L", lrat_file], 120)
                if c is None or "s VERIFIED" not in o:
                    out.fail("proof", "drat-trim -L (LRAT emission) failed")
                else:
                    c, o, e = run_cmd([LRAT_CHECK, cnf_path, lrat_file], 120)
                    if c is None or "VERIFIED" not in o:
                        out.fail("proof", "lrat-check rejected the LRAT proof")

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
    with tempfile.TemporaryDirectory(prefix="fuzz-") as workdir:
        for i in range(args.n):
            seed = args.seed + i
            name, nvars, clauses = generate(seed, i)
            cnf = os.path.join(workdir, "case.cnf")
            write_dimacs(cnf, nvars, clauses)
            outcome = run_case(cnf, solver, oracle, args.timeout,
                               args.lrat, workdir)
            if outcome.failures:
                nfail += 1
                for k, _ in outcome.failures:
                    kinds_count[k] = kinds_count.get(k, 0) + 1
                bundle = save_bundle(outdir, i, seed, name, cnf, outcome,
                                     solver, oracle, args.timeout, args.lrat,
                                     not args.no_shrink)
                print(f"FAIL case {i} seed {seed} ({name}): "
                      f"{outcome.failures} -> {bundle}")
                if nfail >= args.max_failures:
                    print(f"stopping after {nfail} failures")
                    break
            if (i + 1) % 100 == 0:
                print(f"... {i + 1}/{args.n} cases, {nfail} failures",
                      flush=True)
    print(f"fuzz: {args.n} cases, {nfail} failures"
          + (f" {kinds_count}" if kinds_count else ""))
    return 1 if nfail else 0


def cmd_check_one(args):
    solver = shlex.split(args.solver)
    oracle = shlex.split(args.oracle) if args.oracle else None
    with tempfile.TemporaryDirectory(prefix="fuzz-check-") as workdir:
        outcome = run_case(args.cnf, solver, oracle, args.timeout,
                           args.lrat, workdir)
    kinds = {k for k, _ in outcome.failures}
    print(f"verdict={outcome.verdict} oracle={outcome.oracle_verdict} "
          f"failures={outcome.failures}")
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
            outcome = run_case(cnf, solver, oracle, args.timeout,
                               args.lrat, workdir)
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
        p.add_argument("--timeout", type=float, default=10.0)
        p.add_argument("--lrat", action="store_true",
                       help="also exercise drat-trim -L -> lrat-check")

    p = sub.add_parser("run")
    common(p)
    p.add_argument("--n", type=int, default=1000)
    p.add_argument("--seed", type=int, default=20260709)
    p.add_argument("--outdir", default=os.path.join(HERE, "failures"))
    p.add_argument("--max-failures", type=int, default=20)
    p.add_argument("--no-shrink", action="store_true")
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
