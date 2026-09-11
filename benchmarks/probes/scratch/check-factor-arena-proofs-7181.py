#!/usr/bin/env python3
"""Exact retained-parent Factor certificates with DRAT and cake_lpr oracles."""
from itertools import product
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from proof_oracle import proof_check_status
c = root / 'benchmarks/runs/20260910-7181-factor-arena-proofs'
assert not c.exists()
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
for mode in (0, 1):
    cnf = c / f'negative-{mode}.cnf'; proof = cnf.with_suffix('.proof')
    with cnf.open('x') as out: out.write('p cnf 1 1\n1 0\n')
    with proof.open('x') as out: out.write('2 0 1 0\n' if mode else '0\n')
    checker = root / 'tools/checkers/bin' / ('cake_lpr' if mode else 'drat-trim')
    p = run('negative-'+str(mode), [checker, cnf, proof])
    assert proof_check_status(checker.name, p.returncode, p.stdout, p.stderr) == 'rejected'
    bind(checker)
    removed += cnf.stat().st_size + proof.stat().st_size
    cnf.unlink(); proof.unlink()
fixture = root / 'benchmarks/probes/scratch/test-factor-count-proof-6931.cpp'; bind(fixture)
for arm, stem, tree in (('previous', '7162-vivify-revalidation', 'runtime-vivify-compact-7144'),
                        ('candidate', '7180-factor-arena', 'runtime-factor-arena-7180')):
    source = root / '.claude/worktrees' / tree
    pin = root / ('build/pin-' + stem + '-release')
    authority = root / ('benchmarks/runs/20260910-' + stem + '-build/manifest.json')
    b = json.loads(authority.read_text()); bind(authority)
    for rel, digest in b['files'].items(): bind(source / rel, digest)
    bind(pin / 'tactix', b['binaries'][str((pin / 'tactix').relative_to(root))])
    objects = sorted((pin / 'CMakeFiles/tactix_lib.dir').rglob('*.o'))
    assert objects
    for obj in objects: bind(obj)
    exe = c / (arm + '-fixture')
    p = run(arm+'-build', ['/cm/local/apps/gcc/13.1.0/bin/c++', '-std=c++20', '-O2',
        '-I'+str(source / 'src'), '-I'+str(source / 'include'), fixture, *objects, '-o', exe])
    assert p.returncode == 0, p.stderr
    bind(exe); outputs[arm] = []
    for case in product((3, 9, 33), (3, 8), (1, 2, 4), (0, 1), (0, 1), (0, 1)):
        name = arm + '-' + '-'.join(map(str, case)); mode = case[-1]
        cnf = c / (name + '.cnf'); proof = c / (name + '.proof')
        p = run(name+'-fixture', [exe, *case, cnf, proof])
        assert p.returncode == 0, (name, p.stderr)
        checker = root / 'tools/checkers/bin' / ('cake_lpr' if mode else 'drat-trim')
        q = run(name+'-checker', [checker, cnf, proof])
        status = proof_check_status(checker.name, q.returncode, q.stdout, q.stderr)
        assert status == 'verified', (name, status, q.stdout, q.stderr)
        record = dict(name=name, arm=arm, case=case, checker=checker.name, status=status,
            input_sha256=sha(cnf), proof_sha256=sha(proof), proof_bytes=proof.stat().st_size, fixture_stdout=p.stdout)
        records.append(record)
        outputs[arm].append((p.stdout, sha(cnf), sha(proof), proof.stat().st_size))
        removed += cnf.stat().st_size + proof.stat().st_size
        cnf.unlink(); proof.unlink()
assert len(records) == 288 and outputs['previous'] == outputs['candidate']
assert not list(c.glob('*.cnf')) and not list(c.glob('*.proof'))
bind(Path(__file__))
with (c / 'summary.json').open('x') as out:
    json.dump(dict(status='PASS', checked=288, LRAT_cake_lpr=144, DRAT_drat_trim=144,
        negative_controls=2, hashes=hashes, records=records, removed_bytes=removed,
        exact_parent_certificates=True,
        scope='Actual retained7144 and arena7180 libraries, 144 fixed Factor proofs each; widths1/2/4, rows3/9/33, columns3/8, two orders, bounded/unbounded, DRAT/LRAT. Generated quotients cause the final contradiction. Exact input/certificate/state. Only owned fixed-case residue removed.',
        limitation='No real-instance, broad gate, fuzz or promotion claim.'), out, indent=2)
print('PASS288 exact-parent Factor proofs,2 strict negatives; removed', removed, 'owned bytes')
