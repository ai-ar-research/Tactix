#!/usr/bin/env python3
"""Capture exact binary multiplicities at the first changed Sweep boundary."""
import hashlib
import json
from pathlib import Path
import shutil

root=Path(__file__).resolve().parents[3]
old=root/'benchmarks/runs/20260907-5331-rank21-boundary'
new=root/'benchmarks/runs/20260907-5334-rank21-binary-delta'
new.mkdir(exist_ok=False)
for name in ('pins','logs','traces'): (new/name).mkdir()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for arm in ('core','sequential'): shutil.copy2(old/'pins'/arm,new/'pins'/arm)
for name in ('core.patch','sequential.patch','tactix-observer.patch'): shutil.copy2(old/name,new/name)
runner=(old/'run.py').read_text().replace('("core", "sequential", "kissat")','("core", "sequential")')
begin,end=runner.index('env.update('),runner.index('\ntry:')
runner=runner[:begin]+'''env.update({"SAT_EVENT_TRACE_BINARY_SET_PATH": str(C / "traces" / (arm + ".tsv")),
            "SAT_EVENT_TRACE_BINARY_SET_MATCH": "name=Sweep|stage=end",
            "SAT_EVENT_TRACE_BINARY_SET_CONFLICT": "23889",
            "SAT_EVENT_TRACE_BINARY_SET_ONCE": "1",
            "SAT_EVENT_TRACE_FORMULA_SET_MATCH": "name=Sweep|stage=end",
            "SAT_EVENT_TRACE_FORMULA_SET_CONFLICT": "23889",
            "SAT_EVENT_TRACE_SET_STOP": "1"})'''+runner[end:]
(new/'run.py').write_text(runner)
(new/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(new)).replace('rank21-boundary','rank21-binary-delta').replace('--array=0-2','--array=0-1'))
m=json.loads((old/'manifest.json').read_text()); m.pop('job',None); m.pop('kissat_source',None)
m.update(purpose='UNFUZZED exact binary-copy delta at first differing Tactix Sweep end, conflict 23889. No global trace or reference run on a different state.',
    hypothesis='Four additional binary copies from sequential rewriting identify the concrete producer to inspect.',
    acceptance='Both stopped exit 0 and delete scratch; each export has complete begin/end markers and declared row count; compare complete binary multisets. No correctness or timing claim.',
    contract='Full-width 0-1; one bound proof-on solver per task; 12 logical CPUs/48000 MiB; no node constraint or throttle; 600-second cap.',
    observer_note='Standalone binary exporter; the existing SET_STOP selector uses the formula match/conflict without requesting a formula export. Stop is immediately after first selected binary snapshot.',
    hashes={str(p.relative_to(new)):sha(p) for p in [*(new/'pins').iterdir(),*(new.glob('*.patch')),new/'run.py',new/'array.sbatch']})
(new/'manifest.json').write_text(json.dumps(m,indent=2)+'\n')
for p in (new/'pins').iterdir():p.chmod(p.stat().st_mode & ~0o222)
print(new)
