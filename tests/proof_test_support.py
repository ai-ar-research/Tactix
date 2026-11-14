"""Independent formula oracles, the maintained certificate checker and the
shared driver of the proof-fixture tests in proof_fixtures.py."""
from collections import Counter
import hashlib
import importlib.util
import itertools
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace


def proof_runner(root, checkers=('VERIPB', 'CAKE_PB_CNF', 'DRAT_TRIM', 'CAKE_LPR',
                                'DSR_TRIM', 'DSR_COMPRESS', 'SR_CHECK')):
    sys.path.insert(0, str(root / 'tools/bench'))
    spec = importlib.util.spec_from_file_location('tactix_proof_test_runner', root / 'tools/bench/run.py')
    runner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runner)
    for name in checkers:
        checker = getattr(runner, name)
        if not Path(checker).is_file():
            print(f'missing checker: {checker}')
            sys.exit(77)
    return runner


def formula(path):
    clauses, clause = [], []
    variables = None
    for line in path.read_text().splitlines():
        if not line or line.startswith('c'):
            continue
        if line.startswith('p'):
            variables = int(line.split()[2])
            continue
        for literal in map(int, line.split()):
            if literal:
                clause.append(literal)
            else:
                clauses.append(clause)
                clause = []
    assert variables is not None and not clause
    return variables, clauses


def satisfiable(variables, clauses):
    return any(all(any(bool(mask & (1 << (abs(literal) - 1))) == (literal > 0)
                       for literal in clause) for clause in clauses)
               for mask in range(1 << variables))


def check_model(path, variables, clauses):
    literals = list(map(int, path.read_text().split()))
    assert literals and literals.pop() == 0 and len(literals) == variables
    model = set(literals)
    assert all((v in model) != (-v in model) for v in range(1, variables+1))
    assert all(any(literal in model for literal in clause) for clause in clauses)
    return model


def check_unsat(runner, cnf, proof, *, fmt=None, timeout=30):
    from proof_oracle import certificate_route
    content = certificate_route(proof)
    kind = 'pbp' if content == 'pbp' else 'dsr'
    assert fmt in (None, 'auto', kind), (fmt, content)
    route = {'pbp': 'pbp', 'drat': 'dsr-cake_lpr', 'dsr': 'dsr-trestle'}[content]
    checked = runner.check_proof(kind, str(cnf), str(proof), timeout, route=route)
    assert checked[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), (cnf, proof, checked)
    return kind, route


def audit_dsr(runner, cnf, proof, directory, timeout=30):
    from proof_oracle import certificate_route, check_dsr
    directory.mkdir()
    checked = check_dsr(runner.DSR_TRIM, runner.DSR_COMPRESS, runner.SR_CHECK,
                        cnf, proof, directory, timeout, every_lemma=True,
                        cake_lpr=runner.CAKE_LPR, witness_free=certificate_route(proof) == 'drat')
    assert checked[0], (cnf, proof, checked)


def remove_clause(cnf, destination, index=-1):
    lines = cnf.read_text().splitlines()
    header = lines[0].split()
    header[3] = str(int(header[3])-1)
    body = lines[1:]
    del body[index]
    destination.write_text(' '.join(header)+'\n'+'\n'.join(body)+'\n')
    assert satisfiable(*formula(destination)), destination


def require_forward_rejection(runner, cnf, proof, output):
    """Require independent rejection after an invalid-premise checker error."""
    from proof_oracle import proof_check_status
    done = subprocess.run([runner.DSR_TRIM, '-f', str(cnf), str(proof), str(output)],
                          capture_output=True, text=True, timeout=30)
    assert proof_check_status('dsr-trim', done.returncode, done.stdout, done.stderr) == 'rejected', (
        cnf, proof, done.returncode, done.stdout, done.stderr)


def write_cnf(path, variables, clauses):
    path.write_text(f'p cnf {variables} {len(clauses)}\n' +
                    ''.join(' '.join(map(str, clause)) + ' 0\n' for clause in clauses))
    return path


def check_v_model(path, variables, clauses, complete=True):
    """A `v ... 0` model: consistent, total when COMPLETE, satisfying CLAUSES."""
    words = path.read_text().split()
    assert words[0] == 'v' and words[-1] == '0', path
    literals = set(map(int, words[1:-1]))
    assert not any(-literal in literals for literal in literals), path
    values = {abs(literal): literal > 0 for literal in literals}
    assert not complete or len(values) == variables, (path, variables)
    assert all(any(values[abs(x)] == (x > 0) for x in clause) for clause in clauses), path
    return literals


def bundle(stem):
    """STEM.json after every file it hashes matches its recorded SHA-256."""
    meta = json.loads(Path(f'{stem}.json').read_text())
    for suffix, digest in meta['sha256'].items():
        assert hashlib.sha256(Path(f'{stem}{suffix}').read_bytes()).hexdigest() == digest, (stem, suffix)
    return meta


def proof_kind(proof):
    return 'pbp' if Path(proof).read_text().startswith('pseudo-Boolean') else 'dsr'


class Fixture:
    """Runs of one proof fixture and independent checks of what they write.

    Every run under one trace key must print the same trajectory and every
    proof under one proof key must have the same bytes: the proof never steers
    the search. Counts of the checks performed feed the closing report."""

    def __init__(self, root, label, checkers=None):
        self.runner = proof_runner(root, *([checkers] if checkers else []))
        self._scratch = tempfile.TemporaryDirectory(prefix=f'tactix-{label}-')
        self.dir = Path(self._scratch.name)
        self.traces, self.proofs, self.count = {}, {}, Counter()
        self._serial = itertools.count()

    def fresh(self, name):
        return self.dir/f'{next(self._serial)}-{name}'

    def run(self, key, *command, timeout=30, env=None):
        done = subprocess.run([str(part) for part in command], capture_output=True,
                              text=True, timeout=timeout, env=env)
        assert done.returncode == 0, (key, command, done.stdout, done.stderr)
        assert done.stdout == self.traces.setdefault(key, done.stdout), (
            key, command, self.traces[key], done.stdout)
        return done

    def query(self, stem, mode, sat, proof_key, *, fmt=None, audit=None, model=True, timeout=30):
        """STEM.cnf against the brute-force oracle, its model when SAT and, for a
        proof mode, proof-byte identity under PROOF_KEY and, when UNSAT, an
        independent refutation (see refute)."""
        q = SimpleNamespace(cnf=Path(f'{stem}.cnf'), proof=Path(f'{stem}.proof'),
                            kind=None, route=None, model=None)
        q.variables, q.clauses = formula(q.cnf)
        assert satisfiable(q.variables, q.clauses) == sat, stem
        if sat and model:
            q.model = check_model(Path(f'{stem}.model'), q.variables, q.clauses)
            self.count['SAT models'] += 1
        if mode == 'none':
            assert not q.proof.exists(), stem
            return q
        data = q.proof.read_bytes()
        assert data == self.proofs.setdefault(proof_key, data), proof_key
        if not sat:
            q.kind, q.route = self.refute(q.cnf, q.proof, fmt=fmt, audit=audit, timeout=timeout)
        return q

    def refute(self, cnf, proof, *, fmt=None, audit=None, timeout=30):
        """The maintained checker verifies PROOF for CNF (FMT 'header' takes the
        format from the proof); AUDIT 'dsr' or 'trestle' adds an every-lemma DSR
        audit for any DSR proof or for the dsr-trestle route only."""
        kind, route = check_unsat(self.runner, cnf, proof,
                                  fmt=proof_kind(proof) if fmt == 'header' else fmt, timeout=timeout)
        self.count['refutations'] += 1
        if audit == 'dsr' and kind == 'dsr' or audit == 'trestle' and route == 'dsr-trestle':
            audit_dsr(self.runner, cnf, proof, self.fresh('audit'), timeout)
            self.count['every-lemma audits'] += 1
        return kind, route

    def reject(self, kind, route, cnf, proof, *, errors=(), timeout=30):
        """The checker must not accept PROOF for the satisfiable CNF: a clean
        rejection, or a DSR checker error naming one of ERRORS. A backward
        `not in watch pointers` error also needs an independent forward reject."""
        result = self.runner.check_proof(kind, str(cnf), str(proof), timeout, route=route)
        assert not result[0], (cnf, proof, result)
        if result[2] == self.runner.CHK_REJECTED:
            self.count['negative controls'] += 1
            return
        assert kind == 'dsr' and result[2] == self.runner.CHK_ERROR and any(
            message in result[1] for message in errors), (cnf, proof, result)
        self.count['invalid-source checker errors'] += 1
        if 'not in watch pointers' in result[1]:
            require_forward_rejection(self.runner, cnf, proof, self.fresh('forward.lsr'))
            self.count['forward rejects'] += 1

    def reject_without(self, cnf, proof, kind, route, index=-1):
        """PROOF must not refute CNF without its clause INDEX (then SAT)."""
        wrong = self.fresh('missing.cnf')
        remove_clause(cnf, wrong, index)
        self.reject(kind, route, wrong, proof)

    def sat_control(self, variables, clauses):
        wrong = write_cnf(self.fresh('sat.cnf'), variables, clauses)
        assert satisfiable(variables, clauses), wrong
        return wrong

    def reject_every_lemma(self, cnf, proof):
        """The every-lemma DSR audit rejects PROOF at dsr-trim."""
        from proof_oracle import check_dsr
        work = self.fresh('bad')
        work.mkdir()
        runner = self.runner
        result = check_dsr(runner.DSR_TRIM, runner.DSR_COMPRESS, runner.SR_CHECK,
                           cnf, proof, work, 30, every_lemma=True)
        assert not result[0] and result[1] == 'every-lemma:dsr-trim', result
        self.count['corrupted-proof rejects'] += 1

    def report(self, label, note):
        counts = ', '.join(f'{n} {what}' for what, n in sorted(self.count.items()))
        print(f'{label}: {counts}; {note}')


def drive(specs):
    """`python3 proof_fixtures.py NAME FIXTURE ROOT [EXTRA...]` runs specs[NAME]."""
    name, fixture, root, *extra = sys.argv[1:]
    specs[name](Path(fixture), Path(root), *extra)
    return 0
