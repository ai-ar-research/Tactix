#!/usr/bin/env python3
"""Profile exact-work current/parent Rank38 after the rejected layout ablation."""
import csv
import datetime as dt
import json
from pathlib import Path
import shutil
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
subprocess.run([sys.executable, str(root/'tools/slurm/session_deadline.py'),
    '--deadline', '2026-09-10T06:00:00-04:00'], check=True)
old = root/'benchmarks/runs/20260909-6754-rank38-watermark-cost-runtime'
protocol = root/'benchmarks/runs/20260908-5544-small-rank39-profile'
c = root/'benchmarks/runs/20260909-6787-rank38-watermark-profile'
assert not c.exists()
m = json.loads((old/'manifest.json').read_text())
pm = json.loads((protocol/'manifest.json').read_text())
for rel, digest in m['pin_sha256'].items():
    assert sha(old/rel) == digest, rel
assert sha(protocol/'pins/run.py') == pm['pin_sha256']['pins/run.py']
for rel, digest in m['protected_campaign_hashes'].items():
    assert sha(root/rel) == digest, rel
records = {}
for task, arm in enumerate(('candidate', 'previous')):
    p = old/f'results/{task:03d}.json'
    r = json.loads(p.read_text())
    state = classify(r, True)
    assert state['checked'] and state['clean'] and r['rank'] == 38 and r['arm'] == arm
    assert r['binary_sha256'] == m['arms'][arm]['sha256']
    records[arm] = dict(path=str(p.relative_to(root)), sha256=sha(p))
runner = (protocol/'pins/run.py').read_text()
assert runner.count("('candidate', 'kissat')") == 1
runner = runner.replace("('candidate', 'kissat')", "('candidate', 'previous')")
compile(runner, str(c/'pins/run.py'), 'exec')
for name in ('pins', 'logs', 'results', 'provisional', 'claims', 'profiles'):
    (c/name).mkdir(parents=True)
for p in (old/'pins').iterdir():
    assert p.is_file()
    shutil.copy2(p, c/'pins'/p.name)
(c/'pins/run.py').write_text(runner)
with (old/'matrix.tsv').open(newline='') as stream:
    reader = csv.DictReader(stream, delimiter='\t')
    fields = reader.fieldnames
    rows = list(reader)[:2]
assert [r['arm'] for r in rows] == ['candidate', 'previous']
assert [r['task'] for r in rows] == ['0', '1']
assert all(r['rank'] == '38' and r['expected'] == 'UNSAT' for r in rows)
with (c/'matrix.tsv').open('x', newline='') as stream:
    writer = csv.DictWriter(stream, fields, delimiter='\t', lineterminator='\n')
    writer.writeheader(); writer.writerows(rows)
batch = (old/'array.sbatch').read_text().replace(str(old), str(c))
assert batch.count('--array=0-5') == 1
batch = batch.replace('--array=0-5', '--array=0-1').replace('6754-rank38-watermark-cost', '6787-rank38-profile').replace('/pins/runtime_task.py', '/pins/run.py')
(c/'array.sbatch').write_text(batch)
for rel in ('benchmarks/reports/2026-09-09-watermark-layout-ablation.md',
            'benchmarks/reports/2026-09-09-watermark-layout-landing-audit-6785.json',
            'benchmarks/runs/20260909-6777-watermark-propagate-code/relocations-6778.json'):
    m['dependencies'][rel] = sha(root/rel)
m.update(schema='tactix-rank38-matched-release-profile-v1', created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    resource_contract=dict(m['resource_contract'], array='0-1'),
    profile=dict(event='cycles:u', frequency=199, call_graph='dwarf,8192'),
    profile_protocol=dict(path=str((protocol/'manifest.json').relative_to(root)), sha256=sha(protocol/'manifest.json')),
    unprofiled_records=records,
    purpose='Localize execution-efficiency cost of the retained6730 watermark repair against6654 on exact-work Rank38, after6763 restored data offsets but remained neutral.',
    hypothesis='Cycle distribution will distinguish a dominant propagation/front-end shift from increased cost in another unchanged-work operation; static code placement alone is not causality.',
    validation='Both expected UNSAT, exact within-arm full non-timing statistics/counters/certificate bytes and hash, independently checked proofs and zero scratch residue. No lost samples, valid self/callpath reports and hash-bound compressed perf data. Expected outcomes permit provisional investigation while checkers run.',
    acceptance='Diagnostic only: choose the next source-backed observation from dominant cycle samples after full replay. No runtime gain, scheduling defect, instruction-cache cause, or source promotion inferred from percentages alone.',
    submission_contract='One concurrent full-width two-task sampled diagnostic,12logicalCPU/48000MiB/1200solver-only seconds/5h allocation,no node constraint or throttle.',
    repetition='One diagnostic profile pair; any subsequent performance acceptance still requires three independent unprofiled paired waves.',
    test_consideration='No solver/checker or sampling algorithm change. Reuse hash-verified profile wrapper with only its solver-name tuple changed to candidate/previous; assert exactly two pinned arms and matrix identities. Existing219 benchmark/19 watcher tests plus exact within-arm full work/proof replay cover surrounding contracts.',
    limitation='6730 remains isolated/unfuzzed. No old script mutation, Kissat refresh, FlatLists, fuzz access, promotion, or denied push retry.')
m['pin_sha256'] = {str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(), c/'matrix.tsv', c/'array.sbatch']}
with (c/'manifest.json').open('x') as stream:
    json.dump(m, stream, indent=2); stream.write('\n')
for p in (c/'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)
