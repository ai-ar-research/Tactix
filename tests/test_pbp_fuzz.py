#!/usr/bin/env python3
"""Small deterministic whole-run PBP proof/model fuzz with two proof checkers."""

import itertools
import pathlib
import random
import subprocess
import sys
import tempfile


def satisfied(clauses, assignment):
    return all(any(assignment[abs(lit)] == (lit > 0) for lit in clause)
               for clause in clauses)


def brute_force(variables, clauses):
    for values in itertools.product((False, True), repeat=variables):
        assignment = {index + 1: value for index, value in enumerate(values)}
        if satisfied(clauses, assignment):
            return assignment
    return None


def random_clause(rng, variables, planted=None):
    width = rng.randint(2, min(4, variables))
    chosen = rng.sample(range(1, variables + 1), width)
    clause = [variable if rng.getrandbits(1) else -variable
              for variable in chosen]
    if planted is not None and not any(
            planted[abs(literal)] == (literal > 0) for literal in clause):
        at = rng.randrange(len(clause))
        variable = abs(clause[at])
        clause[at] = variable if planted[variable] else -variable
    # Exercise raw-input normalization without changing the formula.
    roll = rng.randrange(8)
    if roll == 0:
        clause.append(clause[-1])
    elif roll == 1:
        clause.extend((chosen[0], -chosen[0]))
    return clause


def make_case(rng, index):
    variables = rng.randint(4, 7)
    if index % 2 == 0:
        planted = {variable: bool(rng.getrandbits(1))
                   for variable in range(1, variables + 1)}
        clauses = [random_clause(rng, variables, planted)
                   for _ in range(rng.randint(12, 28))]
        assert satisfied(clauses, planted)
        return variables, clauses, True
    for _ in range(200):
        clauses = [random_clause(rng, variables)
                   for _ in range(rng.randint(24, 42))]
        if brute_force(variables, clauses) is None:
            return variables, clauses, False
    # Deterministic fallback forbids all assignments to the first three vars.
    clauses = []
    for values in itertools.product((False, True), repeat=3):
        clauses.append([-(i + 1) if value else i + 1
                        for i, value in enumerate(values)])
    return variables, clauses, False


def run(argv, timeout=30):
    return subprocess.run([str(part) for part in argv], text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          timeout=timeout)


def parse_model(output):
    assignment = {}
    for line in output.splitlines():
        if not line.startswith("v "):
            continue
        for token in line[2:].split():
            literal = int(token)
            if literal:
                assignment[abs(literal)] = literal > 0
    return assignment


def main():
    solver = pathlib.Path(sys.argv[1])
    root = pathlib.Path(sys.argv[2])
    veripb = root / "tools/checkers/bin/veripb"
    cakepb = root / "tools/checkers/bin/cake-pb-cnf"
    rng = random.Random(0x504250)
    sat = unsat = corrupted = deletions = 0
    with tempfile.TemporaryDirectory(prefix="tactix-pbp-fuzz-") as raw:
        directory = pathlib.Path(raw)
        for index in range(32):
            variables, clauses, expected_sat = make_case(rng, index)
            cnf = directory / f"case-{index}.cnf"
            proof = directory / f"case-{index}.pbp"
            with cnf.open("w") as stream:
                stream.write(f"p cnf {variables} {len(clauses)}\n")
                for clause in clauses:
                    stream.write(" ".join(map(str, clause)) + " 0\n")
            solved = run([solver, "-q", "--pbp", cnf, proof])
            expected_code = 10 if expected_sat else 20
            assert solved.returncode == expected_code, solved.stdout
            assert ("s SATISFIABLE" if expected_sat else "s UNSATISFIABLE") \
                in solved.stdout
            if expected_sat:
                model = parse_model(solved.stdout)
                assert len(model) >= variables and satisfied(clauses, model)
                sat += 1
                continue
            proof_text = proof.read_text()
            deletions += proof_text.count("del spec ") + \
                proof_text.count("del id ")
            kernel = directory / f"case-{index}.kernel.pbp"
            checked = run([veripb, "-u", "--cnf", "--elaborate", kernel,
                           cnf, proof])
            assert checked.returncode == 0 and "VERIFIED" in checked.stdout, \
                checked.stdout
            independent = run([cakepb, cnf, kernel])
            assert independent.returncode == 0 and \
                "VERIFIED UNSAT" in independent.stdout, independent.stdout
            unsat += 1
            if corrupted < 8:
                bad = directory / f"case-{index}.corrupt.pbp"
                assert "rup >= 1 ;\n" in proof_text
                bad.write_text(proof_text.replace("rup >= 1 ;\n", "", 1))
                rejected = run([veripb, "-u", "--cnf", cnf, bad])
                assert rejected.returncode != 0 or \
                    "VERIFIED UNSATISFIABLE" not in rejected.stdout
                corrupted += 1

        # Regression for a propagated root whose binary reason is deleted by
        # simplification. Streaming PBP must publish the implied unit before
        # the exact deletion; otherwise the following vivification RUPs are
        # valid in the solver but invalid in the checker's weaker database.
        clauses = [
            [1, 2, -3, 4, 5], [1, 2, -3, 4, -5],
            [1, -2, -3, 4, 5], [1, -2, -3, 4, -5],
            [-1, 2, -3, 4, 5], [-1, 2, -3, 4, -5],
            [-1, -2, -3, 4, 5], [-1, -2, -3, 4, -5],
            [4, 3], [-3, -6, 1], [-4],
        ]
        cnf = directory / "root-reason-deletion.cnf"
        proof = directory / "root-reason-deletion.pbp"
        with cnf.open("w") as stream:
            stream.write(f"p cnf 6 {len(clauses)}\n")
            for clause in clauses:
                stream.write(" ".join(map(str, clause)) + " 0\n")
        solved = run([solver, "-q", "--pbp", cnf, proof])
        assert solved.returncode == 20, solved.stdout
        proof_text = proof.read_text()
        assert proof_text.count("del id ") == 9 and \
            proof_text.count("del spec ") == 1, proof_text
        kernel = directory / "root-reason-deletion.kernel.pbp"
        checked = run([veripb, "-u", "--cnf", "--elaborate", kernel,
                       cnf, proof])
        assert checked.returncode == 0 and \
            "VERIFIED UNSATISFIABLE" in checked.stdout, checked.stdout
        independent = run([cakepb, cnf, kernel])
        assert independent.returncode == 0 and \
            "VERIFIED UNSAT" in independent.stdout, independent.stdout
        unsat += 1

        # A propagation-derived root unit may be emitted while later original
        # clauses are still being parsed.  The PBP `f` header counts original
        # CNF constraints rather than the now-interleaved internal clause ids.
        clauses = [[1, 2], [-1], [-2]]
        cnf = directory / "input-derived-unit-before-later-clause.cnf"
        proof = directory / "input-derived-unit-before-later-clause.pbp"
        with cnf.open("w") as stream:
            stream.write(f"p cnf 2 {len(clauses)}\n")
            for clause in clauses:
                stream.write(" ".join(map(str, clause)) + " 0\n")
        solved = run([solver, "-q", "--pbp", cnf, proof])
        assert solved.returncode == 20, solved.stdout
        assert "f 3;\n" in proof.read_text()
        kernel = directory / "input-derived-unit-before-later-clause.kernel.pbp"
        checked = run([veripb, "-u", "--cnf", "--elaborate", kernel,
                       cnf, proof])
        assert checked.returncode == 0 and \
            "VERIFIED UNSATISFIABLE" in checked.stdout, checked.stdout
        independent = run([cakepb, cnf, kernel])
        assert independent.returncode == 0 and \
            "VERIFIED UNSAT" in independent.stdout, independent.stdout
        unsat += 1
    assert deletions > 0
    print(f"PBP fuzz: {sat} SAT models, {unsat} VeriPB+CakePB UNSAT proofs, "
          f"{deletions} exact deletions, {corrupted} corrupted proofs rejected")


if __name__ == "__main__":
    main()
