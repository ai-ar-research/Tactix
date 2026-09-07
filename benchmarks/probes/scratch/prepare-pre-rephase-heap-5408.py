#!/usr/bin/env python3
"""Separate pre-Rephase state from later warm-up/backtrack permutations."""
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5406-rank35-root-heap-events'
c = root / 'benchmarks/runs/20260907-5408-rank35-pre-rephase-heap'
c.mkdir(exist_ok=False)
for d in ('pins','logs','traces','results'): (c / d).mkdir()
for a in ('candidate','kissat'): shutil.copy2(old / 'pins' / a, c / 'pins' / a)
runner = (old / 'run.py').read_text().replace("'tactix-5406-'", "'tactix-5408-'")
runner = runner.replace("'SAT_EVENT_TRACE_HEAP_SET_CONFLICT': '13832'", "'SAT_EVENT_TRACE_HEAP_SET_CONFLICT': '13560'")
runner = runner.replace("'SAT_EVENT_TRACE_HEAP_SET_DECISION': '18441404'", "'SAT_EVENT_TRACE_HEAP_SET_DECISION': '17798663'")
runner = runner.replace("'_EVENT_TRACE_CONFLICT_FROM': '13413'", "'_EVENT_TRACE_CONFLICT_FROM': '13414'")
runner = runner.replace("'_EVENT_TRACE_CONFLICT_TO': '13415'", "'_EVENT_TRACE_CONFLICT_TO': '13563'")
runner = runner.replace("'109797,109796,109877,109815'", "'109797,109796,109877,109815,109841'")
runner = runner.replace('r = dict(scratch=', "env['SAT_DECISION_HEAP_SNAPSHOT'] = '1'\nenv.pop(prefix + '_CHOICE_STATE_TRACE', None)\nr = dict(scratch=")
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)), stop_conflict=13563, stream_from=13560,
         snapshot_conflict=13560, snapshot_decisions=17798663,
         hypothesis='Snapshot the last ordinary pre-Rephase decision and selected root/heap lifecycle events through Rephase 13563, distinguishing pre-existing order from warm-up or reinsertion effects.',
         validation='Two clean stops, prefix count/hash matches the established 5405 endpoint, nonempty unique pre-Rephase heap snapshots and selected events, all pin/trace hashes and certificate cleanup; no timing claim.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(),c / 'run.py',c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
print(c)
