#!/usr/bin/env python3
"""Freeze the tested full-connected caller service and its exact lineage."""
import difflib
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5849-forward-caller'
source = root / '.claude/worktrees/runtime-forward-caller-5849'
parent = root / '.claude/worktrees/runtime-forward-admission-5830'
authority = root / 'benchmarks/runs/20260908-5830-forward-admission/manifest.json'
assert not (c / 'manifest.json').exists()
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
a = json.loads(authority.read_text())
for rel, digest in a['files'].items():
    assert sha(parent / rel) == digest
builds = {name: root / f'build/pin-{number}-forward-caller-{name}'
          for name, number in [('release', 5849), ('bc', 5850), ('trace', 5851), ('journal', 5854)]}
assert sha(builds['release'] / 'tactix') == sha(builds['bc'] / 'tactix')
for name, count in [('release', 95), ('bc', 95), ('trace', 94), ('journal', 95)]:
    assert f'100% tests passed out of {count}' in (c / f'logs/{name}-ctest-final.log').read_text(), name
files = ['src/policies/default_policy.cpp', 'src/rules/strengthen/strengthen.cpp',
         'src/rules/strengthen/strengthen.hpp', 'tests/test_forward_caller.cpp']
changed = [str(p.relative_to(source)) for folder in ('src', 'include', 'tests')
           for p in (source / folder).rglob('*') if p.is_file() and
           (not (parent / p.relative_to(source)).exists() or sha(p) != sha(parent / p.relative_to(source)))]
assert sorted(changed) == sorted(files), changed
def patch(base, names):
    return ''.join(''.join(difflib.unified_diff(
        (base / name).read_text().splitlines(True) if (base / name).exists() else [],
        (source / name).read_text().splitlines(True),
        fromfile='a/' + name if (base / name).exists() else '/dev/null',
        tofile='b/' + name)) for name in names)
(c / 'repair.patch').write_text(patch(parent, files))
(c / 'cumulative.patch').write_text(patch(root, [str(p.relative_to(root))
    for p in sorted((root / 'src').rglob('*')) if p.is_file()]))
for name in files:
    dest = c / 'source' / name
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source / name, dest)
validation = c / 'final-proof-validation.json'
v = json.loads(validation.read_text())
assert v['checked_proofs'] == 40 and v['journal_active_outcomes']
manifest = dict(source=str(source.relative_to(root)), parent=str(parent.relative_to(root)),
    parent_authority=dict(path=str(authority.relative_to(root)), sha256=sha(authority)),
    production=a['production'], purpose='Restore post-C2 full irredundant reconnect, suffix propagation and satisfied-clause cleanup before scheduling BVE. Retain private partial forward semantics and explicit N1 actions. Reuse existing kernel operations and proof-safe root/refutation helpers; no new kernel primitive, option, global contract or tuned budget.',
    test_consideration='95/95 twice,94/94 trace,95/95 journal; actual next-slot two-wave closure, direct mixed chain, empty suffix, six caller conflict cases;40 independently checked DRAT/LRAT proofs including active-journal cases. Inherited public/action/guard/model and continuation coverage retained.',
    files={name: sha(source / name) for name in files},
    binaries={str((b / 'tactix').relative_to(root)): sha(b / 'tactix') for b in builds.values()},
    patches={p.name: sha(p) for p in c.glob('*.patch')},
    validation={str(validation.relative_to(root)): sha(validation),
                'benchmarks/runs/20260908-5845-forward-admission-rank31-prefix/summary.json':
                    sha(root / 'benchmarks/runs/20260908-5845-forward-admission-rank31-prefix/summary.json')},
    logs={p.name: sha(p) for p in (c / 'logs').iterdir()},
    limitation='Not promoted, current-source fuzz remains blocked on previously denied external-transfer approval. Initial parallel BC CTest failed only mocked snapshot cleanup with ENOTEMPTY; complete serial rerun retained. Journal guard/cursor ordering corrected by review before final builds. Runtime and causal gates pending.')
(c / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
for b in builds.values():
    p = b / 'tactix'
    p.chmod(p.stat().st_mode & ~0o222)
print('Frozen5849 caller closure')
