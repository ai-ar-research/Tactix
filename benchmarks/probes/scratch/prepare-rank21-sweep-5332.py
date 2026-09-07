#!/usr/bin/env python3
"""Extend macro tracing and add watch-order hashes at Sweep boundaries only."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5331-rank21-boundary'
new = root / 'benchmarks/runs/20260907-5332-rank21-sweep-boundary'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces'):
    (new / name).mkdir()
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
for arm in ('core', 'sequential'):
    shutil.copy2(old / 'pins' / arm, new / 'pins' / arm)
shutil.copy2(root / 'tools/solvers/variants/kissat-snapshot-5332/build/kissat', new / 'pins/kissat')
for path in old.glob('*.patch'):
    shutil.copy2(path, new / path.name)
shutil.copy2(root / 'benchmarks/probes/scratch/kissat-snapshot-5332.patch', new / 'kissat-snapshot.patch')
runner = (old / 'run.py').read_text().replace('prefix + "_EVENT_TRACE_CONFLICT_TO": "0"','prefix + "_EVENT_TRACE_CONFLICT_TO": "10000"').replace('prefix + "_EVENT_TRACE_STOP_AFTER_CONFLICT": "1"','prefix + "_EVENT_TRACE_STOP_AFTER_CONFLICT": "10001"')
runner = runner.replace('prefix + "_EVENT_TRACE_SNAPSHOT_ONLY": "1",', 'prefix + "_EVENT_TRACE_SNAPSHOT_ONLY": "1",\n            prefix + "_EVENT_TRACE_BOUNDARY_ONLY": "1",\n            "SAT_EVENT_TRACE_FORMULA_MACRO_MATCH": "weep",\n            "SAT_EVENT_TRACE_WATCH_ORDER": "1",')
(new / 'run.py').write_text(runner)
(new / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old),str(new)).replace('rank21-boundary','rank21-sweep-boundary'))
manifest = json.loads((old / 'manifest.json').read_text())
manifest.pop('job', None)
manifest.update(
    purpose='UNFUZZED three-arm macro-state comparison through conflict 10001; order-sensitive watch digests only at Sweep boundaries.',
    acceptance='No truncation; configured conflict stop returns 0 (Tactix) or -15 (Kissat), and scratch deleted. Boundary-only filtering omits terminal analysis records; compare observed macro states without claiming unobserved decisions match.',
    observer_note='Kissat now implements SNAPSHOT_ONLY in a fresh build. Existing BOUNDARY_ONLY excludes analysis_end; stop control remains outside that output filter. 5331 Kissat trace was truncated and is invalid.',
    hashes={str(p.relative_to(new)): sha(p) for p in [*(new/'pins').iterdir(),*(new.glob('*.patch')),new/'run.py',new/'array.sbatch']})
(new / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
for p in (new / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)
