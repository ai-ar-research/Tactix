#!/usr/bin/env python3
"""Independent complete Debug suite; caller may advance on separate gates."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
source = root / '.claude/worktrees/runtime-debug-contracts-7339'
pin = root / 'build/pin-7342-debug-contracts-debug'
campaign = root / 'benchmarks/runs/20260911-7342-debug-contracts-debug'
assert not pin.exists() and not campaign.exists()
(campaign / 'logs').mkdir(parents=True)
files = {str(p.relative_to(source)): sha(p) for directory in ('src','include','tests','examples')
         for p in sorted((source / directory).rglob('*')) if p.is_file()}
files['CMakeLists.txt'] = sha(source / 'CMakeLists.txt')
with (campaign / 'started.json').open('x') as out:
    json.dump(dict(source=str(source.relative_to(root)), files=files,
                   helper_sha256=sha(Path(__file__))), out, indent=2)
commands = [
    ('configure', ['cmake','-S',source,'-B',pin,'-DCMAKE_BUILD_TYPE=Debug',
       '-DTACTIX_ENABLE_EVENT_TRACE=OFF','-DTACTIX_ENABLE_RULE_JOURNAL=OFF',
       '-DTACTIX_ENABLE_PROOF_SHADOW=OFF','-DTACTIX_ENABLE_CONFLICT_STOP=OFF',
       '-DCMAKE_C_COMPILER=/cm/local/apps/gcc/13.1.0/bin/gcc',
       '-DCMAKE_CXX_COMPILER=/cm/local/apps/gcc/13.1.0/bin/g++']),
    ('build', ['cmake','--build',pin,'-j4']),
    ('ctest', ['ctest','--test-dir',pin,'--output-on-failure','-j1'])]
hashes = {}
for name, command in commands:
    command = list(map(str, command))
    result = subprocess.run(command, cwd=campaign, capture_output=True, text=True)
    path = campaign / 'logs' / (name + '.json')
    with path.open('x') as out:
        json.dump(dict(command=command, returncode=result.returncode,
                       stdout=result.stdout, stderr=result.stderr), out, indent=2)
    hashes[str(path.relative_to(root))] = sha(path)
    assert result.returncode == 0, (name,result.stdout[-4000:],result.stderr[-4000:])
    if name == 'ctest': assert '100% tests passed out of 123' in result.stdout
    print(name, 'PASS', flush=True)
for rel, digest in files.items(): assert sha(source / rel) == digest, rel
hashes[str((pin / 'tactix').relative_to(root))] = sha(pin / 'tactix')
hashes[str((campaign / 'started.json').relative_to(root))] = sha(campaign / 'started.json')
with (campaign / 'manifest.json').open('x') as out:
    json.dump(dict(status='PASS', hashes=hashes, tests=123,
                   scope='Complete Debug suite, not Release or sanitizer surrogate.'), out, indent=2)
