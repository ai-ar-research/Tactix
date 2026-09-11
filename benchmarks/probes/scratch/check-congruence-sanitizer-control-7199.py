#!/usr/bin/env python3
"""Classify the string-constructor failure and finish unchanged sanitizer scope."""
import json
import os
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260911-7199-congruence-sanitizer-control'
assert not c.exists()
c.mkdir()
hashes, records, removed = {}, [], []
def bind(p): hashes[str(p.relative_to(root))] = sha(p)
def run(label, command, expected=0):
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    before = set(Path('/tmp').glob('tactix-congruence-index-*'))
    p = subprocess.run(list(map(str, command)), cwd=c, capture_output=True, text=True, env=env)
    log = c / (label + '.json')
    with log.open('x') as out:
        json.dump(dict(command=list(map(str, command)), exit_code=p.returncode, stdout=p.stdout, stderr=p.stderr), out, indent=2)
    bind(log)
    records.append(dict(label=label, exit_code=p.returncode, log=str(log.relative_to(root))))
    for residue in set(Path('/tmp').glob('tactix-congruence-index-*')) - before:
        data = residue.read_bytes()
        assert data.startswith(b'd ') and data.endswith(b' 0\n'), residue
        removed.append(dict(path=str(residue), sha256=sha(residue), bytes=len(data), producer=label))
        residue.unlink()
    if expected is not None: assert p.returncode == expected, (label, p.stdout, p.stderr)
    print(label, p.returncode, flush=True)
    return p
compiler = '/cm/local/apps/gcc/13.1.0/bin/c++'
flags = ['-std=c++20', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-exceptions', '-fno-rtti']
minimal = Path(__file__).with_name('test-string-input-constructor-7199.cpp'); bind(minimal)
run('minimal-build', [compiler, *flags, minimal, '-o', c / 'minimal'])
p = run('minimal-original', [c / 'minimal'], expected=None)
assert p.returncode != 0 and 'basic_string.h:278' in p.stderr and 'unreachable program point' in p.stderr
run('minimal-repaired', [c / 'minimal', '--repaired'])
parent = root / '.claude/worktrees/runtime-vivify-compact-7144'
candidate = root / '.claude/worktrees/runtime-congruence-fused-7196'
pin = root / 'build/pin-7197-congruence-fused-sanitizers'
objects = sorted((pin / 'CMakeFiles/tactix_lib.dir').rglob('*.o')); assert objects
for obj in objects: bind(obj)
for source in (parent, candidate):
    for directory in ('src', 'include'):
        for p in (source / directory).rglob('*'):
            if p.is_file(): bind(p)
include = ['-I'+str(candidate/'src'), '-I'+str(candidate/'include')]
parent_object = c / 'parent-congruence.o'
run('parent-object', [compiler, *flags, *include, '-c', parent / 'src/rules/congruence/congruence.cpp', '-o', parent_object])
old_objects = [parent_object if str(p).endswith('/src/rules/congruence/congruence.cpp.o') else p for p in objects]
assert old_objects.count(parent_object) == 1
original = parent / 'tests/test_congruence_index_cleanup.cpp'; bind(original)
repaired = Path(__file__).with_name('test-congruence-index-read-7199.cpp'); bind(repaired)
before = original.read_text(); after = repaired.read_text()
start = after.index('  // GCC13.1'); end = after.index('\n  if (actual != expected)', start)
assert after[:start] + '  const std::string actual((std::istreambuf_iterator<char>(in)), {});' + after[end:] == before
run('parent-original-build', [compiler, *flags, *include, original, *old_objects, '-o', c/'parent-original'])
p = run('parent-original', [c/'parent-original'], expected=None)
assert p.returncode != 0 and 'basic_string.h:278' in p.stderr and 'unreachable program point' in p.stderr
for arm, selected in (('previous', old_objects), ('candidate', objects)):
    for name, fixture in (('index-repaired', repaired), ('fused-extract', candidate/'tests/test_congruence_fused_extract.cpp')):
        bind(fixture)
        exe = c / (arm+'-'+name)
        run(arm+'-'+name+'-build', [compiler, *flags, *include, fixture, *selected, '-o', exe]); bind(exe)
        output = [run(arm+'-'+name+f'-{i}', [exe]).stdout for i in range(2)]
        assert output[0] == output[1]
# Re-run the other full actual candidate fixtures without reducing scope.
for name in ('test_congruence', 'test_congruence_empty_gates'):
    exe = pin / 'tests' / name; bind(exe)
    output = [run(name+f'-{i}', [exe]).stdout for i in range(2)]
    assert output[0] == output[1]
failed = root/'benchmarks/runs/20260911-7197-congruence-fused-sanitizers'
for p in failed.glob('*.log'): bind(p)
bind(Path(__file__)); bind(parent_object); bind(c/'minimal')
with (c/'summary.json').open('x') as out:
    json.dump(dict(status='PASS', hashes=hashes, records=records, removed=removed,
        classified_failure='GCC13.1 input-iterator std::string constructor reaches its own uninitialized-length SSO check. Standalone poisoned-storage reproducer and retained-parent cleanup test both fail at the same header line. Not a new AND-loop defect.',
        repair='Only the test proof-file reader changes to initialized string plus byte append; exact original byte comparison and all1200 cases preserved. Actual7196 library unchanged. All four Congruence sanitizer fixtures twice; new648-case and repaired1200-case fixtures also pass twice against retained-parent sanitized rule object.',
        limitation='7197 original run remains FAILED and preserved. This successor classifies and repairs its fixture without suppressing any sanitizer. Broad BC, performance and current-source fuzz remain separate gates.'), out, indent=2)
print('PASS classified constructor defect, exact repaired fixture and full sanitizer scope')
