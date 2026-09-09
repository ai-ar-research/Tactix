#!/usr/bin/env python3
"""Extend read-only learning/reduction history without changing solver pins."""
import hashlib
import json
from pathlib import Path
import shutil
from datetime import datetime, timezone

root = Path(__file__).resolve().parents[3]
parent = root / 'benchmarks/runs/20260908-6038-learned-retention'
c = root / 'benchmarks/runs/20260908-6042-retention-history'
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
assert not c.exists()
m = json.loads((parent / 'manifest.json').read_text())
for rel, digest in m['hashes'].items(): assert sha(parent / rel) == digest, rel
assert json.loads((parent / 'summary.json').read_text())['full_parent_replay']
population = json.loads((parent / 'population-analysis.json').read_text())
assert all(r['ranked_common_projection_equal'] for r in population['reductions'])
runner = (parent / 'run.py').read_text().replace('tactix-6038-', 'tactix-6042-')
for old, new in [("env['SAT_LEARNED_STAGE_FROM'] = '240062'", "env['SAT_LEARNED_STAGE_FROM'] = '0'"),
                 ("env['SAT_LEARNED_STAGE_TO'] = '247206'", "env['SAT_LEARNED_STAGE_TO'] = '270978'"),
                 ("env[prefix + '_REDUCE_TRACE_CONFLICT_FROM'] = '245473'", "env[prefix + '_REDUCE_TRACE_CONFLICT_FROM'] = '0'")]:
    assert runner.count(old) == 1
    runner = runner.replace(old, new)
compile(runner, str(c / 'run.py'), 'exec')
c.mkdir()
for name in ('pins', 'logs', 'traces', 'results'): (c / name).mkdir()
for arm in ('candidate', 'kissat'): shutil.copy2(parent / 'pins' / arm, c / 'pins' / arm)
(c / 'run.py').write_text(runner)
(c / 'array.sbatch').write_text((parent / 'array.sbatch').read_text().replace(str(parent), str(c)).replace('6038-learned-retention', '6042-retention-history'))
m.update(parent_campaign=str(parent.relative_to(root)),
    hypothesis='Trace the first differing Reduce population and the ancestry of the extra candidate: reductions52/53/54 have exactly one extra Tactix candidate, shared ranking projection identical. At54 that extra survivor changes victim count3020 vs3019 and deletes the causal Vivify clause.',
    validation='Require exact replay of unchanged6038 traces and partial certificate. Extended Reduce must reproduce old window exactly; extended learned-stage must reproduce old window after sequence normalization. Compare complete multisets, metadata and ranks at every reduction.',
    native_rerun_reason='Missing earlier reduction and learning history; fixed timing denominator unchanged.',
    test_consideration='No solver changes; reuse100/100-tested6030 pins. Add streaming window validation controls and run existing Reduce comparator tests.',
    causal_evidence=dict(path=str((parent / 'population-analysis.json').relative_to(root)), sha256=sha(parent / 'population-analysis.json')),
    created_at=datetime.now(timezone.utc).isoformat())
m['hashes'] = {str(p.relative_to(c)): sha(p) for p in [c / 'run.py', c / 'array.sbatch', *(c / 'pins').iterdir()]}
(c / 'manifest.json').write_text(json.dumps(m, indent=2) + '\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
