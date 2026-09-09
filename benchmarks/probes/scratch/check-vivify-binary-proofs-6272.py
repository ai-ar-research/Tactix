#!/usr/bin/env python3
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260909-6272-vivify-binary-fixed-proofs'
assert not c.exists(); c.mkdir()
source = root/'.claude/worktrees/runtime-vivify-binary-retry-6269'
pin = root/'build/pin-6269-vivify-binary-retry-release'
fixture = root/'benchmarks/probes/scratch/test-vivify-binary-proof-6272.cpp'
assert (root/'benchmarks/runs/20260909-6269-vivify-binary-retry-build/manifest.json').exists()
objects = sorted((pin/'CMakeFiles/tactix_lib.dir').rglob('*.o'))
command = ['/cm/local/apps/gcc/13.1.0/bin/c++', '-std=c++20', '-O2', '-I'+str(source/'src'), '-I'+str(source/'include'), str(fixture), *map(str, objects), '-o', str(c/'fixture')]
with (c/'build.log').open('w') as log: subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
records = []; removed = 0
for instantiate in (0, 1):
    for mode in (0, 1):
        for reuse in (0, 1):
            name = f'{instantiate}-{mode}-{reuse}'; cnf = c/(name+'.cnf'); proof = c/(name+'.proof')
            command = [str(c/'fixture'), str(instantiate), str(mode), str(reuse), str(cnf), str(proof)]
            p = subprocess.run(command, cwd=c, capture_output=True, text=True)
            (c/(name+'-fixture.log')).write_text(p.stdout+p.stderr); assert p.returncode == 0
            checker = root/('tools/checkers/bin/lrat-check' if mode else 'tools/checkers/bin/drat-trim')
            command = [str(checker), str(cnf), str(proof)]
            p = subprocess.run(command, capture_output=True, text=True)
            # Preserve exact checker bytes as escaped JSON, avoiding Git's
            # whitespace normalization of raw drat-trim output.
            assert p.returncode == 0 and ('c VERIFIED' if mode else 's VERIFIED') in p.stdout
            records.append(dict(name=name, command=command, checker_sha256=sha(checker), stdout=p.stdout, stderr=p.stderr,
                input_sha256=sha(cnf), proof_sha256=sha(proof), proof_bytes=proof.stat().st_size))
            removed += cnf.stat().st_size + proof.stat().st_size
            proof.unlink(); cnf.unlink()
(c/'summary.json').write_text(json.dumps(dict(checked=8, records=records, fixture_sha256=sha(fixture), binary_sha256=sha(c/'fixture'),
    removed_bytes=removed, cleanup='Eight generated fixed inputs and checked proofs deleted; exact outputs, hashes and sources retained for reproduction.'), indent=2)+'\n')
print('PASS: eight independently checked DRAT/LRAT proofs; exhaustive model fixture per case; removed bytes', removed)
