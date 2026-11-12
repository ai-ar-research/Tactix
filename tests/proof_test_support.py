"""Independent formula oracles and the maintained certificate checker for tests."""
import importlib.util
from pathlib import Path
import subprocess
import sys


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
