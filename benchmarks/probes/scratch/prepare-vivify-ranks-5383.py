#!/usr/bin/env python3
"""Measure the isolated cumulative stack on the full fixed Rank 1–40 view."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil
root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5314-pose-r2-ranks1-40'
candidate = root / 'benchmarks/runs/20260907-5379-vivify-retry-runtime'
c = root / 'benchmarks/runs/20260907-5383-vivify-retry-ranks1-40'
c.mkdir(exist_ok=False)
for d in ('pins', 'logs', 'results', 'provisional', 'claims'): (c / d).mkdir()
for target, source in [('tactix', candidate / 'pins/candidate'), ('candidate-source.patch', candidate / 'pins/candidate.patch'),
                       ('kissat', old / 'pins/kissat'), ('drat-trim', old / 'pins/drat-trim'),
                       ('checkmodel', old / 'pins/checkmodel'), ('modelcheck.py', old / 'pins/modelcheck.py'),
                       ('runtime_task.py', candidate / 'pins/runtime_task.py')]:
    shutil.copy2(source, c / 'pins' / target)
shutil.copy2(old / 'matrix.tsv', c / 'matrix.tsv')
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
m.pop('slurm_job_id', None); m.pop('fuzz_authority', None)
m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
         parent_campaign=str(old.relative_to(root)), candidate_campaign=str(candidate.relative_to(root)),
         purpose='Full fixed Rank 1–40 checked comparison of isolated cumulative Vivify retry stack against pristine Kissat.',
         hypothesis='Supported cumulative repairs improve the full requested runtime view; quantify remaining per-instance work/cost/memory gaps.',
         validation='80 final independently checked/clean records; preserve solve counts, SAT/UNSAT splits, timeouts, PAR-2, aggregate ratio and per-instance work/memory. Provisional records never establish acceptance.',
         promotion='UNFUZZED investigation material. This full-view measurement does not promote source; B28+C5 and Claude-owned consolidation fuzz remain required.',
         collect_runtime_context=True)
m['arms']['tactix'].update(sha256=sha(c / 'pins/tactix'), source_commit='c2a95a6c79c21a76516cf556b6538e76da369c34',
                           source_patch_sha256=sha(c / 'pins/candidate-source.patch'), source_build='build/pin-5379-vivify-retry-release')
m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
