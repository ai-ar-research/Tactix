#!/usr/bin/env python3
"""64 fixed model-set and final DRAT/LRAT controls; no search or fuzz."""
import itertools
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260909-6657-congruence-empty-proof-control'
assert not c.exists()
authority = root / 'benchmarks/runs/20260909-6654-congruence-empty-gates-build/manifest.json'
a = json.loads(authority.read_text()); source = root / a['source']
pin = root / 'build/pin-6654-congruence-empty-gates-release'
for rel, digest in a['files'].items(): assert sha(source / rel) == digest, rel
assert sha(pin / 'tactix') == a['binaries'][str((pin / 'tactix').relative_to(root))]
fixture = Path(__file__).with_name('test-congruence-empty-proofs-6656.cpp')
objects = sorted((pin / 'CMakeFiles/tactix_lib.dir').rglob('*.o')); assert objects
hashes = {str(p.relative_to(root)): sha(p) for p in [authority, fixture, *objects]}
c.mkdir()
binary = c / 'fixture'
command = ['c++', '-std=c++20', '-O2', '-fno-exceptions', '-fno-rtti', '-I'+str(source/'src'), '-I'+str(source/'include'), str(fixture), *map(str, objects), '-o', str(binary)]
with (c / 'build.log').open('x') as out: subprocess.run(command, stdout=out, stderr=subprocess.STDOUT, check=True)
records = []; removed = 0
for values in itertools.product((0,1), (0,1), (0,1), (1,2), (0,1), (0,1)):
    mode = values[0]; base = c / '-'.join(map(str, values))
    cnf, proof, effects = [base.with_suffix(s) for s in ('.cnf', '.proof', '.effects.json')]
    command = [str(binary), *map(str, values), str(cnf), str(proof), str(effects)]
    p = subprocess.run(command, capture_output=True, text=True)
    fixture_result = dict(command=command, exit_code=p.returncode, stdout=p.stdout, stderr=p.stderr)
    with base.with_suffix('.fixture.json').open('x') as out: json.dump(fixture_result, out, indent=2)
    assert p.returncode == 0, (values, p.stderr)
    checker = root / 'tools/checkers/bin' / ('lrat-check' if mode else 'drat-trim')
    command = [str(checker), str(cnf), str(proof)]
    q = subprocess.run(command, capture_output=True, text=True)
    check = dict(command=command, sha256=sha(checker), exit_code=q.returncode, stdout=q.stdout, stderr=q.stderr)
    with base.with_suffix('.checker.json').open('x') as out: json.dump(check, out, indent=2)
    assert q.returncode == 0 and ('c VERIFIED' if mode else 's VERIFIED') in q.stdout, (values, check)
    state = json.loads(effects.read_text()); assert state['models_before'] == state['models_after'] != 0
    record = dict(values=values, fixture=fixture_result, checker=check, effects=state,
                  cnf_sha256=sha(cnf), proof_sha256=sha(proof), proof_bytes=proof.stat().st_size)
    with base.with_suffix('.result.json').open('x') as out: json.dump(record, out, indent=2)
    records.append(record)
    removed += cnf.stat().st_size + proof.stat().st_size
    cnf.unlink(); proof.unlink()
assert len(records) == 64 and not list(c.glob('*.cnf')) and not list(c.glob('*.proof'))
with (c / 'summary.json').open('x') as out:
    json.dump(dict(cases=64, final_proofs_checked=64, exact_model_sets=64, records=records,
                   hashes=hashes, fixture_binary_sha256=sha(binary), removed_bytes=removed,
                   limitation='Fixed direct rule fixtures only. Existing unchanged DRAT/LRAT checker pipelines; no fuzz, runtime gate, target-year submission acceptance or promotion.'), out, indent=2)
print('PASS64 exact model sets and64 independently checked final proofs; removed bytes', removed)
