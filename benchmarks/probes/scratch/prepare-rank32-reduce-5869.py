#!/usr/bin/env python3
"""Collect the missing Reduce state at Rank32's unchanged common frontier."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-5857-forward-caller-rank32-prefix'
c = root / 'benchmarks/runs/20260908-5869-rank32-reduce-frontier'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((parent / 'manifest.json').read_text())
s = json.loads((parent / 'summary.json').read_text())
assert s['matching_decisions'] == 61154590 and s['suffix_comparison']['left'][0] == 225987
for rel, digest in m['hashes'].items(): assert sha(parent / rel) == digest
native_record = root / m['cached_controls']['kissat']['record']
assert sha(native_record) == m['cached_controls']['kissat']['sha256']
native = native_record.parent.parent / 'pins/kissat'
assert sha(native) == json.loads(native_record.read_text())['binary_sha256']
c.mkdir(exist_ok=False)
for folder in ('pins', 'logs', 'results', 'traces'): (c / folder).mkdir()
shutil.copy2(parent / 'pins/candidate', c / 'pins/candidate')
shutil.copy2(native, c / 'pins/kissat')
runner = (parent / 'run.py').read_text().replace('tactix-5857-', 'tactix-5869-')
runner = runner.replace("str(m['stream_from'])", "str(m['stream_from'] if arm == 'candidate' else 100000)")
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
extra = '''env[prefix + '_REDUCE_TRACE'] = str(c / 'traces' / (arm + '.reduce.tsv'))
env[prefix + '_REDUCE_TRACE_CONFLICT_FROM'] = '225987'
env[prefix + '_REDUCE_TRACE_CONFLICT_TO'] = '225987'
'''
(c / 'run.py').write_text(runner.replace(anchor, extra + anchor))
batch = (parent / 'array.sbatch').read_text().replace(str(parent), str(c))
batch = batch.replace('#SBATCH --array=0-0', '#SBATCH --array=0-1').replace('5857-caller-rank32', '5869-reduce-frontier')
(c / 'array.sbatch').write_text(batch)
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(), parent_campaign=str(parent.relative_to(root)),
    hypothesis='At common225987 state distinguish Reduce candidate/victim/learned-state mismatch from subsequent Reorder. Full Reduce table,not a sampled clause;no production change.',
    validation='Both ordinary300K trace hashes and partial proof bytes must exactly replay cached arms. Candidate decisions start0,native starts100K to match its frozen suffix. Compare reduction scope,rank,selection,glue,usage,reasons and full clause sets.',
    native_rerun_reason='Cached native5771 has decision/boundary traces but lacks Reduce candidates/victims at225987. Diagnostic only;fixed runtime vector unchanged.',
    contract='Two independent12CPU/48000MiB proof-on diagnostic tasks,1200s solver,25min allocation,no throttle/node constraint.',
    resource_contract=dict(m['resource_contract'], array='0-1'),
    test_consideration='Read-only observer configuration using unchanged tested5849 and cached native pins. Existing reduce trace protocol tests retained; require exact ordinary trace/proof replay before interpretation. No schedule-dump observer or early-exit selector is enabled.',
    stream_from_by_arm=dict(candidate=0, kissat=100000),
    limitation='Not solver timing,final checked proof,or full-state identity. No reference refresh.')
m['cached_controls']['candidate'] = dict(record=str((parent / 'results/candidate.json').relative_to(root)),
    sha256=sha(parent / 'results/candidate.json'))
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [c / 'run.py', c / 'array.sbatch', c / 'pins/candidate', c / 'pins/kissat']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
