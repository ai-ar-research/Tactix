#!/usr/bin/env python3
"""Observe all scans involving literal19005 across the selected clause gap."""
import datetime as dt
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3]; sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
old=root/'benchmarks/runs/20260909-6696-rank46-reason-history'
c=root/'benchmarks/runs/20260909-6702-rank46-allscan-reason'; assert not c.exists()
s=json.loads((old/'localization-6699.json').read_text()); assert s['complete_portable_common_rows']==495210
for rel,digest in s['hashes'].items(): assert sha(root/rel)==digest,rel
validated=json.loads((old/'summary-6698.json').read_text()); assert validated['observer_replay_validated']
for rel,digest in validated['hashes'].items(): assert sha(root/rel)==digest,rel
m=json.loads((old/'manifest.json').read_text())
runner=(old/'run.py').read_text().replace('tactix-6696-','tactix-6702-')
runner=runner.replace("'_EVENT_TRACE_CONFLICT_FROM': '0'","'_EVENT_TRACE_CONFLICT_FROM': '106600'")
runner=runner.replace("'_EVENT_TRACE_CONFLICT_TO': '108170'","'_EVENT_TRACE_CONFLICT_TO': '107350'")
runner=runner.replace("env[prefix + '_EVENT_TRACE_WATCH_SCAN_LITERAL'] = '-16365'","env[prefix + '_EVENT_TRACE_WATCH_LITERAL'] = '19005'")
runner=runner.replace("'_EVENT_TRACE_LIMIT': '2000000'","'_EVENT_TRACE_LIMIT': '500000'")
compile(runner,str(c/'run.py'),'exec')
for folder in ('pins','logs','results','traces'): (c/folder).mkdir(parents=True)
for p in (old/'pins').iterdir():
    if p.is_file(): shutil.copy2(p,c/'pins'/p.name)
for a in ('candidate','kissat'):
    p=old/f'results/{a}.json'; r=json.loads(p.read_text()); m['dependencies'][str(p.relative_to(root))]=sha(p)
    for name,digest in r['trace_sha256'].items(): m['dependencies'][str((old/'traces'/name).relative_to(root))]=digest
for name in ('summary-6698.json','localization-6699.json'):
    p=old/name; m['dependencies'][str(p.relative_to(root))]=sha(p)
(c/'run.py').write_text(runner)
(c/'array.sbatch').write_text((old/'array.sbatch').read_text().replace(str(old),str(c)).replace('6696-rank46-reason-history','6702-rank46-allscan-reason'))
m.pop('selected_scan_literal',None)
m.update(created_at=dt.datetime.now(dt.timezone.utc).isoformat(),search_window=[106600,107350],trace_row_cap=500000,
    cached_controls={a:str(old.relative_to(root)) for a in ('candidate','kissat')},selected_watch_literal=19005,
    purpose='ReasonB last selected-16365 state matches106604, first differs107349 after495210 complete matching portable rows. Observe all scans whose clause contains external variable19005 to capture intervening movements. Extra clauses are retained; interpret the exact reasonB key, not variable membership alone.',
    validation='Require complete own1M decisions/prefix/partial proof/prior100K proof prefix and every original standalone record under existing strict normalizers. Global strict-watch rows must lie106600..107350 and include variable19005. Project to scan-16365 and require exact old history rows in the same window whose clauses include19005, preserving metadata, order and duplicates except seq. Other watched literals are new observations, not control mismatches.',
    test_consideration='No solver edit. Existing watch payload and history tests apply; new multi-scan projection logic needs focused tests before interpretation.')
m['hashes']={rel:sha(c/rel) for rel in ('pins/candidate','pins/kissat','pins/certificate_prefix.py','run.py','array.sbatch')}
with (c/'manifest.json').open('x') as f: json.dump(m,f,indent=2); f.write('\n')
for p in (c/'pins').iterdir():
    if p.is_file(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
