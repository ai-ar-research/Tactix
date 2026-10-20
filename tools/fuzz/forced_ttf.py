#!/usr/bin/env python3
"""Targeted soundness fuzzing for balanced-signature forced-TTF rows.

The ordinary CNF generator almost never constructs the shape prefix and one
fixed signed-row pair for every ball.  This leg creates that authenticated
prefix, mutates clauses around it, and grades every completed SAT/UNSAT result
with the shared original-CNF model/proof and differential oracles.  Positive
cases count only when the solver's dedicated ``forced TTF`` counter moves.
"""

import argparse
import concurrent.futures
import itertools
import os
import random
import re
import shlex
import tempfile
import threading

import fuzz
from generators import write_dimacs


HERE = os.path.dirname(os.path.abspath(__file__))
CONTROL = os.path.join(HERE, "controls", "forced-ttf")
ACTION = os.path.join(CONTROL, "balanced.actions")
ACTION_PARAMETERS = (256, 8, 240, 70000, 24, 12379813812177893520, 64)
TIGHT_PARAMETERS = (256, 8, 2, 3, 1, 12379813812177893520, 1)
ACTION_PREFIX = (
    "sequence-repeat=once:1 "
    "step=1:Lucky:0:once:1:0:1:post(0):rulework(0,0):"
    "material(0,0):balanced-signature-model")
ACTION_SUFFIX = ":instances[10]:grids[]:sr[]\n"
BALANCE_RE = re.compile(
    r"^c balance\s+(\d+) calls, (\d+) recognized, (\d+) forced TTF, "
    r"(\d+) candidates, (\d+) restarts, (\d+) steps, "
    r"(\d+) completion rounds, (\d+) SAT, (\d+) interrupted$", re.M)


def action_text(parameters=ACTION_PARAMETERS):
    return ACTION_PREFIX + "(" + ",".join(map(str, parameters)) + ")" + ACTION_SUFFIX


def shape(n, width):
    signatures = (3 ** width - 1) // 2
    pair_variables = width * n * n
    boundary = pair_variables + 2 * n * signatures
    return signatures, pair_variables, boundary


def fixed_pair(n, width, ball):
    _, pair_variables, _ = shape(n, width)
    first = pair_variables + (ball + 1) * (2 * n) + 2 * ball + 1
    return first, first + 1


def ternary_vector(index, width):
    result = [0] * width
    for coordinate in range(width - 1, -1, -1):
        result[coordinate] = index % 3 - 1
        index //= 3
    return result


def balanced_signs(n, width):
    vectors = [ternary_vector(ball + 1, width) for ball in range(n)]
    return [signs for signs in itertools.product((-1, 1), repeat=n)
            if all(sum(signs[ball] * vectors[ball][coordinate]
                       for ball in range(n)) == 0
                   for coordinate in range(width))]


def base_formula(n, width, encodings=None):
    """Return the canonical fixed-row formula and its shape boundary."""
    _, pair_variables, boundary = shape(n, width)
    clauses = []
    for coordinate in range(width):
        for ball in range(n):
            diagonal = 1 + coordinate * n * n + ball * n + ball
            clauses.append([-diagonal])
    clauses.append([boundary + 1])
    encodings = encodings or ["binary"] * n
    for ball, encoding in enumerate(encodings):
        left, right = fixed_pair(n, width, ball)
        if encoding == "binary":
            clauses.append([left, right])
        elif encoding == "guarded":
            clauses.append([-(boundary + 1), left, right])
        else:
            raise ValueError("unknown forced-row encoding: " + encoding)
    return boundary + 1, clauses, pair_variables, boundary


def exclusion_clause(n, width, signs):
    selected = []
    for ball, sign in enumerate(signs):
        left, right = fixed_pair(n, width, ball)
        selected.append(right if sign > 0 else left)
    return [-literal for literal in selected]


def planted_mutations(rng, first_variable, count):
    assignment = {variable: bool(rng.getrandbits(1))
                  for variable in range(first_variable, first_variable + count)}
    clauses = []
    for _ in range(rng.randint(count, 4 * count)):
        width = rng.randint(2, min(5, count))
        variables = rng.sample(list(assignment), width)
        clause = [variable if rng.getrandbits(1) else -variable
                  for variable in variables]
        if not any((literal > 0) == assignment[abs(literal)]
                   for literal in clause):
            chosen = rng.randrange(width)
            variable = abs(clause[chosen])
            clause[chosen] = variable if assignment[variable] else -variable
        clauses.append(clause)
    return clauses


def mutate(seed, index):
    """Return name, dimensions, clauses, verdict, forced reach, and budget."""
    rng = random.Random((seed << 32) ^ index)
    # Fixed row b+1 must stay below P=(3**W-1)/2.  These shapes also have at
    # least one balanced sign vector, so positive cases cannot burn the full
    # reference search ceiling merely because their target is impossible.
    dimensions = ((3, 2), (4, 3), (4, 4))
    n, width = dimensions[index % len(dimensions)]
    scenario = index % 10
    encodings = ["guarded" if rng.getrandbits(1) else "binary"
                 for _ in range(n)]
    nvars, clauses, _, boundary = base_formula(n, width, encodings)
    expected = "SAT"
    expect_forced = True
    tight = False
    tag = "forced"

    if scenario == 1:
        # A positive root-false literal is not an accepted guard.  It remains
        # in a ternary clause in the live arena, as exercised by the unit test.
        left, right = fixed_pair(n, width, 0)
        clauses[-n] = [boundary + 2, left, right]
        clauses.append([-(boundary + 2)])
        nvars = boundary + 2
        expect_forced = False
        tag = "invalid-guard"
    elif scenario == 2:
        clauses.pop()
        expect_forced = False
        tag = "missing-row"
    elif scenario == 3:
        left, right = fixed_pair(n, width, n - 1)
        stride = 2 * n
        clauses[-1] = [left - stride, right - stride]
        expect_forced = False
        tag = "wrong-row"
    elif scenario == 4:
        extra = rng.randint(4, 12)
        clauses.extend(planted_mutations(rng, nvars + 1, extra))
        nvars += extra
        tag = "clause-mutation"
    elif scenario == 5:
        left, right = nvars + 1, nvars + 2
        clauses.extend(([left, right], [left, -right],
                        [-left, right], [-left, -right]))
        nvars += 2
        expected = "UNSAT"
        tag = "disjoint-unsat"
    elif scenario in (6, 7):
        # Force exactly one orientation in each row.  Excluding every balanced
        # orientation makes the Lucky candidate fail whole-CNF validation while
        # preserving non-balanced SAT assignments.  Excluding all orientations
        # is a compact UNSAT control through the same validation path.
        for ball in range(n):
            left, right = fixed_pair(n, width, ball)
            clauses.append([-left, -right])
        signs = (list(itertools.product((-1, 1), repeat=n))
                 if scenario == 7 else balanced_signs(n, width))
        clauses.extend(exclusion_clause(n, width, values) for values in signs)
        expected = "UNSAT" if scenario == 7 else "SAT"
        tag = "validation-poison-" + expected.lower()
    elif scenario == 8:
        tight = True
        tag = "typed-budget"
    elif scenario == 9:
        # Keep recognition exact while mutating representation, order, and a
        # surrounding satisfiable component.
        rng.shuffle(clauses)
        extra = rng.randint(3, 8)
        clauses.extend(planted_mutations(rng, nvars + 1, extra))
        nvars += extra
        tag = "shuffled-mixed"

    return (f"forced-ttf-{tag}-{n}x{width}-{index}", nvars, clauses,
            expected, expect_forced, tight)


def balance_counters(stdout):
    match = BALANCE_RE.search(stdout)
    if not match:
        return None
    keys = ("calls", "recognized", "forced", "candidates", "restarts",
            "steps", "completion", "sat", "interrupted")
    return dict(zip(keys, map(int, match.groups())))


def normalized_stdout(stdout):
    return "\n".join(line for line in stdout.splitlines()
                      if line.startswith(("c ", "s ", "v "))
                      and "timing" not in line
                      and not line.endswith(" ms")) + "\n"


def read_bytes(path):
    with open(path, "rb") as source:
        return source.read()


def controller_command(solver, action, policy_seed, trace=None, replay=None):
    command = solver + [f"--controller-actions={action}"]
    if replay:
        command.append(f"--controller-replay={replay}")
    else:
        command.append(f"--controller-policy={policy_seed}")
    if trace:
        command.append(f"--controller-trace={trace}")
    return command


def trace_control(solver, timeout, policy_seed):
    name, nvars, clauses, _, _, _ = mutate(20261005, 0)
    with tempfile.TemporaryDirectory(prefix="forced-ttf-trace-") as work:
        cnf = os.path.join(work, "case.cnf")
        write_dimacs(cnf, nvars, clauses)
        first = os.path.join(work, "first.trace")
        second = os.path.join(work, "second.trace")
        commands = [
            controller_command(solver, ACTION, policy_seed, trace=first),
            controller_command(solver, ACTION, policy_seed, replay=first),
            controller_command(solver, ACTION, policy_seed, trace=second),
        ]
        artifacts = []
        for run, command in enumerate(commands):
            proof = os.path.join(work, f"{run}.proof")
            code, stdout, stderr = fuzz.run_cmd(command + [cnf, proof], timeout)
            if code != 10:
                raise RuntimeError(
                    f"{name}: trace control run {run} exited {code}: {stderr}")
            counters = balance_counters(stdout)
            if not counters or counters["forced"] == 0:
                raise RuntimeError(f"{name}: trace control missed forced TTF")
            artifacts.append((stdout, read_bytes(proof)))
        if normalized_stdout(artifacts[0][0]) != normalized_stdout(artifacts[1][0]):
            raise RuntimeError(f"{name}: replay counters/model mismatch")
        if artifacts[0][1] != artifacts[1][1] or artifacts[0][1] != artifacts[2][1]:
            raise RuntimeError(f"{name}: record/replay proof mismatch")
        normalize_trace = lambda data: b"\n".join(
            line for line in data.splitlines()
            if not line.startswith(b"elapsed-nanos ")) + b"\n"
        if normalize_trace(read_bytes(first)) != normalize_trace(read_bytes(second)):
            raise RuntimeError(f"{name}: seeded traces differ across processes")
        expected_request = f"\nrequest {fuzz.ACTION_SPACE_VERSION} ".encode()
        if expected_request not in read_bytes(first):
            raise RuntimeError(
                f"{name}: trace omitted action-space v{fuzz.ACTION_SPACE_VERSION} request")


def interruption_control(solver, timeout, policy_seed):
    name, nvars, clauses, _, _, _ = mutate(20261005, 0)
    with tempfile.TemporaryDirectory(prefix="forced-ttf-interrupt-") as work:
        cnf = os.path.join(work, "case.cnf")
        write_dimacs(cnf, nvars, clauses)
        command = controller_command(solver, ACTION, policy_seed)
        # PBP serialization adds deterministic stop polls at powers of two;
        # retain the historical odd periods as controls for poll-placement
        # drift in the underlying constructor.
        for period in (1, 2, 3, 4, 5, 7, 8, 11, 16, 17, 31, 63, 127, 255, 511):
            outputs = []
            for run in range(2):
                proof = os.path.join(work, f"interrupt-{period}-{run}.proof")
                environment = os.environ.copy()
                environment["TACTIX_STOP_FUZZ"] = str(period)
                code, stdout, stderr = fuzz.run_cmd(
                    command + [cnf, proof], timeout, env=environment)
                counters = balance_counters(stdout)
                outputs.append((code, normalized_stdout(stdout), counters, stderr))
            first, second = outputs
            if (first[0] == second[0] and first[1] == second[1] and
                    first[2] and first[2]["forced"] and
                    first[2]["interrupted"] and first[2] == second[2]):
                return period
        raise RuntimeError(
            f"{name}: no deterministic stop period reached forced-TTF interruption")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--solver", required=True,
                        help="base Tactix competition command, without a policy")
    parser.add_argument("--oracle", default=None)
    parser.add_argument("--n", type=int, default=10000)
    parser.add_argument("--seed", type=int, default=20261005)
    parser.add_argument("--policy-seed", type=int, default=2)
    parser.add_argument("--timeout", type=float, default=10.0)
    parser.add_argument("--jobs", type=int,
                        default=int(os.environ.get("SLURM_CPUS_PER_TASK", 8)))
    parser.add_argument("--outdir",
                        default=os.path.join(HERE, "failures", "forced-ttf"))
    parser.add_argument("--max-failures", type=int, default=20)
    parser.add_argument("--minimum-verdicts", type=int, default=0)
    parser.add_argument("--skip-controls", action="store_true")
    args = parser.parse_args()

    fuzz.verify_checker_collection()
    solver = shlex.split(args.solver)
    if any(argument.startswith(("--random-policy", "--controller-policy",
                                "--controller-replay", "--controller-actions"))
           for argument in solver):
        parser.error("--solver must name the base solver without a policy/action file")
    if any(flag in solver for flag in ("--auto-proof", "--dsr", "--drat",
                                       "--lrat")):
        parser.error("forced-TTF fuzz uses the default streaming PBP proof route")
    # Spell out PBP so the shared oracle selects VeriPB and CakePB even when
    # the solver binary has been renamed by a pinned cluster snapshot.
    if "--pbp" not in solver:
        solver.append("--pbp")
    oracle = shlex.split(args.oracle) if args.oracle else None
    os.makedirs(args.outdir, exist_ok=True)
    generated_controls = os.path.join(args.outdir, "controls")
    os.makedirs(generated_controls, exist_ok=True)
    tight_action = os.path.join(generated_controls, "balanced-tight.actions")
    with open(tight_action, "w", encoding="utf-8") as output:
        output.write(action_text(TIGHT_PARAMETERS))

    if not args.skip_controls:
        trace_control(solver, args.timeout, args.policy_seed)
        period = interruption_control(solver, args.timeout, args.policy_seed)
        print("forced-ttf: record/replay and seeded determinism PASS; "
              f"deterministic interruption PASS (TACTIX_STOP_FUZZ={period})",
              flush=True)

    lock = threading.Lock()
    stop = threading.Event()
    counts = {"done": 0, "failures": 0, "SAT": 0, "UNSAT": 0,
              "models_checked": 0, "proofs_checked": 0,
              "differential_checked": 0, "forced_expected": 0,
              "forced_reached": 0, "negative_controls": 0,
              "tight_cases": 0}

    def one(index):
        if stop.is_set():
            return
        name, nvars, clauses, expected, expect_forced, tight = mutate(
            args.seed + index, index)
        with tempfile.TemporaryDirectory(prefix="forced-ttf-case-") as work:
            cnf = os.path.join(work, "case.cnf")
            write_dimacs(cnf, nvars, clauses)
            action = tight_action if tight else ACTION
            policy = controller_command(solver, action, args.policy_seed)
            outcome = fuzz.run_case(cnf, policy, oracle, args.timeout, False, work)
            counters = balance_counters(outcome.solver_out)
            if not counters:
                outcome.fail("coverage", "missing balanced-signature counters")
            elif expect_forced and counters["forced"] == 0:
                outcome.fail("coverage", "balanced_signature_forced_ttf did not move")
            elif not expect_forced and counters["forced"] != 0:
                outcome.fail("coverage", "near-miss incorrectly counted as forced TTF")
            if tight and counters and (counters["restarts"] > 2 or
                                       counters["steps"] > 6 or
                                       counters["completion"] > 1):
                outcome.fail("coverage", "typed balanced-signature budget exceeded")
            if outcome.verdict != expected:
                outcome.fail("differential",
                             f"fixture expected {expected}, got {outcome.verdict}")
            bundle = None
            if outcome.failures:
                bundle = fuzz.save_bundle(
                    args.outdir, index, args.seed + index, name, cnf, outcome,
                    policy, oracle, args.timeout, False, False)
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
            if expect_forced:
                counts["forced_expected"] += 1
                if counters and counters["forced"]:
                    counts["forced_reached"] += 1
            else:
                counts["negative_controls"] += 1
            counts["tight_cases"] += int(tight)
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
    print("forced-ttf: "
          f"{counts['done']}/{args.n} cases, {counts['failures']} failures; "
          f"SAT={counts['SAT']} UNSAT={counts['UNSAT']}; "
          f"models-checked={counts['models_checked']} "
          f"proofs-checked={counts['proofs_checked']} "
          f"differential-checked={counts['differential_checked']}; "
          f"forced-reached={counts['forced_reached']}/"
          f"{counts['forced_expected']} negative-controls="
          f"{counts['negative_controls']} typed-budget={counts['tight_cases']}")
    if counts["forced_reached"] != counts["forced_expected"]:
        print("forced-ttf: REQUIRED FORCED-TTF REACH INCOMPLETE")
        return 1
    if not counts["negative_controls"] or not counts["tight_cases"]:
        print("forced-ttf: REQUIRED CONTROL CLASS MISSING")
        return 1
    if verdicts < args.minimum_verdicts:
        print(f"forced-ttf: verdict coverage {verdicts} below required "
              f"minimum {args.minimum_verdicts}")
        return 1
    return 1 if counts["failures"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
