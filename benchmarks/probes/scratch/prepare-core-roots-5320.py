#!/usr/bin/env python3
"""Trace every root assignment at the repaired Sweep boundary, unchanged pins."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5317-core-unit-candidate'
new = root / 'benchmarks/runs/20260907-5320-core-root-order'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces'):
    (new / name).mkdir()
for arm in ('tactix', 'kissat'):
    shutil.copy2(old / 'pins' / arm, new / 'pins' / arm)
shutil.copy2(old / 'candidate.patch', new / 'candidate.patch')
runner = (old / 'run.py').read_text()
begin = runner.index('env.update(')
end = runner.index('\ntry:', begin)
runner = runner[:begin] + '''env.update({prefix + "_EVENT_TRACE": str(C / "traces" / (arm + ".tsv")),
            prefix + "_EVENT_TRACE_HEAP_VARS_ONLY": "1",
            "SAT_EVENT_TRACE_HEAP_VARS": ",".join(map(str, range(1, 22501))),
            prefix + "_EVENT_TRACE_CONFLICT_FROM": "26913",
            prefix + "_EVENT_TRACE_CONFLICT_TO": "26913",
            prefix + "_EVENT_TRACE_STOP_AFTER_CONFLICT": "26914",
            prefix + "_EVENT_TRACE_LIMIT": "200000"})''' + runner[end:]
(new / 'run.py').write_text(runner)
(new / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(new)).replace('core-unit-candidate', 'core-root-order'))
previous = json.loads((old / 'manifest.json').read_text())
manifest = {k: previous[k] for k in ('source_commit', 'source_patch', 'input_sha256_compressed', 'contract')}
manifest.update(
    purpose='UNFUZZED candidate: compare every root assignment at conflict 26913, not just the 94 explicitly emitted Sweep units.',
    selection='Existing heap-vars observer with external IDs 1..22500; diagnostic selection only, not solver policy.',
    reference_campaign=str(old.relative_to(root)),
    acceptance='Compare ordered root literals and reasons at the repaired boundary; stop at 26914, scratch clean. No verdict or runtime claim.',
    hashes={str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [*(new / 'pins').iterdir(), new / 'candidate.patch', new / 'run.py', new / 'array.sbatch']})
(new / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(new)
