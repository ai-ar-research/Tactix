#!/usr/bin/env python3
"""Seal the tested preconnection cleanup; never modify an existing seal."""
import difflib
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5870-forward-preclean'
source = root / '.claude/worktrees/runtime-forward-preclean-5870'
parent = root / '.claude/worktrees/runtime-forward-caller-5849'
authority = root / 'benchmarks/runs/20260908-5849-forward-caller/manifest.json'
assert not (c / 'manifest.json').exists()
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
a = json.loads(authority.read_text())
for rel, digest in a['files'].items(): assert sha(parent / rel) == digest
builds = {name: root / f'build/pin-{number}-forward-preclean-{name}'
          for name, number in [('release', 5870), ('bc', 5871), ('trace', 5872), ('journal', 5873)]}
assert sha(builds['release'] / 'tactix') == sha(builds['bc'] / 'tactix')
for name, count in [('release', 95), ('bc', 95), ('trace', 94), ('journal', 95)]:
    assert f'100% tests passed out of {count}' in (c / f'logs/{name}-ctest-sealed.log').read_text(), name
files = ['src/rules/strengthen/strengthen.cpp', 'tests/test_forward_caller.cpp']
changed = [str(p.relative_to(source)) for folder in ('src', 'include', 'tests')
           for p in (source / folder).rglob('*') if p.is_file() and
           (not (parent / p.relative_to(source)).exists() or sha(p) != sha(parent / p.relative_to(source)))]
assert sorted(changed) == sorted(files), changed
def patch(base, names):
    return ''.join(''.join(difflib.unified_diff(
        (base / name).read_text().splitlines(True) if (base / name).exists() else [],
        (source / name).read_text().splitlines(True),
        fromfile='a/' + name if (base / name).exists() else '/dev/null', tofile='b/' + name)) for name in names)
(c / 'repair.patch').write_text(patch(parent, files))
(c / 'cumulative.patch').write_text(patch(root, [str(p.relative_to(root))
    for p in sorted((root / 'src').rglob('*')) if p.is_file()]))
for name in files:
    dest = c / 'source' / name
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source / name, dest)
validation = c / 'final-proof-validation.json'
v = json.loads(validation.read_text())
assert v['checked_proofs'] == 40 and len(v['journal_active_outcomes']) == 12
for rel, digest in v['hashes'].items(): assert sha(root / rel) == digest
evidence = [validation, root / 'benchmarks/runs/20260908-5875-forward-preclean-controls/summary.json',
            root / 'benchmarks/runs/20260908-5862-rank31-bve-frontier/schedule-diagnosis.json']
manifest = dict(source=str(source.relative_to(root)), parent=str(parent.relative_to(root)),
    parent_authority=dict(path=str(authority.relative_to(root)), sha256=sha(authority)),
    production=a['production'],
    purpose='Delete incoming-root-satisfied irredundant clauses before full reconnection so they never inflate BVE physical occurrence scores. Preserve lazy stale occurrences for clauses satisfied later during dense propagation. No new primitive, guard, option, effort budget or proof vocabulary.',
    test_consideration='Final rebuilt suites95/95 twice,94/94 trace,95/95 journal. New incoming-C2,already-fixed/empty-suffix and later-derived negative-control occurrence fixtures;SAT models and invariants. Six caller refutations include precleaned clauses.40 independently checked DRAT/LRAT proofs. Actual old/new/native occurrence controls distinguish required eager from preserved lazy cleanup.',
    files={name: sha(source / name) for name in files},
    binaries={str((b / 'tactix').relative_to(root)): sha(b / 'tactix') for b in builds.values()},
    patches={p.name: sha(p) for p in c.glob('*.patch')},
    validation={str(p.relative_to(root)): sha(p) for p in evidence},
    logs={p.name: sha(p) for p in (c / 'logs').iterdir()},
    limitation='Unfuzzed investigation material,not promoted. Current-source fuzz blocked on previously denied external-transfer approval;no retry. Runtime and causal gates pending.5862 failed its300K contract because standalone schedule observer exits after capture;only separately validated initial schedule is evidence.')
(c / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
for b in builds.values():
    p = b / 'tactix'
    p.chmod(p.stat().st_mode & ~0o222)
print('Frozen5870 preconnection cleanup')
