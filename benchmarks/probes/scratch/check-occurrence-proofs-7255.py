#!/usr/bin/env python3
"""Check complete original-CNF proofs after each migrated public dense rule."""
from itertools import product
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from proof_oracle import proof_check_status
c = root / 'benchmarks/runs/20260911-7255-occurrence-proofs'
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
fixture = Path(__file__).with_name('test-occurrence-proof-7255.cpp'); bind(fixture)
cases = [case for case in product(range(7), range(2), range(3), range(2), range(2))
         if case[0] != 6 or case[1]]
assert len(cases) == 156
for arm, stem in (('previous','7249-single-occurrence'), ('candidate','7253-occurrence-cutover')):
    manifest = root / f'benchmarks/runs/20260911-{stem}-build/manifest.json'
    m = json.loads(manifest.read_text()); bind(manifest)
    source = root / m['source']; pin = root / f'build/pin-{stem}-release'
    for rel,digest in m['files'].items(): bind(source / rel,digest)
    bind(pin/'tactix',m['binaries'][str((pin/'tactix').relative_to(root))])
    objects = sorted((pin/'CMakeFiles/tactix_lib.dir').rglob('*.o')); assert objects
    for obj in objects: bind(obj)
    exe = c / (arm+'-fixture')
    p = run(arm+'-build',['/cm/local/apps/gcc/13.1.0/bin/g++','-std=c++20','-O2',
        '-I'+str(source/'src'),'-I'+str(source/'include'),fixture,*objects,'-o',exe])
    assert p.returncode == 0,p.stderr
    bind(exe); outputs[arm] = []
    for case in cases:
        name = arm+'-'+'-'.join(map(str,case)); mode,unsat = case[2:4]
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
    print(arm, '156 fixed cases PASS', flush=True)
assert outputs['previous'] == outputs['candidate'] and len(records) == 312
assert not list(c.glob('*.cnf')) and not list(c.glob('*.proof'))
bind(Path(__file__)); bind(root/'tools/bench/proof_oracle.py')
with (c/'summary.json').open('x') as out:
    json.dump(dict(status='PASS',checked_UNSAT=156,validated_SAT_models=156,negative_controls=2,
        exact_parent_certificates=True,records=records,hashes=hashes,removed_bytes=removed,
        scope='Seven migrated public rule families invoked on original formulas in both connected modes (BVE mixed-only), two literal orders, roots, duplicate binaries and large clauses, followed by dense collection and full solve. DRAT ASCII/binary and LRAT; SAT original clauses checked after reconstruction.',
        limitation='Fixed micro-tests, not fuzz, unrestricted N1 proof coverage or runtime acceptance.'),out,indent=2)
print('PASS156 checked UNSAT proofs,156 SAT models,2 rejecting controls; exact parent output; removed',removed,'owned bytes')
