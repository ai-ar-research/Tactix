#!/usr/bin/env python3
"""Fixed baseline kernel contract, linked to unchanged current release objects."""
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260909-6568-touch-consumer-contract'
assert not c.exists()
authority = root / 'benchmarks/runs/20260909-6424-activation-stamp-build/manifest.json'
a = json.loads(authority.read_text())
source = root / a['source']
pin = root / 'build/pin-6424-activation-stamp-release'
for rel, digest in a['files'].items():
    assert sha(source / rel) == digest, rel
assert sha(pin / 'tactix') == a['binaries'][str((pin / 'tactix').relative_to(root))]
fixture = Path(__file__).with_name('test-touch-consumers-6567.cpp')
objects = sorted((pin / 'CMakeFiles/tactix_lib.dir').rglob('*.o'))
assert objects
hashes = {str(p.relative_to(root)): sha(p) for p in [authority, fixture, Path(__file__).resolve(), *objects]}
c.mkdir()
checks = {}
for name, extra in (('optimized', []), ('ubsan', ['-fsanitize=undefined', '-fno-sanitize-recover=all'])):
    binary = c / name
    command = ['/cm/local/apps/gcc/13.1.0/bin/c++', '-std=c++20', '-O2', *extra,
               '-I' + str(source / 'src'), '-I' + str(source / 'include'),
               str(fixture), *map(str, objects), '-o', str(binary)]
    with (c / (name + '-build.log')).open('x') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    run = subprocess.run([str(binary)], capture_output=True, text=True)
    record = dict(command=command, exit_code=run.returncode, stdout=run.stdout, stderr=run.stderr,
                  binary_sha256=sha(binary))
    with (c / (name + '-run.json')).open('x') as out:
        json.dump(record, out, indent=2)
        out.write('\n')
    assert run.returncode == 0 and not run.stderr, record
    assert run.stdout == 'PASS shared-touch contract: 1300 cases, 6480 exhaustive snapshots; no material-tracker implementation\n'
    checks[name] = record
    print(name, run.stdout.strip(), flush=True)
assert hashes == {rel: sha(root / rel) for rel in hashes}
with (c / 'summary.json').open('x') as out:
    json.dump(dict(hashes=hashes, checks=checks, cases_per_build=1300,
        exhaustive_snapshots_per_build=6480,
        limitation='Actual current kernel methods and fixed shared-touch sequences, not solver execution, fuzz, a candidate material tracker, native forward simulation, proof validation or runtime evidence. UBSan covers fixture/inlined code, not the prebuilt release objects. No certificate/input artifacts created.'), out, indent=2)
    out.write('\n')
