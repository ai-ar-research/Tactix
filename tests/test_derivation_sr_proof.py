"""Temporary derivations must not obstruct a later witnessed continuation."""
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
    spec = importlib.util.spec_from_file_location('conditioned_runner', root/'tools/bench/run.py')
    runner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runner)
    from proof_oracle import certificate_route, check_dsr
    for tool in (runner.VERIPB, runner.CAKE_PB_CNF, runner.DRAT_TRIM,
                 runner.CAKE_LPR, runner.DSR_TRIM, runner.DSR_COMPRESS, runner.SR_CHECK):
        if not Path(tool).is_file():
            print(f'conditioned derivation proof: missing checker {tool}')
            return 77
    traces = {}
    with tempfile.TemporaryDirectory(prefix='tactix-conditioned-') as tmp:
        directory = Path(tmp)
        for case in ('needed', 'unneeded'):
            for mode in ('none', 'pbp', 'dsr'):
                prefix = directory/f'{case}-{mode}'
                done = subprocess.run([fixture, case, prefix, mode],
                                      capture_output=True, text=True, timeout=30)
                assert done.returncode == 0, done.stdout+done.stderr
                assert done.stdout == traces.setdefault(case, done.stdout), (case, mode, traces[case], done.stdout)
                cnf, proof = Path(f'{prefix}.cnf'), Path(f'{prefix}.proof')
                assert not satisfiable(*formula(cnf))
                if mode == 'none':
                    assert not proof.exists()
                    continue
                witness_free = mode == 'dsr' and certificate_route(proof) == 'drat'
                route = 'pbp' if mode == 'pbp' else ('dsr-cake_lpr' if witness_free else 'dsr-trestle')
                result = runner.check_proof(mode, str(cnf), str(proof), 30, route=route)
                assert result[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), (case, mode, result)
                if mode == 'dsr':
                    work = directory/f'{case}-audit'
                    work.mkdir()
                    audited = check_dsr(runner.DSR_TRIM, runner.DSR_COMPRESS, runner.SR_CHECK,
                                        cnf, proof, work, 30, every_lemma=True,
                                        cake_lpr=runner.CAKE_LPR, witness_free=witness_free)
                    assert audited[0], audited
                if case == 'needed':
                    # The native binary is not directly RUP. The globally
                    # licensed unit5 antecedent remains in the old names.
                    addition = 'rup 1 x5 >= 1 ;\n' if mode == 'pbp' else '5 0\n'
                    original = proof.read_text()
                    lines = original.splitlines(keepends=True)
                    assert lines.count(addition) == 1
                    wrong = directory/f'{case}-{mode}-missing-antecedent.proof'
                    wrong.write_text(''.join(line for line in lines if line != addition))
                    rejected = runner.check_proof(mode, str(cnf), str(wrong), 30, route=route)
                    assert not rejected[0] and rejected[2] == runner.CHK_REJECTED, rejected
    print('conditioned derivations: four canonical proofs, two every-lemma audits and two missing-antecedent rejects; no-proof/PBP/DSR trajectories agree')
    return 0


if __name__ == '__main__':
    sys.exit(main())
