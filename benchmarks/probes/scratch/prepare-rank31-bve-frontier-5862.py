#!/usr/bin/env python3
"""Observe first BVE schedule and candidate lifecycle after matched caller exit."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-5856-forward-caller-rank31-prefix'
c = root / 'benchmarks/runs/20260908-5862-rank31-bve-frontier'
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()
m = json.loads((parent / 'manifest.json').read_text())
s = json.loads((parent / 'summary.json').read_text())
assert s['comparisons']['parent']['decisions']['complete_equal']
assert s['comparisons']['kissat']['first_forward_bve_difference']['ordinal'] == 35
for rel, digest in m['hashes'].items(): assert sha(parent / rel) == digest
native_record = root / m['cached_controls']['kissat']['record']
assert sha(native_record) == m['cached_controls']['kissat']['sha256']
native = native_record.parent.parent / 'pins/kissat'
assert sha(native) == json.loads(native_record.read_text())['binary_sha256']
c.mkdir(exist_ok=False)
for folder in ('pins', 'logs', 'results', 'traces'): (c / folder).mkdir()
shutil.copy2(parent / 'pins/candidate', c / 'pins/candidate')
shutil.copy2(native, c / 'pins/kissat')
runner = (parent / 'run.py').read_text().replace('tactix-5856-', 'tactix-5862-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
extra = '''env[prefix + '_BVE_CANDIDATE_TRACE'] = str(c / 'traces' / (arm + '.bve-candidates.tsv'))
env['SAT_BVE_CANDIDATE_CONFLICT'] = '179823'
env[prefix + '_BVE_SCHEDULE_TRACE'] = str(c / 'traces' / (arm + '.bve-schedule.tsv'))
env['SAT_BVE_SCHEDULE_CONFLICT'] = '179823'
env['SAT_BVE_SCHEDULE_ORDINAL'] = '0'
env['SAT_BVE_SCHEDULE_MEMBERS'] = '1'
'''
(c / 'run.py').write_text(runner.replace(anchor, extra + anchor))
batch = (parent / 'array.sbatch').read_text().replace(str(parent), str(c))
batch = batch.replace('#SBATCH --array=0-0', '#SBATCH --array=0-1')
batch = batch.replace('5856-caller-rank31', '5862-bve-frontier')
(c / 'array.sbatch').write_text(batch)
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    parent_campaign=str(parent.relative_to(root)),
    hypothesis='After5849 closes the forward-exit count residual,locate earliest BVE divergence: first heap membership/score/order versus per-candidate application/propagation/budget. Do not infer scheduling identity from clause counts.',
    validation='Require both complete300K ordinary trace hashes and partial proof bytes to replay their cached arms exactly. Compare selected schedule membership/score/topology and candidate lifecycle in order,including units and propagation deltas.',
    native_rerun_reason='Cached native5735 lacks the BVE schedule and candidate lifecycle at179823. This fills missing causal diagnostics only;the6810.066s fixed runtime denominator remains unchanged.',
    test_consideration='Observer configuration only,unchanged immutable binaries. Existing trace protocol tests and40 candidate fixed proofs retained; exact per-arm ordinary trace/proof replay required before interpreting new diagnostic fields.',
    contract='Two separate proof-on stopped diagnostic tasks,12CPU/48000MiB each,1200s solver and25min allocation. No node constraint or throttle;not runtime acceptance.',
    limitation='Candidate/native diagnostic rerun only;not a reference refresh,checked final proof,or full-state-equivalence claim.',
    resource_contract=dict(m['resource_contract'], array='0-1'),
    localization_authority=dict(path=str((parent / 'summary.json').relative_to(root)), sha256=sha(parent / 'summary.json')))
m['cached_controls']['candidate'] = dict(record=str((parent / 'results/candidate.json').relative_to(root)),
    sha256=sha(parent / 'results/candidate.json'))
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in
               [c / 'run.py', c / 'array.sbatch', c / 'pins/candidate', c / 'pins/kissat']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
