#!/usr/bin/env python3
"""Freeze target-dispatch source after actual-code and independent proof checks."""
import difflib
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5741-reuse-target'
source = root / '.claude/worktrees/runtime-reuse-target-5741'
parent = root / '.claude/worktrees/runtime-root-conflict-reuse-5718'
builds = {a:root / f'build/pin-{n}-reuse-target-{a}' for a,n in [('release',5741),('trace',5742),('bc',5743)]}
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
assert not (c / 'manifest.json').exists()
assert sha(builds['release'] / 'tactix') == sha(builds['bc'] / 'tactix')
for arm in builds:
    expected = 91 if arm == 'trace' else 92
    assert f'100% tests passed out of {expected}' in (c / 'logs' / f'ctest-r1-{arm}.log').read_text()
v = json.loads((c / 'fixed-validation/summary.json').read_text())
t = json.loads((c / 'target-validation/summary.json').read_text())
assert v['native_cases'] == 24 and v['all_native_states_equal'] and len(v['micro_proofs']) == 288
assert t['predecessor_cases'] == t['current_cases'] == 2400 and len(t['micro_proofs']) == 4800
assert t['policy_transition_rows'] == 240 and t['policy_transition_rows_equal']
for validation in (v, t):
    for rel, digest in validation['hashes'].items():
        assert sha(root / rel) == digest, rel
files = ['src/rules/analyze/analyze.cpp', 'tests/test_root_conflict_reuse.cpp', 'tests/test_conflict_reuse_targets.cpp']
changed = [str(p.relative_to(source)) for folder in ('src', 'tests') for p in (source / folder).rglob('*')
    if p.is_file() and (not (parent / p.relative_to(source)).exists() or sha(p) != sha(parent / p.relative_to(source)))]
assert sorted(changed) == sorted(files), changed
def patch(base, names):
    return ''.join(''.join(difflib.unified_diff(
        (base / name).read_text().splitlines(True) if (base / name).exists() else [],
        (source / name).read_text().splitlines(True),
        fromfile='a/' + name if (base / name).exists() else '/dev/null', tofile='b/' + name)) for name in names)
(c / 'repair.patch').write_text(patch(parent, files))
(c / 'cumulative.patch').write_text(patch(root, [str(p.relative_to(root)) for p in sorted((root / 'src').rglob('*')) if p.is_file()]))
m = dict(source=str(source.relative_to(root)), parent=str(parent.relative_to(root)),
    production='c2a95a6c79c21a76516cf556b6538e76da369c34',
    purpose='Honor public canonical/explicit clamped reuse targets in normal and OTFS paths; compile-time policy specialization preserves DefaultPolicy chronology.',
    test_consideration='92/92 each independent Release,91/91 trace;2400 current target cases,2400 actual predecessor controls,240 equal policy observations,24 actual native transitions;5088 independent DRAT/LRAT proof checks including2400 parent proofs. Fixture-localization.json explains invalid OTFS reason-watch setup corrected by actual propagation, without changing solver behavior to satisfy the fixture.',
    files={name:sha(source / name) for name in files},
    binaries={str((b / 'tactix').relative_to(root)):sha(b / 'tactix') for b in builds.values()},
    patches={p.name:sha(p) for p in c.glob('*.patch')},
    logs={p.name:sha(p) for p in (c / 'logs').iterdir()},
    validation={str(p.relative_to(root)):sha(p) for folder in ('fixed-validation','target-validation')
        for p in (c / folder).iterdir() if p.suffix in ('.json','.log')},
    initial_fixture={str(p.relative_to(root)):sha(p) for p in [c / 'initial-target-fixture.cpp',c / 'initial-target-fixture.json',c / 'fixture-localization.json']},
    status='Isolated UNFUZZED successor;not submitted or promoted. Full DefaultPolicy replay,B28+C5 and runtime evidence pending. Future full views use fixed Kissat reference,not routine new Kissat solves.')
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print('Frozen5741:92/92 twice,91/91 trace;5088 independently checked micro-proofs')
