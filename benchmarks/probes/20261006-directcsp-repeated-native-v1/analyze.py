#!/usr/bin/env python3
"""Extract raw controller action deltas and whole-run counters without mirroring solver logic."""
import gzip,json,re,sys
from pathlib import Path
run=Path(__file__).resolve().parent
for out in sorted(run.glob('result-'+sys.argv[1]+'-*')):
 producer={};actions=[];errors=[];ended=False
 try:
  with gzip.open(out/'feedback.jsonl.gz','rt') as stream:
   for line in stream:
    if '"identity":"ResumeSearch"' in line: break
    if '"type":"session_end"' in line: ended=True
    if '"type":"action"' not in line: continue
    record=json.loads(line)
    for effort in (record.get('request') or {}).get('producer_effort',[]):
     total=producer.setdefault(effort['family'],{'calls':0,'total_work':0,'candidates':0,'offers':0})
     total['calls']+=1
     for key in ('total_work','candidates','offers'):total[key]+=effort[key]
    if record.get('identity') in ('DirectCspSupport','Propagate','SemanticVariableAdvice'):
     actions.append(record)
 except (OSError,EOFError,json.JSONDecodeError) as error:errors.append(str(error))
 stdout=(out/'stdout.log').read_text()
 counters={k:int(v) for k,v in re.findall(r'^c (decisions|conflicts|propagations)\s+(\d+)',stdout,re.M)}
 families={}
 for family,requests,applications,effective in re.findall(r'^c controller-rule (DirectCspSupport|Propagate|SemanticVariableAdvice) requests=(\d+) applications=(\d+) effective=(\d+)',stdout,re.M):
  families[family]={'requests':int(requests),'applications':int(applications),'effective':int(effective)}
 provenance=[json.loads(line) for line in (out/'provenance.jsonl').read_text().splitlines()]
 publication=[r for r in provenance if r['kind']=='direct-csp-support-complete']
 keys=[r['request']['invocation'] for r in actions if r['identity']=='DirectCspSupport']
 result={'result':json.loads((out/'result.json').read_text()),'feedback_complete':False,'feedback_scope':'startup prefix through first ResumeSearch only; full compressed feedback retained','feedback_errors':errors,'producer':producer,'controller_rules':families,'action_identities':[a['identity'] for a in actions],'actions':actions,'search_counters':counters,'proof_bytes':(out/'proof.dsr').stat().st_size,'publication_records':publication,'unique_support_action_payloads':len(set(keys)),'model_check':json.loads((out/'result.json').read_text())['model_check']}
 (out/'causal-summary.json').write_text(json.dumps(result,indent=2)+'\n')
 print(out.name,json.dumps({k:v for k,v in result.items() if k not in ('actions','publication_records')}))
