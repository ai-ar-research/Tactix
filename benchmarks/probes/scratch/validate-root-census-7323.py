#!/usr/bin/env python3
"""Gate source-census interpretation on exact work, certificate and RSS."""
import importlib.util
import json
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from root_census import parse
spec=importlib.util.spec_from_file_location('fingerprint',Path(__file__).with_name('occurrence-fingerprint-7256.py'))
h=importlib.util.module_from_spec(spec);spec.loader.exec_module(h);h.fixtures()
assert sys.argv[1:] in ([],['--seal'])
c=root/'benchmarks/runs/20260911-7323-rank25-root-census';target=c/'summary.json'
if '--seal' in sys.argv:assert not target.exists(),'successful summary frozen'
hashes={}
def bind(p,digest=None):
 actual=sha(p);assert digest is None or actual==digest,p
 hashes[str(p.relative_to(root))]=actual
bind(c/'manifest.json');bind(c/'launch.json')
m=json.loads((c/'manifest.json').read_text())
for rel,d in m['pin_sha256'].items():bind(c/rel,d)
for rel,d in m['evidence_sha256'].items():bind(root/rel,d)
records,states,rows={}, {}, None
for folder in ('provisional','results'):
 for p in sorted((c/folder).glob('*.json')):
  r=json.loads(p.read_text());bind(p);task=r['task']
  assert task==int(p.stem) and task in (0,1) and r['rank']==25 and r['expected']=='SAT'
  arm=('candidate','previous')[task];assert r['arm']==arm
  state=classify(r,folder=='results');assert state['status'] in ('pending_validation','checked_SAT'),(p,state)
  oracle=m['release_records'][arm];q=root/oracle['path'];bind(q,oracle['sha256'])
  assert h.raw_fingerprint(r)==h.raw_fingerprint(json.loads(q.read_text())),('work/certificate mismatch',p)
  if folder=='results':assert r['binary_sha256']==m['arms'][arm]['sha256']
  if task in records:assert r['solve']==records[task]['solve']
  for key in ('memory_syscalls','proc_memory')+ (('root_census',) if arm=='candidate' else ()):
   e=r['solve'][key];assert not e.get('missing') and not e.get('errors')
   q=c/e['path'];assert q.parent==c/'logs' and q.stat().st_size==e['bytes']>0;bind(q,e['sha256'])
   if key=='proc_memory':assert json.loads(q.read_text().splitlines()[-1])['kind']=='observer_stop'
   if key=='root_census':rows=parse(q.read_text())
  records[task],states[task]=r,state
print('Root census',len(records),'/2 exact outcomes;',sum(s['checked'] for s in states.values()),'checked-clean')
peak_gate=len(records)==2 and all(r['solve']['peak_rss_kb']==2484880 for r in records.values())
print('RSS reproduction gate',peak_gate,{r['arm']:r['solve']['peak_rss_kb'] for r in records.values()})
if rows:
 peak=max(r['max_rss_kib'] for r in rows);first=next(r for r in rows if r['max_rss_kib']==peak)
 print('Census rows',len(rows),'first maximum',first)
bind(Path(__file__));bind(Path(__file__).with_name('occurrence-fingerprint-7256.py'))
if '--seal' in sys.argv:
 assert len(records)==2 and rows,'incomplete solver phase'
 with target.open('x') as f:json.dump(dict(hashes=hashes,records=records,states=states,rows=rows,
  exact_ordinary_rss_reproduced=peak_gate,
  decision='Interpret population/capacity and allocator state provisionally only if exact ordinary RSS reproduced. Otherwise classify observer perturbation, not a memory fix.',
  limitation=m['limitation']),f,indent=2)
 print(target)
