#!/usr/bin/env python3
"""Freeze the GC observer and submit-ready full SAT replay witnesses."""
import csv
import datetime as dt
import difflib
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
build = root / 'benchmarks/runs/20260908-6146-collect-memory-build'
b = json.loads((build / 'summary.json').read_text())
source, parent_source = root / b['source'], root / b['parent']
assert b['release_byte_identical_to_6107']
for rel, digest in b['files'].items():
    assert sha(source / rel) == digest, rel
for rel, digest in b['binaries'].items():
    assert sha(root / rel / 'tactix') == digest, rel
for rel, digest in b['logs'].items():
    assert sha(build / rel) == digest, rel
for kind, count in (('release', 100), ('trace', 99)):
    assert f'100% tests passed out of {count}' in (build / 'logs' / f'retry-{kind}-ctest.log').read_text(), kind
changed = [name for name in b['files'] if not (parent_source / name).exists()
           or sha(parent_source / name) != b['files'][name]]
assert set(changed) == {'src/kernel/kernel.cpp', 'src/diagnostics/event_trace.cpp',
                        'src/diagnostics/event_trace.hpp', 'tests/test_collect_memory.cpp'}, changed
assert not (build / 'manifest.json').exists()
def patch(base, names):
    return ''.join(''.join(difflib.unified_diff(
        (base / n).read_text().splitlines(True) if (base / n).exists() else [],
        (source / n).read_text().splitlines(True),
        fromfile='a/' + n if (base / n).exists() else '/dev/null', tofile='b/' + n, n=0)) for n in names)
(build / 'observer.patch').write_text(patch(parent_source, sorted(changed)))
for name in changed:
    target = build / 'source' / name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source / name, target)
old_temp = build / 'collect-memory-test-PIyw2q'
assert old_temp.is_file() and old_temp.stat().st_size == 0
# The expected old-object abort is terminal and this exact test-owned file is
# empty. It is not a solver certificate, campaign input, or active artifact.
old_temp.unlink()
(build / 'manifest.json').write_text(json.dumps(dict(parent=b['parent'], source=b['source'],
    build_summary_sha256=sha(build / 'summary.json'),
    parent_authority='benchmarks/runs/20260908-6136-substitute-journal/manifest.json',
    parent_authority_sha256=sha(root / 'benchmarks/runs/20260908-6136-substitute-journal/manifest.json'),
    files={name: b['files'][name] for name in changed}, binaries=b['binaries'],
    patch_sha256=sha(build / 'observer.patch'), patch_format='zero-context unified; apply --unidiff-zero',
    cleanup=dict(path=str(old_temp.relative_to(root)), bytes=0, reason='empty test-owned file after intentional old-object assertion'),
    production_change=False, test_consideration=b['test_consideration']), indent=2) + '\n')
parent = root / 'benchmarks/runs/20260908-6129-publication-memory'
pm = json.loads((parent / 'manifest.json').read_text())
for rel, digest in pm['pin_sha256'].items():
    assert sha(parent / rel) == digest, rel
c = root / 'benchmarks/runs/20260908-6148-collect-memory'
assert not c.exists()
for name in ('pins', 'logs', 'results', 'provisional', 'claims'):
    (c / name).mkdir(parents=True)
for rel in pm['pin_sha256']:
    p = parent / rel
    if p.parent == parent / 'pins' and p.name != 'tactix':
        target = c / 'pins' / p.name
        if target.exists():
            assert sha(target) == pm['pin_sha256'][rel], target
        else:
            shutil.copy2(p, target)
pin = 'build/pin-6151-collect-memory-trace'
shutil.copy2(root / pin / 'tactix', c / 'pins/tactix')
shutil.copy2(parent / 'matrix.tsv', c / 'matrix.tsv')
batch = (parent / 'array.sbatch').read_text().replace(str(parent), str(c)).replace('6129-memory', '6148-collect')
(c / 'array.sbatch').write_text(batch)
m = dict(pm, created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    parent_campaign=str(parent.relative_to(root)),
    arms=dict(tactix=dict(pm['arms']['tactix'], sha256=sha(c / 'pins/tactix'), source_build=pin)),
    build_authority=dict(path=str((build / 'manifest.json').relative_to(root)), sha256=sha(build / 'manifest.json')),
    hypothesis=b['hypothesis'],
    decision='Require checked original models,cleanup,full non-timing stats and emitted proof bytes equal6115; validate8 collector stages and locate first peak-increasing interval relative to BVE. Observer timing is not acceptance.',
    test_consideration=b['test_consideration'], limitations=b['limitation'])
m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in [*(c / 'pins').iterdir(), *(root / rel / 'tactix' for rel in b['binaries'])]:
    p.chmod(p.stat().st_mode & ~0o222)
print(c)
