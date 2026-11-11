"""Witnessed continuation isolates obsolete sources; copy bridges are checked."""
from proof_test_support import proof_runner
from pathlib import Path
import subprocess
import sys
import tempfile
import re


def main():
    fixture, root = map(Path, sys.argv[1:])
    runner = proof_runner(root, checkers=('VERIPB', 'CAKE_PB_CNF', 'DSR_TRIM', 'DSR_COMPRESS', 'SR_CHECK'))
    from proof_oracle import check_dsr, canonical_checker_cnf, proof_check_status, stage_environment

    with tempfile.TemporaryDirectory(prefix='tactix-native-sr-') as tmp:
        directory = Path(tmp)
        trace = None
        for mode, route in (('none', None), ('pbp', 'pbp'), ('dsr', 'dsr-trestle')):
            prefix = directory/mode
            done = subprocess.run([fixture, prefix, mode], capture_output=True, text=True, timeout=30)
            assert done.returncode == 0, done.stdout+done.stderr
            if trace is None:
                trace = done.stdout
            assert done.stdout == trace, (mode, trace, done.stdout)
            cnf, proof = Path(f'{prefix}.cnf'), Path(f'{prefix}.proof')
            if mode == 'none':
                assert not proof.exists()
                continue
            verified = runner.check_proof(mode, str(cnf), str(proof), 30, route=route)
            assert verified[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), verified
            deletion = 'del spec 1 ~x1 1 x3 >= 1 ;\n' if mode == 'pbp' else 'd -1 3 0\n'
            text = proof.read_text()
            assert deletion not in text
            # Old names are immutable by default. An optional deletion after
            # checked imports, before cloning, must also remain sound.
            retained = directory/f'{mode}-deleted-old-source.proof'
            lines = text.splitlines(keepends=True)
            # Locate the definition of the copied assumption y=2.
            if mode == 'pbp':
                pattern = r'^red 1 x(\d+) 1 ~x2 >= 1 : x\1 -> 1 ;$'
            else:
                pattern = r'^(\d+) -2 0$'
            candidates = [(i, re.match(pattern, line.strip())) for i, line in enumerate(lines)]
            first_copy, match = next((i, m) for i, m in candidates if m and int(m[1]) > 5)
            fresh = int(match[1])
            lines.insert(first_copy, deletion)
            retained.write_text(''.join(lines))
            checked = runner.check_proof(mode, str(cnf), str(retained), 30, route=route)
            assert checked[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), checked
            # Corrupt the reverse equivalence: its resolvent with the first
            # definition is now ~y, contradicted by the exact query's y unit.
            # Neither RUP nor RAT can license that addition.
            lines = text.splitlines(keepends=True)
            before = (f'red 1 ~x{fresh} 1 x2 >= 1 : x{fresh} -> 0 ;\n'
                      if mode == 'pbp' else f'-{fresh} 2 0\n')
            after = before.replace('1 x2', '1 ~x2') if mode == 'pbp' else f'-{fresh} -2 0\n'
            assert lines.count(before) == 1
            lines[lines.index(before)] = after
            retired = (f'del spec 1 ~x{fresh} 1 x2 >= 1 ;\n'
                       if mode == 'pbp' else f'd -{fresh} 2 0\n')
            assert lines.count(retired) == 1
            lines[lines.index(retired)] = (retired.replace('1 x2', '1 ~x2')
                                           if mode == 'pbp' else f'd -{fresh} -2 0\n')
            wrong = directory/f'{mode}-corrupted-copy-bridge.proof'
            wrong.write_text(''.join(lines))
            if mode == 'pbp':
                rejected = runner.check_proof(mode, str(cnf), str(wrong), 30, route=route)
                assert not rejected[0] and rejected[2] == runner.CHK_REJECTED, rejected
            else:
                good = directory/'forward-good'
                good.mkdir()
                checked = check_dsr(runner.DSR_TRIM, runner.DSR_COMPRESS, runner.SR_CHECK,
                                    cnf, proof, good, 30, every_lemma=True)
                assert checked[0], checked
                kept = directory/'forward-retained'
                kept.mkdir()
                checked = check_dsr(runner.DSR_TRIM, runner.DSR_COMPRESS, runner.SR_CHECK,
                                    cnf, retained, kept, 30, every_lemma=True)
                assert checked[0], checked
                bad = directory/'forward-bad'
                bad.mkdir()
                checker_cnf = bad/'checker.cnf'
                canonical_checker_cnf(cnf, checker_cnf)
                checked = subprocess.run([runner.DSR_TRIM, '-f', checker_cnf, wrong, bad/'proof.lsr'],
                                         capture_output=True, text=True, timeout=30,
                                         env=stage_environment('dsr-trim'))
                assert proof_check_status('dsr-trim', checked.returncode, checked.stdout,
                                          checked.stderr) == 'rejected', checked
                assert 'No UP contradiction for RAT clause' in checked.stdout+checked.stderr, checked
    print('native SR: canonical/forward proofs tolerate retained old sources; corrupted copy bridges reject; mode trajectories agree')
    return 0


if __name__ == '__main__':
    sys.exit(main())
