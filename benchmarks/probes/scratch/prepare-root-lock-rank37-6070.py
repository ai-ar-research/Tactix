#!/usr/bin/env python3
"""Prepare one current-source Rank37 probe against cached native0--1M."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
runs = root / 'benchmarks/runs'
c = runs / '20260908-6070-root-lock-rank37-prefix'
assert not c.exists()
old = runs / '20260908-6060-root-lock-rank32-prefix'
native = runs / '20260908-5478-rank37-bound-extended'
early = runs / '20260907-5463-rank37-bound-prefix'
authority = runs / '20260908-6049-reduce-root-lock'

def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''):
            h.update(block)
    return h.hexdigest()

om = json.loads((old / 'manifest.json').read_text())
nm = json.loads((native / 'manifest.json').read_text())
for rel, digest in om['hashes'].items():
    assert sha(old / rel) == digest, rel
for rel, digest in om['diagnostic_source_hashes'].items():
    assert sha(root / om['diagnostic_source'] / rel) == digest, rel
selection = runs / '20260908-6061-root-lock-ranks1-40/next-losses.json'
loss = json.loads(selection.read_text())['losses'][0]
assert loss['rank'] == 37 and loss['extra_seconds'] > 0
for base in (native, early):
    r = json.loads((base / 'results/kissat.json').read_text())
    assert r['exit_code'] == 0 and r['scratch_deleted']
    for name, digest in r['trace_sha256'].items():
        assert sha(base / 'traces' / name) == digest, name
for name in ('pins', 'traces', 'logs', 'results'):
    (c / name).mkdir(parents=True)
shutil.copy2(old / 'pins/candidate', c / 'pins/candidate')
(c / 'pins/candidate').chmod(0o555)
runner = (old / 'run.py').read_text().replace('tactix-6060-', 'tactix-6070-')
lines = runner.splitlines(keepends=True)
removed = [line for line in lines if "env[prefix + '_REDUCE_TRACE" in line]
assert len(removed) == 3
runner = ''.join(line for line in lines if line not in removed)
assert '_REDUCE_TRACE' not in runner and '_EVENT_TRACE' in runner
compile(runner, str(c / 'run.py'), 'exec')
(c / 'run.py').write_text(runner)
batch = (old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6060-root-lock-rank32', '6070-root-lock-rank37')
assert '--array=0-0' in batch and '--cpus-per-task=12' in batch and '--mem=48000M' in batch
assert '%' not in batch.split('--array=')[1].splitlines()[0] and '--nodelist' not in batch
(c / 'array.sbatch').write_text(batch)
binding = lambda p: dict(record=str(p.relative_to(root)), sha256=sha(p))
m = dict(candidate_authority=str(authority.relative_to(root)), candidate_source=om['candidate_source'],
    authority_sha256=sha(authority / 'manifest.json'), diagnostic_source=om['diagnostic_source'],
    diagnostic_source_hashes=om['diagnostic_source_hashes'], input=nm['input'],
    input_sha256_compressed=nm['input_sha256_compressed'], stop_conflict=1000000, stream_from=0,
    common_decisions=35346388, cached_early_native=binding(early / 'results/kissat.json'),
    cached_controls=dict(kissat=binding(native / 'results/kissat.json')),
    selection=dict(path=str(selection.relative_to(root)), sha256=sha(selection), rank=37, excess_seconds=loss['extra_seconds']),
    hypothesis='Largest current full40 loss: compare all current decisions from0 through1M to cached native; localize first portable worse-side mechanism, including any early difference before old188502 split.',
    validation='Bound all files,verify overlap exactly once and actual first100K common hash;stop1M and cleanup required. No assumed parent replay, full-run identity or runtime gain.',
    resource_contract=dict(partition='cpu-dense-hwulab-q', cpus_per_task=12, memory_mib=48000, array='0-0', throttle=None, node_constraint=None, solver_timeout=1200, allocation_minutes=25),
    test_consideration='No solver change or new observer. Reuse6054 suite98/98 and corrected immutable trace pin. Only input/stop/cache bindings differ; prepare asserts runner changes and exact allocations; comparator retains three overlap tests and benchmark decision-prefix tests.',
    created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    limitation='Diagnostic partial proof only. No native rerun,denominator refresh,source promotion or fuzz-artifact access.')
m['hashes'] = {name: sha(c / name) for name in ('pins/candidate', 'run.py', 'array.sbatch')}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(c)
