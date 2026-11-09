#!/usr/bin/env python3
"""Independent certificates for fixed signed count-conservation networks."""
from pathlib import Path
import os
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/bench'))
from proof_oracle import check_routed, pb_proof_has_header

pins = ROOT / 'tools/checkers/bin'
checkers = ROOT / 'tools/checkers/bin'


def check_dsr(cnf, proof, work):
    """A follow-on DSR proof, by its content's route."""
    checked = check_routed(checkers/'dsr-trim', checkers/'dsr-compress',
                           checkers/'srcheck', checkers/'drat-trim',
                           checkers/'cake_lpr', cnf, proof, work, timeout=60)
    assert checked[0] and checked[3] in ('drat', 'dsr'), (proof.name, checked)


def masked(text):
    """The statistics every proof mode shares."""
    skip = re.compile(r'^c (timing|phasecost|auto-proof|proof-route|follow-on)'
                      r'|nanos|seconds| ms\b|%\)')
    return [re.sub(r'[0-9]+\.[0-9]+', '<f>', line) for line in text.splitlines()
            if line.startswith('c ') and not skip.search(line)]


def dsr_follow_on(cnf, work):
    """--dsr: the main run's statistics equal --pbp's, a follow-on writes the
    proof, and a pipe receives the same bytes as a file."""
    pbp = subprocess.run([sys.argv[2], '--pbp', str(cnf), str(work/'main.pbp')],
                         capture_output=True, text=True)
    assert pbp.returncode == 20, pbp
    for options in (['--dsr'], ['--dsr', '--no-binary']):
        proof = work / f'{cnf.stem}{"-text" if len(options) > 1 else ""}.only-dsr'
        run = subprocess.run([sys.argv[2], *options, str(cnf), str(proof)],
                             capture_output=True, text=True)
        assert run.returncode == 20, run
        assert 'c follow-on   startup-count-pb\n' in run.stdout, run.stdout
        assert re.search(r'^c follow-on   20 result,', run.stdout, re.M), run.stdout
        assert re.search(r'^c proof-route dsr-', run.stdout, re.M), run.stdout
        assert masked(run.stdout) == masked(pbp.stdout), cnf
        check_dsr(cnf, proof, work)
        fifo = work / 'fifo'
        os.mkfifo(fifo)
        with open(work/'piped', 'wb') as piped:
            reader = subprocess.Popen(['cat', str(fifo)], stdout=piped)
            run = subprocess.run([sys.argv[2], *options, str(cnf), str(fifo)],
                                 capture_output=True, text=True)
            assert reader.wait() == 0 and run.returncode == 20, run
        fifo.unlink()
        assert (work/'piped').read_bytes() == proof.read_bytes(), cnf
    print(cnf.name, '--dsr follow-on verified; main statistics equal --pbp; '
          'pipe equals file')

with tempfile.TemporaryDirectory(prefix='count-preservation-') as directory:
    work = Path(directory)
    subprocess.run([sys.argv[1], str(work)], check=True)
    proofs = list(work.glob('*.pbp'))
    assert len(proofs) == 4
    for proof in proofs:
        cnf = proof.with_suffix('.cnf')
        for candidate in (proof, work / 'automatic.pbp'):
            if candidate != proof:
                result = subprocess.run([sys.argv[2], str(cnf), str(candidate)],
                                        capture_output=True, text=True)
                assert result.returncode == 20, result
            assert pb_proof_has_header(candidate)
            checked = check_routed(None, None, None, None, None, cnf, candidate,
                                   work, timeout=60, veripb=pins/'veripb',
                                   cake_pb=pins/'cake-pb-cnf')
            assert checked[0], (proof.name, checked)
        no_proof = subprocess.run([sys.argv[2], str(cnf)], capture_output=True)
        assert no_proof.returncode == 20
        print(proof.name, 'API + default VeriPB/CakePB verified; no-proof agrees')
    # The C API's DSR sessions, after a direct application and a default solve.
    dsr_proofs = list(work.glob('*.dsr'))
    assert len(dsr_proofs) == 2
    for proof in dsr_proofs:
        check_dsr(work / (proof.stem.split('-')[0] + '.cnf'), proof, work)
    print('API DSR follow-on proofs verified')
    for name in ('count3', 'count3r'):
        dsr_follow_on(work / f'{name}.cnf', work)
    # Original identities survive duplicate literals, duplicate clauses, and
    # tautologies; input-normalization DSR remains private and is discarded.
    lines = (work/'count3r.cnf').read_text().splitlines()
    clauses = [line.split()[0] + ' ' + line for line in lines[1:]]
    clauses += ['1 -1 0', clauses[0]]
    variables = lines[0].split()[2]
    cnf = work/'normalized.cnf'
    cnf.write_text(f'p cnf {variables} {len(clauses)}\n'+'\n'.join(clauses)+'\n')
    proof = work/'normalized.pbp'
    result = subprocess.run([sys.argv[2], str(cnf), str(proof)], capture_output=True)
    assert result.returncode == 20 and pb_proof_has_header(proof), result
    checked = check_routed(None, None, None, None, None, cnf, proof, work,
                           timeout=60, veripb=pins/'veripb', cake_pb=pins/'cake-pb-cnf')
    assert checked[0], checked
    dsr_follow_on(cnf, work)
    # Deleting the derivation must not be accepted as a counting certificate.
    proof.write_text('pseudo-Boolean proof version 3.0\noutput NONE ;\n'
                     'conclusion UNSAT : -1 ;\nend pseudo-Boolean proof ;\n')
    assert not check_routed(None, None, None, None, None, cnf, proof, work,
                            timeout=60, veripb=pins/'veripb',
                            cake_pb=pins/'cake-pb-cnf')[0]
    print('raw-normalization proof verified; missing-derivation control rejected')
