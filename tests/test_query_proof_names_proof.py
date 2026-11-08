"""Check copied sources and late variable names after DSR helper allocation."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    fixture, root = map(Path, sys.argv[1:])
    sys.path.insert(0, str(root/'tools/bench'))
    spec = importlib.util.spec_from_file_location('query_names_runner', root/'tools/bench/run.py')
    runner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runner)
    from proof_oracle import check_dsr
    for tool in (runner.VERIPB, runner.CAKE_PB_CNF, runner.DSR_TRIM,
                 runner.DSR_COMPRESS, runner.SR_CHECK):
        if not Path(tool).is_file():
            print(f'query proof names: missing checker {tool}')
            return 77
    with tempfile.TemporaryDirectory(prefix='tactix-query-proof-names-') as tmp:
        unlicensed = subprocess.run([fixture, Path(tmp)/'unlicensed', 'unlicensed'],
                                    capture_output=True, text=True, timeout=30)
        assert unlicensed.returncode == 1 and 'native clause has no checked incremental certificate source' in unlicensed.stderr, unlicensed
        for mode in ('pbp', 'dsr', 'auto'):
            prefix = Path(tmp)/mode
            done = subprocess.run([fixture, prefix, mode], capture_output=True, text=True, timeout=30)
            assert done.returncode == 0, (mode, done.stdout, done.stderr)
            cnf, proof = Path(f'{prefix}.cnf'), Path(f'{prefix}.proof')
            kind = 'pbp' if proof.read_text().startswith('pseudo-Boolean') else 'dsr'
            result = runner.check_proof(kind, str(cnf), str(proof), 30,
                                        route='pbp' if kind == 'pbp' else 'dsr-trestle')
            assert result[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), (mode, result)
            if kind == 'dsr':
                work = Path(tmp)/f'{mode}-forward'
                work.mkdir()
                result = check_dsr(runner.DSR_TRIM, runner.DSR_COMPRESS, runner.SR_CHECK,
                                   cnf, proof, work, 30, every_lemma=True)
                assert result[0], result
    print('query proof names: PBP/canonical/forward DSR verify; all DSR helper events and physical names match bytes')
    return 0


if __name__ == '__main__':
    sys.exit(main())
