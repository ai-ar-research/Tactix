#!/usr/bin/env python3
"""Exact-parent fixed Vivify certificates with independent strict negatives."""
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260910-7165-vivify-revalidation-proofs'
assert not c.exists()
c.mkdir()
fixture = root / 'benchmarks/probes/scratch/test-vivify-count-cache-proof-6386.cpp'
hashes = {}
def bind(p, expected=None):
    value = sha(p)
    assert expected is None or value == expected, p
    hashes[str(p.relative_to(root))] = value
bind(fixture); bind(Path(__file__))
records, negatives, removed = [], [], 0
reference = {}
for arm, stem, directory in (
    ('control', '7030-collect-retry', '20260910-7030-collect-retry-build'),
    ('candidate', '7162-vivify-revalidation', '20260910-7162-vivify-revalidation-build')):
    authority = root / 'benchmarks/runs' / directory / 'manifest.json'
    bind(authority); m = json.loads(authority.read_text())
    source = root / m['source']; pin = root / ('build/pin-' + stem + '-release')
    for rel, digest in m['files'].items(): bind(source / rel, digest)
    bind(pin / 'tactix', m['binaries'][str((pin / 'tactix').relative_to(root))])
    objects = sorted((pin / 'CMakeFiles/tactix_lib.dir').rglob('*.o'))
    assert objects
    for p in objects: bind(p)
    binary = c / (arm + '-fixture')
    command = ['/cm/local/apps/gcc/13.1.0/bin/c++', '-std=c++20', '-O2',
        '-I' + str(source / 'src'), '-I' + str(source / 'include'), str(fixture),
        *map(str, objects), '-o', str(binary)]
    result = subprocess.run(command, capture_output=True, text=True)
    with (c / (arm + '-build.json')).open('x') as out:
        json.dump(dict(command=command, exit_code=result.returncode,
                       stdout=result.stdout, stderr=result.stderr), out, indent=2)
    assert result.returncode == 0, result.stderr
    bind(binary)
    for scope in range(6):
        for reuse in (0, 1):
            for flush in (0, 1):
                for mode in (0, 1):
                    case = f'{scope}-{reuse}-{flush}-{mode}'
                    name = arm + '-' + case
                    cnf, proof = c / (name + '.cnf'), c / (name + '.proof')
                    command = [str(binary), str(scope), str(reuse), str(flush),
                               str(mode), str(cnf), str(proof)]
                    p = subprocess.run(command, cwd=c, capture_output=True, text=True)
                    assert p.returncode == 0, (name, p.stdout, p.stderr)
                    checker = root / ('tools/checkers/bin/lrat-check' if mode else 'tools/checkers/bin/drat-trim')
                    bind(checker)
                    token = 'c VERIFIED' if mode else 's VERIFIED'
                    check = [str(checker), str(cnf), str(proof)]
                    q = subprocess.run(check, capture_output=True, text=True)
                    assert q.returncode == 0 and token in q.stdout.splitlines(), (name, q.stdout, q.stderr)
                    fingerprint = (sha(cnf), sha(proof), proof.stat().st_size, p.stdout)
                    if arm == 'control': reference[case] = fingerprint
                    else: assert fingerprint == reference[case], (name, 'parent certificate mismatch')
                    records.append(dict(name=name, fixture_command=command, fixture_stdout=p.stdout,
                        command=check, exit_code=q.returncode, stdout=q.stdout, stderr=q.stderr,
                        input_sha256=fingerprint[0], proof_sha256=fingerprint[1], proof_bytes=fingerprint[2]))
                    data = proof.read_bytes()
                    lines = data.splitlines(keepends=True)
                    assert len(lines) > 1
                    for kind, payload in (('empty', b''), ('truncated', b''.join(lines[:len(lines)//2]))):
                        bad = c / (name + '-' + kind + '.proof')
                        with bad.open('xb') as out: out.write(payload)
                        check = [str(checker), str(cnf), str(bad)]
                        n = subprocess.run(check, capture_output=True, text=True)
                        assert n.returncode != 0 and token not in n.stdout.splitlines(), (name, kind, n.stdout, n.stderr)
                        negatives.append(dict(name=name, kind=kind, command=check, exit_code=n.returncode,
                            stdout=n.stdout, stderr=n.stderr, proof_sha256=sha(bad)))
                        removed += bad.stat().st_size; bad.unlink()
                    removed += cnf.stat().st_size + proof.stat().st_size
                    cnf.unlink(); proof.unlink()
    print(arm, '48 positive and96 negative checks PASS', flush=True)
for rel, digest in hashes.items(): assert sha(root / rel) == digest, rel
assert not list(c.glob('*.cnf')) and not list(c.glob('*.proof'))
with (c / 'summary.json').open('x') as out:
    json.dump(dict(status='PASS', hashes=hashes, records=records, negatives=negatives,
        checked=len(records), negative_controls=len(negatives), exact_parent_certificates=True,
        removed_bytes=removed, limitation='Fixed proof-backed canonical/fallback fixtures; not a broad solver gate or current-source fuzz.'), out, indent=2)
print('PASS exact parent proofs and zero certificate/input residue; removed bytes', removed)
