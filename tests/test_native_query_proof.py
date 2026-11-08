"""Check exact-query certificates/models after native continuation, in two schedules."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile


def require_forward_rejection(runner, cnf, proof, output):
    """A malformed-premise backward-checker error is never a clean rejection."""
    from proof_oracle import proof_check_status
    done = subprocess.run([runner.DSR_TRIM, '-f', str(cnf), str(proof), str(output)],
                          capture_output=True, text=True, timeout=30)
    assert proof_check_status('dsr-trim', done.returncode, done.stdout, done.stderr) == 'rejected', (
        cnf, proof, done.returncode, done.stdout, done.stderr)


def formula(path):
    clauses, clause = [], []
    variables = None
    for line in path.read_text().splitlines():
        if not line or line.startswith('c'):
            continue
        if line.startswith('p'):
            variables = int(line.split()[2])
            continue
        for lit in map(int, line.split()):
            if lit:
                clause.append(lit)
            else:
                clauses.append(clause)
                clause = []
    assert variables is not None and not clause
    return variables, clauses


def main():
    fixture, root = map(Path, sys.argv[1:])
    sys.path.insert(0, str(root/'tools/bench'))
    spec = importlib.util.spec_from_file_location('tactix_bench_run', root/'tools/bench/run.py')
    runner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runner)
    from proof_oracle import certificate_route
    for tool in (runner.VERIPB, runner.CAKE_PB_CNF, runner.DRAT_TRIM, runner.CAKE_LPR,
                 runner.DSR_TRIM, runner.DSR_COMPRESS, runner.SR_CHECK):
        if not Path(tool).is_file():
            print(f'native query proof: missing checker {tool}')
            return 77
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
                            model = set(map(int, Path(f'{prefix}-{query}.model').read_text().split()))
                            assert all((v in model) != (-v in model) for v in range(1, variables+1))
                            assert all(any(lit in model for lit in clause) for clause in clauses)
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
                        route = 'pbp' if mode == 'pbp' else ('dsr-cake_lpr' if certificate_route(proof) == 'drat' else 'dsr-trestle')
                        result = runner.check_proof(mode, str(cnf), str(proof), 30, route=route)
                        assert result[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), (key, result)
                        # Omit exactly the last current assumption. Exhaustion
                        # or a parser error does not count as this rejection.
                        lines = cnf.read_text().splitlines()
                        header = lines[0].split()
                        header[3] = str(int(header[3])-1)
                        wrong = directory/'missing-current-assumption.cnf'
                        wrong.write_text(' '.join(header)+'\n'+'\n'.join(lines[1:-1])+'\n')
                        _, wrong_formula = formula(wrong)
                        assert any(all(any(bool(mask & (1 << (abs(lit)-1))) == (lit > 0)
                                           for lit in clause) for clause in wrong_formula)
                                   for mask in range(1 << variables))
                        result = runner.check_proof(mode, str(wrong), str(proof), 30, route=route)
                        assert not result[0] and result[2] == runner.CHK_REJECTED, (key, result)
                        checked += 1
    print(f'native continuation: {checked} checked proofs/negative controls, {models} checked SAT models; exact query oracle, trajectories and schedule proof bytes agree')
    return 0


if __name__ == '__main__':
    sys.exit(main())
