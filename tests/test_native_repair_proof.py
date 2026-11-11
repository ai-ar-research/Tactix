"""Native repair groups preserve exact repeated-query proofs and trajectories."""
from proof_test_support import audit_dsr, check_model, check_unsat, formula, proof_runner, remove_clause, satisfiable
from pathlib import Path
import subprocess
import sys
import tempfile

CASES = ('condition', 'cce', 'binary', 'ata', 'substitute')
ANSWERS = (10, 20, 20, 10)


def main():
    fixture, root = map(Path, sys.argv[1:])
    runner = proof_runner(root)

    proofs, traces = {}, {}
    checked = models = 0
    with tempfile.TemporaryDirectory(prefix='tactix-native-repair-') as tmp:
        directory = Path(tmp)
        for mode in ('none', 'pbp', 'dsr'):
            for schedule in ('direct', 'dynamic'):
                for case in CASES:
                    prefix = directory/f'{case}-{mode}-{schedule}'
                    done = subprocess.run([fixture, case, prefix, mode, schedule],
                                          capture_output=True, text=True, timeout=30)
                    context = case, mode, schedule
                    assert done.returncode == 0, (context, done.stdout, done.stderr)
                    assert done.stdout == traces.setdefault(case, done.stdout), (context, done.stdout, traces[case])
                    for query, answer in enumerate(ANSWERS, 1):
                        cnf = Path(f'{prefix}-{query}.cnf')
                        proof = Path(f'{prefix}-{query}.proof')
                        variables, clauses = formula(cnf)
                        assert satisfiable(variables, clauses) == (answer == 10), (context, query)
                        if answer == 10:
                            check_model(Path(f'{prefix}-{query}.model'), variables, clauses)
                            models += 1
                        if mode == 'none':
                            assert not proof.exists()
                            continue
                        key = case, query, mode
                        assert proof.read_bytes() == proofs.setdefault(key, proof.read_bytes()), (context, query)
                        if answer == 10:
                            continue
                        _, route = check_unsat(runner, str(cnf), str(proof), fmt=mode)
                        if route == 'dsr-trestle':
                            work = directory/f'audit-{case}-{query}-{schedule}'
                            audit_dsr(runner, cnf, proof, work)
                        # A prior query's certificate cannot certify a different
                        # assumption cube. Establish SAT independently first.
                        wrong = directory/'missing-assumption.cnf'
                        remove_clause(cnf, wrong)
                        result = runner.check_proof(mode, str(wrong), str(proof), 30, route=route)
                        assert not result[0] and result[2] == runner.CHK_REJECTED, (key, result)
                        checked += 1
    print(f'native repair: {checked} checked refutations/negative controls and {models} checked SAT models; trajectories and proof bytes agree')
    return 0


if __name__ == '__main__':
    sys.exit(main())
