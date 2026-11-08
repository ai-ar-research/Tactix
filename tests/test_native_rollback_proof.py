"""Original-query oracles after restriction rollback on one Kernel/H."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
from test_native_query_proof import formula, require_forward_rejection
from test_native_query_append_proof import satisfiable


def main():
    fixture, root = map(Path, sys.argv[1:])
    sys.path.insert(0, str(root/'tools/bench'))
    spec = importlib.util.spec_from_file_location('rollback_runner', root/'tools/bench/run.py')
    runner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runner)
    from proof_oracle import certificate_route, check_dsr
    for tool in (runner.VERIPB, runner.CAKE_PB_CNF, runner.DRAT_TRIM, runner.CAKE_LPR,
                 runner.DSR_TRIM, runner.DSR_COMPRESS, runner.SR_CHECK):
        if not Path(tool).is_file():
            print(f'native rollback: missing checker {tool}')
            return 77
    traces, proofs = {}, {}
    checked = models = audits = rejects = forward_rejects = 0
    with tempfile.TemporaryDirectory(prefix='tactix-native-rollback-') as tmp:
        directory = Path(tmp)
        for case in ('assumption', 'append'):
            for variant in ('plain', 'collect', 'compact'):
                for mode in ('none', 'pbp', 'dsr', 'auto'):
                    for schedule in ('direct', 'dynamic'):
                        prefix = directory/f'{case}-{variant}-{mode}-{schedule}'
                        done = subprocess.run([fixture, case, variant, prefix, mode, schedule],
                                              capture_output=True, text=True, timeout=30)
                        assert done.returncode == 0, (case, variant, mode, schedule, done.stdout, done.stderr)
                        key = case, variant
                        assert done.stdout == traces.setdefault(key, done.stdout), (key, mode, schedule, done.stdout)
                        for query in range(2, 6):
                            stem = f'{prefix}-{query}'
                            cnf, proof = Path(stem+'.cnf'), Path(stem+'.proof')
                            variables, clauses = formula(cnf)
                            sat = query != 3
                            assert satisfiable(variables, clauses) == sat, (key, query)
                            if sat:
                                model = set(map(int, Path(stem+'.model').read_text().split()))
                                assert all((v in model) != (-v in model) for v in range(1, variables+1))
                                assert all(any(lit in model for lit in c) for c in clauses)
                                if query == 2:
                                    assert -3 in model
                                models += 1
                            if mode == 'none':
                                assert not proof.exists()
                                continue
                            text = proof.read_text()
                            proof_key = case, variant, mode, query
                            assert text == proofs.setdefault(proof_key, text), proof_key
                            if sat:
                                continue
                            kind = 'pbp' if text.startswith('pseudo-Boolean') else 'dsr'
                            free = kind == 'dsr' and certificate_route(proof) == 'drat'
                            route = 'pbp' if kind == 'pbp' else ('dsr-cake_lpr' if free else 'dsr-trestle')
                            result = runner.check_proof(kind, str(cnf), str(proof), 30, route=route)
                            assert result[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), (proof_key, result)
                            checked += 1
                            if kind == 'dsr':
                                work = directory/f'{case}-{variant}-{mode}-{schedule}-audit'
                                work.mkdir()
                                result = check_dsr(runner.DSR_TRIM, runner.DSR_COMPRESS, runner.SR_CHECK,
                                                   cnf, proof, work, 30, every_lemma=True,
                                                   cake_lpr=runner.CAKE_LPR, witness_free=free)
                                assert result[0], (proof_key, result)
                                audits += 1
                            for missing in ([1], [-1, 3]):
                                wrong_clauses = list(clauses)
                                wrong_clauses.remove(missing)
                                assert satisfiable(variables, wrong_clauses), (proof_key, missing)
                                wrong = directory/'missing-query-premise.cnf'
                                wrong.write_text(f'p cnf {variables} {len(wrong_clauses)}\n'+
                                                 ''.join(' '.join(map(str, c))+' 0\n' for c in wrong_clauses))
                                result = runner.check_proof(kind, str(wrong), str(proof), 30, route=route)
                                assert not result[0], (proof_key, missing, result)
                                if result[2] == runner.CHK_REJECTED:
                                    rejects += 1
                                else:
                                    assert kind == 'dsr' and result[2] == runner.CHK_ERROR and 'not in watch pointers' in result[1], (proof_key, missing, result)
                                    require_forward_rejection(runner, wrong, proof, directory/'negative-forward.lsr')
                                    forward_rejects += 1
    print(f'native rollback: {checked} canonical proofs, {audits} DSR every-lemma audits, '
          f'{rejects} missing-premise rejects, {forward_rejects} forward rejects after backward checker errors, {models} SAT models; '
          'same Kernel/H, native trajectories and schedule bytes agree')
    return 0


if __name__ == '__main__':
    sys.exit(main())
