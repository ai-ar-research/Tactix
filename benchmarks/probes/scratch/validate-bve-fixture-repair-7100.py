#!/usr/bin/env python3
"""Direct fixture repair validation, all sanitizer checks enabled; no fuzz."""
import json
import os
from pathlib import Path
import resource
import signal
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
out = root / 'benchmarks/runs/20260910-7100-bve-fixture-repair'
assert not out.exists()
(out / 'logs').mkdir(parents=True)
authority = root / 'benchmarks/runs/20260910-7087-bve-views-build/manifest.json'
m = json.loads(authority.read_text())
frozen = root / m['source']
source = root / '.claude/worktrees/runtime-bve-fixture-repair-7100'
fixture = 'tests/test_bve_antecedent_view.cpp'
hashes = {}
def bind(path):
    hashes[str(path.relative_to(root))] = sha(path)
for rel, digest in m['files'].items():
    assert sha(frozen / rel) == digest, rel
    if rel != fixture:
        assert sha(source / rel) == digest, rel
assert sha(source / fixture) != sha(frozen / fixture)
bind(authority); bind(source / fixture); bind(Path(__file__))
build = root / 'build/pin-7087-bve-views-sanitizers'
objects = sorted((build / 'CMakeFiles/tactix_lib.dir').rglob('*.o'))
assert len(objects) > 30
for obj in objects: bind(obj)
env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:abort_on_error=1',
           UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
checks = []
def no_core(): resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
def run(name, args, crash=False):
    args = list(map(str, args))
    p = subprocess.run(args, cwd=root, env=env, capture_output=True, text=True,
                       preexec_fn=no_core)
    log = out / 'logs' / (name + '.json')
    record = dict(command=args, exit_code=p.returncode, stdout=p.stdout, stderr=p.stderr)
    with log.open('x') as stream: json.dump(record, stream, indent=2)
    bind(log)
    if crash:
        assert p.returncode == -signal.SIGABRT, record
        assert 'assigned_count + k.unassigned == k.nvars' in p.stderr, record
    else:
        assert p.returncode == 0, record
        assert not any(x in p.stderr for x in ('ERROR: AddressSanitizer', 'LeakSanitizer', 'runtime error:')), record
    checks.append(dict(name=name, command=args, exit_code=p.returncode, expected_abort=crash,
                       log=str(log.relative_to(root))))
    print(name, 'EXPECTED_ABORT' if crash else 'PASS', flush=True)
exe = out / 'test_bve_antecedent_view'
run('compile-fixed', ['/cm/local/apps/gcc/13.1.0/bin/c++', '-std=c++20', '-O1', '-g',
    '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-exceptions', '-fno-rtti',
    '-I' + str(source / 'src'), '-I' + str(source / 'include'), source / fixture,
    *objects, '-o', exe])
bind(exe)
original = build / 'tests/test_bve_antecedent_view'; bind(original)
run('original-reproduced', [original], crash=True)
run('fixed-view', [exe])
for name in ('test_bve', 'test_bve_memory'):
    binary = build / 'tests' / name; bind(binary)
    run(name, [binary])
for rel, digest in m['files'].items(): assert sha(frozen / rel) == digest, rel
with (out / 'summary.json').open('x') as stream:
    json.dump(dict(status='PASS', checks=checks, hashes=hashes,
        environment={k: env[k] for k in ('ASAN_OPTIONS', 'UBSAN_OPTIONS')},
        scope='Fixed BVE test against original fully ASan/UBSan instrumented Debug objects; original abort reproduced, corrected view and full BVE/memory tests pass with leak detection enabled. Only fixture changed. No fuzz or real-instance run.',
        authority='User explicitly instructed Codex to stop Claude and fix directly.'), stream, indent=2)
print(out / 'summary.json', flush=True)
