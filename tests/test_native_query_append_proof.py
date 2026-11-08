"""Native suffix admission: exact F+A, exhaustive answers, models and proofs."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
from test_native_query_proof import formula

CASES = {'bve': (10, 20, 10), 'bce': (10, 20), 'normalize': (10, 20, 10),
         'growth': (10, 20, 20, 10, 10, 20), 'unitconflict': (10, 20), 'empty': (10, 20),
         'duplicate': (10, 20, 10), 'licensed': (10, 20, 10), 'default': (10, 20, 20, 10)}


def satisfiable(variables, clauses):
    return any(all(any(bool(mask & (1 << (abs(lit)-1))) == (lit > 0)
                       for lit in clause) for clause in clauses)
               for mask in range(1 << variables))


def main():
    fixture, root = map(Path, sys.argv[1:])
    sys.path.insert(0, str(root/'tools/bench'))
    spec = importlib.util.spec_from_file_location('append_runner', root/'tools/bench/run.py')
    runner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runner)
    from proof_oracle import certificate_route, check_dsr
    for tool in (runner.VERIPB, runner.CAKE_PB_CNF, runner.DRAT_TRIM, runner.CAKE_LPR,
                 runner.DSR_TRIM, runner.DSR_COMPRESS, runner.SR_CHECK):
        if not Path(tool).is_file():
            print(f'native append proof: missing checker {tool}')
            return 77
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
                            model = set(map(int, Path(f'{prefix}-{query}.model').read_text().split()))
                            assert all((v in model) != (-v in model) for v in range(1, variables+1))
                            assert all(any(lit in model for lit in clause) for clause in clauses)
                            models += 1
                        if mode == 'none':
                            assert not proof.exists()
                            continue
                        key = case, query, mode
                        assert proof.read_bytes() == proofs.setdefault(key, proof.read_bytes()), key
                        if answer == 10:
                            continue
                        route = 'pbp' if mode == 'pbp' else (
                            'dsr-cake_lpr' if certificate_route(proof) == 'drat' else 'dsr-trestle')
                        result = runner.check_proof(mode, str(cnf), str(proof), 30, route=route)
                        assert result[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), (key, result)
                        if route == 'dsr-trestle':
                            work = directory/f'audit-{case}-{query}-{schedule}'
                            work.mkdir()
                            result = check_dsr(runner.DSR_TRIM, runner.DSR_COMPRESS, runner.SR_CHECK,
                                               cnf, proof, work, 30, every_lemma=True)
                            assert result[0], result
                        # Remove the critical last assumption or new permanent
                        # clause. Independently establish that the control is SAT.
                        lines = cnf.read_text().splitlines()
                        header = lines[0].split()
                        header[3] = str(int(header[3])-1)
                        wrong = directory/'missing-current-constraint.cnf'
                        wrong.write_text(' '.join(header)+'\n'+'\n'.join(lines[1:-1])+'\n')
                        _, constraints = formula(wrong)
                        assert satisfiable(variables, constraints), (key, constraints)
                        result = runner.check_proof(mode, str(wrong), str(proof), 30, route=route)
                        assert not result[0] and result[2] == runner.CHK_REJECTED, (key, result)
                        checked += 1
    print(f'native append: {checked} checked refutations/negative controls and {models} checked SAT models; exhaustive oracle, trajectories and schedule proof bytes agree')
    return 0


if __name__ == '__main__':
    sys.exit(main())
