#!/usr/bin/env python3
"""Freeze trace-free stopped-prefix cost pins after decision-prefix closure."""
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
c = root / 'benchmarks/runs/20260907-5382-prefix-cost'
old = root / 'benchmarks/runs/20260907-5379-vivify-retry-runtime'
c.mkdir(exist_ok=False)
for d in ('pins', 'logs', 'results'): (c / d).mkdir()
shutil.copy2(root / 'build/pin-5382-prefix-cost/tactix', c / 'pins/tactix')
for n in ('kissat', 'runtime_task.py', 'candidate.patch'): shutil.copy2(old / 'pins' / n, c / 'pins' / n)
shutil.copy2(root / 'benchmarks/probes/scratch/prefix-cost-task-5382.py', c / 'pins/run.py')
base = root / 'benchmarks/runs/20260907-5381-vivify-retry-extended'
s = (base / 'array.sbatch').read_text().replace(str(base), str(c)).replace('/run.py', '/pins/run.py')
(c / 'array.sbatch').write_text(s)
m = dict(purpose='Price the now-identical million-conflict Rank-34 prefix with semantic tracing disabled.',
         hypothesis='Residual cost exists even along an identical decision prefix; distinguish logical work counters from engineering throughput and inprocessing timers.',
         input='benchmarks/cache/tierD24-26/0ae917c2e0c1d39cbe411c29b2f5ff08-dislog_a14_x14_n24.cnf.xz',
         input_sha256_compressed='ae23202ebf6de47a653fb74430a7603e155a321f2826929864e9bfb69c41ebbd',
         parent_campaign=str(old.relative_to(root)), trajectory_evidence=str(base.relative_to(root)),
         build='build/pin-5382-prefix-cost', features=dict(event_trace=False, conflict_stop=True),
         contract='Full-width 0-1, 12 logical CPUs/48000 MiB, one bound solver, proof-on, no throttle/node constraint. Perf/topology/VM measurements retained.',
         acceptance='Both intentional zero/UNKNOWN stops at one million conflicts with clean scratch; counters must establish actual stop. This is not a checked verdict or performance acceptance.',
         limitations='One diagnostic wave. Prior trace pins establish identical decisions, not full state identity; final analysis/stop bookkeeping can differ. Do not infer identical propagation work or attribute unmatched rule timers. No whole-solve or promotion claim.')
m['hashes'] = {str(p.relative_to(c)): hashlib.sha256(p.read_bytes()).hexdigest() for p in [*(c / 'pins').iterdir(), c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
