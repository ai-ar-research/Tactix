"""Exact-query certificates/models for same-query repair and prior results."""
from proof_test_support import audit_dsr, check_model, check_unsat, formula, proof_runner, require_forward_rejection, satisfiable
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    fixture, root = map(Path, sys.argv[1:])
    runner = proof_runner(root)

    traces, proofs = {}, {}
    canonical = models = audits = rejects = source_errors = forward_rejects = 0
    cases = [(name, outcome) for name in ('bve', 'condition', 'sr', 'rat', 'binding', 'alias-binding')
             for outcome in ('sat', 'unsat')]
    cases += [(f'from-{origin}', kind) for origin in ('sat', 'unsat', 'unknown')
              for kind in ('assumption', 'append')]
    with tempfile.TemporaryDirectory(prefix='tactix-query-repair-') as tmp:
        directory = Path(tmp)
        for name, outcome in cases:
            transition = name.startswith('from-')
            for mode in ('none', 'pbp', 'dsr', 'auto'):
                for schedule in ('direct', 'registry', 'default'):
                    prefix = directory/f'{name}-{outcome}-{mode}-{schedule}'
                    done = subprocess.run([fixture, name, outcome, prefix, mode, schedule],
                                          capture_output=True, text=True, timeout=45)
                    assert done.returncode == 0, (name, outcome, mode, schedule, done.stdout, done.stderr)
                    key = name, outcome, schedule
                    assert done.stdout == traces.setdefault(key, done.stdout), (key, mode, done.stdout)
                    for query in (range(1, 5) if transition else (None,)):
                        stem = f'{prefix}-{query}' if transition else str(prefix)
                        cnf, proof = Path(stem+'.cnf'), Path(stem+'.proof')
                        variables, clauses = formula(cnf)
                        unknown = transition and query == 1 and name == 'from-unknown'
                        sat = (query != 3 and not (query == 1 and name == 'from-unsat')
                               if transition else outcome == 'sat')
                        assert satisfiable(variables, clauses) == sat, (key, query)
                        if sat and not unknown:
                            model = check_model(Path(stem + '.model'), variables, clauses)
                            if transition and query == 2:
                                assert -3 in model
                            models += 1
                        if mode == 'none':
                            assert not proof.exists()
                            continue
                        text = proof.read_text()
                        # DefaultPolicy is a separate scheduler. Direct/native
                        # and registry schedules must emit the same bytes.
                        schedule_key = 'native' if schedule != 'default' else schedule
                        proof_key = name, outcome, mode, query, schedule_key
                        assert text == proofs.setdefault(proof_key, text), proof_key
                        if sat or unknown:
                            continue
                        kind, route = check_unsat(runner, str(cnf), str(proof), timeout=45)
                        canonical += 1
                        if kind == 'dsr':
                            work = directory/f'{name}-{outcome}-{mode}-{schedule}-{query}-audit'
                            audit_dsr(runner, cnf, proof, work, timeout=45)
                            audits += 1
                        missing = ([5], [-5, 6, 7]) if not transition else (
                            ([8], [-8, 9, 10]) if query == 1 else ([1], [-1, 3]))
                        for premise in missing:
                            wrong_clauses = list(clauses)
                            # Preserve input slots/IDs so the control isolates
                            # the missing logical premise, not shifted names.
                            wrong_clauses[wrong_clauses.index(premise)] = [1, -1]
                            assert satisfiable(variables, wrong_clauses), (proof_key, premise)
                            wrong = directory/'missing-premise.cnf'
                            wrong.write_text(f'p cnf {variables} {len(wrong_clauses)}\n'+
                                             ''.join(' '.join(map(str, c))+' 0\n' for c in wrong_clauses))
                            result = runner.check_proof(kind, str(wrong), str(proof), 45, route=route)
                            assert not result[0], (proof_key, premise, result)
                            if result[2] == runner.CHK_REJECTED:
                                rejects += 1
                            else:
                                # An imported original clause can immediately
                                # contradict this independently SAT control.
                                # dsr-trim labels that malformed reduction a
                                # fatal error. Preserve its error status; it is
                                # neither verification nor a clean rejection.
                                assert premise == missing[1] and kind == 'dsr', (proof_key, premise, result)
                                assert result[2] == runner.CHK_ERROR and any(
                                    message in result[1] for message in
                                    ('claims contradiction', 'No matching clause found for deletion.', 'not in watch pointers')), (proof_key, premise, result)
                                if 'not in watch pointers' in result[1]:
                                    require_forward_rejection(runner, wrong, proof, directory/'negative-forward.lsr')
                                    forward_rejects += 1
                                source_errors += 1
    print(f'query repair: {canonical} canonical proofs, {audits} DSR every-lemma audits, '
          f'{rejects} missing-premise rejects, {source_errors} invalid-source checker errors ({forward_rejects} independently rejected forward), '
          f'{models} SAT models; same-query identity, '
          'tickets, frozen prior proofs and per-schedule mode counters agree')
    return 0


if __name__ == '__main__':
    sys.exit(main())
