#!/usr/bin/env python3
"""Bracket the selected large watch's position at three Rephase entries."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5415-rank35-warmup-implications'
c = root / 'benchmarks/runs/20260907-5416-rank35-watch-origin'
c.mkdir(exist_ok=False)
for d in ('pins', 'logs', 'traces', 'results'): (c / d).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
runner = (old / 'run.py').read_text().replace("'tactix-5415-'", "'tactix-5416-'")
runner = runner.replace("arm = ('candidate', 'kissat')[int(sys.argv[1])]", "task = int(sys.argv[1])\narm = ('candidate', 'kissat')[task % 2]\ntag = f'{task // 2:02d}-' + arm\npoint = (2002, 8033, 13563)[task // 2]")
runner = runner.replace("arm + '", "tag + '")
runner = runner.replace('r = dict(scratch=', """env[prefix + '_MACRO_WATCH_TRACE'] = str(c / 'traces' / (tag + '.watch.tsv'))
env['SAT_MACRO_WATCH_EVENT'] = 'rephase_begin'
env['SAT_MACRO_WATCH_EXTERNAL'] = '-145'
env['SAT_MACRO_WATCH_CONFLICT'] = str(point)
r = dict(snapshot_conflict=point, scratch=""")
# The task-specific trace glob must match its output tag, not all same-arm rows.
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)).replace('--array=0-1', '--array=0-5'))
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)), snapshot_conflicts=[2002, 8033, 13563],
         hypothesis='Locate when the ternary reason watch (-109796,-145,109797) moves ahead of the binary implications in Tactix; compare full large-watch ordinal/pose snapshots at earlier Rephase entries.',
         contract='Full-width 0–5, one bound proof-on solver per task, 12 logical CPUs/48000 MiB, no throttle/node constraint. Diagnostic, not timing acceptance.',
         validation='Six clean stopped prefixes and decision streams identical per arm to 5415, nonempty selected macro snapshots at their exact conflict, all pin/trace hashes and scratch cleanup.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(c)
