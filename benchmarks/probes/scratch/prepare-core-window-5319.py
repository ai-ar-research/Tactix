#!/usr/bin/env python3
"""Extend the immutable 5317 candidate through the known later decision tie."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5317-core-unit-candidate'
new = root / 'benchmarks/runs/20260907-5319-core-decision-window'
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
            prefix + "_EVENT_TRACE_TRAJECTORY_ONLY": "1",
            prefix + "_EVENT_TRACE_KEEP_SNAPSHOTS": "1",
            prefix + "_EVENT_TRACE_CONFLICT_FROM": "50000",
            prefix + "_EVENT_TRACE_CONFLICT_TO": "75451",
            prefix + "_EVENT_TRACE_STOP_AFTER_CONFLICT": "75452",
            prefix + "_EVENT_TRACE_LIMIT": "1000000"})''' + runner[end:]
(new / 'run.py').write_text(runner)
(new / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(new)).replace('core-unit-candidate', 'core-decision-window'))
previous = json.loads((old / 'manifest.json').read_text())
manifest = {k: previous[k] for k in ('source_commit', 'source_patch', 'input_sha256_compressed', 'contract')}
manifest.update(
    purpose='UNFUZZED candidate: test whether original-core unit repair also closes the later warmup decision tie at conflict 75451.',
    reference_campaign=str(old.relative_to(root)),
    acceptance='Ordered decision stream including warmup matches through the old divergence, bounded trace stops without truncation, scratch deleted. No verdict or runtime claim.',
    hashes={str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [*(new / 'pins').iterdir(), new / 'candidate.patch', new / 'run.py', new / 'array.sbatch']})
(new / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(new)
