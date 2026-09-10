#!/usr/bin/env python3
"""One-shot strict old/new BVE certificate control; no real solver run."""
import itertools
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from proof_oracle import proof_check_status
out = root / 'benchmarks/runs/20260910-7089-bve-view-proofs'
assert not out.exists()
(out / 'logs').mkdir(parents=True)
hashes, records, fingerprints = {}, [], {}
def bind(path): hashes[str(path.relative_to(root))] = sha(path)
def run(name, args):
    args = list(map(str, args))
    p = subprocess.run(args, cwd=out, capture_output=True, text=True)
    log = out / 'logs' / (name + '.json')
    with log.open('x') as stream:
        json.dump(dict(command=args, exit_code=p.returncode, stdout=p.stdout, stderr=p.stderr), stream, indent=2)
        stream.write('\n')
    bind(log)
    return p
fixture = root / 'benchmarks/probes/scratch/test-bve-view-proofs-7089.cpp'
bind(fixture)
for mode, name in ((0, 'drat-trim'), (1, 'cake_lpr')):
    cnf, proof = out / f'negative-{mode}.cnf', out / f'negative-{mode}.proof'
    cnf.write_text('p cnf 1 1\n1 0\n')
    proof.write_text('2 0 1 0\n' if mode else '0\n')
    checker = root / 'tools/checkers/bin' / name; bind(checker)
    p = run(f'negative-{mode}', [checker, cnf, proof])
    assert proof_check_status(name, p.returncode, p.stdout, p.stderr) == 'rejected'
    bind(cnf); bind(proof)
for arm, stem in (('previous', '7030-collect-retry'), ('candidate', '7087-bve-views')):
    authority = root / f'benchmarks/runs/20260910-{stem}-build/manifest.json'
    manifest = json.loads(authority.read_text()); bind(authority)
    source = root / manifest['source']
    for rel, digest in manifest['files'].items(): assert sha(source / rel) == digest, rel
    build = root / ('build/pin-' + stem + '-release')
    objects = sorted((build / 'CMakeFiles/tactix_lib.dir').rglob('*.o'))
    for obj in objects: bind(obj)
    exe = out / (arm + '-fixture')
    p = run(arm + '-compile', ['/cm/local/apps/gcc/13.1.0/bin/c++', '-std=c++20', '-O2',
        '-fno-exceptions', '-fno-rtti', '-I' + str(source / 'src'), '-I' + str(source / 'include'),
        fixture, *objects, '-o', exe])
    assert p.returncode == 0, p.stderr
    bind(exe); fingerprints[arm] = []
    for case in itertools.product(range(3), (1, 2), (0, 1), (0, 1), (0, 1)):
        name = arm + '-' + '-'.join(map(str, case))
        cnf, proof = out / (name + '.cnf'), out / (name + '.proof')
        p = run(name, [exe, *case, cnf, proof]); assert p.returncode == 0, (name, p.stderr)
        checker = root / 'tools/checkers/bin' / ('cake_lpr' if case[-1] else 'drat-trim')
        q = run(name + '-check', [checker, cnf, proof])
        assert proof_check_status(checker.name, q.returncode, q.stdout, q.stderr) == 'verified', (name, q.stdout, q.stderr)
        bind(cnf); bind(proof)
        fingerprint = [p.stdout, sha(cnf), sha(proof), proof.stat().st_size]
        fingerprints[arm].append(fingerprint)
        records.append(dict(arm=arm, case=case, status='verified', checker=checker.name, fingerprint=fingerprint))
assert len(records) == 96 and fingerprints['previous'] == fingerprints['candidate']
bind(Path(__file__)); bind(root / 'tools/bench/proof_oracle.py')
# Fixed small proofs are retained for reproduction; no active campaign scratch.
with (out / 'summary.json').open('x') as stream:
    json.dump(dict(checked=96, negative_controls=2, exact_parent_certificates=True,
        records=records, hashes=hashes, retained_fixed_artifacts=True,
        scope='Root-free and filtered large BVE antecedents, reversed literal order, physical duplicates, dense/sparse N1 calls, forced arena growth before witnesses; final UNSAT depends on BVE resolvents. Not fuzz or runtime acceptance.'), stream, indent=2)
    stream.write('\n')
print('PASS96 strict proofs (48 cake_lpr/48 DRAT),2 negatives; exact both arms')
