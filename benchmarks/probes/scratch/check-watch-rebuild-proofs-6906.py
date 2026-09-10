#!/usr/bin/env python3
"""Check exact reconnect-dependent DRAT/LRAT witnesses and negative controls."""
from pathlib import Path
from itertools import product
import json
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260910-6906-watch-rebuild-proofs'
assert not c.exists()
c.mkdir()
fixture = root/'benchmarks/probes/scratch/test-watch-rebuild-proof-6906.cpp'
manifest = root/'benchmarks/runs/20260910-6902-watch-rebuild-build/manifest.json'
m = json.loads(manifest.read_text())
for rel,digest in m['files'].items(): assert sha(root/m['source']/rel) == digest,rel
for rel,digest in m['binaries'].items(): assert sha(root/rel) == digest,rel
records = []; negative = []; removed = 0; outputs = {}; hashes = {}
def run(name,command):
    p = subprocess.run(list(map(str,command)),cwd=c,capture_output=True,text=True)
    log = c/(name+'.json')
    with log.open('x') as stream:
        json.dump(dict(command=list(map(str,command)),returncode=p.returncode,stdout=p.stdout,stderr=p.stderr),stream,indent=2)
        stream.write('\n')
    hashes[str(log.relative_to(root))] = sha(log)
    return p
for lrat in (False,True):
    cnf = c/f'negative-{int(lrat)}.cnf'; proof = cnf.with_suffix('.proof')
    cnf.write_text('p cnf 1 1\n1 0\n')
    proof.write_text('2 0 1 0\n' if lrat else '0\n')
    checker = root/'tools/checkers/bin'/('lrat-check' if lrat else 'drat-trim')
    p = run('negative-'+str(int(lrat)),[checker,cnf,proof])
    assert p.returncode != 0 and ('c VERIFIED' if lrat else 's VERIFIED') not in p.stdout
    negative.append(dict(lrat=lrat,returncode=p.returncode,input_sha256=sha(cnf),proof_sha256=sha(proof),checker_sha256=sha(checker)))
    removed += cnf.stat().st_size + proof.stat().st_size
    cnf.unlink(); proof.unlink()
for arm,suffix,source_name in (
    ('previous','6730-reduce-watermark','reduce-watermark-6730'),
    ('candidate','6902-watch-rebuild','watch-rebuild-6902'),
):
    source = root/('.claude/worktrees/runtime-'+source_name)
    pin = root/('build/pin-'+suffix+'-release')
    objects = sorted((pin/'CMakeFiles/tactix_lib.dir').rglob('*.o'))
    assert objects
    for p in objects: hashes[str(p.relative_to(root))] = sha(p)
    exe = c/(arm+'-fixture')
    command = ['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-O2',
               '-I'+str(source/'src'),'-I'+str(source/'include'),str(fixture),*map(str,objects),'-o',str(exe)]
    p = run(arm+'-build',command)
    assert p.returncode == 0,p.stderr
    hashes[str(exe.relative_to(root))] = sha(exe)
    outputs[arm] = []
    for mode,before,flush,lrat,reversed_ in product(range(4),range(2),range(2),range(2),range(2)):
        name = f'{arm}-{mode}-{before}-{flush}-{lrat}-{reversed_}'
        cnf = c/(name+'.cnf'); proof = c/(name+'.proof')
        p = run(name+'-fixture',[exe,mode,before,flush,lrat,reversed_,cnf,proof])
        assert p.returncode == 0,(name,p.stderr)
        checker = root/'tools/checkers/bin'/('lrat-check' if lrat else 'drat-trim')
        q = run(name+'-checker',[checker,cnf,proof])
        assert q.returncode == 0 and ('c VERIFIED' if lrat else 's VERIFIED') in q.stdout and 'not verified' not in q.stdout.lower(),(name,q.stdout,q.stderr)
        record = dict(name=name,input_sha256=sha(cnf),proof_sha256=sha(proof),proof_bytes=proof.stat().st_size,checker_sha256=sha(checker),stdout=p.stdout)
        records.append(record)
        outputs[arm].append((p.stdout,sha(cnf),sha(proof),proof.stat().st_size))
        removed += cnf.stat().st_size + proof.stat().st_size
        cnf.unlink(); proof.unlink()
assert len(records) == 128 and outputs['previous'] == outputs['candidate']
for p in (Path(__file__),fixture,manifest): hashes[str(p.relative_to(root))] = sha(p)
assert not list(c.glob('*.cnf')) and not list(c.glob('*.proof'))
with (c/'summary.json').open('x') as stream:
    json.dump(dict(checked=128,records=records,negative_controls=negative,hashes=hashes,removed_bytes=removed,
        limitation='64 fixed reconnection-dependent proofs per actual source arm; connected/drop/flush/attached dense, roots before/after, both cleanup flags and watch orientations. Actual M1 emits final refutation through a large antecedent, not an injected checker-only contradiction. Two SAT negative controls reject empty proofs. Exact output/input/certificate bytes; not B/C, full50, memory or current-source fuzz acceptance.'),stream,indent=2)
    stream.write('\n')
print('PASS 128 exact fixed proofs and2 negative controls; removed',removed,'bytes')
