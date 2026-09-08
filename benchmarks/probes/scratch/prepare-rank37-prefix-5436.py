#!/usr/bin/env python3
"""Localize the largest current full-view loss using the latest isolated pin."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5433-rank35-inprocess-prefix'
c = root / 'benchmarks/runs/20260907-5436-rank37-inprocess-prefix'
c.mkdir(exist_ok=False)
for d in ('pins', 'logs', 'traces', 'results'): (c / d).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
runner = (old / 'run.py').read_text().replace("'tactix-5433-'", "'tactix-5436-'")
runner = runner.replace("env['SAT_MACRO_BOUNDARY_CONFLICT_FROM'] = '13833'",
                        "env['SAT_MACRO_BOUNDARY_CONFLICT_FROM'] = '0'")
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
def sha(p):
    h = hashlib.sha256()
    with p.open('rb') as f:
        for block in iter(lambda: f.read(1048576), b''): h.update(block)
    return h.hexdigest()
m = json.loads((old / 'manifest.json').read_text())
for key in ('slurm_job_id', 'matched_prefix_authority'): m.pop(key, None)
m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    parent_campaign=str(old.relative_to(root)), stop_conflict=100000,
    stream_from=0, common_decisions=1,
    input='benchmarks/cache/tierD24-26/35748015c5f16c9ee7e4d1894af497a0-oski15a01b39s_opt.cnf.xz',
    hypothesis='Rank 37 is the largest Rephase full-view loss (621.930/344.429 seconds, conflict ratio 1.8725). Localize the first portable decision split on the latest general root-entry candidate before selecting a component repair.',
    validation='Two clean controlled stops, immutable hashes and complete decision streams; compare all portable rows and normalized rule-boundary active/clause fields. Partial certificates are hashed then deleted. No timing or complete correctness claim.',
    continuation='Investigation proceeds with pending correctness checks as provisional passes. Actual rejected certificates or solver failures interrupt dependent work.')
m['input_sha256_compressed'] = sha(root / m['input'])
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
