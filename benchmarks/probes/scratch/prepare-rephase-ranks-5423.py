#!/usr/bin/env python3
"""Preserve the fixed full Rank 1–40 protocol for root-closure repair."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260907-5395-eliminate-entry-ranks1-40'
authority = root / 'benchmarks/runs/20260907-5417-rephase-root'
c = root / 'benchmarks/runs/20260907-5423-rephase-root-ranks1-40'
binary = root / 'build/pin-5419-rephase-root-release/tactix'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(binary) == 'f86419c4763ea5a8372661648d552c6b47c7b3375441ebc879fe9088f171c10c'
c.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'results', 'provisional', 'claims'):
    (c / name).mkdir()
for name in ('kissat', 'drat-trim', 'checkmodel', 'modelcheck.py', 'runtime_task.py'):
    shutil.copy2(old / 'pins' / name, c / 'pins' / name)
shutil.copy2(binary, c / 'pins/tactix')
shutil.copy2(authority / 'cumulative.patch', c / 'pins/candidate-source.patch')
shutil.copy2(old / 'matrix.tsv', c / 'matrix.tsv')
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
m = json.loads((old / 'manifest.json').read_text())
m.pop('slurm_job_id', None)
m.pop('candidate_campaign', None)
m.update(created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
         parent_campaign=str(old.relative_to(root)),
         candidate_authority=str(authority.relative_to(root)),
         causal_confirmation='benchmarks/runs/20260907-5418-rank35-rephase-confirmation/summary.json',
         purpose='Full unchanged Rank 1–40 comparison of cumulative Rephase root-closure candidate against pristine Kissat.',
         hypothesis='Restoring reference root closure before Rephase improves the full runtime view; quantify work, time and memory rather than infer global improvement from the closed Rank-35 witness.',
         validation='80 independently checked and cleaned final records; preserve solve counts, SAT/UNSAT splits, timeouts, PAR-2 and per-instance work/memory. Expected solver results with pending certificates guide dependent work provisionally, never establish final acceptance.',
         promotion='UNFUZZED investigation material, not source promotion; B28+C5 and one Claude-owned consolidation fuzz campaign remain required.',
         collect_runtime_context=True)
m['arms']['tactix'].update(sha256=sha(binary),
    source_patch_sha256=sha(c / 'pins/candidate-source.patch'),
    source_build='build/pin-5419-rephase-root-release')
m['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
