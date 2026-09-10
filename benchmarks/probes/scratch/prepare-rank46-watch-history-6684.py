#!/usr/bin/env python3
"""Observe the selected scan from startup without changing either solver pin."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]; sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
prior=json.loads((root/'benchmarks/reports/2026-09-09-congruence-runtime-rank46-audit-6679.json').read_text())
for group in ('owned_hashes','protected_campaign_hashes','andrew_report'):
    for rel,digest in prior[group].items(): assert sha(root/rel)==digest,rel
old=root/'benchmarks/runs/20260909-6681-rank46-conflict-watch'
c=root/'benchmarks/runs/20260909-6684-rank46-watch-history'; assert not c.exists()
s=json.loads((old/'summary-6683.json').read_text()); assert s['observer_replay_validated']
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest,rel
m=json.loads((old/'manifest.json').read_text())
runner=(old/'run.py').read_text().replace('tactix-6681-','tactix-6684-')
runner=runner.replace("'_EVENT_TRACE_CONFLICT_FROM': '108330'", "'_EVENT_TRACE_CONFLICT_FROM': '0'")
runner=runner.replace("'_EVENT_TRACE_LIMIT': '250000'", "'_EVENT_TRACE_LIMIT': '2000000'")
anchor="env[prefix + '_EVENT_TRACE_WATCH_VALUES'] = '1'"; assert runner.count(anchor)==1
runner=runner.replace(anchor,anchor+"\nenv[prefix + '_EVENT_TRACE_WATCH_ONLY_STRICT'] = '1'")
compile(runner,str(c/'run.py'),'exec')
for folder in ('pins','logs','results','traces'): (c/folder).mkdir(parents=True)
for p in (old/'pins').iterdir():
    if p.is_file(): shutil.copy2(p,c/'pins'/p.name)
for arm in ('candidate','kissat'):
    p=old/f'results/{arm}.json'; r=json.loads(p.read_text()); m['dependencies'][str(p.relative_to(root))]=sha(p)
    for name,digest in r['trace_sha256'].items(): m['dependencies'][str((old/'traces'/name).relative_to(root))]=digest
p=old/'summary-6683.json'; m['dependencies'][str(p.relative_to(root))]=sha(p)
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6681-rank49-prefix','6684-rank46-watch-history'))
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),search_window=[0,108341],trace_row_cap=2000000,
    cached_controls={a:str(old.relative_to(root)) for a in ('candidate','kissat')},
    standalone_controls={a:'benchmarks/runs/20260909-6673-rank46-long-prefix' for a in ('candidate','kissat')},
    purpose='Locate first observed relative-order or pose difference for the two all-false Rank46 conflict clauses before108338. Strict watch-only snapshots of the complete-16719 scan list from startup; do not refresh timing references or change solver code.',
    validation='Require exact own1M decision/prefix/partial proof/prior100K proof prefix. Standalone macro/boundaries preserve the old6673 control allowing only source-backed macro extras and native analysis/end in0..108341. New global stream contains only selected watch rows and projects exactly to every old6681 watch row in108330..108341 except sequence offsets. Reject capped output; retain duplicate entries and complete lists.',
    test_consideration='No solver edits; use existing fixed pins and strict watch hooks.202 maintained benchmark tests cover framing/replay; add tests for any new history grouping or projection logic before interpretation.')
m['hashes']={rel:sha(c/rel) for rel in ('pins/candidate','pins/kissat','pins/certificate_prefix.py','run.py','array.sbatch')}
with (c/'manifest.json').open('x') as f: json.dump(m,f,indent=2); f.write('\n')
for p in (c/'pins').iterdir():
    if p.is_file(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
