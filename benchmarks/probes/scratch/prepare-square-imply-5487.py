#!/usr/bin/env python3
"""Trace selected implication reasons behind Square's saved-phase history."""
import datetime
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[3]
old = root / 'benchmarks/runs/20260908-5482-square-phase'
c = root / 'benchmarks/runs/20260908-5487-square-implications'
validated = json.loads((old / 'observer-validation.json').read_text())
assert validated['parent_replay'] and validated['native_extra_analysis_end'] == 476
runner = (old / 'run.py').read_text().replace("'tactix-5482-'", "'tactix-5487-'")
assert runner.count("'_EVENT_TRACE_PHASE_ONLY'") == 1
runner = runner.replace("'_EVENT_TRACE_PHASE_ONLY'", "'_EVENT_TRACE_IMPLY_ONLY'")
runner = runner.replace("'.phase.tsv'", "'.imply.tsv'")
runner = runner.replace("'SAT_EVENT_TRACE_PHASE_VAR': '43773'", "'SAT_EVENT_TRACE_IMPLY_LITERALS': '43773'")
compile(runner, str(c / 'run.py'), 'exec')
c.mkdir(exist_ok=False)
for name in ('pins', 'logs', 'traces', 'results'):
    (c / name).mkdir()
for arm in ('candidate', 'kissat'):
    shutil.copy2(old / 'pins' / arm, c / 'pins' / arm)
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((old / 'array.sbatch').read_text().replace(str(old), str(c)))
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
m = json.loads((old / 'manifest.json').read_text())
m.update(parent_campaign=str(old.relative_to(root)),
         created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
         hypothesis='Both solvers honor saved phases at decision5849. Tactix saves negative43773 at conflict492, while earlier assignment histories already differ. Trace selected implication reasons from startup through502 to locate assignment/representation ownership; do not change phase priority or assume equal auxiliary-variable definitions.',
         validation='Two clean stops and complete per-arm decision/prefix replay against5471; unchanged candidate boundaries; native original boundary equality allowing only source-backed extra analysis/end rows with validated sequence/window checks. Inspect selected implication reasons, not timing.',
         phase_authority=dict(path=str((old / 'observer-validation.json').relative_to(root)),
                              sha256=sha(old / 'observer-validation.json')),
         prior_summary_sha256=sha(old / 'summary.json'),
         test_consideration='Configuration-only reuse of tested IMPLY_ONLY observers. Syntax check, full parent replay and the eight normalization fixtures guard the diagnostic; no solver code change.')
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in
               [*(c / 'pins').iterdir(), c / 'run.py', c / 'array.sbatch']}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir():
    p.chmod(p.stat().st_mode & ~0o222)
print(c)
