#!/usr/bin/env python3
"""Three independent checked SAT ownership probes on unchanged6107 source."""
import csv
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
parent = root / 'benchmarks/runs/20260908-6115-substitute-publication-ranks1-50'
pm = json.loads((parent / 'manifest.json').read_text())
build = root / 'benchmarks/runs/20260908-6127-publication-diagnostic-builds/summary.json'
bm = json.loads(build.read_text())
pin = 'build/pin-6127-publication-trace'
assert sha(root / pin / 'tactix') == bm['binaries'][pin]
for rel, digest in bm['source_sha256'].items():
    assert sha(root / bm['source'] / rel) == digest, rel
for rel, digest in pm['pin_sha256'].items():
    assert sha(parent / rel) == digest, rel
with (parent / 'matrix.tsv').open(newline='') as stream:
    reader = csv.DictReader(stream, delimiter='\t')
    fields = reader.fieldnames
    indexed = {int(r['rank']): r for r in reader}
rows = [dict(indexed[rank], task=str(task)) for task, rank in enumerate((21, 25, 9))]
assert all(r['arm'] == 'tactix' and r['expected'] == 'SAT' for r in rows)
release_records = {}
for r in rows:
    path = parent / 'results' / f"{int(r['rank'])-1:03d}.json"
    record = json.loads(path.read_text())
    assert record['valid'] and record['verdict'] == 'SAT'
    assert record['artifact_deleted'] and record['derived_artifacts_deleted'] and not record['cleanup_pending']
    release_records[str(path.relative_to(root))] = sha(path)
c = root / 'benchmarks/runs/20260908-6129-publication-memory'
assert not c.exists()
for name in ('pins', 'logs', 'results', 'provisional', 'claims'):
    (c / name).mkdir(parents=True)
for name in ('runtime_task.py', 'drat-trim', 'checkmodel', 'modelcheck.py', 'candidate-source.patch'):
    shutil.copy2(parent / 'pins' / name, c / 'pins' / name)
shutil.copy2(root / pin / 'tactix', c / 'pins/tactix')
shutil.copy2(root / 'tools/bench/slot_memory_task.py', c / 'pins/run.py')
with (c / 'matrix.tsv').open('x', newline='') as stream:
    w = csv.DictWriter(stream, fields, delimiter='\t', lineterminator='\n')
    w.writeheader(); w.writerows(rows)
batch = (parent / 'array.sbatch').read_text().replace(str(parent), str(c))
assert batch.count('#SBATCH --array=0-49') == 1
batch = batch.replace('#SBATCH --array=0-49', '#SBATCH --array=0-2')
batch = batch.replace('--job-name=runtime-checked', '--job-name=6129-memory')
batch = batch.replace('/pins/runtime_task.py', '/pins/run.py')
(c / 'array.sbatch').write_text(batch)
protected = {}
for name in ('20260908-6108-rank41-50-matched',
             '20260908-6114-substitute-publication-BC-gate',
             '20260908-6115-substitute-publication-ranks1-50',
             '20260908-6121-fresh-accepted-ranks1-50'):
    old = root / 'benchmarks/runs' / name
    for item in ('manifest.json', 'array.sbatch', 'launch.json'):
        path = old / item
        protected[str(path.relative_to(root))] = sha(path)
fresh = json.loads((root / 'benchmarks/runs/20260908-6121-fresh-accepted-ranks1-50/manifest.json').read_text())
protected.update(fresh['protected_campaign_hashes'])
for rel, digest in protected.items():
    assert sha(root / rel) == digest, rel
m = dict(schema='tactix-slot-memory-diagnostic-v1', created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    arms=dict(tactix=dict(pm['arms']['tactix'], sha256=sha(c / 'pins/tactix'), source_build=pin)),
    input_sha256={r['instance']: pm['input_sha256'][r['instance']] for r in rows},
    resource_contract=dict(pm['resource_contract'], array='0-2'), collect_runtime_context=True,
    parent_campaign=str(parent.relative_to(root)), release_records=release_records,
    build_authority=dict(path=str(build.relative_to(root)), sha256=sha(build)),
    protected_campaign_hashes=protected,
    hypothesis='Ranks21,25,9 lead checked fresh accepted-vs-Kissat memory excess. Attribute current cumulative6107 ownership after earlier dense-watch repairs, before choosing a new repair.',
    decision='Allthree complete expected SAT solves, independently checked original-input models, clean scratch, trace hashes, exact non-timing logical counters and certificate bytes versus6115. Locate retained pool or extra work; observer peak/time is diagnostic only.',
    test_consideration='No solver source/semantics/runner oracle edit. Complete trace/journal CTest suites; six new wrapper tests (39 total benchmark tests) cover per-task paths, checker exclusion, environment restoration, missing trace, existing evidence and invalid task.',
    limitations='Compile-time event trace only; no conflict stop or other observer selected. Slot snapshots do not exhaustively inventory transient rule-local scratch. Not timing acceptance, not a hard-memory guarantee, not promotion; latestsource fuzz remains required.')
m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for path in (c / 'pins').iterdir():
    path.chmod(path.stat().st_mode & ~0o222)
print(c)
