"""Check query projection bridges and restrictions against the exact input."""
from proof_test_support import proof_runner
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    fixture, root = map(Path, sys.argv[1:3])
    default_fixture = Path(sys.argv[3]) if len(sys.argv) > 3 else None
    runner = proof_runner(root)
    from proof_oracle import certificate_route, check_dsr

    with tempfile.TemporaryDirectory(prefix='tactix-query-projection-') as raw:
        directory = Path(raw)
        scenarios = [(name, schedule) for name in ('bve', 'substitute', 'sr', 'ulc', 'opposite')
                     for schedule in ('direct', 'registry')]
        if default_fixture: scenarios.append(('default', 'production'))
        checked_proofs = 0
        for scenario, schedule in scenarios:
            trace = None
            for mode in ('none', 'pbp', 'dsr'):
                prefix = directory/f'{scenario}-{schedule}-{mode}'
                command = ([default_fixture, prefix, mode] if scenario == 'default'
                           else [fixture, prefix, mode, scenario, schedule])
                done = subprocess.run(command,
                                      text=True, capture_output=True, timeout=30)
                assert done.returncode == 0, done.stdout+done.stderr
                if trace is None: trace = done.stdout
                assert done.stdout == trace, (scenario, mode, trace, done.stdout)
                if mode == 'none': continue
                cnf, proof = Path(f'{prefix}.cnf'), Path(f'{prefix}.proof')
                route = 'pbp' if mode == 'pbp' else ('dsr-cake_lpr'
                    if certificate_route(proof) == 'drat' else 'dsr-trestle')
                checked = runner.check_proof(mode, str(cnf), str(proof), 30, route=route)
                assert checked[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), checked
                if route == 'dsr-trestle':
                    work = directory/f'audit-{scenario}-{schedule}'
                    work.mkdir()
                    audit = check_dsr(runner.DSR_TRIM, runner.DSR_COMPRESS,
                                      runner.SR_CHECK, cnf, proof, work, 30,
                                      every_lemma=True)
                    assert audit[0], audit
                lines = cnf.read_text().splitlines()
                header = lines[0].split()
                # Remove the guard assumption; ULC additionally has an
                # unrelated assumption that must remain in the control.
                remove = -2 if scenario == 'ulc' else -1
                header[3] = str(int(header[3])-1)
                wrong = directory/'without-assumption.cnf'
                body = lines[1:]
                del body[remove]
                wrong.write_text(' '.join(header)+'\n'+'\n'.join(body)+'\n')
                rejected = runner.check_proof(mode, str(wrong), str(proof), 30, route=route)
                assert not rejected[0] and rejected[2] == runner.CHK_REJECTED, rejected
                checked_proofs += 1
    print(f'query projection: {checked_proofs} proofs and negative controls pass; proof modes agree')
    return 0


if __name__ == '__main__':
    sys.exit(main())
