#!/usr/bin/env python3
from pathlib import Path
from collections import defaultdict
import json,re,sys
r=Path(sys.argv[1]).resolve();config=json.loads((r/'config.json').read_text());entries=json.loads((r/'corpus.json').read_text())['entries']
rows=[json.loads(p.read_text()) for p in sorted((r/'results').glob('*.json'))];expected=len(entries)*config['repetitions']
families=defaultdict(lambda:defaultdict(lambda:dict(pairs=0,solved=0,par2_sum=0,groups=0,summary_decisions=0)))
errors=[];retention=[];meaningful=[]
for row in rows:
 if not row['complete']:continue
 for arm,value in row['arms'].items():
  f=families[row['entry']['family']][arm];f['pairs']+=1;f['par2_sum']+=value['par2'];f['solved']+=value['status'] in ['SAT','UNSAT-unchecked']
  if value['status'] in ['unexpected-result','invalid-model']:errors.append([row['entry']['gbd_id'],row['repetition'],arm,value['status']])
  for line in value['nonlinear']:
   if 'stage=finish ' in line:
    for key in ['groups','summary-decisions']:
     m=re.search(r'\b'+key+r'=(\d+)',line)
     if m:f['summary_decisions' if key=='summary-decisions' else key]+=int(m[1])
  if row['arms']['baseline']['status'] in ['SAT','UNSAT-unchecked'] and value['status']!=row['arms']['baseline']['status']:retention.append([row['entry']['gbd_id'],row['repetition'],arm])
 if any(row['arms'][a]['nonlinear'] for a in ['definitions','priority']):meaningful.append(row['entry']['gbd_id'])
for family,arms in families.items():
 for arm,values in arms.items():values['par2_mean']=values['par2_sum']/values['pairs']
 base=arms['baseline']['par2_mean']
 for arm,values in arms.items():values['ratio_to_baseline']=values['par2_mean']/base
complete=len(rows)==expected and all(row['complete'] for row in rows)
target_retention=[item for item in retention if item[2]=='priority']
passes=complete and not errors and not target_retention and len(families)==2 and all(arms['priority']['ratio_to_baseline']<=.75 for arms in families.values())
result=dict(complete=complete,expected_tasks=expected,present_tasks=len(rows),complete_tasks=sum(row['complete'] for row in rows),families=dict(families),errors=errors,lost_baseline_solves=retention,provisional_runtime_target=passes,goal_complete=False,qualification='UNSAT independently unchecked; semantic/proof and causal-control interpretation required before claiming the full goal',instances_reaching_research_hook=len(set(meaningful)))
print(json.dumps(result,indent=2));(r/'summary.json').write_text(json.dumps(result,indent=2)+'\n')
