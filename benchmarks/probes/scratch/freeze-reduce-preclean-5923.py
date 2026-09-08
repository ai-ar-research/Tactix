#!/usr/bin/env python3
"""Freeze the composition only after full suites and independent proof checks."""
import difflib
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5920-reduce-preclean'
source = root / '.claude/worktrees/runtime-reduce-preclean-5920'
parent = root / '.claude/worktrees/runtime-reduce-root-5900'
assert not (c / 'manifest.json').exists()
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
authorities = [root / 'benchmarks/runs' / n / 'manifest.json' for n in
               ('20260908-5900-reduce-root', '20260908-5870-forward-preclean')]
for path in authorities:
    a = json.loads(path.read_text())
    for rel, digest in a['files'].items(): assert sha(root / a['source'] / rel) == digest
builds = {name: root / f'build/pin-{number}-reduce-preclean-{name}' for name, number in
          [('release',5920),('bc',5921),('trace',5922),('journal',5923)]}
assert sha(builds['release'] / 'tactix') == sha(builds['bc'] / 'tactix')
for name, count in [('release',96),('bc',96),('trace',95),('journal',96)]:
    assert f'100% tests passed out of {count}' in (c / f'logs/{name}-ctest-final.log').read_text()
files = ['src/policies/default_policy.cpp', 'src/policies/default_policy.hpp',
         'src/rules/strengthen/strengthen.cpp', 'tests/test_reduce_root_guidance.cpp',
         'tests/test_forward_caller.cpp']
changed = [str(p.relative_to(source)) for folder in ('src','include','tests')
           for p in (source / folder).rglob('*') if p.is_file() and
           (not (parent / p.relative_to(source)).exists() or sha(p) != sha(parent / p.relative_to(source)))]
assert sorted(changed) == ['src/rules/strengthen/strengthen.cpp', 'tests/test_forward_caller.cpp'], changed
def patch(base, names):
    return ''.join(''.join(difflib.unified_diff(
        (base / name).read_text().splitlines(True) if (base / name).exists() else [],
        (source / name).read_text().splitlines(True),
        fromfile='a/' + name if (base / name).exists() else '/dev/null', tofile='b/' + name)) for name in names)
(c / 'repair.patch').write_text(patch(parent, changed))
(c / 'cumulative.patch').write_text(patch(root, [str(p.relative_to(root))
    for p in sorted((root / 'src').rglob('*')) if p.is_file()]))
for name in files:
    dest = c / 'source' / name
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source / name, dest)
validation = c / 'final-proof-validation.json'
v = json.loads(validation.read_text())
assert v['checked_proofs'] == 46
for rel, digest in v['hashes'].items(): assert sha(root / rel) == digest
evidence = [validation, *authorities,
    root / 'benchmarks/runs/20260908-5917-reduce-root-next-choice/choice-validation.json',
    root / 'benchmarks/runs/20260908-5908-reduce-root-ranks1-40/solver-checkpoint.json']
m = dict(source=str(source.relative_to(root)), parent=str(parent.relative_to(root)),
    parent_authority=dict(path=str(authorities[0].relative_to(root)), sha256=sha(authorities[0])),
    production=json.loads(authorities[0].read_text())['production'],
    purpose='Compose independently tested Reduce root guidance and incoming-root preconnection cleanup. Current5900 Rank32 first differs after second forward pass at271079 by three clauses; test whether cleanup closes this boundary and improves full work. The standalone5870 regression is not accepted or hidden.',
    test_consideration='Release96/96 twice,trace95/95,journal96/96;46 independently checked DRAT/LRAT proofs,actual old/new/native guidance controls. New interaction test derives roots during Reduce after trail truncation, verifies guidance removal, then reconnects dense occurrences and checks physical BVE counts, preserved unrelated guidance and original SAT model. Inherited preconnection/lazy-lifetime and raw Reduce-neutrality tests retained.',
    files={name:sha(source / name) for name in files},
    binaries={str((b / 'tactix').relative_to(root)):sha(b / 'tactix') for b in builds.values()},
    patches={p.name:sha(p) for p in c.glob('*.patch')},
    validation={str(p.relative_to(root)):sha(p) for p in evidence},
    logs={p.name:sha(p) for p in (c / 'logs').iterdir()},
    limitation='Unfuzzed isolated composition experiment,not promoted. Require full40,B28+C5 and current-source fuzz before acceptance;denied external transfer is not retried. Earlier source/runners/pins remain immutable.')
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for build in builds.values():
    binary = build / 'tactix'
    binary.chmod(binary.stat().st_mode & ~0o222)
print('Frozen5920 combined repair and interaction test')
