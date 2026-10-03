import hashlib,json,pathlib,shlex,subprocess,time
root=pathlib.Path('/home/hwu/SAT-solving')
r=pathlib.Path(__file__).resolve().parent
pin=root/'build/pin-shared-policy-feedback-debug-f6f01982'
compiler='/cm/local/apps/gcc/13.1.0/bin/c++'
flags=['-g','-std=c++20','-Wall','-Wextra','-fno-exceptions','-fno-rtti','-DTACTIX_ENABLE_PROOF_SHADOW=1']
includes=['-I'+str(root/'src'),'-I'+str(root/'include')]
link=shlex.split((pin/'tests/CMakeFiles/test_substitute_execution.dir/link.txt').read_text())
objects=[str((pin/'tests'/a).resolve()) for a in link if a.endswith(('.o','.a')) and 'test_substitute_execution.cpp.o' not in a]
results=[]
def run(name,args):
 start=time.monotonic()
 with (r/(name+'.log')).open('w') as out:
  result=subprocess.run(args,cwd=root,stdout=out,stderr=subprocess.STDOUT)
 results.append({'name':name,'command':args,'exit':result.returncode,'seconds':time.monotonic()-start})
 (r/'build-results.json').write_text(json.dumps(results,indent=2)+'\n')
 print(name, result.returncode, flush=True)
 if result.returncode: raise SystemExit(result.returncode)
for name in ['terminal_feedback','terminal_execution']:
 run('original-'+name,[compiler,*flags,*includes,str(r/(name+'.cpp')),*objects,'-o',str(r/('original-'+name))])
replacement={}
for name in ['src/rules/substitute/substitute.cpp','src/policies/default_policy.cpp']:
 target=r/(pathlib.Path(name).stem+'.patched.o')
 source=r/name if (r/name).exists() else root/name
 run('patched-'+pathlib.Path(name).stem,[compiler,*flags,'-I'+str(r/'src'),*includes,'-c',str(source),'-o',str(target)])
 original=str(pin/'CMakeFiles/tactix_lib.dir'/str(name+'.o'))
 replacement[original]=str(target)
updated=[replacement.get(a,a) for a in objects]
assert len(set(replacement)&set(objects))==2
for name in ['terminal_feedback','terminal_execution']:
 run('patched-'+name,[compiler,*flags,'-I'+str(r/'src'),*includes,str(r/(name+'.cpp')),*updated,'-o',str(r/('patched-'+name))])
(r/'build-binding.json').write_text(json.dumps({'base_pin':str(pin),'solver_source_changes':0,'artifacts':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in r.iterdir() if p.is_file() and (p.name.startswith(('original-terminal','patched-terminal')) and p.suffix!='.log')}},indent=2)+'\n')
