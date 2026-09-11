#!/usr/bin/env python3
"""Build an isolated memory observer with byte-exact disabled controls."""
from concurrent.futures import ThreadPoolExecutor
import difflib
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260911-7285-memory-only-build'
assert not c.exists(), 'preserve successful or failed build evidence'
(c / 'logs').mkdir(parents=True)
arms = {
    'candidate': ('runtime-realloc-memory-only-7285', '7271-dense-realloc', '7267-dense-realloc'),
    'previous': ('runtime-occurrence-memory-only-7285', '7253-occurrence-cutover', '7253-occurrence-cutover')}
expected = {'CMakeLists.txt', 'src/diagnostics/event_trace.hpp',
    'src/diagnostics/memory_only.cpp', 'src/kernel/kernel.cpp',
    'src/policies/default_policy.cpp', 'tests/CMakeLists.txt',
    'tests/test_collect_memory.cpp', 'tests/test_memory_only.cpp'}
hashes, sources, commands, parents, patches = {}, {}, {}, {}, {}


def bind(p, digest=None):
    actual = sha(p)
    assert digest is None or digest == actual, p
    hashes[str(p.relative_to(root))] = actual


for arm, (stem, authority, pin) in arms.items():
    p = root / f'benchmarks/runs/20260911-{authority}-build/manifest.json'
    parent = json.loads(p.read_text())
    bind(p)
    parents[arm] = parent
    source = root / '.claude/worktrees' / stem
    sources[arm] = source
    files = {str(p.relative_to(source)): sha(p)
             for directory in ('src', 'include', 'tests')
             for p in (source / directory).rglob('*') if p.is_file()}
    files['CMakeLists.txt'] = sha(source / 'CMakeLists.txt')
    changed = {rel for rel, digest in files.items() if parent['files'].get(rel) != digest}
    assert changed == expected, (arm, changed ^ expected)
    patch = ''
    for rel in sorted(files):
        bind(source / rel, files[rel])
        if rel not in changed:
            continue
        old = root / parent['source'] / rel
        patch += ''.join(difflib.unified_diff(
            old.read_text().splitlines(True) if old.exists() else [],
            (source / rel).read_text().splitlines(True),
            fromfile='a/' + rel if old.exists() else '/dev/null', tofile='b/' + rel))
    p = c / f'{arm}-observer.patch'
    with p.open('x') as out:
        out.write(patch)
    bind(p)
    patches[arm] = str(p.relative_to(root))
    for rel, digest in parent['files'].items():
        bind(root / parent['source'] / rel, digest)
    binary = root / f'build/pin-{pin}-release/tactix'
    bind(binary, parent['binaries'][str(binary.relative_to(root))])


def run(label, command):
    command = list(map(str, command))
    p = subprocess.run(command, cwd=c, text=True, capture_output=True)
    log = c / 'logs' / (label + '.log')
    with log.open('x') as out:
        out.write(p.stdout + p.stderr)
    commands[label] = dict(command=command, exit_code=p.returncode)
    assert p.returncode == 0, (label, p.returncode, p.stdout, p.stderr)
    print(label, 'PASS', flush=True)
    return p.stdout


def build(item):
    arm, enabled = item
    kind = 'observer' if enabled else 'control'
    label = arm + '-' + kind
    out = root / f'build/pin-7285-memory-{label}'
    assert not out.exists(), out
    run(label + '-configure', ['cmake', '-S', sources[arm], '-B', out,
        '-DCMAKE_BUILD_TYPE=Release', '-DTACTIX_ENABLE_EVENT_TRACE=OFF',
        '-DTACTIX_ENABLE_RULE_JOURNAL=OFF', '-DTACTIX_ENABLE_PROOF_SHADOW=OFF',
        '-DTACTIX_ENABLE_CONFLICT_STOP=OFF',
        '-DTACTIX_ENABLE_MEMORY_ONLY=' + ('ON' if enabled else 'OFF'),
        '-DCMAKE_C_COMPILER=/cm/local/apps/gcc/13.1.0/bin/gcc',
        '-DCMAKE_CXX_COMPILER=/cm/local/apps/gcc/13.1.0/bin/g++'])
    run(label + '-build', ['cmake', '--build', out, '-j2'])
    run(label + '-ctest', ['ctest', '--test-dir', out, '--output-on-failure', '-j1'])
    binary = out / 'tactix'
    original = root / f'build/pin-{arms[arm][2]}-release/tactix'
    if not enabled:
        assert sha(binary) == sha(original), ('disabled control differs', arm)
    markers = binary.read_bytes()
    assert (b'TACTIX_SLOT_MEMORY_TRACE' in markers) == enabled
    assert b'TACTIX_EVENT_TRACE\x00' not in markers
    symbols = subprocess.check_output(['nm', '-C', binary], text=True)
    assert ('tactix::memory_only_checkpoint(' in symbols) == enabled
    assert 'tactix::EventTrace::' not in symbols
    return label, dict(path=str(binary.relative_to(root)), sha256=sha(binary),
                       observer=enabled, exact_disabled_control=not enabled)


with ThreadPoolExecutor(max_workers=4) as pool:
    binaries = dict(pool.map(build, [(arm, enabled) for arm in arms for enabled in (False, True)]))
for rel, digest in list(hashes.items()):
    assert sha(root / rel) == digest, rel
for p in (c / 'logs').iterdir():
    bind(p)
bind(Path(__file__))
with (c / 'manifest.json').open('x') as out:
    json.dump(dict(status='PASS', hashes=hashes, binaries=binaries,
        sources={arm: str(source.relative_to(root)) for arm, source in sources.items()},
        changed=sorted(expected), patches=patches, commands=commands,
        hypothesis='The full EVENT_TRACE build perturbs Rank31 allocator history enough to reverse the observed Release RSS regression. Isolate existing memory checkpoints while retaining all other Release no-op tracing paths.',
        gate='Both OFF controls byte-identical to actual7267/7253 Release. Both observer variants pass full suites and enabled/disabled/bad-path capacity-boundary tests, with no EventTrace linked. Real pair must reproduce whole own Release work/certificates and the adverse RSS direction before attribution; exact structural rows and absolute peaks reported, never substituted silently for Release.',
        limitation='Diagnostic only, no runtime speedup or memory fix claimed. Checkpoint calls and allocator sampling can still perturb execution. No normal runtime pin, source, budgeting, rule guard, proof path or scheduling changed. Independent proofs asynchronous; current-source fuzz not accepted.'), out, indent=2)
print(c / 'manifest.json')
