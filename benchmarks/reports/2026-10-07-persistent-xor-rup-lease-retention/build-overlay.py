from pathlib import Path
import subprocess,shlex,json,hashlib,shutil,sys
r=Path(__file__).resolve().parents[2]
kind=sys.argv[1] if len(sys.argv)>1 else 'sanitize'
assert kind in ['debug','sanitize']
old=r/'build/persistent-xor-sorted-cumulative-20261007';v6=r/'build/persistent-xor-memory-fixed-cumulative-20261007'
new=r/'build/persistent-xor-rup-retention-20261007';new.mkdir(exist_ok=True)
src=new/'source';src.mkdir(exist_ok=True);out=new/kind;out.mkdir(exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
changes=['src/proof/proof.cpp','tests/test_proof_premise_lease_lifecycle.cpp']
for p in changes:
 dest=src/p;dest.parent.mkdir(parents=True,exist_ok=True);assert dest.exists() and sha(dest)==sha(r/p),p
binding=json.loads((v6/'source-bindings.json').read_text());older=json.loads((old/'source-bindings.json').read_text())
for p,h in binding['source_file_hashes'].items():assert sha(v6/'source'/p)==h,p
for p,h in older['source_file_hashes'].items():
 if p not in ['src/rules/semantic_extension/affine_matrix.cpp',*changes]:assert sha(v6/'source'/p)==h,p
build=old/kind;flags={}
for line in (build/'CMakeFiles/tactix_lib.dir/flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);flags[k]=shlex.split(v)
compiler=next(x.split('=',1)[1] for x in (build/'CMakeCache.txt').read_text().splitlines() if x.startswith('CMAKE_CXX_COMPILER:FILEPATH='))
cf=flags['CXX_DEFINES']+flags['CXX_INCLUDES']+flags['CXX_FLAGS'];commands=[];reused={}
def run(args):
 commands.append(args);subprocess.run(args,check=True)
objects={}
for relative,source in [('src/rules/semantic_extension/affine_matrix.cpp',v6/'source/src/rules/semantic_extension/affine_matrix.cpp'),('src/proof/proof.cpp',src/'src/proof/proof.cpp'),('tests/test_proof_premise_lease_lifecycle.cpp',src/'tests/test_proof_premise_lease_lifecycle.cpp')]:
 obj=out/(source.name+'.o');run([compiler,*cf,'-c',str(source),'-o',str(obj)]);objects[relative]=obj
for target in ['test_sr_proof_encoding','test_population_observer','test_affine_matrix_persistent_lifecycle','test_affine_matrix_persistent_frontend','test_default_affine_search','test_proof_premise_lease_lifecycle']:
 if target in ['test_sr_proof_encoding','test_population_observer']:
  tf={}
  for line in (build/'tests/CMakeFiles'/f'{target}.dir/flags.make').read_text().splitlines():
   if ' = ' in line:
    key,value=line.split(' = ',1);tf[key]=shlex.split(value)
  testobj=out/(target+'.cpp.o')
  run([compiler,*tf['CXX_DEFINES'],*tf['CXX_INCLUDES'],*tf['CXX_FLAGS'],'-c',str(v6/'source/tests'/(target+'.cpp')),'-o',str(testobj)])
  objects['tests/'+target+'.cpp']=testobj
 args=shlex.split((build/'tests/CMakeFiles'/f'{target}.dir/link.txt').read_text());nextoutput=False
 for i,arg in enumerate(args):
  if nextoutput:args[i]=str(out/target);nextoutput=False;continue
  if arg=='-o':nextoutput=True;continue
  if arg.endswith(('.o','.a','.so')):
   resolved=(Path(arg) if Path(arg).is_absolute() else build/'tests'/arg).resolve()
   substitute=next((obj for rel,obj in objects.items() if resolved.name==Path(rel).name+'.o'),None)
   if substitute:args[i]=str(substitute)
   else:args[i]=str(resolved);reused[str(resolved)]=sha(resolved)
 run(args)
result={'v5_source_tree_sha256':older['source_tree_sha256'],'v6_source_tree_sha256':binding['source_tree_sha256'],'all_v6_source_hashes_verified':True,'reused_source_compatibility_verified':True,'frozen_overlays':{p:sha(src/p) for p in changes},'replacement_objects':{p:sha(o) for p,o in objects.items()},'reused_objects':reused,'compiler':compiler,'flags':cf,'commands':commands,'binaries':{p.name:sha(p) for p in out.glob('test_*') if not p.name.endswith('.o')}}
(new/(kind+'-build-bindings.json')).write_text(json.dumps(result,indent=2)+'\n');print('RUP Debug coherent overlay built')
