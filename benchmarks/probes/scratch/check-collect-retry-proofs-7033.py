#!/usr/bin/env python3
"""Strong fixed collector proofs on actual parent and isolated candidate."""
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from proof_oracle import proof_check_status
c = root / 'benchmarks/runs/20260910-7033-collect-retry-proofs'
assert not c.exists()
(c / 'logs').mkdir(parents=True)
hashes, records, fingerprints = {}, [], {}
removed = 0
def bind(p): hashes[str(p.relative_to(root))] = sha(p)
def run(name, command):
    command = list(map(str, command))
    r = subprocess.run(command, cwd=c, capture_output=True, text=True)
    log = c / 'logs' / (name + '.json')
    with log.open('x') as out:
        json.dump(dict(command=command, returncode=r.returncode,
                       stdout=r.stdout, stderr=r.stderr), out, indent=2)
        out.write('\n')
    bind(log)
    return r
def consume(p):
    global removed
    removed += p.stat().st_size
    p.unlink()
for lrat in (False, True):
    cnf, proof = c / 'negative.cnf', c / 'negative.proof'
    with cnf.open('x') as out: out.write('p cnf 1 1\n1 0\n')
    with proof.open('x') as out: out.write('2 0 1 0\n' if lrat else '0\n')
    checker = root / 'tools/checkers/bin' / ('cake_lpr' if lrat else 'drat-trim')
    bind(checker)
    r = run('negative-' + checker.name, [checker, cnf, proof])
    assert proof_check_status(checker.name, r.returncode, r.stdout, r.stderr) == 'rejected'
    consume(cnf); consume(proof)
for arm, prefix, date in [('current', '6730-reduce-watermark', '20260909'),
                          ('candidate', '7030-collect-retry', '20260910')]:
    manifest = root / f'benchmarks/runs/{date}-{prefix}-build/manifest.json'
    m = json.loads(manifest.read_text()); bind(manifest)
    for rel, digest in m['files'].items(): assert sha(root / m['source'] / rel) == digest, rel
    binary = root / f'build/pin-{prefix}-release/tests/test_reduce_root_lock'
    bind(binary)
    directory = c / arm; directory.mkdir()
    r = run(arm + '-fixture', [binary, directory])
    assert r.returncode == 0, r.stderr
    fingerprints[arm] = {}
    for cnf in sorted(directory.glob('*.cnf')):
        proof = cnf.with_suffix('.proof')
        checker = root / 'tools/checkers/bin' / ('cake_lpr' if cnf.stem.endswith('-lrat') else 'drat-trim')
        r = run(arm + '-' + cnf.stem, [checker, cnf, proof])
        assert proof_check_status(checker.name, r.returncode, r.stdout, r.stderr) == 'verified', (arm, cnf.name, r.stdout, r.stderr)
        fingerprint = dict(input_sha256=sha(cnf), proof_sha256=sha(proof), proof_bytes=proof.stat().st_size)
        fingerprints[arm][cnf.name] = fingerprint
        records.append(dict(arm=arm, name=cnf.name, checker=checker.name, **fingerprint))
        consume(cnf); consume(proof)
    assert len(fingerprints[arm]) == 8 and not list(directory.iterdir())
assert fingerprints['current'] == fingerprints['candidate']
for p in (Path(__file__), root / 'tools/bench/proof_oracle.py'): bind(p)
with (c / 'summary.json').open('x') as out:
    json.dump(dict(checked=16, negative_controls=2, exact_parent_certificates=True,
        records=records, hashes=hashes, removed_bytes=removed,
        scope='Existing root/elevated compact/tail collector SAT-model and UNSAT DRAT/LRAT fixed fixtures, protected nonroot/pending reasons, both actual arms. New40-state retry matrix separately pins advice-only effect. Strong cake_lpr replaces historically unsound legacy lrat-check.',
        limitation='Fixed micro-tests, not fuzz or a runtime acceptance gate.'), out, indent=2)
    out.write('\n')
print('16 strict fixed proofs, two rejecting controls, exact parent certificates PASS; removed', removed, 'bytes')
