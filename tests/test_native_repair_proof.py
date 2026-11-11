"""Native repair groups: capture transparency and exact repeated-query proofs."""
from proof_test_support import formula, proof_runner, satisfiable
from pathlib import Path
import subprocess
import sys
import tempfile

CASES = ('condition', 'cce', 'binary', 'ata', 'substitute')
ANSWERS = (10, 20, 20, 10)


def main():
    fixture, root = map(Path, sys.argv[1:])
    runner = proof_runner(root)
    from proof_oracle import certificate_route, check_dsr

    proofs, traces, initial = {}, {}, {}
    checked = models = 0
    with tempfile.TemporaryDirectory(prefix='tactix-native-repair-') as tmp:
        directory = Path(tmp)
        for mode in ('none', 'pbp', 'dsr'):
            for schedule in ('direct', 'dynamic'):
                for capture in ('off', 'capture'):
                    for case in CASES:
                        prefix = directory/f'{case}-{mode}-{schedule}-{capture}'
                        done = subprocess.run([fixture, case, prefix, mode, capture, schedule],
                                              capture_output=True, text=True, timeout=30)
                        context = case, mode, schedule, capture
                        assert done.returncode == 0, (context, done.stdout, done.stderr)
                        first = done.stdout.splitlines()[0]
                        assert first == initial.setdefault(case, first), (context, first, initial[case])
                        if capture == 'capture':
                            assert done.stdout == traces.setdefault(case, done.stdout), (context, done.stdout, traces[case])
                        answers = ANSWERS if capture == 'capture' else ANSWERS[:1]
                        for query, answer in enumerate(answers, 1):
                            cnf = Path(f'{prefix}-{query}.cnf')
                            proof = Path(f'{prefix}-{query}.proof')
                            variables, clauses = formula(cnf)
                            assert satisfiable(variables, clauses) == (answer == 10), (context, query)
                            if answer == 10:
                                model = set(map(int, Path(f'{prefix}-{query}.model').read_text().split()))
                                assert all((v in model) != (-v in model) for v in range(1, variables+1))
                                assert all(any(lit in model for lit in clause) for clause in clauses)
                                models += 1
                            if mode == 'none':
                                assert not proof.exists()
                                continue
                            key = case, query, mode
                            assert proof.read_bytes() == proofs.setdefault(key, proof.read_bytes()), (context, query)
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
                            # A prior query's certificate cannot certify a different
                            # assumption cube. Establish SAT independently first.
                            lines = cnf.read_text().splitlines()
                            header = lines[0].split()
                            header[3] = str(int(header[3])-1)
                            wrong = directory/'missing-assumption.cnf'
                            wrong.write_text(' '.join(header)+'\n'+'\n'.join(lines[1:-1])+'\n')
                            _, constraints = formula(wrong)
                            assert satisfiable(variables, constraints), (key, constraints)
                            result = runner.check_proof(mode, str(wrong), str(proof), 30, route=route)
                            assert not result[0] and result[2] == runner.CHK_REJECTED, (key, result)
                            checked += 1
    print(f'native repair: {checked} checked refutations/negative controls and {models} checked SAT models; capture transparency, trajectories and proof bytes agree')
    return 0


if __name__ == '__main__':
    sys.exit(main())
