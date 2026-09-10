#!/usr/bin/env python3
"""Independent fully instrumented build for the approved view lifetime scope."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
out = root / 'benchmarks/runs/20260910-7090-bve-view-sanitizers'
build = root / 'build/pin-7087-bve-views-sanitizers'
assert not out.exists() and not build.exists()
out.mkdir()
authority = root / 'benchmarks/runs/20260910-7087-bve-views-build/manifest.json'
m = json.loads(authority.read_text())
source = root / m['source']
for rel, digest in m['files'].items(): assert sha(source / rel) == digest
checks = []
commands = [
    ['cmake', '-S', source, '-B', build, '-DCMAKE_BUILD_TYPE=Debug',
     '-DCMAKE_CXX_FLAGS=-O1 -fsanitize=address,undefined -fno-omit-frame-pointer',
     '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'],
    ['cmake', '--build', build, '-j4', '--target', 'test_bve_antecedent_view', 'test_bve', 'test_bve_memory'],
    ['ctest', '--test-dir', build, '-R', '^bve(_antecedent_view|_memory)?$', '--output-on-failure']]
for index, command in enumerate(commands):
    command = list(map(str, command))
    log = out / f'{index}.log'
    with log.open('x') as stream:
        p = subprocess.run(command, cwd=root, stdout=stream, stderr=subprocess.STDOUT)
    checks.append(dict(command=command, exit_code=p.returncode, log=str(log.relative_to(root)), sha256=sha(log)))
    assert p.returncode == 0, log
    print(index, 'PASS', flush=True)
hashes = {str(authority.relative_to(root)): sha(authority), str(Path(__file__).relative_to(root)): sha(Path(__file__))}
for name in ('test_bve_antecedent_view', 'test_bve', 'test_bve_memory'):
    binary = build / 'tests' / name
    hashes[str(binary.relative_to(root))] = sha(binary)
with (out / 'summary.json').open('x') as stream:
    json.dump(dict(checks=checks, hashes=hashes, scope='Fully ASan/UBSan-instrumented BVE preparation, full BVE and BVE-memory fixed tests. No fuzz or real-instance solving.'), stream, indent=2)
    stream.write('\n')
