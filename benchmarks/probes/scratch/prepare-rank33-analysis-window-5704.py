#!/usr/bin/env python3
"""Inspect only the five conflicts across the first unequal score increment."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
root=Path(__file__).resolve().parents[3]
parent=root/'benchmarks/runs/20260908-5698-rank33-score-bumps'
c=root/'benchmarks/runs/20260908-5704-rank33-analysis-window'
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda:stream.read(8<<20),b''):h.update(block)
    return h.hexdigest()
s=json.loads((parent/'summary.json').read_text())
assert s['parent_full_replay'] and s['first_admission_difference_index'] is None
assert s['first_equal_admission_value_difference_index']==520
runner=(parent/'run.py').read_text().replace('tactix-5698-','tactix-5704-')
anchor="assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor)==1
runner=runner.replace(anchor,"""# The earlier9457 formula dumps are not part of this analysis observer.
# Their selector sites differ under an active EventTrace; omit them explicitly.
env.pop('SAT_EVENT_TRACE_FORMULA_SET_PATH', None)
env.pop('SAT_EVENT_TRACE_BINARY_SET_PATH', None)
env.update({prefix + '_EVENT_TRACE': str(c / 'traces' / (arm + '.analysis.tsv')),
    prefix + '_EVENT_TRACE_CONTROL_ONLY': '1',
    prefix + '_EVENT_TRACE_CONFLICT_FROM': '182089',
    prefix + '_EVENT_TRACE_CONFLICT_TO': '182093',
    prefix + '_EVENT_TRACE_STOP_AFTER_CONFLICT': '1000001',
    prefix + '_EVENT_TRACE_SKIP_WATCH': '1',
    prefix + '_EVENT_TRACE_SKIP_HEAP': '1',
    prefix + '_EVENT_TRACE_SKIP_IMPLY': '1',
    prefix + '_EVENT_TRACE_SKIP_DECISIONS': '1',
    prefix + '_EVENT_TRACE_LIMIT': '100000'})
# Prefix stop remains1M. The unreachable event-stop suppresses irrelevant
# macro snapshots before it; only five conflict-analysis windows are retained.
""")
compile(runner,str(c/'run.py'),'exec')
c.mkdir(exist_ok=False)
for name in ('pins','logs','traces','results'):(c/name).mkdir()
for arm in ('candidate','kissat'):shutil.copy2(parent/'pins'/arm,c/'pins'/arm)
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((parent/'array.sbatch').read_text().replace(str(parent),str(c)))
m=json.loads((parent/'manifest.json').read_text())
m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    parent_campaign=str(parent.relative_to(root)),parent_summary_sha256=sha(parent/'summary.json'),
    hypothesis='Selected admissions match, but182093 uses an increment exactly one decay generation ahead in Tactix;182089 still matches. Observe conflict/learned/analyzed/reuse transitions at182089-182093 to distinguish an empty/inactive bump transaction,OTFS round or root-conflict reuse discrepancy.',
    validation='Complete1M parent decisions,prefix,macro,heap,choice and selected-bump bytes plus exact partial-proof bytes. Only predeclared9457 formula/binary dumps omitted; native extra analysis/end journal rows normalized with existing tested parser,all original rows unchanged. Strict22-field selected analysis sequence/window/cap checks.',
    analysis_window=[182089,182093],omitted_parent_dumps=['formula.tsv','binary.tsv'],
    test_consideration='No new solver/observer code;89/89 source5696 remains immutable. New window parser fixtures and existing8 boundary-normalization fixtures validate this configuration; full replay/proof required.',
    limitation='Diagnostic event-stop1000001 is deliberately beyond unchanged prefix-stop1M,only to suppress irrelevant snapshots. No full verdict or timing claim; source-backed observer normalization does not authorize semantic drift.')
m['hashes']={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'run.py',c/'array.sbatch']}
(c/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (c/'pins').iterdir():p.chmod(p.stat().st_mode&~0o222)
command=['sbatch','--parsable',str((c/'array.sbatch').relative_to(root))]
job=subprocess.run(command,cwd=root,capture_output=True,text=True,check=True).stdout.strip();assert job.isdecimal()
(c/'submission.json').write_text(json.dumps(dict(job=job,command=command,manifest_sha256=sha(c/'manifest.json')),indent=2)+'\n')
print('Submitted five-conflict analysis window',job)
