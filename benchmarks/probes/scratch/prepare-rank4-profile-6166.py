#!/usr/bin/env python3
"""Profile current Release and current native rank4, preserving checked gates."""
import csv
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
fresh = root / 'benchmarks/runs/20260908-6121-fresh-accepted-ranks1-50'
current = root / 'benchmarks/runs/20260908-6115-substitute-publication-ranks1-50'
protocol = root / 'benchmarks/runs/20260908-5544-small-rank39-profile'
fm = json.loads((fresh / 'manifest.json').read_text())
cm = json.loads((current / 'manifest.json').read_text())
pm = json.loads((protocol / 'manifest.json').read_text())
for campaign, manifest in ((fresh, fm), (current, cm), (protocol, pm)):
    for rel, digest in manifest['pin_sha256'].items():
        assert sha(campaign / rel) == digest, rel
c = root / 'benchmarks/runs/20260909-6166-rank4-release-profile'
assert not c.exists()
for name in ('pins', 'logs', 'results', 'provisional', 'claims', 'profiles'):
    (c / name).mkdir(parents=True)
for name in ('runtime_task.py', 'checkmodel', 'modelcheck.py', 'drat-trim'):
    shutil.copy2(fresh / 'pins' / name, c / 'pins' / name)
shutil.copy2(current / 'pins/tactix', c / 'pins/candidate')
shutil.copy2(fresh / 'pins/kissat', c / 'pins/kissat')
shutil.copy2(protocol / 'pins/run.py', c / 'pins/run.py')
with (fresh / 'matrix.tsv').open(newline='') as stream:
    reader = csv.DictReader(stream, delimiter='\t')
    fields = reader.fieldnames
    rows = [r for r in reader if r['rank'] == '4']
assert len(rows) == 2 and [r['arm'] for r in rows] == ['tactix', 'kissat']
for i, row in enumerate(rows):
    row['task'] = str(i)
    if row['arm'] == 'tactix': row['arm'] = 'candidate'
with (c / 'matrix.tsv').open('x', newline='') as stream:
    writer = csv.DictWriter(stream, fields, delimiter='\t', lineterminator='\n')
    writer.writeheader(); writer.writerows(rows)
batch = (fresh / 'array.sbatch').read_text().replace(str(fresh), str(c))
assert batch.count('--array=0-99') == 1
batch = batch.replace('--array=0-99', '--array=0-1').replace('/pins/runtime_task.py', '/pins/run.py')
(c / 'array.sbatch').write_text(batch)
candidate = dict(cm['arms']['tactix'], path='pins/candidate')
candidate.pop('source_patch'); candidate.pop('source_patch_sha256')
baseline_paths = {'candidate': current / 'results/003.json', 'kissat': fresh / 'results/007.json'}
for arm, path in baseline_paths.items():
    record = json.loads(path.read_text())
    assert record['valid'] and record['verdict'] == 'UNSAT' and record['expected_match']
    assert record['artifact_deleted'] and record['derived_artifacts_deleted'] and not record['cleanup_pending']
    assert record['binary_sha256'] == sha(c / 'pins' / arm)
m = dict(fm, schema='tactix-rank4-release-profile-v1',
    created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    arms=dict(candidate=candidate, kissat=fm['arms']['kissat']),
    input_sha256={rows[0]['instance']: fm['input_sha256'][rows[0]['instance']]},
    resource_contract=dict(fm['resource_contract'], array='0-1'),
    profile=dict(event='cycles:u', frequency=199, call_graph='dwarf,8192'),
    current_authority=dict(path=str((current / 'manifest.json').relative_to(root)), sha256=sha(current / 'manifest.json')),
    profile_protocol=dict(path=str((protocol / 'manifest.json').relative_to(root)), sha256=sha(protocol / 'manifest.json')),
    unprofiled_records={arm: dict(path=str(path.relative_to(root)), sha256=sha(path)) for arm, path in baseline_paths.items()},
    prefix_authority=dict(path='benchmarks/runs/20260909-6160-rank4-current-prefix/summary.json',
                          sha256=sha(root / 'benchmarks/runs/20260909-6160-rank4-current-prefix/summary.json')),
    authorization='Bounded native diagnostic allowed by fixed50 protocol; no reference timing refresh. Structural memory proposal remains pending and is not implemented here.',
    purpose='Localize ordinary current Release rank4 execution cost after907045 matched decisions through650K conflicts.',
    hypothesis='Dominant search operation cost,not demonstrated scheduling divergence,explains the remaining small rank4 gap. Rank hotspots before a source-backed local improvement.',
    validation='Both expected UNSAT verdicts independently checked,zero scratch residue,no lost samples,within-arm proof bytes/hash and logical counters exactly replay unprofiled records. Candidate all non-timing statistics replay. Sampling time is not acceptance.',
    test_consideration='No solver,checker orsampling implementation edit. Reuse hash-verified protocol;assert exact two-arm matrix and baseline identities. Existing39benchmark and13watcher tests cover unchanged surrounding contracts.',
    repetition='One paired diagnostic profile; any subsequent performance acceptance still requires three paired unprofiled waves.')
m['protected_campaign_hashes'] = dict(fm['protected_campaign_hashes'])
for name in ('manifest.json', 'launch.json', 'array.sbatch'):
    m['protected_campaign_hashes'][str((fresh / name).relative_to(root))] = sha(fresh / name)
m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
