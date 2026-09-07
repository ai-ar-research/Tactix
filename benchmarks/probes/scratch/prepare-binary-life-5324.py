#!/usr/bin/env python3
"""Trace exact producers of the binary owner-562 ordering difference."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5321-all-root-order'
new = root / 'benchmarks/runs/20260907-5324-binary-producers'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces'):
    (new / name).mkdir()
for arm in ('tactix', 'kissat'):
    shutil.copy2(old / 'pins' / arm, new / 'pins' / arm)
for name in ('candidate.patch', 'tactix-observer.patch', 'kissat-observer.patch'):
    shutil.copy2(old / name, new / name)
runner = (old / 'run.py').read_text().replace(
    '"SAT_EVENT_TRACE_ALL_ROOTS": "1",',
    '''"SAT_EVENT_TRACE_ALL_ROOTS": "1",
            prefix + "_BINARY_LIFECYCLE_TRACE": str(C / "traces" / (arm + "-binary.tsv")),
            prefix + "_BINARY_LIFECYCLE_PAIRS": ";".join(
                "562," + str(other) for other in (-841,-583,-587,-581,-1038,-1110,-690,-616,-1205,-1112,-649)),''')
(new / 'run.py').write_text(runner)
(new / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(new)).replace('all-root-order', 'binary-producers'))
previous = json.loads((old / 'manifest.json').read_text())
manifest = {k: previous[k] for k in ('source_commit', 'source_patch', 'input_sha256_compressed', 'contract', 'kissat_source', 'acceptance')}
manifest.update(
    purpose='Core-only unfuzzed source: identify producers/source clauses of the owner-562 watch-order mismatch during Sweep; existing lifecycle observer, eleven selected pairs.',
    reference_campaign=str(old.relative_to(root)),
    hashes={str(p.relative_to(new)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in [*(new / 'pins').iterdir(), new / 'candidate.patch', new / 'run.py', new / 'array.sbatch',
                      new / 'tactix-observer.patch', new / 'kissat-observer.patch']})
(new / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(new)
