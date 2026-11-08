"""Exact-query namespace refresh across and within native queries."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import os
from test_native_query_proof import formula
from test_native_query_append_proof import satisfiable


def main():
    fixture, root = map(Path, sys.argv[1:])
    sys.path.insert(0, str(root/'tools/bench'))
    spec = importlib.util.spec_from_file_location('namespace_runner', root/'tools/bench/run.py')
    runner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runner)
    from proof_oracle import certificate_route, check_dsr
    for tool in (runner.VERIPB, runner.CAKE_PB_CNF, runner.DSR_TRIM,
                 runner.DSR_COMPRESS, runner.SR_CHECK, runner.CAKE_LPR, runner.DRAT_TRIM):
        if not Path(tool).is_file():
            print(f'query namespace proof: missing checker {tool}')
            return 77
    checked = audits = 0
    with tempfile.TemporaryDirectory(prefix='tactix-query-names-') as tmp:
        directory = Path(tmp)
        for case in ('cross', 'within', 'repeat', 'replay', 'fresh'):
            trace = None
            for mode in ('none', 'pbp', 'dsr', 'auto', 'auto-pbp'):
                prefix = directory/f'{case}-{mode}'
                env = os.environ.copy()
                if mode == 'auto-pbp':
                    env['TACTIX_AUTO_PROOF_SWITCH_AT'] = '1'
                done = subprocess.run([fixture, case, prefix, mode], capture_output=True, text=True, timeout=30, env=env)
                assert done.returncode == 0, (case, mode, done.stdout, done.stderr)
                if trace is None:
                    trace = done.stdout
                assert done.stdout == trace, (case, mode, trace, done.stdout)
                cnf, proof = Path(f'{prefix}.cnf'), Path(f'{prefix}.proof')
                assert not satisfiable(*formula(cnf))
                if mode == 'none':
                    assert not proof.exists()
                    continue
                kind = 'pbp' if proof.read_text().startswith('pseudo-Boolean') else 'dsr'
                if mode == 'auto-pbp' and case != 'replay':
                    assert kind == 'pbp'
                witness_free = kind == 'dsr' and certificate_route(proof) == 'drat'
                route = 'pbp' if kind == 'pbp' else 'dsr-cake_lpr' if witness_free else 'dsr-trestle'
                verified = runner.check_proof(kind, str(cnf), str(proof), 30, route=route)
                assert verified[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), (case, mode, verified)
                checked += 1
                if kind == 'dsr':
                    work = directory/f'{case}-{mode}-forward'
                    work.mkdir()
                    result = check_dsr(runner.DSR_TRIM, runner.DSR_COMPRESS, runner.SR_CHECK,
                                       cnf, proof, work, 30, every_lemma=True,
                                       cake_lpr=runner.CAKE_LPR, witness_free=witness_free)
                    assert result[0], (case, mode, result)
                    audits += 1
                # A false refutation cannot pass: replace the independent
                # UNSAT component by tautologies without changing dimensions.
                lines = cnf.read_text().splitlines()
                wrong = directory/f'{case}-{mode}-sat.cnf'
                wrong.write_text('\n'.join('5 -5 0' if set(map(int, line.split()))-{0} in
                    ({5,6},{-5,6},{5,-6},{-5,-6}) else line
                    for line in lines if not line.startswith('p'))+'\n')
                wrong.write_text(lines[0]+'\n'+wrong.read_text())
                assert satisfiable(*formula(wrong))
                rejected = runner.check_proof(kind, str(wrong), str(proof), 30, route=route)
                assert not rejected[0] and rejected[2] == runner.CHK_REJECTED, (case, mode, rejected)
    print(f'query namespaces: {checked} canonical proofs, {audits} forward audits, SAT-input controls rejected; native trajectories and copy work agree')
    return 0


if __name__ == '__main__':
    sys.exit(main())
