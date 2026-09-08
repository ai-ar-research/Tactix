#!/usr/bin/env python3
"""Extend the repaired Rank-35 prefix to locate the next portable divergence."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5418-rank35-rephase-confirmation'
c = root / 'benchmarks/runs/20260907-5424-rank35-rephase-prefix'
c.mkdir(exist_ok=False)
for d in ('pins', 'logs', 'traces', 'results'): (c / d).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
runner = (old / 'run.py').read_text().replace("'tactix-5418-'", "'tactix-5424-'")
start = runner.index("env['SAT_DECISION_STREAM_CONFLICT_FROM']")
end = runner.index('r = dict(scratch=', start)
runner = runner[:start] + '''env['SAT_DECISION_STREAM_CONFLICT_FROM'] = str(m['stream_from'])
env[prefix + '_MACRO_BOUNDARY_TRACE'] = str(c / 'traces' / (arm + '.macro.tsv'))
env['SAT_MACRO_BOUNDARY_CONFLICT_FROM'] = '13833'
def interrupted(signum, frame):
    raise SystemExit(128 + signum)
signal.signal(signal.SIGTERM, interrupted)
signal.signal(signal.SIGINT, interrupted)
print('scratch=' + str(scratch), flush=True)
''' + runner[end:]
runner = runner.replace("['timeout', '600',", "['timeout', '1200',")
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('--time=00:12:00', '--time=00:25:00'))
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
for key in ('choice_conflict', 'choice_variable', 'supersession', 'snapshot_conflict', 'snapshot_decisions'):
    m.pop(key, None)
m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
         parent_campaign=str(old.relative_to(root)), stop_conflict=100000,
         stream_from=13833, common_decisions=18516218,
         matched_prefix_authority=str(old.relative_to(root)),
         hypothesis='After closing the first Rank-35 split, determine whether the repaired decision prefix matches through 100000 conflicts; otherwise localize the next decision and macro boundary split.',
         validation='Two bounded stops, immutable pin checks and scratch cleanup. Compare complete prefix hashes, common prefix authority and streamed decisions. Diagnostic only: no accepted solve, proof check or timing claim.',
         continuation='Independent diagnostic proceeds while 5421/5422/5423 proofs run; any actual correctness rejection pauses dependent work.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
