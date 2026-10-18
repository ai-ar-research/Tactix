#!/usr/bin/env python3
"""Focused soundness fuzzing for typed controller actions (historical v5 name).

Ordinary random CNFs cannot supply caller-owned GF(3) equation identities or
Direct CSP domain rows.  This leg keeps one authenticated structure intact,
varies GF(3) right-hand sides, and mutates a component disjoint from Direct CSP.
Each case must execute an effective typed action before its independently
checked SAT model or UNSAT proof counts.
"""

import argparse
import concurrent.futures
import os
import random
import shlex
import tempfile
import threading

import fuzz
from generators import write_dimacs

HERE = os.path.dirname(os.path.abspath(__file__))
CONTROL = os.path.join(HERE, "controls", "controller-v5")


def gf3_base(rhs=(0, 0, 0), contradictory=False):
    clauses = []
    values = []
    domain_count = 3 if contradictory else 7
    for domain in range(domain_count):
        first = 3 * domain + 1
        row = [first, first + 1, first + 2]
        values.append(row)
        clauses.append(row)
        for left in range(3):
            for right in range(left + 1, 3):
                clauses.append([-row[left], -row[right]])

    def equation(domains, coefficients, target):
        for packed in range(3 ** len(domains)):
            rest, total, clause = packed, 0, []
            for domain, coefficient in zip(domains, coefficients):
                value = rest % 3
                rest //= 3
                total = (total + coefficient * value) % 3
                clause.append(-values[domain][value])
            if total != target:
                clauses.append(clause)

    if contradictory:
        # E0 + (-E1) entails x0-x2=a-b, while E2 asserts the next
        # residue. Vary a,b without losing the selected pair's unpublished
        # consequence or the complete system's inconsistency.
        first_rhs, second_rhs = rhs[0], rhs[1]
        # The first selected pair therefore has a real unpublished Boolean
        # consequence even though the complete affine system is inconsistent.
        equation((0, 1), (1, 1), first_rhs)
        equation((1, 2), (1, 1), second_rhs)
        equation((0, 2), (1, 2),
                 (first_rhs - second_rhs + 1) % 3)
    else:
        equation((0, 1, 2), (1, 1, 1), rhs[0])
        equation((2, 3, 4), (1, 1, 1), rhs[1])
        equation((4, 5, 6), (1, 1, 1), rhs[2])
    return 3 * domain_count, clauses


def direct_csp_base():
    return 7, [
        [1, 2, 3], [-1, -2], [-1, -3], [-2, -3],
        [4, 5, 6, 7], [-4, -5], [-4, -6], [-4, -7], [-5, -6],
        [-5, -7], [-6, -7], [-1, -7], [-2, -4], [-2, -6],
        [-2, -7], [-3, -5],
    ]


def mutate(seed, index):
    """Return (name, family, action file, nvars, clauses, expected class).

    GF(3) cases vary equation right-hand sides and add a contradictory equation
    for UNSAT. DirectCSP's fresh component never mentions a structure variable;
    SAT clauses follow a planted assignment and UNSAT cases receive a final
    four-clause two-variable refutation. This gives a deterministic 50/50
    model/proof oracle mix without changing the typed action identity.
    """
    rng = random.Random((seed << 32) ^ index)
    if index % 4 < 2:
        family = "Gf3AffineConsequence"
        action = os.path.join(CONTROL, "gf3.actions")
        rhs = tuple(rng.randrange(3) for _ in range(3))
        nvars, clauses = gf3_base(rhs, contradictory=bool(index % 2))
        tag = "gf3"
    else:
        family = "DirectCspSupport"
        action = os.path.join(CONTROL, "direct-csp.actions")
        nvars, clauses = direct_csp_base()
        tag = "direct-csp"
    clauses = [list(clause) for clause in clauses]

    expected = "UNSAT" if index % 2 else "SAT"
    if family == "DirectCspSupport":
        # DirectCSP authenticates the selected domains and relation rather than
        # requiring the whole CNF to belong to that theory.  Mutate a disjoint
        # planted component; its clauses remain unit-free so the action sees
        # the assignment-free root snapshot its guard requires.
        extra = rng.randint(4, 18)
        first = nvars + 1
        assignment = {var: bool(rng.getrandbits(1))
                      for var in range(first, first + extra)}
        for _ in range(rng.randint(extra, 4 * extra)):
            width = rng.randint(2, min(5, extra))
            variables = rng.sample(range(first, first + extra), width)
            clause = [var if rng.getrandbits(1) else -var for var in variables]
            if not any((literal > 0) == assignment[abs(literal)]
                       for literal in clause):
                chosen = rng.randrange(width)
                var = abs(clause[chosen])
                clause[chosen] = var if assignment[var] else -var
            clauses.append(clause)
        nvars += extra
        if index % 2:
            left, right = nvars + 1, nvars + 2
            clauses.extend(([left, right], [left, -right],
                            [-left, right], [-left, -right]))
            nvars += 2
    return (f"controller-v5-{tag}-{expected.lower()}-{index}", family,
            action, nvars, clauses, expected)


def normalized_stdout(stdout):
    return "\n".join(
        line for line in stdout.splitlines()
        if line.startswith(("c ", "s ", "v "))
        and "timing" not in line and not line.endswith(" ms")) + "\n"


def normalized_trace(data):
    """Remove recorded wall time, which replay consumes but never compares."""
    return b"\n".join(line for line in data.splitlines()
                       if not line.startswith(b"elapsed-nanos ")) + b"\n"


def read_bytes(path):
    with open(path, "rb") as source:
        return source.read()


def trace_control(solver, timeout, seed, index):
    name, family, action, nvars, clauses, _ = mutate(seed, index)
    with tempfile.TemporaryDirectory(prefix="controller-v5-trace-") as work:
        cnf = os.path.join(work, "case.cnf")
        write_dimacs(cnf, nvars, clauses)
        first_trace = os.path.join(work, "first.trace")
        second_trace = os.path.join(work, "second.trace")
        artifacts = []
        commands = [
            solver + [f"--controller-policy={seed}",
                      f"--controller-actions={action}",
                      f"--controller-trace={first_trace}",
                      f"--structural-provenance={work}/first.jsonl"],
            solver + [f"--controller-replay={first_trace}",
                      f"--controller-actions={action}",
                      f"--structural-provenance={work}/replay.jsonl"],
            solver + [f"--controller-policy={seed}",
                      f"--controller-actions={action}",
                      f"--controller-trace={second_trace}",
                      f"--structural-provenance={work}/second.jsonl"],
        ]
        for run, command in enumerate(commands):
            proof = os.path.join(work, f"{run}.proof")
            code, stdout, stderr = fuzz.run_cmd(command + [cnf, proof], timeout)
            if code not in (10, 20):
                raise RuntimeError(
                    f"{name}: trace control run {run} exited {code}: {stderr}")
            artifacts.append((code, stdout, read_bytes(proof)))
        if artifacts[0][0] != artifacts[1][0] or artifacts[0][0] != artifacts[2][0]:
            raise RuntimeError(f"{name}: record/replay verdict mismatch")
        if artifacts[0][2] != artifacts[1][2] or artifacts[0][2] != artifacts[2][2]:
            raise RuntimeError(f"{name}: record/replay proof mismatch")
        if normalized_stdout(artifacts[0][1]) != normalized_stdout(artifacts[1][1]):
            raise RuntimeError(f"{name}: replay counters/model mismatch")
        first_trace_data = read_bytes(first_trace)
        second_trace_data = read_bytes(second_trace)
        if normalized_trace(first_trace_data) != normalized_trace(second_trace_data):
            raise RuntimeError(f"{name}: seeded traces differ across processes")
        if family.encode() not in first_trace_data:
            raise RuntimeError(f"{name}: trace omitted typed {family} request")
        expected_request = f"\nrequest {fuzz.ACTION_SPACE_VERSION} ".encode()
        if expected_request not in first_trace_data:
            raise RuntimeError(
                f"{name}: trace did not bind action-space v{fuzz.ACTION_SPACE_VERSION}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--solver", required=True,
                        help="base Tactix competition command, without a policy")
    parser.add_argument("--oracle", default=None)
    parser.add_argument("--n", type=int, default=10000)
    parser.add_argument("--seed", type=int, default=20261004)
    parser.add_argument("--policy-seed", type=int, default=2,
                        help="controller seed; must select the typed action before search")
    parser.add_argument("--timeout", type=float, default=10.0)
    parser.add_argument("--jobs", type=int,
                        default=int(os.environ.get("SLURM_CPUS_PER_TASK", 8)))
    parser.add_argument("--outdir",
                        default=os.path.join(HERE, "failures", "controller-v5"))
    parser.add_argument("--max-failures", type=int, default=20)
    parser.add_argument("--minimum-verdicts", type=int, default=0)
    parser.add_argument("--skip-trace-controls", action="store_true")
    args = parser.parse_args()

    fuzz.verify_checker_collection()
    solver = shlex.split(args.solver)
    if any(arg.startswith(("--random-policy", "--controller-policy",
                           "--controller-replay", "--controller-actions"))
           for arg in solver):
        parser.error("--solver must name the base solver without a policy/action file")
    if "--lrat" in solver:
        parser.error("typed v5 actions require a clausal proof stream, not --lrat")
    # The public actions deliberately reject the automatic startup-PB route.
    # Pin this focused leg to Tactix's canonical clausal certificate.
    solver.append("--dsr")
    oracle = shlex.split(args.oracle) if args.oracle else None
    os.makedirs(args.outdir, exist_ok=True)

    if not args.skip_trace_controls:
        trace_control(solver, args.timeout, args.policy_seed, 0)
        trace_control(solver, args.timeout, args.policy_seed, 2)
        print("controller-v5: GF3/DirectCSP record/replay and seeded determinism PASS",
              flush=True)

    lock = threading.Lock()
    stop = threading.Event()
    counts = {"done": 0, "failures": 0, "SAT": 0, "UNSAT": 0,
              "models_checked": 0, "proofs_checked": 0,
              "differential_checked": 0,
              "gf3_effective": 0, "direct_effective": 0}

    def one(index):
        if stop.is_set():
            return
        name, family, action, nvars, clauses, expected = mutate(
            args.seed + index, index)
        with tempfile.TemporaryDirectory(prefix="controller-v5-case-") as work:
            policy = solver + [f"--controller-policy={args.policy_seed}",
                               f"--controller-actions={action}",
                               "--structural-provenance={workdir}/provenance.jsonl"]
            cnf = os.path.join(work, "case.cnf")
            write_dimacs(cnf, nvars, clauses)
            outcome = fuzz.run_case(cnf, policy, oracle, args.timeout,
                                    False, work)
            provenance = os.path.join(work, "provenance.jsonl")
            if os.path.isfile(provenance):
                outcome.proof_artifacts.append(provenance)
            missing = fuzz.missing_controller_requirements(
                fuzz.controller_coverage(outcome.solver_out),
                applications=(family,), effective=(family,))
            if missing:
                outcome.fail("coverage", "required typed action missing: " +
                             ", ".join(missing))
            if outcome.verdict != expected:
                outcome.fail("differential",
                             f"fixture expected {expected}, got {outcome.verdict}")
            bundle = None
            if outcome.failures:
                bundle = fuzz.save_bundle(
                    args.outdir, index, args.seed + index, name, cnf, outcome,
                    policy, oracle, args.timeout, False, False,
                    ("--require-controller-applied", family,
                     "--require-controller-effective", family))
        with lock:
            counts["done"] += 1
            failure_kinds = {kind for kind, _ in outcome.failures}
            if outcome.verdict in ("SAT", "UNSAT"):
                counts[outcome.verdict] += 1
            if outcome.verdict == "SAT" and "model" not in failure_kinds:
                counts["models_checked"] += 1
            if outcome.verdict == "UNSAT" and "proof" not in failure_kinds:
                counts["proofs_checked"] += 1
            if (oracle and outcome.oracle_verdict in ("SAT", "UNSAT") and
                    "differential" not in failure_kinds):
                counts["differential_checked"] += 1
            coverage = fuzz.controller_coverage(outcome.solver_out)
            if coverage and coverage["rule"].get(family, {}).get("effective", 0):
                key = "gf3_effective" if family == "Gf3AffineConsequence" \
                    else "direct_effective"
                counts[key] += 1
            if outcome.failures:
                counts["failures"] += 1
                print(f"FAIL {name}: {outcome.failures} -> {bundle}", flush=True)
                if counts["failures"] >= args.max_failures:
                    stop.set()
            if counts["done"] % 100 == 0:
                print(f"... {counts['done']}/{args.n} cases, "
                      f"{counts['failures']} failures", flush=True)

    jobs = max(1, args.jobs)
    if jobs == 1:
        for index in range(args.n):
            if stop.is_set():
                break
            one(index)
    else:
        with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
            list(pool.map(one, range(args.n)))

    verdicts = counts["SAT"] + counts["UNSAT"]
    print("controller-v5: "
          f"{counts['done']}/{args.n} cases, {counts['failures']} failures; "
          f"SAT={counts['SAT']} UNSAT={counts['UNSAT']}; "
          f"models-checked={counts['models_checked']} "
          f"proofs-checked={counts['proofs_checked']} "
          f"differential-checked={counts['differential_checked']}; "
          f"effective GF3={counts['gf3_effective']} "
          f"DirectCSP={counts['direct_effective']}")
    if counts["gf3_effective"] == 0 or counts["direct_effective"] == 0:
        print("controller-v5: REQUIRED TYPED ACTION FAMILY NEVER EFFECTIVE")
        return 1
    if verdicts < args.minimum_verdicts:
        print(f"controller-v5: verdict coverage {verdicts} below required "
              f"minimum {args.minimum_verdicts}")
        return 1
    return 1 if counts["failures"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
