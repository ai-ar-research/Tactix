#!/usr/bin/env python3
"""Resolve the first coarse Rank-35 split without inferring cause from counts."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5433-rank35-inprocess-prefix'
c = root / 'benchmarks/runs/20260907-5437-rank35-sweep-binary-boundary'
c.mkdir(exist_ok=False)
for d in ('pins', 'logs', 'traces', 'results'): (c / d).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
shutil.copy2(root / 'build/pin-5417-rephase-root-trace/tactix', c / 'pins/previous')
runner = (old / 'run.py').read_text().replace("'tactix-5433-'", "'tactix-5437-'")
runner = runner.replace("('candidate', 'kissat')", "('previous', 'candidate', 'kissat')")
anchor = 'def interrupted(signum, frame):'
assert runner.count(anchor) == 1
runner = runner.replace(anchor, '''env.update({prefix + '_EVENT_TRACE': str(c / 'traces' / (arm + '.events.tsv')),
    prefix + '_EVENT_TRACE_SNAPSHOT_ONLY': '1',
    prefix + '_EVENT_TRACE_BOUNDARY_ONLY': '1',
    prefix + '_EVENT_TRACE_KEEP_SNAPSHOTS': '1',
    prefix + '_EVENT_TRACE_FORMULA_MACRO': '1',
    prefix + '_EVENT_TRACE_SKIP_HEAP': '1',
    prefix + '_EVENT_TRACE_SKIP_WATCH': '1',
    prefix + '_EVENT_TRACE_SKIP_IMPLY': '1',
    prefix + '_EVENT_TRACE_CONFLICT_FROM': '19365',
    prefix + '_EVENT_TRACE_CONFLICT_TO': '19365',
    prefix + '_EVENT_TRACE_LIMIT': '10000',
    'SAT_EVENT_TRACE_FORMULA_MACRO_MATCH': 'weep',
    'SAT_EVENT_TRACE_BINARY_SET_PATH': str(c / 'traces' / (arm + '.binaries.tsv')),
    'SAT_EVENT_TRACE_BINARY_SET_CONFLICT': '19365',
    'SAT_EVENT_TRACE_BINARY_SET_MATCH': 'weep'})
''' + anchor)
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('--array=0-1', '--array=0-2'))
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda: f.read(1048576), b''): h.update(block)
    return h.hexdigest()
m = json.loads((old / 'manifest.json').read_text())
m.pop('slurm_job_id', None)
m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    parent_campaign=str(old.relative_to(root)), stop_conflict=19366,
    stream_from=19365, common_decisions=18516218,
    previous_authority='benchmarks/runs/20260907-5417-rephase-root/manifest.json',
    localization_authority='benchmarks/runs/20260907-5424-rank35-rephase-prefix/boundary-localization.json',
    hypothesis='First coarse split is Sweep end at conflict 19365: active/large counts match but binary counts are 204961/204963. Compare before/after binary multisets, formula digests and the new phase-entry candidate to distinguish duplicate retirement, genuine clause differences, and closure effects before altering Sweep.',
    validation='Three exit-zero proof-on bounded stops with identical prior confirmed prefix; immutable pins, complete selected snapshots, no trace truncation, partial certificate hashes and cleanup. No final verdict, proof-check or timing acceptance.',
    contract='Full-width 0–2, one bound solver per task, 12 logical CPUs/48000 MiB, no throttle/node constraint. Existing proof checks run independently.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
