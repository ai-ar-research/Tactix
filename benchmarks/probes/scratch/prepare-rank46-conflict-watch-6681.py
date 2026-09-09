#!/usr/bin/env python3
"""Inspect the selected physical watch list before differing conflict clauses."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root = Path(__file__).resolve().parents[3]; sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old = root/'benchmarks/runs/20260909-6676-rank46-search-window'
c = root/'benchmarks/runs/20260909-6681-rank46-conflict-watch'; assert not c.exists()
s = json.loads((old/'summary-6678.json').read_text()); assert s['observer_replay_validated']
for rel,digest in s['hashes'].items(): assert sha(root/rel) == digest,rel
conflicts = {a:[r for r in s['conflicts'][a] if r['conflicts'] == 108338] for a in ('candidate','kissat')}
assert all(len(rows) == 1 for rows in conflicts.values())
def semantic(r): return {k:v for k,v in (p.split('=',1) for p in r['payload'].split('|')) if k in ('order','set','levels')}
assert semantic(conflicts['candidate'][0]) != semantic(conflicts['kissat'][0])
m = json.loads((old/'manifest.json').read_text())
runner = (old/'run.py').read_text().replace('tactix-6676-','tactix-6681-')
anchor = '\ndef interrupted(signum, frame):'; assert runner.count(anchor) == 1
runner = runner.replace(anchor,"\nenv.pop(prefix + '_EVENT_TRACE_SKIP_WATCH')\nenv[prefix + '_EVENT_TRACE_WATCH_SCAN_LITERAL'] = '-16719'\nenv[prefix + '_EVENT_TRACE_WATCH_POSE'] = '1'\nenv[prefix + '_EVENT_TRACE_WATCH_VALUES'] = '1'\n"+anchor)
compile(runner,str(c/'run.py'),'exec')
for folder in ('pins','logs','results','traces'): (c/folder).mkdir(parents=True)
for p in (old/'pins').iterdir():
    if p.is_file(): shutil.copy2(p,c/'pins'/p.name)
for arm in ('candidate','kissat'):
    p = old/f'results/{arm}.json'; r = json.loads(p.read_text()); m['dependencies'][str(p.relative_to(root))] = sha(p)
    for name,digest in r['trace_sha256'].items(): m['dependencies'][str((old/'traces'/name).relative_to(root))] = digest
p = old/'summary-6678.json'; m['dependencies'][str(p.relative_to(root))] = sha(p)
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6676','6681'))
m.update(cached_controls={a:str(old.relative_to(root)) for a in ('candidate','kissat')},
    created_at=dt.datetime.now(dt.timezone.utc).isoformat(),selected_scan_literal=-16719,
    selected_conflict=108338,conflict_records=conflicts,
    purpose='Portable conflict order/set/levels agree108330..108337, differ108338 despite same preceding implication sequence. Both conflict heads are16859,16719 and last shared implications arise from16719 watches. Snapshot full watch list for propagated-16719, with clause pose/values; establish membership/order before proposing repair.',
    validation='Require exact own1M decisions/prefix/partial proof/prior100K proof prefix and all standalone traces. Global search may only add watch rows for scan-16719 within declared detail window, with corresponding sequence offsets; every original row must remain identical. Compare portable clause identities/positions/values, not raw IDs across solvers.',
    test_consideration='Existing watch snapshot hooks only, no solver edits.196 benchmark tests apply; strict observer subsequence checks and selected payload framing precede interpretation. No queue, propagation or cleanup change authorized merely by first differing clause.')
m['hashes'] = {rel:sha(c/rel) for rel in ('pins/candidate','pins/kissat','pins/certificate_prefix.py','run.py','array.sbatch')}
with (c/'manifest.json').open('x') as out: json.dump(m,out,indent=2); out.write('\n')
for p in (c/'pins').iterdir():
    if p.is_file(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
