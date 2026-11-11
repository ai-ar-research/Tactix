"""Deleted RUP antecedents survive as checked query-certificate derivations."""
from proof_test_support import formula, proof_runner, satisfiable
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    fixture, root = map(Path, sys.argv[1:])
    runner = proof_runner(root)
    from proof_oracle import certificate_route

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
                        model = set(map(int, Path(f'{prefix}-{query}.model').read_text().split()))
                        assert all((v in model) != (-v in model) for v in range(1, variables+1))
                        assert all(any(lit in model for lit in clause) for clause in clauses)
                        models += 1
                    if mode == 'none':
                        assert not proof.exists()
                        continue
                    key = query, mode
                    assert proof.read_bytes() == proofs.setdefault(key, proof.read_bytes()), key
                    if answer == 10:
                        continue
                    route = 'pbp' if mode == 'pbp' else ('dsr-cake_lpr' if certificate_route(proof) == 'drat' else 'dsr-trestle')
                    result = runner.check_proof(mode, str(cnf), str(proof), 30, route=route)
                    assert result[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), (key, result)
                    lines = cnf.read_text().splitlines()
                    header = lines[0].split()
                    header[3] = str(int(header[3])-1)
                    wrong = directory/'missing-assumption.cnf'
                    wrong.write_text(' '.join(header)+'\n'+'\n'.join(lines[1:-1])+'\n')
                    _, constraints = formula(wrong)
                    assert satisfiable(variables, constraints), (key, constraints)
                    result = runner.check_proof(mode, str(wrong), str(proof), 30, route=route)
                    assert not result[0] and result[2] == runner.CHK_REJECTED, (key, result)
                    checked += 1
    print(f'derivation history: {checked} checked refutations/negative controls and {models} checked SAT models; proof-mode records/trajectories and schedule proof bytes agree')
    return 0


if __name__ == '__main__':
    sys.exit(main())
