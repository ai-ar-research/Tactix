#!/usr/bin/env python3
"""Locate the sequential rewrite's first macro-state difference on Rank 21."""
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5327-bridge-root-order'
new = root / 'benchmarks/runs/20260907-5331-rank21-boundary'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces'):
    (new / name).mkdir()
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
for arm, path in (
    ('core', root / 'build/pin-5331-core-trace/tactix'),
    ('sequential', root / 'build/pin-5325-sequential-trace/tactix'),
    ('kissat', old / 'pins/kissat')):
    shutil.copy2(path, new / 'pins' / arm)
for name, path in (
    ('core.patch', root / 'benchmarks/probes/scratch/core-unit-release-5318.patch'),
    ('sequential.patch', root / 'benchmarks/probes/scratch/sequential-core-release-5325.patch'),
    ('tactix-observer.patch', old / 'tactix-observer.patch'),
    ('kissat-observer.patch', old / 'kissat-observer.patch')):
    shutil.copy2(path, new / name)
runner = (old / 'run.py').read_text()
runner = runner.replace('("tactix", "kissat")', '("core", "sequential", "kissat")')
runner = runner.replace('prefix = arm.upper()', 'prefix = "KISSAT" if arm == "kissat" else "TACTIX"')
begin, end = runner.index('env.update('), runner.index('\ntry:')
runner = runner[:begin] + '''env.update({prefix + "_EVENT_TRACE": str(C / "traces" / (arm + ".tsv")),
            prefix + "_EVENT_TRACE_SNAPSHOT_ONLY": "1",
            prefix + "_EVENT_TRACE_KEEP_SNAPSHOTS": "1",
            prefix + "_EVENT_TRACE_FORMULA_MACRO": "1",
            prefix + "_EVENT_TRACE_SKIP_HEAP": "1",
            prefix + "_EVENT_TRACE_SKIP_WATCH": "1",
            prefix + "_EVENT_TRACE_SKIP_IMPLY": "1",
            prefix + "_EVENT_TRACE_CONFLICT_FROM": "0",
            prefix + "_EVENT_TRACE_CONFLICT_TO": "0",
            prefix + "_EVENT_TRACE_STOP_AFTER_CONFLICT": "1",
            prefix + "_EVENT_TRACE_LIMIT": "10000"})''' + runner[end:]
runner = runner.replace('0ae917c2e0c1d39cbe411c29b2f5ff08-dislog_a14_x14_n24', 'ac3887576c2ce781202b9fe0fdc31420-pj2008_k80')
(new / 'run.py').write_text(runner)
(new / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old),str(new)).replace('bridge-root-order','rank21-boundary').replace('--array=0-1','--array=0-2'))
prior = json.loads((old / 'manifest.json').read_text())
manifest = dict(source_commit=prior['source_commit'], documentation_checkpoint='4d0c3c97e7f6170bf57e701801178e7375ac32af',
    purpose='UNFUZZED diagnostic: compare core-only, sequential and pristine Kissat simplification snapshots through the first conflict on Rank 21.',
    hypothesis='Sequential rewriting first changes a simplification boundary image; localize that state before another semantic edit.',
    acceptance='No truncation; all traces reach conflict 1 and scratch is deleted. Compare core/sequential state fields and portable formula hashes against Kissat. Stopped runs are neither timing nor correctness acceptance.',
    contract='Full-width 0-2; one bound proof-on solver per task; 12 logical CPUs, 48000 MiB, no throttle or node constraint; 600-second cap.',
    input_sha256_compressed='07b881ae5d1057ccbfdd564ddebf1a8f6e45582542b96aee21974ccdcd446450',
    kissat_source=prior['kissat_source'],
    release_sha256=dict(core='d6701f2e4c158b056f13956c4f02acf12b38c5c81805757c6066bdd56ef4b72a',sequential='e7e82dd1d83db8529e09dd0dba62c2314096df6a08533a9f0faf47cd3a20aee9'),
    hashes={str(p.relative_to(new)): sha(p) for p in [*(new/'pins').iterdir(),*(new.glob('*.patch')),new/'run.py',new/'array.sbatch']})
(new / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
for p in (new / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)
