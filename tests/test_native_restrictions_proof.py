"""Exact query proofs/models for restriction-dependent native continuation."""
from proof_test_support import formula, proof_runner, satisfiable
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def main():
    fixture, root = map(Path, sys.argv[1:])
    runner = proof_runner(root)
    from proof_oracle import certificate_route, check_dsr

    traces, proofs = {}, {}
    checked = models = audits = corruptions = 0
    with tempfile.TemporaryDirectory(prefix='tactix-native-restrictions-') as tmp:
        directory = Path(tmp)
        for case in ('sr-sat', 'sr-unsat', 'rat-sat', 'rat-unsat', 'assumption', 'append'):
            recovery = case in ('assumption', 'append')
            for mode in ('none', 'pbp', 'dsr', 'auto'):
                for schedule in ('direct', 'dynamic'):
                    prefix = directory/f'{case}-{mode}-{schedule}'
                    done = subprocess.run([fixture, case, prefix, mode, schedule],
                                          capture_output=True, text=True, timeout=30)
                    assert done.returncode == 0, (case, mode, schedule, done.stdout, done.stderr)
                    assert done.stdout == traces.setdefault(case, done.stdout), (case, mode, schedule, traces[case], done.stdout)
                    queries = (None,) if recovery else ((2,) if case.endswith('unsat') else (2, 3, 4))
                    for query in queries:
                        stem = str(prefix) if recovery else f'{prefix}-{query}'
                        cnf, proof = Path(stem+'.cnf'), Path(stem+'.proof')
                        variables, clauses = formula(cnf)
                        sat = recovery or case.endswith('-sat') and query != 3
                        assert satisfiable(variables, clauses) == sat, (case, query)
                        if sat:
                            model = set(map(int, Path(stem+'.model').read_text().split()))
                            assert all((v in model) != (-v in model) for v in range(1, variables+1))
                            assert all(any(lit in model for lit in clause) for clause in clauses)
                            models += 1
                        if mode == 'none':
                            assert not proof.exists()
                            continue
                        key = case, query, mode
                        text = proof.read_text()
                        assert text == proofs.setdefault(key, text), key
                        if sat:
                            continue
                        kind = 'pbp' if text.startswith('pseudo-Boolean') else 'dsr'
                        free = kind == 'dsr' and certificate_route(proof) == 'drat'
                        if case.startswith('rat-') and kind == 'dsr':
                            assert free, (case, mode, query,
                                          'witness-free RAT replay unexpectedly needs SR')
                        route = 'pbp' if kind == 'pbp' else ('dsr-cake_lpr' if free else 'dsr-trestle')
                        result = runner.check_proof(kind, str(cnf), str(proof), 30, route=route)
                        assert result[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), (key, result)
                        if kind == 'dsr':
                            work = directory/f'{case}-{query}-{mode}-{schedule}-audit'
                            work.mkdir()
                            result = check_dsr(runner.DSR_TRIM, runner.DSR_COMPRESS, runner.SR_CHECK,
                                               cnf, proof, work, 30, every_lemma=True,
                                               cake_lpr=runner.CAKE_LPR, witness_free=free)
                            assert result[0], (key, result)
                            audits += 1
                        wrong_clauses = ([c for c in clauses if set(map(abs, c)) != {9, 10}]
                                         if case.endswith('unsat') else clauses[:-1])
                        assert satisfiable(variables, wrong_clauses), key
                        wrong = directory/'sat-input.cnf'
                        wrong.write_text(f'p cnf {variables} {len(wrong_clauses)}\n'+
                                         ''.join(' '.join(map(str, c))+' 0\n' for c in wrong_clauses))
                        result = runner.check_proof(kind, str(wrong), str(proof), 30, route=route)
                        assert not result[0] and result[2] == runner.CHK_REJECTED, (key, result)
                        checked += 1
                        if case == 'sr-unsat':
                            if kind == 'pbp':
                                match = re.search(r'^red 1 x(\d+) >= 1 : x\1 -> 1 x(\d+) -> 0 x(\d+) -> 1 ;$', text, re.M)
                                assert match
                                old = match[0]
                                changed = text.replace(old, old.replace(f'x{match[3]} -> 1', f'x{match[3]} -> 0'), 1)
                            else:
                                lines = text.splitlines(keepends=True)
                                candidates = []
                                for i, line in enumerate(lines):
                                    if line.startswith('d'):
                                        continue
                                    fields = list(map(int, line.split()))
                                    if (len(fields) >= 8 and fields[-4] > 0 and fields[-3] == -(fields[-4]+1)
                                            and fields[-2] == fields[-4]+2 and fields[2] == fields[-4]):
                                        candidates.append((i, fields))
                                assert len(candidates) >= 2
                                for i, fields in candidates[:2]:
                                    fields[-2] *= -1
                                    lines[i] = ' '.join(map(str, fields))+'\n'
                                changed = ''.join(lines)
                            corrupted = directory/'corrupted-restriction-witness.proof'
                            corrupted.write_text(changed)
                            if kind == 'pbp':
                                result = runner.check_proof(kind, str(cnf), str(corrupted), 30, route=route)
                                assert not result[0] and result[2] == runner.CHK_REJECTED, result
                            else:
                                bad = directory/f'{mode}-{schedule}-bad'
                                bad.mkdir()
                                result = check_dsr(runner.DSR_TRIM, runner.DSR_COMPRESS, runner.SR_CHECK,
                                                   cnf, corrupted, bad, 30, every_lemma=True)
                                assert not result[0] and result[1] == 'every-lemma:dsr-trim', result
                            corruptions += 1
    print(f'native restrictions: {checked} canonical proofs/SAT-input rejects, {audits} DSR every-lemma audits, '
          f'{corruptions} corrupted-witness rejects, {models} SAT models; native trajectories and schedule bytes agree')
    return 0


if __name__ == '__main__':
    sys.exit(main())
