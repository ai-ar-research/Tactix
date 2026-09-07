#!/usr/bin/env python3
"""Validate filtered observers against retained full bytes before trusting them."""
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5398-rank35-prefix'
c = root / 'benchmarks/runs/20260907-5400-window-validation'
c.mkdir(exist_ok=False)
for d in ('pins', 'logs', 'traces', 'results'):
    (c / d).mkdir()
for arm, binary in [('candidate', 'build/pin-5399-window-observer/tactix'),
                    ('kissat', 'tools/solvers/variants/kissat-window-observer-5399/build/kissat')]:
    shutil.copy2(root / binary, c / 'pins' / arm)
runner = (old / 'run.py').read_text()
runner = runner.replace("'SAT_DECISION_PREFIX_COMMON': '1'", "'SAT_DECISION_PREFIX_COMMON': str(m.get('common_decisions', 1))")
runner = runner.replace('r = dict(arm=arm,', "env['SAT_DECISION_STREAM_CONFLICT_FROM'] = str(m['stream_from'])\nr = dict(arm=arm,")
runner = runner.replace('r = dict(arm=arm,', """env[prefix + '_CHOICE_STATE_TRACE'] = str(c / 'traces' / (arm + '.choice.tsv'))
env['SAT_CHOICE_STATE_CONFLICT'] = str(m['choice_conflict'])
env['SAT_CHOICE_STATE_VARIABLE'] = str(m['choice_variable'])
env[prefix + '_MACRO_BOUNDARY_TRACE'] = str(c / 'traces' / (arm + '.macro.tsv'))
env['SAT_MACRO_BOUNDARY_CONFLICT_FROM'] = '13000'
r = dict(arm=arm,""")
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)),
         candidate_authority='benchmarks/runs/20260907-5399-window-observers/manifest.json',
         candidate_source='.claude/worktrees/runtime-window-observer-5399',
         input=m['input'], stop_conflict=13833, stream_from=13800,
         choice_conflict=13832, choice_variable=483970, common_decisions=18516149,
         hypothesis='Locate whether the first Rank-35 choice split is assignment, score/heap or focused queue state. The new stream window must reproduce the retained 5398 bytes exactly; cumulative prefix hashing remains unfiltered.',
         validation='Two clean stops, exact nonempty decision window versus retained old full bytes, matching prior boundary prefix, independently computed cumulative prefix/hash, all pin hashes and partial-certificate cleanup.')
m['input_sha256_compressed'] = sha(root / m['input'])
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)
