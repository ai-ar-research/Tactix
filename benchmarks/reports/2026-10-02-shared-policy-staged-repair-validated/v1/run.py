import hashlib,importlib.util,json,os,resource,subprocess,sys,time
from pathlib import Path
r=Path(__file__).resolve().parent
root=r.parents[2]
build=root/'build/staged-debug-repair-7e2c5eb1-runtime'
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
run('inspect_policy_feedback',[sys.executable,str(r/'inspect-wrapper.py'),str(pin/'tests/test_feedback_trace'),str(pin/'tests/test_default_execution_signature')])
run('feedback_frontend',[sys.executable,str(root/'tests/test_feedback_frontend.py'),str(build/'tactix')])
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
