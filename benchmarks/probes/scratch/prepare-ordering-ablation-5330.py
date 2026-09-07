#!/usr/bin/env python3
"""Localize which general ordering change first alters Rank-21 search work."""
import csv
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5329-bridge-unit-small'
new = root / 'benchmarks/runs/20260907-5330-ordering-ablation'
new.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'results', 'provisional', 'claims'):
    (new / name).mkdir()
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
variants = [
    ('current', old / 'pins/shipped', None),
    ('core', root / 'build/pin-5318-core-release/tactix', root / 'benchmarks/probes/scratch/core-unit-release-5318.patch'),
    ('physical', root / 'build/pin-5322-core-repose/tactix', root / 'benchmarks/probes/scratch/core-repose-release-5322.patch'),
    ('sequential', root / 'build/pin-5325-sequential/tactix', root / 'benchmarks/probes/scratch/sequential-core-release-5325.patch'),
    ('combined', root / 'build/pin-5326-composed/tactix', root / 'benchmarks/probes/scratch/composed-core-release-5326.patch'),
    ('bridge', old / 'pins/candidate', old / 'pins/candidate.patch')]
arms = {}
for name, binary, patch in variants:
    shutil.copy2(binary, new / 'pins' / name)
    arms[name] = dict(path='pins/' + name, sha256=sha(binary),
                      source_commit='c2a95a6c79c21a76516cf556b6538e76da369c34', arguments=[])
    if patch:
        destination = new / 'pins' / (name + '.patch')
        shutil.copy2(patch, destination)
        arms[name].update(source_patch=str(destination.relative_to(new)), source_patch_sha256=sha(destination))
for name in ('checkmodel', 'drat-trim', 'modelcheck.py', 'runtime_task.py'):
    shutil.copy2(old / 'pins' / name, new / 'pins' / name)
with (old / 'matrix.tsv').open(newline='') as source:
    reader = csv.DictReader(source, delimiter='\t')
    fields = reader.fieldnames
    row = next(row for row in reader if row['rank'] == '21')
with (new / 'matrix.tsv').open('w', newline='') as target:
    writer = csv.DictWriter(target, fieldnames=fields, delimiter='\t')
    writer.writeheader()
    for i, (name, _, _) in enumerate(variants):
        writer.writerow(dict(row, task=i, arm=name))
(new / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(new)))
manifest = json.loads((old / 'manifest.json').read_text())
manifest.pop('slurm_job_id', None)
manifest.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(), arms=arms,
    hypothesis='Core-only preserves Rank-21 counters; identify whether physical publication, sequential rewriting, their combination, or bridge reposing first changes them.',
    comparison='Six immutable general-purpose variants, same single Rank-21 input and proof-on allocation. This selects the next causal trace, not a formula-specific default.',
    decision='All six SAT models verified and scratch clean; compare work counters to identify a mechanism. No decision-grade timing claim or promotion from this single wave.',
    parent_campaign=str(old.relative_to(root)),
    candidate_validation='Each semantic variant has focused unit/proof validation; core-only and final bridge variant have all 77 CTests. All variants except current remain UNFUZZED.')
manifest['input_sha256'] = {row['instance']: manifest['input_sha256'][row['instance']]}
manifest['pin_sha256'] = {str(p.relative_to(new)): sha(p) for p in (new / 'pins').iterdir()}
for name in ('matrix.tsv', 'array.sbatch'):
    manifest['pin_sha256'][name] = sha(new / name)
(new / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
for p in (new / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(new)
