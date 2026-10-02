import hashlib,importlib.util,json,os,resource,subprocess,sys,time
from pathlib import Path
r=Path(__file__).resolve().parent
root=r.parents[2]
build=root/'build/staged-debug-repair-v2-7e2c5eb1-runtime'
pin=root/'build/pin-shared-policy-feedback-debug-0531c2f1'
work=r/'work';work.mkdir()
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
env={k:v for k,v in os.environ.items() if not k.startswith(('TACTIX_','SAT_'))}
results=[]
def run(name,args,expected=0,timeout=600):
 start=time.monotonic();p=subprocess.run(args,cwd=root,env=env,capture_output=True,text=True,timeout=timeout)
 (r/(name+'.stdout')).write_text(p.stdout);(r/(name+'.stderr')).write_text(p.stderr)
 row={'name':name,'command':list(map(str,args)),'expected_exit':expected,'actual_exit':p.returncode,'seconds':time.monotonic()-start,'passed':p.returncode==expected}
 results.append(row);(r/'results.json').write_text(json.dumps(results,indent=2)+'\n')
 print(name,':',p.returncode,flush=True)
 if p.returncode!=expected:raise SystemExit(1)
run('event_trace_zero_overhead',[sys.executable,str(root/'tests/test_no_event_trace_symbols.py'),str(build/'tactix'),'--debug-stubs'])
run('execution_support',[str(build/'test_execution_support')])
for e,j in ((0,0),(1,0),(0,1),(1,1)):
 run(f'feedback_provenance-e{e}-j{j}',[str(build/f'test_feedback_provenance-e{e}-j{j}')])
run('inspect_policy_feedback',[sys.executable,str(r/'inspect-wrapper.py'),str(pin/'tests/test_feedback_trace'),str(build/'test_default_execution_signature')])
run('feedback_frontend',[sys.executable,str(root/'tests/test_feedback_frontend.py'),str(build/'tactix')])
# Check the changed cold descriptions on productive default fixtures.
left=work/'original-default';right=work/'repaired-default';left.mkdir();right.mkdir()
run('original_default_signatures',[str(pin/'tests/test_default_execution_signature'),str(left),'feedback'])
run('repaired_default_signatures',[str(build/'test_default_execution_signature'),str(right),'feedback'])
assert (r/'original_default_signatures.stdout').read_bytes()==(r/'repaired_default_signatures.stdout').read_bytes()
sys.path.insert(0,str(root/'tools/fuzz'));sys.path.insert(0,str(root/'tools/bench'))
from modelcheck import check_files
from proof_oracle import check_routed
pairs=[];seen={'Reduce':0,'Restart':0}
def canonical(value):
 if isinstance(value,dict):
  out={}
  for key,item in value.items():
   if key in ('description','native_result') and isinstance(item,str):
    item=item.replace(';victims=',';victim-ids=')
    import re
    item=re.sub(r'keep-level=(\d+);purpose=scheduled',r'retained-level=\1;purpose=scheduled',item)
   out[key]=canonical(item)
  return out
 if isinstance(value,list):return [canonical(x) for x in value]
 return value
for cnf in sorted(left.glob('*.cnf')):
 mate=right/cnf.name;assert cnf.read_bytes()==mate.read_bytes();stem=cnf.stem
 proof=left/(stem+'.drat');newproof=right/proof.name;assert proof.read_bytes()==newproof.read_bytes()
 lhs=[json.loads(line) for line in (left/(stem+'.feedback.jsonl')).read_text().splitlines()];rhs=[json.loads(line) for line in (right/(stem+'.feedback.jsonl')).read_text().splitlines()]
 ldet=[canonical(x) for x in lhs if x['type']!='elapsed'];rdet=[canonical(x) for x in rhs if x['type']!='elapsed'];assert ldet==rdet,stem
 for row in rhs:
  if row.get('type')=='action' and row.get('identity') in seen and row.get('native_request'):
   desc=row['native_request']['description']
   if row['identity']=='Reduce' and ';victim-ids=' in desc:seen['Reduce']+=1
   if row['identity']=='Restart' and 'retained-level=' in desc:seen['Restart']+=1
 models=(left/(stem+'.model')).exists()
 for directory in (left,right):
  if models:
   ok,message=check_files(str(directory/cnf.name),str(directory/(stem+'.model')));assert ok,message
  else:
   output=work/(directory.name+'-'+stem+'-oracle');output.mkdir()
   ok,stage,message,_=check_routed(None,None,None,root/'tools/checkers/bin/drat-trim',root/'tools/checkers/bin/cake_lpr',directory/cnf.name,directory/proof.name,output,timeout=60);assert ok,(stage,message)
 pairs.append({'profile_case':stem,'proof_sha256':hashlib.sha256(proof.read_bytes()).hexdigest(),'independent_oracle':'model' if models else 'drat-trim+CakeLPR','both_checked':True,'deterministic_feedback_equal_after_only_description_renames':True})
assert len(pairs)==14 and all(seen.values()),seen
(r/'default-correspondence.json').write_text(json.dumps({'pairs':pairs,'count':14,'models_checked':14,'proofs_checked':14,'changed_description_records_reached':seen,'normalized_fields':['elapsed records excluded','description/native_result: two intentional cold label renames'],'printed_work_state_phase_score_and_certificate_signatures':'byte-identical','scope':'Fourteen preserved productive fixture profiles, not universal runtime or full Debug acceptance.'},indent=2)+'\n')
# Validate ignored disabled diagnostic settings through the real frontend.
spec=importlib.util.spec_from_file_location('modelcheck',root/'tools/fuzz/modelcheck.py');model=importlib.util.module_from_spec(spec);spec.loader.exec_module(model)
cnf=work/'metadata.cnf';cnf.write_text('p cnf 2 3\n1 2 0\n-1 2 0\n1 -2 0\n')
keys=('TACTIX_EVENT_TRACE','SAT_EVENT_TRACE_WATCH_VALUES','TACTIX_RULE_JOURNAL','SAT_RULE_JOURNAL_SKIP_DIGEST')
checks=[]
for policy,options in (('default',[]),('controller',['--controller-policy=7'])):
 for arm,binary in (('repaired',build/'tactix'),('original-negative',build/'control-tactix')):
  record=work/f'{policy}-{arm}.jsonl';first={**env,**{k:'disabled-A' for k in keys}};second={**env,**{k:'disabled-B' for k in keys}}
  commands=[[str(binary),'-q',*options,f'--feedback={record}',str(cnf)],[str(binary),'-q',*options,f'--feedback-replay={record}',str(cnf)]]
  a=subprocess.run(commands[0],env=first,capture_output=True,text=True,timeout=30);b=subprocess.run(commands[1],env=second,capture_output=True,text=True,timeout=30)
  (work/f'{policy}-{arm}-record.stdout').write_text(a.stdout);(work/f'{policy}-{arm}-replay.stdout').write_text(b.stdout);(work/f'{policy}-{arm}-replay.stderr').write_text(b.stderr)
  assert a.returncode==10,(a.returncode,a.stderr)
  assert b.returncode==(10 if arm=='repaired' else 1),(b.returncode,b.stderr)
  metadata=json.loads(record.read_text().splitlines()[0])['metadata']
  assert all((k+'=') not in metadata for k in keys) if arm=='repaired' else all((k+'=') in metadata for k in keys)
  for suffix,output in (('record',a.stdout),('replay',b.stdout)):
   path=work/f'{policy}-{arm}-{suffix}.model';path.write_text(output);ok,message=model.check_files(str(cnf),str(path));assert ok,message
  assert a.stdout==b.stdout
  checks.append({'policy':policy,'arm':arm,'record_exit':a.returncode,'replay_exit':b.returncode,'models_checked':2,'stdout_equal':True,'disabled_setting_values':'A versus B','commands':commands})
(r/'metadata-controls.json').write_text(json.dumps({'cases':checks,'models_checked':8,'repaired_pairs':2,'original_causal_negative_pairs':2,'scope':'Disabled-feature environment settings only; no broad fuzz, all-configuration or full Debug acceptance.'},indent=2)+'\n')
print('All fixed staged-repair checks passed; 8 additional original-CNF models verified.',flush=True)
