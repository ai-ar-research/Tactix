"""Check query projection bridges and restrictions against the exact input."""
from proof_test_support import audit_dsr, check_unsat, proof_runner, remove_clause
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    fixture, root = map(Path, sys.argv[1:3])
    default_fixture = Path(sys.argv[3]) if len(sys.argv) > 3 else None
    runner = proof_runner(root)

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
                _, route = check_unsat(runner, str(cnf), str(proof), fmt=mode)
                if route == 'dsr-trestle':
                    work = directory/f'audit-{scenario}-{schedule}'
                    audit_dsr(runner, cnf, proof, work)
                wrong = directory/'without-assumption.cnf'
                # ULC's unrelated final assumption stays in the control.
                remove_clause(cnf, wrong, -2 if scenario == 'ulc' else -1)
                rejected = runner.check_proof(mode, str(wrong), str(proof), 30, route=route)
                assert not rejected[0] and rejected[2] == runner.CHK_REJECTED, rejected
                checked_proofs += 1
    print(f'query projection: {checked_proofs} proofs and negative controls pass; proof modes agree')
    return 0


if __name__ == '__main__':
    sys.exit(main())
