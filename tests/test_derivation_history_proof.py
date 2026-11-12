"""Deleted RUP antecedents survive as checked query-certificate derivations."""
from proof_test_support import check_model, check_unsat, formula, proof_runner, remove_clause, satisfiable
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    fixture, root = map(Path, sys.argv[1:])
    runner = proof_runner(root)

    checked = models = 0
    traces, proofs = {}, {}
    with tempfile.TemporaryDirectory(prefix='tactix-derivation-') as tmp:
        directory = Path(tmp)
        for mode in ('none', 'pbp', 'dsr'):
            for schedule in ('direct', 'dynamic'):
                prefix = directory/f'{mode}-{schedule}'
                done = subprocess.run([fixture, prefix, mode, schedule],
                                      capture_output=True, text=True, timeout=30)
                assert done.returncode == 0, (mode, schedule, done.stdout, done.stderr)
                assert done.stdout == traces.setdefault('all', done.stdout), (mode, schedule, done.stdout, traces['all'])
                for query, answer in enumerate((10, 20, 10, 20, 10), 1):
                    cnf, proof = Path(f'{prefix}-{query}.cnf'), Path(f'{prefix}-{query}.proof')
                    variables, clauses = formula(cnf)
                    assert satisfiable(variables, clauses) == (answer == 10), query
                    if answer == 10:
                        check_model(Path(f'{prefix}-{query}.model'), variables, clauses)
                        models += 1
                    if mode == 'none':
                        assert not proof.exists()
                        continue
                    key = query, mode
                    assert proof.read_bytes() == proofs.setdefault(key, proof.read_bytes()), key
                    if answer == 10:
                        continue
                    _, route = check_unsat(runner, str(cnf), str(proof), fmt=mode)
                    wrong = directory/'missing-assumption.cnf'
                    remove_clause(cnf, wrong)
                    result = runner.check_proof(mode, str(wrong), str(proof), 30, route=route)
                    assert not result[0] and result[2] == runner.CHK_REJECTED, (key, result)
                    checked += 1
    print(f'derivation history: {checked} checked refutations/negative controls and {models} checked SAT models; proof-mode records/trajectories and schedule proof bytes agree')
    return 0


if __name__ == '__main__':
    sys.exit(main())
