#!/usr/bin/env python3
"""Freeze capacity-only observer and reuse immutable checked witnesses."""
import datetime as dt
import difflib
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
build = root / 'benchmarks/runs/20260909-6154-bve-capacity-build'
b = json.loads((build / 'summary.json').read_text())
source, parent_source = root / b['source'], root / b['parent']
assert b['release_byte_identical_to_6107']
for rel, digest in b['files'].items():
    assert sha(source / rel) == digest, rel
for rel, digest in b['binaries'].items():
    assert sha(root / rel / 'tactix') == digest, rel
for rel, digest in b['logs'].items():
    assert sha(build / rel) == digest, rel
for kind, count in (('release', 101), ('trace', 100)):
    assert f'100% tests passed out of {count}' in (build / 'logs' / f'{kind}-ctest.log').read_text(), kind
changed = [name for name in b['files'] if not (parent_source / name).exists()
           or sha(parent_source / name) != b['files'][name]]
assert set(changed) == {'src/diagnostics/event_trace.cpp', 'tests/test_bve_memory.cpp'}, changed
assert not (build / 'manifest.json').exists()
patch = ''.join(''.join(difflib.unified_diff(
    (parent_source / n).read_text().splitlines(True) if (parent_source / n).exists() else [],
    (source / n).read_text().splitlines(True),
    fromfile='a/' + n if (parent_source / n).exists() else '/dev/null', tofile='b/' + n, n=0)) for n in changed)
(build / 'observer.patch').write_text(patch)
for name in changed:
    target = build / 'source' / name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source / name, target)
# Only the terminal expected-failure test can own this one empty file.
temps = list(build.glob('bve-memory-test-*'))
assert len(temps) == 1 and temps[0].is_file() and temps[0].stat().st_size == 0
assert 'rows.size() == 3' in (build / 'logs/old-negative.log').read_text()
temporary = temps[0].resolve()
assert temporary.parent == build.resolve()
temporary.unlink()
(build / 'manifest.json').write_text(json.dumps(dict(parent=b['parent'], source=b['source'],
    build_summary_sha256=sha(build / 'summary.json'),
    parent_authority='benchmarks/runs/20260908-6146-collect-memory-build/manifest.json',
    parent_authority_sha256=sha(root / 'benchmarks/runs/20260908-6146-collect-memory-build/manifest.json'),
    files={name: b['files'][name] for name in changed}, binaries=b['binaries'],
    patch_sha256=sha(build / 'observer.patch'), patch_format='zero-context unified; apply --unidiff-zero',
    cleanup=dict(path=str(temporary.relative_to(root)), bytes=0, reason='empty test-owned file after intentional old-object assertion'),
    production_change=False, test_consideration=b['test_consideration']), indent=2) + '\n')
parent = root / 'benchmarks/runs/20260908-6148-collect-memory'
pm = json.loads((parent / 'manifest.json').read_text())
for rel, digest in pm['pin_sha256'].items():
    assert sha(parent / rel) == digest, rel
c = root / 'benchmarks/runs/20260909-6156-bve-capacity'
assert not c.exists()
for name in ('pins', 'logs', 'results', 'provisional', 'claims'):
    (c / name).mkdir(parents=True)
for rel in pm['pin_sha256']:
    p = parent / rel
    if p.parent == parent / 'pins' and p.name != 'tactix':
        shutil.copy2(p, c / 'pins' / p.name)
pin = 'build/pin-6155-bve-capacity-trace'
shutil.copy2(root / pin / 'tactix', c / 'pins/tactix')
shutil.copy2(parent / 'matrix.tsv', c / 'matrix.tsv')
batch = (parent / 'array.sbatch').read_text().replace(str(parent), str(c)).replace('6148-collect', '6156-capacity')
(c / 'array.sbatch').write_text(batch)
m = dict(pm, created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    parent_campaign=str(parent.relative_to(root)),
    arms=dict(tactix=dict(pm['arms']['tactix'], sha256=sha(c / 'pins/tactix'), source_build=pin)),
    build_authority=dict(path=str((build / 'manifest.json').relative_to(root)), sha256=sha(build / 'manifest.json')),
    hypothesis=b['hypothesis'],
    decision='Require checked original models,cleanup,full non-timing stats and emitted proof bytes equal6115; old6148 slot/collector capacity sequence must replay. Attribute capacity-transition intervals without claiming single-allocation causality.',
    test_consideration=b['test_consideration'], limitations=b['limitation'])
m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in [*(c / 'pins').iterdir(), *(root / rel / 'tactix' for rel in b['binaries'])]:
    p.chmod(p.stat().st_mode & ~0o222)
print(c)
