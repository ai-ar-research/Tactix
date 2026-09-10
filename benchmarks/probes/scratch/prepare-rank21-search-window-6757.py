#!/usr/bin/env python3
"""Observe the focused conflict window preceding Rank21's first choice split."""
import datetime as dt
import json
from pathlib import Path
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
subprocess.run([sys.executable, str(root/'tools/slurm/session_deadline.py'),
    '--deadline', '2026-09-10T06:00:00-04:00'], check=True)
old = root/'benchmarks/runs/20260909-6751-rank21-long-prefix'
c = root/'benchmarks/runs/20260909-6757-rank21-search-window'
assert not c.exists()
s = json.loads((old/'summary-6753.json').read_text())
assert s['bounded_stops_clean']
assert s['decisions']['matching_rows'] == 25946740
assert s['decisions']['left'] == [25102, 25451310, 14, -402373, 0]
assert s['decisions']['right'] == [25102, 25451310, 14, -402380, 0]
for rel, digest in s['hashes'].items():
    assert sha(root/rel) == digest, rel
m = json.loads((old/'manifest.json').read_text())
runner = (old/'run.py').read_text().replace('tactix-6751-', 'tactix-6757-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"
assert runner.count(anchor) == 1
runner = runner.replace(anchor, """env.update({prefix + '_EVENT_TRACE': str(c/'traces'/(arm+'.search.tsv')),
    prefix + '_EVENT_TRACE_CONFLICT_FROM': '25098',
    prefix + '_EVENT_TRACE_CONFLICT_TO': '25103',
    prefix + '_EVENT_TRACE_SEARCH_ONLY': '1', prefix + '_EVENT_TRACE_SKIP_WATCH': '1',
    prefix + '_EVENT_TRACE_SKIP_HEAP': '1', prefix + '_EVENT_TRACE_SKIP_RANDEC_DRAWS': '1',
    prefix + '_EVENT_TRACE_SKIP_ANALYSIS_TICKS': '1', prefix + '_EVENT_TRACE_LIMIT': '250000'})""")
compile(runner, str(c/'run.py'), 'exec')
for folder in ('pins', 'logs', 'results', 'traces'):
    (c/folder).mkdir(parents=True)
for p in (old/'pins').iterdir():
    if p.is_file():
        shutil.copy2(p, c/'pins'/p.name)
for arm in ('candidate', 'kissat'):
    p = old/f'results/{arm}.json'
    r = json.loads(p.read_text())
    m['dependencies'][str(p.relative_to(root))] = sha(p)
    for name, digest in r['trace_sha256'].items():
        m['dependencies'][str((old/'traces'/name).relative_to(root))] = digest
for p in (old/'summary-6753.json', root/'benchmarks/probes/scratch/validate-square-phase-5486.py'):
    m['dependencies'][str(p.relative_to(root))] = sha(p)
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old), str(c)).replace('6751', '6757'))
m.update(cached_controls={a: str(old.relative_to(root)) for a in ('candidate', 'kissat')},
    search_window=[25098, 25103], trace_row_cap=250000,
    created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    purpose='Localize Rank21 focused-mode first differing choice at25102/25451310 after25946740 matching decisions. Inspect implication/conflict/analysis state before changing queue policy or formula handling.',
    hypothesis='A differing implication or learned reason before the focused choice can explain the queue/assignment residual; the earlier decision-neutral Sweep count discrepancy is not presumed causal.',
    validation='Exact own50K decisions, prefix and partial certificate; retain exact native old10K certificate prefix. Candidate macro may add only source-backed trace_begin/retier/checkpoint via tested subsequence validator. Native boundaries may add only analysis/end inside25098..25103. All other old outputs exact; reject empty/capped detail.',
    test_consideration='Immutable solver and observer configuration only. Reuse search-window, macro-replay, decision-prefix and certificate-prefix regression tests; add a focused fixed regression if diagnosis selects a source change.',
    limitation='Diagnostic only, not new baseline timing, complete SAT-model validation, or a claim that any count difference is harmful.')
m['hashes'] = {rel: sha(c/rel) for rel in ('pins/candidate', 'pins/kissat', 'pins/certificate_prefix.py', 'run.py', 'array.sbatch')}
with (c/'manifest.json').open('x') as out:
    json.dump(m, out, indent=2)
    out.write('\n')
for p in (c/'pins').iterdir():
    if p.is_file():
        p.chmod(p.stat().st_mode & ~0o222)
print(c)
