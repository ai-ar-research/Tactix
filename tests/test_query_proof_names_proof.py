"""Check copied sources and late variable names after DSR helper allocation."""
from proof_test_support import audit_dsr, formula, proof_runner, satisfiable
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    fixture, root = map(Path, sys.argv[1:])
    runner = proof_runner(root)

    with tempfile.TemporaryDirectory(prefix='tactix-query-proof-names-') as tmp:
        unlicensed = subprocess.run([fixture, Path(tmp)/'unlicensed', 'unlicensed'],
                                    capture_output=True, text=True, timeout=30)
        assert unlicensed.returncode == 1 and 'native clause has no checked incremental certificate source' in unlicensed.stderr, unlicensed
        for mode in ('pbp', 'dsr', 'auto', 'staged'):
            prefix = Path(tmp)/mode
            done = subprocess.run([fixture, prefix, mode], capture_output=True, text=True, timeout=30)
            assert done.returncode == 0, (mode, done.stdout, done.stderr)
            cnf, proof = Path(f'{prefix}.cnf'), Path(f'{prefix}.proof')
            if mode == 'staged':
                lines = proof.read_text().splitlines()
                assert 'd 3 4 0' in lines and 'd 5 6 0' not in lines, lines
            kind = 'pbp' if proof.read_text().startswith('pseudo-Boolean') else 'dsr'
            result = runner.check_proof(kind, str(cnf), str(proof), 30,
                                        route='pbp' if kind == 'pbp' else 'dsr-trestle')
            assert result[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), (mode, result)
            if kind == 'dsr':
                work = Path(tmp)/f'{mode}-forward'
                audit_dsr(runner, cnf, proof, work)
        for mode in ('pbp', 'dsr'):
            sizes = []
            for variables in (64, 128):
                prefix = Path(tmp)/f'scale-{mode}-{variables}'
                subprocess.run([fixture, prefix, mode, str(variables)], check=True, timeout=30)
                cnf, proof = Path(f'{prefix}.cnf'), Path(f'{prefix}.proof')
                result = runner.check_proof(mode, str(cnf), str(proof), 30,
                                            route='pbp' if mode == 'pbp' else 'dsr-cake_lpr')
                assert result[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), result
                if mode == 'dsr':
                    work = Path(tmp)/f'scale-forward-{variables}'
                    audit_dsr(runner, cnf, proof, work)
                sizes.append(proof.stat().st_size)
                # Binding a valid stream to a satisfiable source must fail.
                wrong = prefix.with_suffix('.wrong.cnf')
                wrong.write_text(cnf.read_text().replace('-1 -2 0', '-1 2 0'))
                assert satisfiable(*formula(wrong))
                rejected = runner.check_proof(mode, str(wrong), str(proof), 30,
                                              route='pbp' if mode == 'pbp' else 'dsr-cake_lpr')
                assert not rejected[0] and rejected[2] == runner.CHK_REJECTED, rejected
            assert sizes[1] < 3*sizes[0], (mode, sizes)
            print(f'query copy scaling {mode}: {sizes} bytes for 64/128 variables')
    print('query proof names: PBP/canonical/forward DSR verify; all DSR helper events and physical names match bytes')
    return 0


if __name__ == '__main__':
    sys.exit(main())
