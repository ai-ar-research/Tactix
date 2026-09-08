#!/usr/bin/env python3
"""Use phase detail because IMPLY_ONLY disables the startup snapshot hooks."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260908-5496-square-definition'
c = root / 'benchmarks/runs/20260908-5497-square-definition-phase'
assert json.loads((old / 'summary.json').read_text())['clean_stops'] == 2
runner = (old / 'run.py').read_text().replace("'tactix-5496-'", "'tactix-5497-'")
runner = runner.replace("'_EVENT_TRACE_IMPLY_ONLY'", "'_EVENT_TRACE_PHASE_ONLY'")
runner = runner.replace("'.imply.tsv'", "'.phase.tsv'")
runner = runner.replace("'SAT_EVENT_TRACE_IMPLY_LITERALS'", "'SAT_EVENT_TRACE_PHASE_VAR'")
assert '_EVENT_TRACE_IMPLY_ONLY' not in runner
compile(runner, str(c / 'run.py'), 'exec')
c.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces', 'results'):
    (c / name).mkdir()
for arm in ('candidate', 'kissat'):
    shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)),
         created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
         prior_summary_sha256=sha(old / 'summary.json'),
         observer_failure='5496 has two clean stops and full replay, but IMPLY_ONLY returns false from want_detail/detail_enabled, suppressing slot snapshots. Preserve its missing-snapshot comparator failure. Reuse PHASE_ONLY from validated5482; no binary/source edit.',
         validation='Require exact complete decisions/prefix/boundary/phase streams against5482, clean stops, trace/pin hashes, selected binary framing and factor-exit large snapshot. Eight snapshot fixtures; no performance or complete proof claim.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in
               [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)
