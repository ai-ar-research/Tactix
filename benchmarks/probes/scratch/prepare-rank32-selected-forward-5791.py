#!/usr/bin/env python3
"""Prepare a paired, source-unchanged selected forward-action diagnostic."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-5771-reuse-target-rank32-extended'
witness = root / 'benchmarks/runs/20260908-5787-rank32-native-forward-end-formula/summary.json'
campaign = root / 'benchmarks/runs/20260908-5791-rank32-selected-forward'

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(8 << 20), b''): h.update(block)
    return h.hexdigest()

base = json.loads((parent / 'manifest.json').read_text())
for rel, digest in base['hashes'].items(): assert sha(parent / rel) == digest
summary = json.loads(witness.read_text())
assert summary['observer_parent_trace_and_proof_equal']
assert summary['candidate_only_copies'] == 0 and summary['kissat_only_copies'] == 8
clause = summary['kissat_only'][0]['row'][0][1]
assert len(clause) == 10
selector = ','.join(map(str, clause))
campaign.mkdir(exist_ok=False)
for folder in ('pins', 'logs', 'results', 'traces'): (campaign / folder).mkdir()
for arm in ('candidate', 'kissat'):
    shutil.copy2(parent / 'pins' / arm, campaign / 'pins' / arm)
runner = (parent / 'run.py').read_text().replace('tactix-5771-', 'tactix-5791-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
extra = f'''env[prefix + '_FORWARD_CLAUSE_TRACE'] = str(c / 'traces' / (arm + '.forward.tsv'))
env[prefix + '_EVENT_TRACE_FORWARD_CLAUSE'] = {selector!r}
env['SAT_EVENT_TRACE_FORWARD_CLAUSE_CONFLICT'] = '179463'
'''
assert '_FORWARD_CLAUSE_TRACE_STOP' not in extra
(campaign / 'run.py').write_text(runner.replace(anchor, extra + anchor))
(campaign / 'array.sbatch').write_text((parent / 'array.sbatch').read_text().replace(str(parent), str(campaign)))
m = dict(base)
m.update(parent_campaign=str(parent.relative_to(root)), selected_clause=clause,
         witness_authority=dict(path=str(witness.relative_to(root)), sha256=sha(witness)),
         hypothesis='Determine why Tactix deletes a clause retained by Kissat at the first differing forward boundary; do not assume valid extra simplification should be disabled.',
         validation='Both arms must exactly replay all parent300K ordinary trace hashes and partial proof bytes. Additional selected forward traces must exist and contain an action for the exact witnessed clause.',
         native_rerun_reason='Selected candidate scheduling/action/partner missing from existing evidence; diagnostic only, fixed runtime baseline unchanged.',
         test_consideration='No solver code change: use existing immutable trace hooks with exact ten-literal selection. Compare full parent replay to reject observer side effects; inspect selected records to reject missed selectors.')
m['hashes'] = {str(p.relative_to(campaign)): sha(p) for p in (campaign / 'run.py', campaign / 'array.sbatch', campaign / 'pins/candidate', campaign / 'pins/kissat')}
(campaign / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (campaign / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(campaign)
