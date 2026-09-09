#!/usr/bin/env python3
"""Current largest UNSAT gap against retained native trace; no new native run."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from decision_prefix import read_prefix

c = root / 'benchmarks/runs/20260909-6605-material-rank31-prefix'
assert not c.exists()
build = root / 'benchmarks/runs/20260909-6579-material-schedule-build/manifest.json'
a = json.loads(build.read_text())
for rel, digest in a['files'].items(): assert sha(root / a['source'] / rel) == digest, rel
binary = root / 'build/pin-6579-material-schedule-trace/tactix'
assert sha(binary) == a['binaries'][str(binary.relative_to(root))]
bc = root / 'benchmarks/runs/20260909-6584-material-schedule-BC-gate'
dependencies = {}
for p in sorted((bc / 'results').glob('*.json')):
    status = classify(json.loads(p.read_text()), True)
    assert status['checked'] and status['clean'], p
    dependencies[str(p.relative_to(root))] = sha(p)
assert len(dependencies) == 66
full = root / 'benchmarks/reports/2026-09-09-material-full50-6596/summary.json'
s = json.loads(full.read_text())
rows = s['rows']; assert len(rows) == 50
r = max((r for r in rows if r['expected'] == 'UNSAT'), key=lambda r: r['gap_seconds'])
assert r['rank'] == 31 and r['candidate_conflicts'] == 9019812
dependencies[str(full.relative_to(root))] = sha(full)
native = root / 'benchmarks/runs/20260908-5893-preclean-rank31-extended'
cached = root / 'benchmarks/runs/20260908-5735-root-reuse-rank31-prefix'
for campaign in (native, cached):
    p = campaign / 'results/kissat.json'; record = json.loads(p.read_text())
    assert record['exit_code'] == 0 and record['scratch_deleted']
    assert record['binary_sha256'] == sha(campaign / 'pins/kissat')
    dependencies[str(p.relative_to(root))] = sha(p)
    for rel, digest in record['trace_sha256'].items():
        path = campaign / 'traces' / rel; assert sha(path) == digest
        dependencies[str(path.relative_to(root))] = digest
prefix = read_prefix(cached / 'traces/kissat.prefix.tsv')
assert prefix['count'] == 89336421 and prefix['hash'] == 'f7f8f88b101c79fa'
nm = json.loads((native / 'manifest.json').read_text())
assert nm['input'] == 'benchmarks/cache/' + r['instance']
assert sha(root / nm['input']) == nm['input_sha256_compressed']
old = root / 'benchmarks/runs/20260909-6376-rank35-stamp-prefix'
om = json.loads((old / 'manifest.json').read_text())
for rel in ('run.py', 'array.sbatch'): assert sha(old / rel) == om['hashes'][rel]
for folder in ('pins', 'logs', 'traces', 'results'): (c / folder).mkdir(parents=True)
shutil.copy2(binary, c / 'pins/candidate')
runner = (old / 'run.py').read_text().replace("('candidate', 'previous')", "('candidate',)").replace('tactix-6376-', 'tactix-6605-')
(c / 'run.py').write_text(runner)
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6376-rank35-stamp-prefix', '6605-material-rank31-prefix').replace('--array=0-1\n', '--array=0\n')
assert '--array=0\n' in batch
(c / 'array.sbatch').write_text(batch)
prior = root / 'benchmarks/reports/2026-09-09-material-full50-audit-6602.json'
protected = json.loads(prior.read_text())['protected_campaign_hashes']
for rel, digest in protected.items(): assert sha(root / rel) == digest
m = dict(created_at=dt.datetime.now(dt.timezone.utc).isoformat(), rank=31,
    input=nm['input'], input_sha256_compressed=nm['input_sha256_compressed'],
    stop_conflict=1000000, stream_from=300000, common_decisions=prefix['count'], prior_common_hash=prefix['hash'],
    candidate_authority=dict(path=str(build.relative_to(root)), sha256=sha(build)),
    cached_native=str(native.relative_to(root)), cached_common=str(cached.relative_to(root)),
    dependencies=dependencies, protected_campaign_hashes=protected,
    resource_contract=dict(partition='cpu-dense-hwulab-q', cpus_per_task=12, memory_mib=48000,
        array='0', throttle=None, node_constraint=None, solve_timeout=1200, scheduler_wall_seconds=1500),
    hypothesis='Rank31 is the largest current UNSAT excess in complete6596. Test current6579 against the full retained native1M trace, validating the inherited300K prefix before any later localization.',
    validation='Hash-bind source, pins, complete native/current traces and partial proof; require requested stop and zero scratch residue. If the old300K premise changed, report earlier_prefix_changed and investigate earlier; never mislabel it a solver failure or skip directly to the old394397 split.',
    selection=dict(path=str(full.relative_to(root)), sha256=sha(full), rule='largest UNSAT gap_seconds over all50'),
    test_consideration='No solver/observer/checker change. Reuse immutable6376 runner with candidate-only arm and scratch label. Maintained prefix/framing/common-hash and complete-stream tests; validator explicitly checks zero threshold-overlap for retained native trace and full row/counter accounting.',
    limitation='Diagnostic only; partial proof hashed/deleted, not independently checked final certificate or runtime acceptance. No new Kissat run/reference refresh, no source-fuzz access, no production change.')
m['hashes'] = {rel: sha(c / rel) for rel in ('pins/candidate', 'run.py', 'array.sbatch')}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
(c / 'pins/candidate').chmod((c / 'pins/candidate').stat().st_mode & ~0o222)
print(c)
