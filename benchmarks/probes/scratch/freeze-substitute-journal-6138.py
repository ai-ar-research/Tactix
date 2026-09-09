#!/usr/bin/env python3
"""Freeze diagnostic-only restoration and its byte-identical Release proof."""
import difflib
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260908-6136-substitute-journal'
b = json.loads((c / 'build-summary.json').read_text())
source, parent = root / b['source'], root / b['parent']
authority = root / 'benchmarks/runs/20260908-6107-substitute-publication/manifest.json'
a = json.loads(authority.read_text())
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
assert not (c / 'manifest.json').exists()
for rel, digest in a['files'].items():
    assert sha(parent / rel) == digest, rel
for rel, digest in b['files'].items():
    assert sha(source / rel) == digest, rel
for rel, digest in b['binaries'].items():
    assert sha(root / rel / 'tactix') == digest, rel
changed = []
for folder in ('src', 'include', 'tests'):
    names = {str(p.relative_to(base)) for base in (parent, source)
             for p in (base / folder).rglob('*') if p.is_file()}
    for name in sorted(names):
        assert (parent / name).exists() and (source / name).exists(), name
        if sha(parent / name) != sha(source / name):
            changed.append(name)
assert sorted(changed) == sorted(b['files'])
assert sha(source / 'CMakeLists.txt') == sha(parent / 'CMakeLists.txt')
for kind in ('release', 'journal'):
    assert '100% tests passed out of 99' in (c / 'logs' / f'{kind}-ctest.log').read_text()
assert 'transactions == 1' in (c / 'logs/old-negative.log').read_text()
subprocess.run(['git', 'diff', '--exit-code', a['production'], '--', 'src', 'include', 'CMakeLists.txt'],
               cwd=root, check=True)
def patch(base, names):
    return ''.join(''.join(difflib.unified_diff(
        (base / n).read_text().splitlines(True) if (base / n).exists() else [],
        (source / n).read_text().splitlines(True),
        fromfile='a/' + n if (base / n).exists() else '/dev/null', tofile='b/' + n, n=0)) for n in names)
(c / 'repair.patch').write_text(patch(parent, sorted(changed)))
(c / 'cumulative.patch').write_text(patch(root, [str(p.relative_to(root)) for p in sorted((root / 'src').rglob('*')) if p.is_file()]))
for name in changed:
    out = c / 'source' / name
    out.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source / name, out)
m = dict(source=b['source'], parent=b['parent'], production=a['production'],
    parent_authority=dict(path=str(authority.relative_to(root)), sha256=sha(authority)),
    files=b['files'], binaries={rel + '/tactix': digest for rel, digest in b['binaries'].items()},
    patches={p.name: sha(p) for p in c.glob('*.patch')},
    patch_format='zero-context unified patches; apply with --unidiff-zero',
    build_summary_sha256=sha(c / 'build-summary.json'), logs=b['logs'],
    release_byte_identical_to_6107=True,
    purpose='Restore the unbudgeted caller-composed Substitute transaction and source meter in compile-time journal builds. Includes final reconnection. No Release instructions, rule/API/guard/proof/model behavior, budgets or scheduling change.',
    test_consideration=b['test_consideration'],
    limitation='Budget-aware Substitute remains schedule-only; per-round transaction coverage is open, not implicitly repaired. 6107 checked gates remain authority for the identical Release binary; parent fuzz remains pending, no promotion or runtime gain.')
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for rel in m['binaries']:
    p = root / rel
    p.chmod(p.stat().st_mode & ~0o222)
print('Frozen6136: Release byte identity,99/99 twice and expected old missing-transaction failure')
