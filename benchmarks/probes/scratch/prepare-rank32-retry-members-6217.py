#!/usr/bin/env python3
"""Trace one missing member's producer and entry state of all five omissions."""
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
old = root / 'benchmarks/runs/20260909-6208-rank32-schedule-membership'
s = json.loads((old / 'summary.json').read_text())
subjects = [r['external'] for r in s['kissat_only']]
assert s['parent_replay'] and s['candidate_only'] == [] and not s['shared_score_or_occurrence_differences']
assert subjects == [337674, 390671, 399915, 409823, 444046]
c = root / 'benchmarks/runs/20260909-6217-rank32-retry-members'
assert not c.exists()
for name in ('pins', 'traces', 'results', 'logs'): (c / name).mkdir(parents=True)
m = json.loads((old / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(old / rel) == digest
for arm in ('candidate', 'kissat'): shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
runner = (old / 'run.py').read_text().replace('tactix-6208-', 'tactix-6217-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, "env['SAT_BVE_SCHEDULE_SELECTED'] = '337674,390671,399915,409823,444046'\nenv[prefix + '_RETRY_PRODUCER_TRACE'] = str(c / 'traces' / (arm + '.retry-producer.tsv'))\nenv['SAT_RETRY_PRODUCER_FROM'] = '0'\nenv['SAT_RETRY_PRODUCER_TO'] = '386968'\nenv['SAT_RETRY_PRODUCER_EXTERNAL'] = '337674'\n" + anchor)
compile(runner, str(c / 'run.py'), 'exec'); (c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('6208-rank32', '6217-rank32'))
m.update(parent_summary=dict(path=str((old / 'summary.json').relative_to(root)), sha256=sha(old / 'summary.json')),
    purpose='Determine active/fixed/retired state for allfive native-only initialmembers and preceding retry-mark producers for337674.',
    hypothesis='A different removal/eligibility lifecycle explains the first omittedmember;shared scores and occurrences alone do not establish this.',
    validation='Every6208 trace and partialproof must replay exactly except schedule.tsv,whose entire originalheader/member bytes must be an exact prefix followed only by five selected-state rows. Only retry-producer.tsv is new; source labels are not assumed equivalent.',
    test_consideration='No new solver source. Existing tested selected-state and retry-producer adapters reused with complete parent replay. Validate bounded selected IDs, state schema and appended-schedule framing before interpreting source histories.')
m['hashes'] = {rel: sha(c / rel) for rel in ('pins/candidate', 'pins/kissat', 'run.py', 'array.sbatch')}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
