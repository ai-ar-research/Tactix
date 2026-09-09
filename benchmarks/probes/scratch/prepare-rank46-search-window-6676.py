#!/usr/bin/env python3
"""Capture the conflict/implication window before Rank46's later branch split."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]; sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old = root/'benchmarks/runs/20260909-6673-rank46-long-prefix'
c = root/'benchmarks/runs/20260909-6676-rank46-search-window'; assert not c.exists()
s = json.loads((old/'summary-6675.json').read_text())
assert s['prior_decisions_and_partial_proofs_exact']
assert s['first_difference_after_validated100k']['index'] == 409828
for rel,digest in s['hashes'].items(): assert sha(root/rel) == digest,rel
m = json.loads((old/'manifest.json').read_text())
runner = (old/'run.py').read_text().replace('tactix-6673-','tactix-6676-')
anchor = "assert not any(key.endswith('_EVENT_TRACE') for key in env)"; assert runner.count(anchor) == 1
runner = runner.replace(anchor,"""env.update({prefix + '_EVENT_TRACE': str(c/'traces'/(arm+'.search.tsv')),
    prefix + '_EVENT_TRACE_CONFLICT_FROM': '108330',
    prefix + '_EVENT_TRACE_CONFLICT_TO': '108341',
    prefix + '_EVENT_TRACE_SEARCH_ONLY': '1', prefix + '_EVENT_TRACE_SKIP_WATCH': '1',
    prefix + '_EVENT_TRACE_SKIP_HEAP': '1', prefix + '_EVENT_TRACE_SKIP_RANDEC_DRAWS': '1',
    prefix + '_EVENT_TRACE_SKIP_ANALYSIS_TICKS': '1', prefix + '_EVENT_TRACE_LIMIT': '250000'})""")
compile(runner,str(c/'run.py'),'exec')
for folder in ('pins','logs','results','traces'): (c/folder).mkdir(parents=True)
for p in (old/'pins').iterdir():
    if p.is_file(): shutil.copy2(p,c/'pins'/p.name)
for arm in ('candidate','kissat'):
    p = old/f'results/{arm}.json'; r = json.loads(p.read_text()); m['dependencies'][str(p.relative_to(root))] = sha(p)
    for name,digest in r['trace_sha256'].items(): m['dependencies'][str((old/'traces'/name).relative_to(root))] = digest
for p in (old/'summary-6675.json', root/'benchmarks/probes/scratch/validate-square-phase-5486.py'):
    m['dependencies'][str(p.relative_to(root))] = sha(p)
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6673','6676'))
m.update(cached_controls={a:str(old.relative_to(root)) for a in ('candidate','kissat')},
    search_window=[108330,108341],trace_row_cap=250000,created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
    purpose='Rank46 decisions largely agree through100K apart from two-row swap; later split108339/403138 picks13046 versus12721,focused. Inspect conflict-analysis/implication state before this split, without treating earlier swap or BVE99580 as the cause.',
    validation='Exact own1M decisions, prefix, partial certificate and old100K certificate prefix required. Candidate macro may add only source-backed trace_begin/retier/checkpoint rows via tested replay validator. Native may add only analysis/end within108330..108341 via5486 fixtures. All other original outputs must agree; reject capped/empty detail.',
    test_consideration='Observer configuration only;196 benchmark tests, including six prefix-byte controls and seven search-window/replay fixtures. Add fixed regression if a source defect is isolated.')
m['hashes'] = {rel:sha(c/rel) for rel in ('pins/candidate','pins/kissat','pins/certificate_prefix.py','run.py','array.sbatch')}
with (c/'manifest.json').open('x') as out: json.dump(m,out,indent=2); out.write('\n')
for p in (c/'pins').iterdir():
    if p.is_file(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
