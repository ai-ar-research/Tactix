#!/usr/bin/env python3
"""Build observer-only6508 with strict Release identity and actual-rule tests."""
from concurrent.futures import ThreadPoolExecutor
import difflib
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260909-6508-congruence-stages-build'
p = json.loads((c / 'parents.json').read_text())
source, old = root / p['candidate'], root / p['source']
assert not (c / 'manifest.json').exists()
assert sha(root / p['authority']['path']) == p['authority']['sha256']
def run(name, command):
    with (c / 'logs' / (name + '.log')).open('x') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, cwd=root)
    print(name, 'PASS', flush=True)
def build(kind):
    out = root / ('build/pin-6508-congruence-stages-' + kind)
    assert not out.exists()
    run(kind+'-configure', ['cmake', '-S', str(source), '-B', str(out), '-DCMAKE_BUILD_TYPE=Release',
        '-DTACTIX_ENABLE_CONFLICT_STOP=OFF', '-DTACTIX_ENABLE_EVENT_TRACE='+('ON' if kind=='trace' else 'OFF')])
    run(kind+'-build', ['cmake', '--build', str(out), '-j4'])
    run(kind+'-ctest', ['ctest', '--test-dir', str(out), '--output-on-failure', '-j1'])
    count = 109 if kind == 'release' else 108
    assert f'100% tests passed out of {count}' in (c/'logs'/(kind+'-ctest.log')).read_text()
    return str((out/'tactix').relative_to(root)), sha(out/'tactix')
with ThreadPoolExecutor(max_workers=2) as pool:
    binaries = dict(pool.map(build, ('release', 'trace')))
assert binaries['build/pin-6508-congruence-stages-release/tactix'] == '5946f59800adbf1bbbb20c64ac4bcbcd00ff8dc980ebfdc04edd844db2cd7eb4'
for rel, digest in p['files'].items():
    assert sha(old/rel) == digest, rel
files = {str(f.relative_to(source)): sha(f) for folder in ('src','include','tests')
    for f in (source/folder).rglob('*') if f.is_file()}
changed = {r for r,d in files.items() if p['files'].get(r) != d}
assert changed == {'src/rules/congruence/congruence.cpp','tests/CMakeLists.txt','tests/test_congruence_stage_trace.cpp'}, changed
patch = ''
for rel in sorted(changed):
    a,b = old/rel,source/rel
    patch += ''.join(difflib.unified_diff(a.read_text().splitlines(True) if a.exists() else [],
        b.read_text().splitlines(True), fromfile='a/'+rel if a.exists() else '/dev/null', tofile='b/'+rel,n=0))
(c/'candidate.patch').write_text(patch)
parent = json.loads((root/p['authority']['path']).read_text())
(c/'manifest.json').write_text(json.dumps(dict(source=p['candidate'], files=files, binaries=binaries,
    source_commit=parent.get('source_commit'), parents_sha256=sha(c/'parents.json'),
    parent_authority=p['authority'], patch_sha256=sha(c/'candidate.patch'), release_exact_parent=True,
    logs={str(f.relative_to(c)):sha(f) for f in (c/'logs').iterdir()},
    tests='109 Release and108 trace; actual Congruence disabled/selected/wrong-conflict state and proof identity.',
    limitation='Observer only. Actual full parent1M ordinary trace and partial-proof replay required before interpretation; no production promotion.'),indent=2)+'\n')
print(json.dumps(binaries,indent=2))
