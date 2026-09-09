#!/usr/bin/env python3
"""Build/test distinct Release/BC/trace pins for the general removal repair."""
from concurrent.futures import ThreadPoolExecutor
import difflib
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
source = root / '.claude/worktrees/runtime-substitute-retry-6231'
parent = root / '.claude/worktrees/runtime-learned-sort-6169'
c = root / 'benchmarks/runs/20260909-6231-substitute-retry-build'
assert not (c / 'manifest.json').exists()
def run(name, command, cwd=None):
    with (c / 'logs' / (name + '.log')).open('x') as out:
        p = subprocess.run(command, cwd=cwd, stdout=out, stderr=subprocess.STDOUT)
    assert p.returncode == 0, (name, p.returncode)
    print(name, 'PASS', flush=True)
def build(kind):
    out = root / ('build/pin-6231-substitute-retry-' + kind)
    assert not out.exists()
    command = ['cmake', '-S', str(source), '-B', str(out), '-DCMAKE_BUILD_TYPE=Release']
    if kind == 'trace': command += ['-DTACTIX_ENABLE_EVENT_TRACE=ON']
    run(kind + '-configure', command)
    run(kind + '-build', ['cmake', '--build', str(out), '-j4'])
    run(kind + '-ctest', ['ctest', '--test-dir', str(out), '--output-on-failure', '-j1'])
    run(kind + '-image-retry', ['env', 'TACTIX_SUBSTITUTE_IMAGE_PATH=1', str(out / 'tests/test_substitute_retry')], cwd=c)
    return str((out / 'tactix').relative_to(root)), sha(out / 'tactix')
with ThreadPoolExecutor(max_workers=3) as pool:
    binaries = dict(pool.map(build, ('release', 'bc', 'trace')))
assert binaries['build/pin-6231-substitute-retry-release/tactix'] == binaries['build/pin-6231-substitute-retry-bc/tactix']
files = {str(p.relative_to(source)): sha(p) for d in ('src', 'include', 'tests')
         for p in sorted((source / d).rglob('*')) if p.is_file()}
files['CMakeLists.txt'] = sha(source / 'CMakeLists.txt')
changed = [r for r in files if not (parent / r).exists() or sha(parent / r) != files[r]]
assert set(changed) == {'src/rules/substitute/substitute.cpp', 'tests/test_substitute_retry.cpp'}
patches = {}
for name, base in (('incremental.patch', parent), ('cumulative.patch', root)):
    patch = ''
    for rel in sorted(files):
        a, b = base / rel, source / rel
        if a.exists() and sha(a) == sha(b): continue
        patch += ''.join(difflib.unified_diff(a.read_text().splitlines(True) if a.exists() else [],
            b.read_text().splitlines(True), fromfile='a/'+rel if a.exists() else '/dev/null', tofile='b/'+rel, n=0))
    (c / name).write_text(patch); patches[name] = sha(c / name)
(c / 'manifest.json').write_text(json.dumps(dict(source=str(source.relative_to(root)), parent=str(parent.relative_to(root)),
    source_commit='c2a95a6c79c21a76516cf556b6538e76da369c34', production='c2a95a6c79c21a76516cf556b6538e76da369c34',
    files=files, binaries=binaries, patches=patches, patch_application='git apply --unidiff-zero',
    hypothesis='Publish per-physical-copy canonical Substitute removal advice after binary-unit closure; omit root-fixed endpoints and surviving rewrites. N1/proof/reconstruction unchanged.',
    test_consideration='Actual frozen parent fails36 fixed state-effect fixtures without crashing. Both proof modes/public instances/import orders/duplicates and forced image path covered. Full source suites in three independent pins; additional reference/model/proof fixtures and semantic gates required.',
    status='Isolated unpromoted/unfuzzed; runtime and broad certificates not yet measured.',
    logs={str(p.relative_to(c)): sha(p) for p in (c / 'logs').iterdir()}), indent=2)+'\n')
print(json.dumps(binaries, indent=2))
