from pathlib import Path
import json,hashlib
run=Path(__file__).resolve().parent
rows=[]
for i in range(3):
 case=run/f'case-{i}';result=json.loads((case/'result.json').read_text());assert result['policy_paths_verified']
 feedback={}
 for policy in ['default','random']:
  trace=case/f'policy-{policy}.feedback.jsonl'
  actions=[d for d in map(json.loads,trace.read_text().splitlines()) if d.get('type')=='action' and d.get('identity')=='Lucky']
  assert len(actions)==(1 if policy=='default' else 2)
  for action in actions:assert action['instance']==22
  last=actions[-1];stats=last['after']['native'];prior=last['before']['native']
  assert stats['ramsey_startup_unsat']==1 and stats['ramsey_startup_additions']==result['additions']
  assert stats['decisions']==0 and stats['conflicts']==0
  for field in ['recognition_work','certificate_work','nodes','additions','helpers']:
   assert stats['ramsey_startup_'+field]>prior['ramsey_startup_'+field]
  if policy=='default':
   actual=last['native_request'];assert actual['currency']=='ramsey-work'
   assert actual['amount']==20000000
   assert actual['work']==stats['ramsey_startup_recognition_work']+stats['ramsey_startup_certificate_work']
  else:
   assert actions[0]['after']['native']['ramsey_startup_attempts']==1
   assert actions[0]['after']['native']['ramsey_startup_additions']==0
  feedback[policy]={'sha256':hashlib.sha256(trace.read_bytes()).hexdigest(),'actions':len(actions),'terminal_stop':last['stop'],'ramsey_counters':{k:v for k,v in stats.items() if k.startswith('ramsey_startup_')}}
 rows.append({'case':i,'result':result,'feedback':feedback})
(run/'collection.json').write_text(json.dumps({'cases':rows,'scope':'Targeted actual policy/replay paths; not fuzz or full acceptance'},indent=2)+'\n')
print('Verified all three policy/replay cases and their raw feedback')
