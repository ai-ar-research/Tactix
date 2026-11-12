"""Check exact-query certificates/models after native continuation, in two schedules."""
from proof_test_support import check_model, check_unsat, formula, proof_runner, remove_clause
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    fixture, root = map(Path, sys.argv[1:])
    runner = proof_runner(root)

    checked = models = 0
    traces, proofs = {}, {}
    with tempfile.TemporaryDirectory(prefix='tactix-native-query-') as tmp:
        directory = Path(tmp)
        for mode in ('none', 'pbp', 'dsr'):
            for schedule in ('direct', 'dynamic'):
                for case in ('chain', 'compact', 'large', 'shrink', 'root', 'archive'):
                    prefix = directory/f'{case}-{mode}-{schedule}'
                    done = subprocess.run([fixture, case, prefix, mode, schedule],
                                          capture_output=True, text=True, timeout=30)
                    assert done.returncode == 0, (case, mode, schedule, done.stdout, done.stderr)
                    assert done.stdout == traces.setdefault(case, done.stdout), (case, mode, schedule, traces[case], done.stdout)
                    for query in range(1, 6 if case == 'chain' else 4):
                        cnf, proof = Path(f'{prefix}-{query}.cnf'), Path(f'{prefix}-{query}.proof')
                        variables, clauses = formula(cnf)
                        satisfiable = any(all(any(bool(mask & (1 << (abs(lit)-1))) == (lit > 0)
                                                      for lit in clause) for clause in clauses)
                                          for mask in range(1 << variables))
                        expected_sat = query in (1, 4) or (query == 3 and case not in ('root', 'chain'))
                        assert satisfiable == expected_sat, (case, query)
                        if satisfiable:
                            check_model(Path(f'{prefix}-{query}.model'), variables, clauses)
                            models += 1
                        if mode == 'none':
                            assert not proof.exists()
                            continue
                        # The direct and registry schedules select the same
                        # actions; each written format must preserve its bytes.
                        key = (case, query, mode)
                        assert proof.read_bytes() == proofs.setdefault(key, proof.read_bytes()), key
                        if satisfiable:
                            continue
                        _, route = check_unsat(runner, str(cnf), str(proof), fmt=mode)
                        # Omit exactly the last current assumption. Exhaustion
                        # or a parser error does not count as this rejection.
                        wrong = directory/'missing-current-assumption.cnf'
                        remove_clause(cnf, wrong)
                        result = runner.check_proof(mode, str(wrong), str(proof), 30, route=route)
                        assert not result[0] and result[2] == runner.CHK_REJECTED, (key, result)
                        checked += 1
    print(f'native continuation: {checked} checked proofs/negative controls, {models} checked SAT models; exact query oracle, trajectories and schedule proof bytes agree')
    return 0


if __name__ == '__main__':
    sys.exit(main())
