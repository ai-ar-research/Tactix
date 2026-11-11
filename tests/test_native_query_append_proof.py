"""Native suffix admission: exact F+A, exhaustive answers, models and proofs."""
from pathlib import Path
import subprocess
import sys
import tempfile
from proof_test_support import audit_dsr, check_model, check_unsat, formula, proof_runner, remove_clause, satisfiable

CASES = {'bve': (10, 20, 10), 'bce': (10, 20), 'normalize': (10, 20, 10),
         'growth': (10, 20, 20, 10, 10, 20), 'unitconflict': (10, 20), 'empty': (10, 20),
         'duplicate': (10, 20, 10), 'licensed': (10, 20, 10), 'default': (10, 20, 20, 10)}


def main():
    fixture, root = map(Path, sys.argv[1:])
    runner = proof_runner(root)
    proofs, traces = {}, {}
    checked = models = 0
    with tempfile.TemporaryDirectory(prefix='tactix-native-append-') as tmp:
        directory = Path(tmp)
        for mode in ('none', 'pbp', 'dsr'):
            for schedule in ('direct', 'dynamic'):
                for case, answers in CASES.items():
                    prefix = directory/f'{case}-{mode}-{schedule}'
                    done = subprocess.run([fixture, case, prefix, mode, schedule],
                                          capture_output=True, text=True, timeout=30)
                    assert done.returncode == 0, (case, mode, schedule, done.stdout, done.stderr)
                    assert done.stdout == traces.setdefault(case, done.stdout), (case, mode, schedule, traces[case], done.stdout)
                    for query, answer in enumerate(answers, 1):
                        cnf, proof = Path(f'{prefix}-{query}.cnf'), Path(f'{prefix}-{query}.proof')
                        variables, clauses = formula(cnf)
                        assert satisfiable(variables, clauses) == (answer == 10), (case, query)
                        if answer == 10:
                            check_model(Path(f'{prefix}-{query}.model'), variables, clauses)
                            models += 1
                        if mode == 'none':
                            assert not proof.exists()
                            continue
                        key = case, query, mode
                        assert proof.read_bytes() == proofs.setdefault(key, proof.read_bytes()), key
                        if answer == 10:
                            continue
                        _, route = check_unsat(runner, str(cnf), str(proof), fmt=mode)
                        if route == 'dsr-trestle':
                            work = directory/f'audit-{case}-{query}-{schedule}'
                            audit_dsr(runner, cnf, proof, work)
                        # Remove the critical last assumption or new permanent
                        # clause. Independently establish that the control is SAT.
                        wrong = directory/'missing-current-constraint.cnf'
                        remove_clause(cnf, wrong)
                        result = runner.check_proof(mode, str(wrong), str(proof), 30, route=route)
                        assert not result[0] and result[2] == runner.CHK_REJECTED, (key, result)
                        checked += 1
    print(f'native append: {checked} checked refutations/negative controls and {models} checked SAT models; exhaustive oracle, trajectories and schedule proof bytes agree')
    return 0


if __name__ == '__main__':
    sys.exit(main())
