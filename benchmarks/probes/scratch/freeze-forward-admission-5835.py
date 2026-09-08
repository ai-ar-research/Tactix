#!/usr/bin/env python3
"""Freeze the tested C2 admission repair, retaining its complete predecessor."""
import difflib
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-5830-forward-admission'
source = root / '.claude/worktrees/runtime-forward-admission-5830'
parent = root / '.claude/worktrees/runtime-forward-unit-5798'
authority = root / 'benchmarks/runs/20260908-5798-forward-unit/manifest.json'
builds = {name: root / f'build/pin-{number}-forward-admission-{name}'
          for name, number in [('release', 5830), ('bc', 5831), ('trace', 5832)]}
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
assert not (c / 'manifest.json').exists()
a = json.loads(authority.read_text())
for rel, digest in a['files'].items():
    assert sha(parent / rel) == digest, rel
assert sha(builds['release'] / 'tactix') == sha(builds['bc'] / 'tactix')
for name in builds:
    assert f'100% tests passed out of {93 if name == "trace" else 94}' in (c / f'logs/{name}-ctest.log').read_text()
proofs = json.loads((c / 'fixed-proofs/summary.json').read_text())
assert proofs['checked_micro_proofs'] == 16 and proofs['binary_sha256'] == sha(builds['release'] / 'tests/test_forward_unit')
edge = root / 'benchmarks/runs/20260908-5834-forward-admission-edges/summary.json'
assert len(json.loads(edge.read_text())['checked_conflict_proofs']) == 12
files = ['src/rules/strengthen/strengthen.cpp', 'tests/test_forward_marks.cpp']
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
(c / 'cumulative.patch').write_text(patch(root, [str(p.relative_to(root)) for p in sorted((root / 'src').rglob('*')) if p.is_file()]))
for name in files:
    dest = c / 'source' / name
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source / name, dest)
manifest = dict(source=str(source.relative_to(root)), parent=str(parent.relative_to(root)),
    parent_authority=dict(path=str(authority.relative_to(root)), sha256=sha(authority)),
    production=a['production'], purpose='C2 consumes clause-addition marks,not synthetic root-fixed notifications. Shared C1/CCE helper and unrestricted public actions remain intact. No new option,budget,kernel primitive,or proof vocabulary.',
    test_consideration='94/94 independent Release and BC,93/93 trace;eight new root-mark/public-action/model cases,17 inherited continuation states,4 supplementary edge states,28 independently checked DRAT/LRAT proofs. Actual native root-mark and caller controls retained;real Rank32 origin proven by neutral5822 replay.',
    files={name: sha(source / name) for name in files},
    binaries={str((b / 'tactix').relative_to(root)): sha(b / 'tactix') for b in builds.values()},
    patches={p.name: sha(p) for p in c.glob('*.patch')},
    validation={str(p.relative_to(root)): sha(p) for p in [edge, c / 'fixed-proofs/summary.json',
        root / 'benchmarks/runs/20260908-5822-forward-mark-rank32-replay/summary.json',
        root / 'benchmarks/runs/20260908-5817-forward-mark-observer/fixed-control/summary.json']},
    logs={p.name: sha(p) for p in (c / 'logs').iterdir()},
    limitation='UNFUZZED isolated candidate,not promoted. Admission is intentionally changed,so runtime benefit requires causal and cumulative measurements. Separate post-C2 dense closure defect remains unrepaired. CaDiCaL excludes fixed-literal candidates;that stronger restriction is not adopted.')
(c / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
for b in builds.values():
    p = b / 'tactix'
    p.chmod(p.stat().st_mode & ~0o222)
print('Frozen5830:94/94 twice,93/93 trace,28 checked micro-proofs,eight new admission cases')
