"""Independently check failed cores, their certificates and mode parity."""
from proof_test_support import audit_dsr, check_unsat, formula, proof_runner, satisfiable
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    fixture, root = map(Path, sys.argv[1:])
    runner = proof_runner(root)

    traces = {}
    checked = audits = rejects = source_errors = cores = 0
    names = ('binary', 'large', 'root', 'clash', 'alias', 'negative-alias',
             'learned', 'base', 'restriction', 'quota', 'disabled')
    with tempfile.TemporaryDirectory(prefix='tactix-query-results-') as tmp:
        directory = Path(tmp)
        for name in names:
            for mode in ('none', 'pbp', 'dsr', 'auto'):
                for schedule in ('direct', 'registry', 'default'):
                    prefix = directory/f'{name}-{mode}-{schedule}'
                    done = subprocess.run([fixture, name, prefix, mode, schedule],
                                          capture_output=True, text=True, timeout=45)
                    key = name, schedule
                    assert done.returncode == 0, (key, mode, done.stdout, done.stderr)
                    assert done.stdout == traces.setdefault(key, done.stdout), (key, mode, done.stdout)
                    variables, clauses = formula(Path(str(prefix)+'.cnf'))
                    assert not satisfiable(variables, clauses), key
                    # Exact raw assumptions follow each fixture's permanent F.
                    count = {'clash': 0, 'base': 2, 'learned': 4,
                             'large': 1, 'root': 1, 'restriction': 1}.get(name, 2)
                    permanent = clauses[:count]
                    assumptions = [c[0] for c in clauses[count:]]
                    core = list(map(int, Path(str(prefix)+'.core').read_text().split()))
                    assert core.pop() == 0 and len(core) == len(set(core)), key
                    assert set(core) <= set(assumptions), (key, core, assumptions)
                    assert not satisfiable(variables, permanent+[[lit] for lit in core]), (key, core)
                    cores += 1
                    if name == 'restriction':
                        # Root SR may suggest {-1}, but that cube is SAT in F.
                        assert satisfiable(variables, permanent+[[-1]])
                        assert set(core) == set(assumptions)
                    reduced = Path(str(prefix)+'-core.cnf')
                    if mode == 'none':
                        assert not Path(str(prefix)+'.proof').exists()
                        continue
                    stems = [prefix] + ([Path(str(prefix)+'-core')] if reduced.exists() else [])
                    for stem in stems:
                        cnf, proof = Path(str(stem)+'.cnf'), Path(str(stem)+'.proof')
                        kind, route = check_unsat(runner, str(cnf), str(proof), timeout=45)
                        checked += 1
                        if kind == 'dsr':
                            work = directory/(stem.name+'-audit')
                            audit_dsr(runner, cnf, proof, work, timeout=45)
                            audits += 1
                        # A certificate cannot refute this independently SAT input.
                        wrong = directory/'satisfiable-control.cnf'
                        v, cs = formula(cnf)
                        wrong.write_text(f'p cnf {v} {len(cs)}\n'+
                                         ''.join('1 -1 0\n' for _ in cs))
                        result = runner.check_proof(kind, str(wrong), str(proof), 45, route=route)
                        assert not result[0], (stem, result)
                        if result[2] == runner.CHK_REJECTED:
                            rejects += 1
                        else:
                            # Replacing original inputs also invalidates named
                            # deletions; preserve this diagnostic as an error,
                            # separately from clean rejection and verification.
                            assert kind == 'dsr' and result[2] == runner.CHK_ERROR, (stem, result)
                            assert 'No matching clause found for deletion.' in result[1], (stem, result)
                            source_errors += 1
    print(f'query results: {cores} independently UNSAT cores, {checked} canonical proofs, '
          f'{audits} DSR every-lemma audits, {rejects} SAT-input rejects, '
          f'{source_errors} invalid-source checker errors; '
          'original assumption coordinates and per-schedule proof-mode counters agree')
    return 0


if __name__ == '__main__':
    sys.exit(main())
