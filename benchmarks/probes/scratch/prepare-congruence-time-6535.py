#!/usr/bin/env python3
"""One unchanged Release Rank32 solve with existing Congruence stage timers."""
import csv
import datetime as dt
import json
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha

parent = root / 'benchmarks/runs/20260909-6439-activation-ranks1-50'
c = root / 'benchmarks/runs/20260909-6535-rank32-congruence-time'
assert not (c / 'manifest.json').exists() and not (c / 'launch.json').exists()
for name in ('claims', 'results', 'provisional'):
    assert not list((c / name).glob('*'))
m = json.loads((parent / 'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items(): assert sha(parent / rel) == digest, rel
record_path = parent / 'results/031.json'
record = json.loads(record_path.read_text())
assert record['rank'] == 32 and record['valid'] and record['artifact_deleted']
assert record['proof_check']['result'] == 'verified' and not record['cleanup_pending']
with (parent / 'matrix.tsv').open() as stream:
    reader = csv.DictReader(stream, delimiter='\t')
    fields = reader.fieldnames
    rows = [row for row in reader if row['rank'] == '32']
assert len(rows) == 1 and rows[0]['arm'] == 'tactix'
rows[0]['task'] = '0'
for name in ('pins', 'logs', 'results', 'provisional', 'claims'):
    (c / name).mkdir(parents=True, exist_ok=True)
for name in ('tactix', 'checkmodel', 'modelcheck.py', 'drat-trim'):
    if (c / 'pins' / name).exists():
        assert sha(c / 'pins' / name) == sha(parent / 'pins' / name)
    else:
        shutil.copy2(parent / 'pins' / name, c / 'pins' / name)
runner = (parent / 'pins/runtime_task.py').read_text()
anchor = '        affinity = sorted(os.sched_getaffinity(0))'
assert runner.count(anchor) == 1
runner = runner.replace(anchor, '        os.environ["TACTIX_CONG_TIME"] = "1"\n' + anchor)
anchor = '"solver_stderr_tail": tail(stderr),'
assert runner.count(anchor) == 1
runner = runner.replace(anchor, '"solver_stderr_tail": tail(stderr, 256),')
if (c / 'pins/runtime_task.py').exists():
    assert (c / 'pins/runtime_task.py').read_text() == runner
else:
    (c / 'pins/runtime_task.py').write_text(runner)
if (c / 'matrix.tsv').exists():
    with (c / 'matrix.tsv').open() as stream:
        assert list(csv.DictReader(stream, delimiter='\t')) == rows
else:
    with (c / 'matrix.tsv').open('x', newline='') as stream:
        writer = csv.DictWriter(stream, fields, delimiter='\t', lineterminator='\n')
        writer.writeheader()
        writer.writerows(rows)
batch = (parent / 'array.sbatch').read_text().replace(str(parent), str(c))
assert batch.count('--array=0-49') == 1
batch = batch.replace('--array=0-49', '--array=0-0')
if (c / 'array.sbatch').exists():
    assert (c / 'array.sbatch').read_text() == batch
else:
    (c / 'array.sbatch').write_text(batch)
protected = json.loads((root / 'benchmarks/reports/2026-09-09-retention-attribution-audit-6533.json').read_text())['protected_campaign_hashes']
for rel, digest in protected.items(): assert sha(root / rel) == digest, rel
result = dict(
    schema='tactix-rank32-existing-congruence-timers-v1',
    created_at=dt.datetime.now(dt.timezone.utc).isoformat(), arms=m['arms'],
    input_sha256={rows[0]['instance']: m['input_sha256'][rows[0]['instance']]},
    resource_contract=dict(m['resource_contract'], array='0-0'),
    checkers={name: dict(path='pins/' + name, sha256=sha(c / 'pins' / name))
              for name in ('checkmodel', 'modelcheck.py', 'drat-trim')},
    collect_runtime_context=m.get('collect_runtime_context', False),
    diagnostic_environment={'TACTIX_CONG_TIME': '1'},
    authorities={key: dict(path=str(path.relative_to(root)), sha256=sha(path)) for key, path in {
        'parent': parent / 'manifest.json', 'work_oracle': record_path,
        'profile': root / 'benchmarks/runs/20260909-6372-rank32-release-profile/solver-summary-6378.json',
        'prior_audit': root / 'benchmarks/reports/2026-09-09-retention-attribution-audit-6533.json',
    }.items()},
    protected_campaign_hashes=protected,
    purpose='Determine whether small-clause indexing dominates Congruence before pursuing the source-observed over-arity tail scan. Existing full Factor totals match native; no claim of complete per-call work equality.',
    hypothesis='Index time may be a worthwhile part of the remaining Congruence cost; otherwise deprioritize the tail-scan repair as a primary runtime lever.',
    validation='Expected full UNSAT, exact own6439 non-timing statistics and proof bytes/hash, all38 cumulative timer rows, independent proof checking and cleanup. Timer overhead means this is not a speed measurement or acceptance.',
    test_consideration='Only isolated runner configuration changes: inject the existing timer after environment sanitization, retain256 diagnostic lines, and assert exact immutable inputs/pins. Add focused timer-reader tests; solver source and behavior are unchanged.',
    repetition='One diagnostic solve, not three-wave runtime acceptance; no Kissat rerun or reference refresh.',
)
assert len(result['arms']) == 1
assert sha(c / 'pins/tactix') == record['binary_sha256'] == result['arms']['tactix']['sha256']
result['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
with (c / 'manifest.json').open('x') as stream:
    json.dump(result, stream, indent=2)
    stream.write('\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
