#!/usr/bin/env python3
"""Independent actual-parent/candidate DRAT/LRAT checks of Sweep filtering."""
from pathlib import Path
import json
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260910-6845-sweep-rootfree-proofs'
assert not c.exists(); c.mkdir()
fixture = root/'benchmarks/probes/scratch/test-sweep-rootfree-proof-6845.cpp'
build = root/'benchmarks/runs/20260910-6838-sweep-rootfree-build/manifest.json'
assert build.exists()
records = []; removed = 0; outputs = {}; binaries = {}
for arm, suffix in (('previous', '6730-reduce-watermark'), ('candidate', '6838-sweep-rootfree')):
    pin = root/('build/pin-'+suffix+'-release')
    source = root/('.claude/worktrees/runtime-'+('reduce-watermark-6730' if arm == 'previous' else 'sweep-rootfree-6838'))
    objects = sorted((pin/'CMakeFiles/tactix_lib.dir').rglob('*.o'))
    exe = c/(arm+'-fixture')
    command = ['/cm/local/apps/gcc/13.1.0/bin/c++','-std=c++20','-O2','-I'+str(source/'src'),'-I'+str(source/'include'),str(fixture),*map(str,objects),'-o',str(exe)]
    with (c/(arm+'-build.log')).open('x') as log: subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    binaries[arm] = sha(exe); outputs[arm] = []
    for roots in range(3):
        for position in range(5):
            for strip in (0,1):
                for mode in (0,1):
                    name = f'{arm}-{roots}-{position}-{strip}-{mode}'
                    cnf = c/(name+'.cnf'); proof = c/(name+'.proof')
                    invocation = [str(exe),str(roots),str(position),str(strip),str(mode),str(cnf),str(proof)]
                    p = subprocess.run(invocation, cwd=c, capture_output=True, text=True)
                    with (c/(name+'-fixture.json')).open('x') as log:
                        json.dump(dict(command=invocation, returncode=p.returncode, stdout=p.stdout, stderr=p.stderr), log, indent=2); log.write('\n')
                    assert p.returncode == 0, (name, p.stderr)
                    checker = root/('tools/checkers/bin/lrat-check' if mode else 'tools/checkers/bin/drat-trim')
                    command = [str(checker),str(cnf),str(proof)]
                    q = subprocess.run(command, capture_output=True, text=True)
                    record = dict(name=name, command=command, returncode=q.returncode, checker_sha256=sha(checker), stdout=q.stdout, stderr=q.stderr,
                        fixture_stdout=p.stdout, input_sha256=sha(cnf), proof_sha256=sha(proof), proof_bytes=proof.stat().st_size)
                    with (c/(name+'-checker.json')).open('x') as log:
                        json.dump(record, log, indent=2); log.write('\n')
                    assert q.returncode == 0 and ('c VERIFIED' if mode else 's VERIFIED') in q.stdout and 'not verified' not in q.stdout.lower(), (name,q.stdout,q.stderr)
                    outputs[arm].append((p.stdout,sha(cnf),sha(proof),proof.stat().st_size))
                    records.append(record)
                    removed += cnf.stat().st_size + proof.stat().st_size
                    proof.unlink(); cnf.unlink()
assert outputs['previous'] == outputs['candidate']
with (c/'summary.json').open('x') as log:
    json.dump(dict(checked=len(records), records=records, removed_bytes=removed, binaries=binaries,
        fixture_sha256=sha(fixture), source_manifest_sha256=sha(build),
        limitation='60 independent fixed proofs per arm, exact source-control outputs/certificate bytes. Root-free, all/mixed root-false clauses, five literal positions, strip on/off, general emitter. Final empty chain needs the emitted equivalence; no checker-only contradictory units. Not broad B/C or current-source fuzz acceptance.'),log,indent=2); log.write('\n')
print('PASS',len(records),'fixed proofs; removed',removed,'bytes')
