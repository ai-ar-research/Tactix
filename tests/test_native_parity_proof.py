"""Independently check native XOR retention, later SR, exact queries and models."""
from proof_test_support import audit_dsr, check_model, check_unsat, formula, proof_runner, satisfiable
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    fixture, root = map(Path, sys.argv[1:])
    runner = proof_runner(root)
    from proof_oracle import check_dsr

    traces, proofs = {}, {}
    checked = models = audits = corruptions = 0
    with tempfile.TemporaryDirectory(prefix='tactix-native-parity-') as tmp:
        directory = Path(tmp)
        for case in ('sat', 'unsat'):
            for mode in ('none', 'pbp', 'dsr', 'auto'):
                for schedule in ('direct', 'dynamic'):
                    prefix = directory/f'{case}-{mode}-{schedule}'
                    done = subprocess.run([fixture, case, prefix, mode, schedule],
                                          capture_output=True, text=True, timeout=30)
                    assert done.returncode == 0, (case, mode, schedule, done.stdout, done.stderr)
                    assert done.stdout == traces.setdefault(case, done.stdout), (case, mode, schedule, traces[case], done.stdout)
                    for query in ((2, 3, 4) if case == 'sat' else (2, 3)):
                        cnf, proof = Path(f'{prefix}-{query}.cnf'), Path(f'{prefix}-{query}.proof')
                        variables, clauses = formula(cnf)
                        sat = case == 'sat' and query != 2
                        assert satisfiable(variables, clauses) == sat
                        if sat:
                            check_model(Path(f'{prefix}-{query}.model'), variables, clauses)
                            models += 1
                        if mode == 'none':
                            assert not proof.exists()
                            continue
                        key = case, query, mode
                        text = proof.read_text()
                        assert text == proofs.setdefault(key, text), key
                        if sat:
                            continue
                        kind, route = check_unsat(runner, str(cnf), str(proof))
                        if kind == 'dsr':
                            work = directory/f'{case}-{query}-{mode}-{schedule}-audit'
                            audit_dsr(runner, cnf, proof, work)
                            audits += 1
                        # Remove the final assumption and, for the UNSAT base,
                        # the independent hard component. Both sources of
                        # inconsistency must be removed from the q2 control.
                        wrong_clauses = clauses[:-1] if case == 'sat' else clauses[:8]+clauses[12:-1]
                        assert satisfiable(variables, wrong_clauses)
                        wrong = directory/'sat-input.cnf'
                        wrong.write_text(f'p cnf {variables} {len(wrong_clauses)}\n'+
                                         ''.join(' '.join(map(str, c))+' 0\n' for c in wrong_clauses))
                        result = runner.check_proof(kind, str(wrong), str(proof), 30, route=route)
                        assert not result[0] and result[2] == runner.CHK_REJECTED, (key, result)
                        checked += 1
                        if case == 'unsat' and query == 3:
                            # Alter a restated XOR premise to a clause that the
                            # permanent input and current cube do not entail.
                            old = 'rup 1 ~x1 1 x2 1 x3 >= 1 ;\n' if kind == 'pbp' else '-1 2 3 0\n'
                            new = old.replace('~x1', 'x1') if kind == 'pbp' else '1 2 3 0\n'
                            assert old in text, (key, text)
                            corrupted = directory/'corrupted-xor-source.proof'
                            corrupted.write_text(text.replace(old, new, 1))
                            if kind == 'pbp':
                                result = runner.check_proof(kind, str(cnf), str(corrupted), 30, route=route)
                                assert not result[0] and result[2] == runner.CHK_REJECTED, result
                            else:
                                bad = directory/f'{case}-{query}-{mode}-{schedule}-bad'
                                bad.mkdir()
                                result = check_dsr(runner.DSR_TRIM, runner.DSR_COMPRESS, runner.SR_CHECK,
                                                   cnf, corrupted, bad, 30, every_lemma=True)
                                assert not result[0] and result[1] == 'every-lemma:dsr-trim', result
                            corruptions += 1
    print(f'native parity: {checked} canonical proofs/SAT-input rejects, {audits} DSR every-lemma audits, '
          f'{corruptions} corrupted-source rejects, {models} SAT models; native trajectories and schedule bytes agree')
    return 0


if __name__ == '__main__':
    sys.exit(main())
