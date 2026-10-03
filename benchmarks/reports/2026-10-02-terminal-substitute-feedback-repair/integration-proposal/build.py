import json,pathlib,shlex,subprocess,time
root=pathlib.Path('/home/hwu/SAT-solving')
r=pathlib.Path(__file__).resolve().parent
stage=root/'build/shared-policy-terminal-substitute-f6f01982-staging'
pin=root/'build/pin-shared-policy-feedback-debug-f6f01982'
compiler='/cm/local/apps/gcc/13.1.0/bin/c++'
flags=['-g','-std=c++20','-Wall','-Wextra','-fno-exceptions','-fno-rtti','-DTACTIX_ENABLE_PROOF_SHADOW=1']
includes=['-I'+str(root/'src'),'-I'+str(root/'include')]
link=shlex.split((pin/'tests/CMakeFiles/test_substitute_execution.dir/link.txt').read_text())
objects=[str((pin/'tests'/a).resolve()) for a in link if a.endswith(('.o','.a')) and 'test_substitute_execution.cpp.o' not in a]
replacements={str(pin/'CMakeFiles/tactix_lib.dir/src/rules/substitute/substitute.cpp.o'):str(stage/'substitute.patched.o'),str(pin/'CMakeFiles/tactix_lib.dir/src/policies/default_policy.cpp.o'):str(stage/'default_policy.patched.o')}
updated=[replacements.get(a,a) for a in objects]
results=[]
def run(name,command):
 start=time.monotonic()
 with (r/(name+'.log')).open('w') as output:
  done=subprocess.run(command,cwd=root,stdout=output,stderr=subprocess.STDOUT)
 results.append({'name':name,'command':command,'exit':done.returncode,'seconds':time.monotonic()-start})
 (r/'build-results.json').write_text(json.dumps(results,indent=2)+'\n')
 print(name,done.returncode,flush=True)
 if done.returncode:raise SystemExit(done.returncode)
for name in ['substitute_execution','policy_feedback']:
 run('negative-'+name,[compiler,*flags,*includes,str(r/'tests'/('test_'+name+'.cpp')),*objects,'-o',str(r/('negative-'+name))])
for name in ['substitute_execution','policy_feedback','substitute','substitute_publication','native_policy_feedback','default_execution_signature','inprocess_execution_signature']:
 source=r/'tests'/('test_'+name+'.cpp')
 if not source.exists():source=root/'tests'/('test_'+name+'.cpp')
 run('patched-'+name,[compiler,*flags,'-I'+str(r/'src'),*includes,str(source),*updated,'-o',str(r/('patched-'+name))])
