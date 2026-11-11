"""Optional native weakening capture must preserve search and proof bytes."""
from proof_test_support import proof_runner
from pathlib import Path
import subprocess
import sys
import tempfile


def clauses(path):
    words = []
    for line in path.read_text().splitlines():
        if line and line[0] not in 'cp':
            words.extend(map(int, line.split()))
    result, current = [], []
    for literal in words:
        if literal:
            current.append(literal)
        else:
            result.append(current)
            current = []
    assert not current
    return result


def main():
    fixture, root = map(Path, sys.argv[1:])
    runner = proof_runner(root)
    from proof_oracle import certificate_route, check_dsr

    checked = models = 0
    with tempfile.TemporaryDirectory(prefix='tactix-weakening-') as tmp:
        directory = Path(tmp)
        for case in ('sat', 'unsat'):
            trace = None
            for mode in ('none', 'pbp', 'dsr'):
                control_bytes = None
                for capture in ('off', 'capture'):
                    prefix = directory/f'{case}-{mode}-{capture}'
                    done = subprocess.run([fixture, prefix, mode, capture, case],
                                          capture_output=True, text=True, timeout=30)
                    assert done.returncode == 0, done.stdout+done.stderr
                    if trace is None:
                        trace = done.stdout
                    assert trace == done.stdout, (case, mode, capture, trace, done.stdout)
                    cnf, proof = Path(f'{prefix}.cnf'), Path(f'{prefix}.proof')
                    formula = clauses(cnf)
                    satisfiable = any(all(any(bool(mask & (1 << (abs(lit)-1))) == (lit > 0)
                                                  for lit in clause) for clause in formula)
                                      for mask in range(1 << 7))
                    assert satisfiable == (case == 'sat')
                    if case == 'sat':
                        model = set(map(int, Path(f'{prefix}.model').read_text().split()))
                        assert all((v in model) != (-v in model) for v in range(1, 8))
                        assert all(any(lit in model for lit in clause) for clause in formula)
                        models += 1
                    if mode == 'none':
                        assert not proof.exists()
                        continue
                    data = proof.read_bytes()
                    if control_bytes is None:
                        control_bytes = data
                    assert data == control_bytes, (case, mode, 'capture changed proof bytes')
                    if case == 'sat':
                        continue
                    route = 'pbp' if mode == 'pbp' else (
                        'dsr-cake_lpr' if certificate_route(proof) == 'drat' else 'dsr-trestle')
                    result = runner.check_proof(mode, str(cnf), str(proof), 30, route=route)
                    assert result[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), result
                    if route == 'dsr-trestle':
                        work = directory/f'audit-{capture}'
                        work.mkdir()
                        audit = check_dsr(runner.DSR_TRIM, runner.DSR_COMPRESS, runner.SR_CHECK,
                                          cnf, proof, work, 30, every_lemma=True)
                        assert audit[0], audit
                    lines = cnf.read_text().splitlines()
                    header = lines[0].split()
                    header[3] = str(int(header[3])-1)
                    wrong = directory/'missing-core-clause.cnf'
                    wrong.write_text(' '.join(header)+'\n'+'\n'.join(lines[1:-1])+'\n')
                    result = runner.check_proof(mode, str(wrong), str(proof), 30, route=route)
                    assert not result[0] and result[2] == runner.CHK_REJECTED, result
                    checked += 1
    print(f'weakening capture: {checked} checked proofs/negative controls, {models} checked models; search and proof bytes agree')
    return 0


if __name__ == '__main__':
    sys.exit(main())
