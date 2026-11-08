"""Witnessed continuation isolates obsolete sources; copy witnesses are checked."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile
import re


def main():
    fixture, root = map(Path, sys.argv[1:])
    sys.path.insert(0, str(root/'tools/bench'))
    spec = importlib.util.spec_from_file_location('native_sr_runner', root/'tools/bench/run.py')
    runner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runner)
    from proof_oracle import check_dsr
    for tool in (runner.VERIPB, runner.CAKE_PB_CNF, runner.DSR_TRIM,
                 runner.DSR_COMPRESS, runner.SR_CHECK):
        if not Path(tool).is_file():
            print(f'native SR proof: missing checker {tool}')
            return 77
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
            first_copy = next(i for i, line in enumerate(lines)
                              if (' -> ' in line if mode == 'pbp' else
                                  not line.startswith('d') and len(line.split()) > 14))
            lines.insert(first_copy, deletion)
            retained.write_text(''.join(lines))
            checked = runner.check_proof(mode, str(cnf), str(retained), 30, route=route)
            assert checked[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), checked
            # Corrupt the (y v z) clone's mapping y' -> y. The changed
            # image (~y v z) is not licensed by the original sources plus y.
            if mode == 'pbp':
                match = re.search(r'x(\d+) -> x(\d+)\b', text)
                assert match
                offset = int(match[1])-int(match[2])
                lines = text.splitlines(keepends=True)
                prefix = f'red 1 x{offset+2} 1 x{offset+3} >= 1'
                candidates = [i for i, line in enumerate(lines) if line.startswith(prefix)]
                assert len(candidates) == 1, candidates
                i = candidates[0]
                before, after = f'x{offset+2} -> x2', f'x{offset+2} -> ~x2'
                assert before in lines[i]
                lines[i] = lines[i].replace(before, after)
            else:
                lines = text.splitlines(keepends=True)
                # Full uniform maps end in (..., fresh5, 5). Their smallest
                # source variable is fresh1; helper guards are below it.
                maps = [list(map(int, line.split())) for line in lines
                        if not line.startswith('d') and len(line.split()) > 14]
                assert maps
                offset = maps[0][-3]-5
                candidates = [i for i, line in enumerate(lines)
                              if not line.startswith('d') and len(line.split()) > 14
                              and {offset+2, offset+3} <= set(map(int, line.split()[:4]))]
                assert candidates
                i = candidates[0]
                before, after = f' {offset+2} 2 ', f' {offset+2} -2 '
                assert before in lines[i]
                lines[i] = lines[i].replace(before, after)
            wrong = directory/f'{mode}-corrupted-copy-witness.proof'
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
                checked = check_dsr(runner.DSR_TRIM, runner.DSR_COMPRESS, runner.SR_CHECK,
                                    cnf, wrong, bad, 30, every_lemma=True)
                assert (not checked[0] and checked[1] == 'every-lemma:dsr-trim' and
                        'No UP contradiction for RAT clause' in checked[2]), checked
    print('native SR: canonical/forward proofs tolerate retained old sources; corrupted copy witnesses reject; mode trajectories agree')
    return 0


if __name__ == '__main__':
    sys.exit(main())
