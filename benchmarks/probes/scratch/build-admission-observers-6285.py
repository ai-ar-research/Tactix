#!/usr/bin/env python3
"""Build read-only observers, require exact parent Release and full tests."""
from concurrent.futures import ThreadPoolExecutor
import difflib
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
c = root/'benchmarks/runs/20260909-6283-admission-observer-build'
parents = json.loads((c/'parents.json').read_text())
assert not (c/'manifest.json').exists()
source = root/'.claude/worktrees/runtime-admission-observer-6283'
native = root/'tools/solvers/variants/kissat-admission-observer-6283'
def run(name, command, cwd=None):
    with (c/'logs'/(name+'.log')).open('x') as log:
        subprocess.run(command, cwd=cwd, stdout=log, stderr=subprocess.STDOUT, check=True)
    print(name, 'PASS', flush=True)
def build(kind):
    if kind == 'native':
        run('native-configure', ['./configure'], native); run('native-build', ['make', '-j4'], native)
        fixture = root/'benchmarks/probes/scratch/test-native-admission-6285.c'
        run('native-fixture-build', ['gcc', '-O2', '-I'+str(native/'src'), '-I'+str(native/'build'), str(fixture), str(native/'build/libkissat.a'), '-lm', '-o', str(c/'native-fixture')])
        run('native-fixture', [str(c/'native-fixture')])
        return str((native/'build/kissat').relative_to(root)), sha(native/'build/kissat')
    out = root/('build/pin-6283-admission-'+kind); assert not out.exists()
    command = ['cmake', '-S', str(source), '-B', str(out), '-DCMAKE_BUILD_TYPE=Release']
    if kind == 'trace': command += ['-DTACTIX_ENABLE_EVENT_TRACE=ON']
    run(kind+'-configure', command); run(kind+'-build', ['cmake', '--build', str(out), '-j4'])
    run(kind+'-ctest', ['ctest', '--test-dir', str(out), '--output-on-failure', '-j1'])
    return str((out/'tactix').relative_to(root)), sha(out/'tactix')
with ThreadPoolExecutor(max_workers=3) as pool: binaries = dict(pool.map(build, ('release','trace','native')))
assert binaries['build/pin-6283-admission-release/tactix'] == sha(root/'build/pin-6269-vivify-binary-retry-release/tactix')
files = {}; patches = {}
for path, parent in parents['parents'].items():
    new = root/path; old = root/parent['source']
    current = {str(p.relative_to(new)):sha(p) for folder in ('src','include','tests') if (new/folder).exists()
               for p in (new/folder).rglob('*') if p.is_file()}
    changed = {p for p,digest in current.items() if parent['files'].get(p) != digest}
    expected = {'src/policies/default_policy.cpp','tests/test_eliminate_admission_trace.cpp'} if new == source else {'src/eliminate.c','src/makefile'}
    assert changed == expected, changed
    if new == native:
        assert (new/'src/makefile').read_text() == (old/'src/makefile').read_text().replace(str(old),str(new))
    patch = ''
    for rel in sorted(changed - {'src/makefile'}):
        a,b = old/rel,new/rel
        patch += ''.join(difflib.unified_diff(a.read_text().splitlines(True) if a.exists() else [], b.read_text().splitlines(True),
             fromfile='a/'+rel if a.exists() else '/dev/null', tofile='b/'+rel, n=0))
    p = c/('candidate.patch' if new == source else 'native.patch'); p.write_text(patch)
    patches[p.name] = sha(p); files[path] = current
(c/'manifest.json').write_text(json.dumps(dict(files=files, binaries=binaries, patches=patches,
    parents_sha256=sha(c/'parents.json'), release_exact_parent=True,
    native_fixture=(c/'logs/native-fixture.log').read_text(),
    limitation='Read-only admission diagnostics. No strategy/rule change. Full per-arm trace/proof replay required before interpreting the selected Rank50 gate; actual candidate source is6269, not legacy6107.'), indent=2)+'\n')
print(json.dumps(binaries, indent=2))
