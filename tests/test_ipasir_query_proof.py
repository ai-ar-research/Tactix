"""Independent original-query checks for a client using only the C ABI."""
import hashlib
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
from test_native_query_proof import formula
from test_native_query_append_proof import satisfiable


def main():
    fixture, root = map(Path, sys.argv[1:])
    sys.path.insert(0, str(root/'tools/bench'))
    spec = importlib.util.spec_from_file_location('ipasir_runner', root/'tools/bench/run.py')
    runner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runner)
    from proof_oracle import certificate_route
    import json
    for tool in (runner.VERIPB, runner.CAKE_PB_CNF, runner.DRAT_TRIM,
                 runner.CAKE_LPR, runner.DSR_TRIM, runner.DSR_COMPRESS, runner.SR_CHECK):
        if not Path(tool).is_file():
            print(f'IPASIR query: missing checker {tool}')
            return 77
    queries = {1: [[1, 2], [-1]], 2: [[1, 2], [-2]],
               3: [[1, 2], [7], [-1], [-2]], 4: [[1, 2]],
               5: [[1, 2], [3], [-3]], 6: [[1, 2], [3]],
               7: [[1, 2], [3], []], 8: [[9], [-9]], 9: []}
    proofs = models = rejects = source_errors = 0
    with tempfile.TemporaryDirectory(prefix='tactix-ipasir-c-client-') as tmp:
        prefix = Path(tmp)/'query'
        done = subprocess.run([fixture, prefix], capture_output=True, text=True, timeout=45)
        assert done.returncode == 0, (done.stdout, done.stderr)
        for number, expected in queries.items():
            stem = Path(str(prefix)+f'-{number}')
            cnf, proof = Path(str(stem)+'.cnf'), Path(str(stem)+'.proof')
            variables, clauses = formula(cnf)
            assert clauses == expected, (number, clauses)
            meta = json.loads(Path(str(stem)+'.json').read_text())
            assert meta['query'] == (number if number < 8 else number-7)
            assert meta['generation'] > 0
            for suffix, digest in meta['sha256'].items():
                assert hashlib.sha256(Path(str(stem)+suffix).read_bytes()).hexdigest() == digest
            sat = satisfiable(variables, clauses)
            assert meta['result'] == (10 if sat else 20)
            if sat:
                model = {int(x) for x in Path(str(stem)+'.model').read_text().split() if x not in ('v', '0')}
                assert all((v in model) != (-v in model) for v in range(1, variables+1))
                assert all(any(lit in model for lit in clause) for clause in clauses)
                models += 1
                continue
            core = list(map(int, Path(str(stem)+'.core').read_text().split()))
            assert core.pop() == 0
            assert core == {3: [7, -1, -2], 5: [-3], 7: [], 8: [9, -9]}[number]
            kind = 'pbp' if proof.read_text().startswith('pseudo-Boolean') else 'dsr'
            free = kind == 'dsr' and certificate_route(proof) == 'drat'
            route = 'pbp' if kind == 'pbp' else 'dsr-cake_lpr' if free else 'dsr-trestle'
            result = runner.check_proof(kind, str(cnf), str(proof), 45, route=route)
            assert result[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), (number, result)
            proofs += 1
            wrong = Path(tmp)/'satisfiable-control.cnf'
            wrong.write_text(f'p cnf {variables} {len(clauses)}\n'+''.join('1 -1 0\n' for _ in clauses))
            result = runner.check_proof(kind, str(wrong), str(proof), 45, route=route)
            assert not result[0], (number, result)
            if result[2] == runner.CHK_REJECTED:
                rejects += 1
            else:
                assert kind == 'dsr' and result[2] == runner.CHK_ERROR, (number, result)
                assert 'No matching clause found for deletion.' in result[1], (number, result)
                source_errors += 1
    print(f'IPASIR C client: {models} models, {proofs} canonical original-query proofs, '
          f'{rejects} SAT-input rejects, {source_errors} invalid-source checker errors; '
          'nine exact input snapshots and bundle hashes verified')
    return 0


if __name__ == '__main__':
    sys.exit(main())
