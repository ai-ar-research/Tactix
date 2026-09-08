#!/usr/bin/env python3
"""Extend the proved common300K prefix to locate the later full-run loss."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-5879-forward-preclean-rank31-prefix'
c = root / 'benchmarks/runs/20260908-5893-preclean-rank31-extended'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((parent / 'manifest.json').read_text())
s = json.loads((parent / 'summary.json').read_text())
assert s['comparisons']['kissat']['decisions']['complete_equal']
assert s['comparisons']['kissat']['decisions']['matching_rows'] == 89336421
for rel, digest in m['hashes'].items(): assert sha(parent / rel) == digest
binding = m['cached_controls']['kissat']
native_record = root / binding['record']
assert sha(native_record) == binding['sha256']
native = native_record.parent.parent / 'pins/kissat'
assert sha(native) == json.loads(native_record.read_text())['binary_sha256']
c.mkdir(exist_ok=False)
for folder in ('pins', 'logs', 'results', 'traces'): (c / folder).mkdir()
shutil.copy2(parent / 'pins/candidate', c / 'pins/candidate')
shutil.copy2(native, c / 'pins/kissat')
(c / 'run.py').write_text((parent / 'run.py').read_text().replace('tactix-5879-', 'tactix-5893-'))
(c / 'array.sbatch').write_text((parent / 'array.sbatch').read_text().replace(str(parent), str(c)).replace('#SBATCH --array=0-0', '#SBATCH --array=0-1').replace('5879-preclean-rank31', '5893-preclean-rank31-extended'))
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(), parent_campaign=str(parent.relative_to(root)),
    stop_conflict=1000000, stream_from=300000, common_decisions=89336421,
    hypothesis='5870 closes all89336421 decisions through300K but full Rank31 worsens502.337 to702.344s,2.770179x native conflicts. Localize earliest later decision/macro/forward-BVE divergence before retaining this as a useful performance repair.',
    validation='Both new cumulative common hashes must equal each frozen300K prefix hash. Compare complete new suffix300K..1M and portable boundaries. Expected1M stop,hash-bound binaries,input and scratch cleanup. No acceptance timing or final certificate claim.',
    contract='Two independent12CPU/48000MiB diagnostic tasks,1200s solver/25min allocation,no throttle/node constraint.',
    limitation='Isolated regressing candidate;not promoted or accepted. Native rerun adds previously missing300K..1M causal trace,not a new runtime denominator.',
    resource_contract=dict(m['resource_contract'], array='0-1'),
    cached_controls=dict(candidate=dict(record=str((parent / 'results/candidate.json').relative_to(root)),sha256=sha(parent / 'results/candidate.json')),kissat=binding),
    test_consideration='No solver change;extend existing tested prefix observer with common-count89336421 and complete300K suffix. Existing prefix overlap/chunk boundary tests retained. No broad event tracer or extra early-exit selector.',
    runtime_selection=dict(path='benchmarks/runs/20260908-5880-forward-preclean-ranks1-40/next-losses.json',sha256=sha(root / 'benchmarks/runs/20260908-5880-forward-preclean-ranks1-40/next-losses.json')))
m['hashes'] = {str(p.relative_to(c)):sha(p) for p in [c / 'run.py', c / 'array.sbatch', c / 'pins/candidate', c / 'pins/kissat']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
