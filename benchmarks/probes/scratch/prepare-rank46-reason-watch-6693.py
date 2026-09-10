#!/usr/bin/env python3
"""Select the watch list feeding the first differing implication reason."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]; sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old=root/'benchmarks/runs/20260909-6688-rank46-earlier-window'
c=root/'benchmarks/runs/20260909-6693-rank46-reason-watch'; assert not c.exists()
s=json.loads((old/'summary-6690.json').read_text()); assert s['observer_replay_validated']
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest,rel
d=s['raw_event_comparison']['imply']['first_raw_difference']; assert d['index']==6849
assert all(d[a]['conflicts']==108084 and d[a]['decisions']==399066 for a in ('candidate','kissat'))
reasons={a:dict(p.split('=',1) for p in d[a]['payload'].split('|'))['reason'] for a in ('candidate','kissat')}
assert reasons['candidate']!=reasons['kissat']
m=json.loads((old/'manifest.json').read_text())
runner=(old/'run.py').read_text().replace('tactix-6688-','tactix-6693-')
anchor='\ndef interrupted(signum, frame):'; assert runner.count(anchor)==1
runner=runner.replace(anchor,"\nenv.pop(prefix + '_EVENT_TRACE_SKIP_WATCH')\nenv[prefix + '_EVENT_TRACE_WATCH_SCAN_LITERAL'] = '-16365'\nenv[prefix + '_EVENT_TRACE_WATCH_POSE'] = '1'\nenv[prefix + '_EVENT_TRACE_WATCH_VALUES'] = '1'\n"+anchor)
compile(runner,str(c/'run.py'),'exec')
for folder in ('pins','logs','results','traces'): (c/folder).mkdir(parents=True)
for p in (old/'pins').iterdir():
    if p.is_file(): shutil.copy2(p,c/'pins'/p.name)
for a in ('candidate','kissat'):
    p=old/f'results/{a}.json'; r=json.loads(p.read_text()); m['dependencies'][str(p.relative_to(root))]=sha(p)
    for name,digest in r['trace_sha256'].items(): m['dependencies'][str((old/'traces'/name).relative_to(root))]=digest
p=old/'summary-6690.json'; m['dependencies'][str(p.relative_to(root))]=sha(p)
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6688-rank49-prefix','6693-rank46-reason-watch'))
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),cached_controls={a:str(old.relative_to(root)) for a in ('candidate','kissat')},
    selected_scan_literal=-16365,selected_conflict=108084,selected_decision=399066,selected_level=1195,reason_keys=reasons,
    purpose='First6849 implication rows in108080..108170 match; both next imply16358 with different12-literal reasons108084/399066/level1195. Immediately preceding shared binaries have16365 as false antecedent. Inspect complete-16365 watch list before attribution; later actual implication order diverges108122 and clause poses diverge108138.',
    validation='Require exact all own standalone traces,1M decisions/prefix/partial proof and prior100K proof prefix. Global may only add watch rows for scan-16365 in108080..108170; all original rows exact except sequence. Retain all entries, values and poses; do not assume both reasons present or unit until validated.',
    test_consideration='Existing watch observer tests cover selected framing, exact original-row replay, duplicates and invalid additions.207 benchmark tests apply; no solver edit or duplicate fuzz campaign.')
m['hashes']={rel:sha(c/rel) for rel in ('pins/candidate','pins/kissat','pins/certificate_prefix.py','run.py','array.sbatch')}
with (c/'manifest.json').open('x') as f: json.dump(m,f,indent=2); f.write('\n')
for p in (c/'pins').iterdir():
    if p.is_file(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
