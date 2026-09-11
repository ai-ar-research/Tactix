#!/usr/bin/env python3
"""Bind actual parent/candidate libraries to valid root-binary proof witnesses."""
from itertools import product
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from proof_oracle import proof_check_status
c = root / 'benchmarks/runs/20260911-7219-root-binary-proofs'
assert not c.exists(), 'preserve successful or failed evidence'
(c / 'logs').mkdir(parents=True)
hashes, records, outputs, removed = {}, [], {}, 0
def bind(p, expected=None):
    digest = sha(p)
    assert expected is None or digest == expected, p
    hashes[str(p.relative_to(root))] = digest
def run(name, command):
    command = list(map(str, command))
    p = subprocess.run(command, cwd=c, capture_output=True, text=True)
    log = c / 'logs' / (name + '.json')
    with log.open('x') as out:
        json.dump(dict(command=command, returncode=p.returncode, stdout=p.stdout, stderr=p.stderr), out, indent=2)
    bind(log)
    return p
def consume(*paths):
    global removed
    for p in paths:
        assert p.parent == c and p.suffix in ('.cnf','.proof')
        removed += p.stat().st_size
        p.unlink()
for mode in (0, 2):
    cnf = c / f'negative-{mode}.cnf'; proof = cnf.with_suffix('.proof')
    with cnf.open('x') as out: out.write('p cnf 1 1\n1 0\n')
    with proof.open('x') as out: out.write('2 0 1 0\n' if mode == 2 else '0\n')
    checker = root / 'tools/checkers/bin' / ('cake_lpr' if mode == 2 else 'drat-trim')
    p = run('negative-'+str(mode), [checker, cnf, proof]); bind(checker)
    assert proof_check_status(checker.name,p.returncode,p.stdout,p.stderr) == 'rejected'
    consume(cnf,proof)
fixture = Path(__file__).with_name('test-root-binary-proof-7219.cpp'); bind(fixture)
for arm, stem, date in (('previous','7162-vivify-revalidation','20260910'),
                         ('candidate','7218-root-binary','20260911')):
    manifest = root / f'benchmarks/runs/{date}-{stem}-build/manifest.json'
    m = json.loads(manifest.read_text()); bind(manifest)
    source = root / m['source']; pin = root / f'build/pin-{stem}-release'
    for rel,digest in m['files'].items(): bind(source / rel,digest)
    bind(pin/'tactix',m['binaries'][str((pin/'tactix').relative_to(root))])
    objects = sorted((pin/'CMakeFiles/tactix_lib.dir').rglob('*.o')); assert objects
    for obj in objects: bind(obj)
    exe = c / (arm+'-fixture')
    p = run(arm+'-build',['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-O2',
        '-I'+str(source/'src'),'-I'+str(source/'include'),fixture,*objects,'-o',exe])
    assert p.returncode == 0,p.stderr
    bind(exe); outputs[arm] = []
    for case in product((1,2,3),(0,1,2),(1,3),(0,1),(0,1),(0,1,2)):
        name = arm+'-'+'-'.join(map(str,case)); unsat,mode = case[-2:]
        cnf = c/(name+'.cnf'); proof = c/(name+'.proof')
        p = run(name+'-fixture',[exe,*case,cnf,proof])
        assert p.returncode == 0,(name,p.stdout,p.stderr)
        checker_name = None
        if unsat:
            checker_name = 'cake_lpr' if mode == 2 else 'drat-trim'
            q = run(name+'-checker',[root/'tools/checkers/bin'/checker_name,cnf,proof])
            assert proof_check_status(checker_name,q.returncode,q.stdout,q.stderr) == 'verified',(name,q.stdout,q.stderr)
        fingerprint = dict(input_sha256=sha(cnf),proof_sha256=sha(proof),proof_bytes=proof.stat().st_size,stdout=p.stdout)
        outputs[arm].append(fingerprint)
        records.append(dict(arm=arm,case=case,checker=checker_name,**fingerprint))
        consume(cnf,proof)
assert outputs['previous'] == outputs['candidate'] and len(records) == 432
assert not list(c.glob('*.cnf')) and not list(c.glob('*.proof'))
bind(Path(__file__)); bind(root/'tools/bench/proof_oracle.py')
with (c/'summary.json').open('x') as out:
    json.dump(dict(status='PASS',checked_UNSAT=216,validated_SAT_models=216,negative_controls=2,
        exact_parent_certificates=True,records=records,hashes=hashes,removed_bytes=removed,
        scope='Valid original-formula root-satisfied binary deletion, either/both root endpoints, three import orders, duplicates, mixed large watch, root/elevated collection, idempotence, DRAT ASCII/binary and LRAT. SAT models checked against every original clause.',
        limitation='Fixed micro-tests, not fuzz, general proof coverage or performance acceptance.'),out,indent=2)
print('PASS216 checked UNSAT proofs,216 SAT models,2 rejecting controls; exact parent output; removed',removed,'owned bytes')
