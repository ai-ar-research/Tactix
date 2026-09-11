#!/usr/bin/env python3
"""Repair clone-only support routing; validate unchanged7267 compiled pins."""
import json
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha

source = root / '.claude/worktrees/runtime-dense-realloc-7267'
parent = root / '.claude/worktrees/runtime-single-occurrence-cutover-7253'
failed = root / 'benchmarks/runs/20260911-7267-dense-realloc-build'
destination = root / 'benchmarks/runs/20260911-7270-dense-realloc-build'
assert not destination.exists(), 'never overwrite either validation attempt'
prior_logs = {str(p.relative_to(root)): sha(p) for p in (failed / 'logs').iterdir()}
assert len(prior_logs) == 9
for kind in ('release', 'bc', 'trace'):
    log = (failed / 'logs' / (kind + '-ctest.log')).read_text()
    assert '22 tests failed' in log and 'FileNotFoundError' in log
    assert "tools/docs/check-config-wiring.py': [Errno 2]" in log
    assert '***Exception' not in log

# Reproduce the green parent's existing support routing, not new solver code.
support = {}
for name in ('tools', 'benchmarks', 'DEVELOPMENT_PLAN.md'):
    reference = parent / name
    target = source / name
    assert reference.is_symlink() and reference.resolve() == root / name
    assert not target.exists() and not target.is_symlink()
    target.symlink_to(reference.readlink(), target_is_directory=reference.is_dir())
    support[name] = str(target.readlink())

pins = {str(p.relative_to(root)): sha(p)
        for kind in ('release', 'bc', 'trace')
        for p in (root / ('build/pin-7267-dense-realloc-' + kind)).rglob('*')
        if p.is_file() and (p.name == 'tactix' or p.suffix == '.o')}
assert len(pins) > 100

# Expand the failed writer without executing it. Retain every existing fixture,
# actual-parent oracle and fatal-path control; only skip already-green builds.
original = Path(__file__).with_name('build-dense-realloc-7267.py')
outer = original.read_text()
last = "exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})"
assert outer.count(last) == 1
scope = {'__file__': str(original), '__name__': 'expand_only'}
exec(compile(outer.replace(last, 'expanded = code'), str(original), 'exec'), scope)
code = scope['expanded']
assert code.count('20260911-7267-dense-realloc-build') == 1
code = code.replace('20260911-7267-dense-realloc-build', '20260911-7270-dense-realloc-build')
start = code.index('    assert not out.exists()')
end = code.index("    run(kind + '-ctest'", start)
code = code[:start] + '    assert out.is_dir()\n' + code[end:]
line = "    run(kind + '-ctest', ['ctest', '--test-dir', out, '--output-on-failure', '-j1'])"
assert code.count(line) == 1
code = code.replace(line, line.replace('    run(', '    full = run(') + "\n    assert '100% tests passed, 0 tests failed' in full and 'Skipped' not in full\n    assert ('out of 118' if kind == 'trace' else 'out of 119') in full")
exec(compile(code, str(original), 'exec'), {'__file__': str(Path(__file__)), '__name__': '__main__'})

for rel, digest in {**prior_logs, **pins}.items():
    assert sha(root / rel) == digest, rel
with (destination / 'support-repair.json').open('x') as out:
    json.dump(dict(status='PASS', previous_attempt=str(failed.relative_to(root)),
        classification='Missing source-copy support routes caused22 Python/tool failures and2 skipped checker tests per suite; compiled kernel/rule fixtures passed. All full suites rerun without skips, binary/object hashes unchanged.',
        failed_logs=prior_logs, unchanged_compiled_files=pins, support_routes=support,
        helper_sha256=sha(Path(__file__)), original_writer_sha256=sha(original)), out, indent=2)
print('PASS: repaired support routing only; all failed logs and compiled pins unchanged')
