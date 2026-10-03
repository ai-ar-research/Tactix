import hashlib,json,os,pathlib,subprocess,sys
root=pathlib.Path('/home/hwu/SAT-solving')
r=pathlib.Path(__file__).resolve().parent
stage=root/'build/shared-policy-terminal-substitute-f6f01982-integration'
def verify():
 for name,expected in json.loads((r/'manifest.json').read_text())['sha256'].items():
  assert hashlib.sha256(pathlib.Path(name).read_bytes()).hexdigest()==expected,name
 subprocess.run([sys.executable,str(root/'benchmarks/runs/20261002-shared-policy-feedback-f6f01982-preparation/verify.py')],check=True)
verify()
results=[]
for name in ['negative-substitute_execution','negative-policy_feedback','patched-substitute_execution','patched-policy_feedback','patched-substitute','patched-substitute_publication','patched-native_policy_feedback','patched-default_execution_signature','patched-inprocess_execution_signature']:
 env={k:v for k,v in os.environ.items() if not k.startswith(('TACTIX_','SAT_'))}
 done=subprocess.run([str(stage/name)],cwd=r,capture_output=True,text=True,timeout=300,env=env)
 (r/(name+'.stdout')).write_text(done.stdout)
 (r/(name+'.stderr')).write_text(done.stderr)
 negative=name.startswith('negative-')
 accepted=done.returncode==-6 if negative else done.returncode==0
 if negative:
  accepted &= ('large_watches_connected' in done.stderr and 'Assertion' in done.stderr)
 results.append({'name':name,'exit':done.returncode,'expected':'SIGABRT at watch-boundary assertion' if negative else '0','accepted':bool(accepted)})
 print(name,done.returncode,accepted,flush=True)
verify()
summary={'scope':'Two causal removal controls and seven staged focused tests, not final candidate acceptance','root_solver_modified':False,'controls':results,'accepted':all(x['accepted'] for x in results),'preflight_postflight_verified':True}
(r/'results.json').write_text(json.dumps(summary,indent=2)+'\n')
raise SystemExit(0 if summary['accepted'] else 1)
