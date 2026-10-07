from pathlib import Path
import subprocess,shlex,shutil,json,hashlib,sys
r=Path(__file__).resolve().parent.parent
old=r/'build/persistent-xor-sorted-cumulative-20261007'
new=r/'build/persistent-xor-memory-fixed-cumulative-20261007'
kind=sys.argv[1];build=old/kind;out=new/(kind+'-replacement');out.mkdir(exist_ok=False)
oldsrc=old/'source';src=new/'source'
oldbinding=json.loads((old/'source-bindings.json').read_text())
for p,h in oldbinding['source_file_hashes'].items():
 if p!='src/rules/semantic_extension/affine_matrix.cpp':
  assert hashlib.sha256((src/p).read_bytes()).hexdigest()==h,p
flags={}
for line in (build/'CMakeFiles/tactix_lib.dir/flags.make').read_text().splitlines():
 if ' = ' in line:
  k,v=line.split(' = ',1);flags[k]=shlex.split(v)
cache=(build/'CMakeCache.txt').read_text().splitlines()
compiler=next(x.split('=',1)[1] for x in cache if x.startswith('CMAKE_CXX_COMPILER:FILEPATH='))
compileflags=flags['CXX_DEFINES']+flags['CXX_INCLUDES']+flags['CXX_FLAGS']
obj=out/'affine_matrix.cpp.o'
commands=[]
def run(argv):
 commands.append(argv);subprocess.run(argv,check=True)
run([compiler,*compileflags,'-c',str(src/'src/rules/semantic_extension/affine_matrix.cpp'),'-o',str(obj)])
reused_objects={}
targets=['test_affine_matrix_sorted_extension','test_affine_matrix_sorted_extension_budget','test_affine_matrix_incremental_capture','test_affine_matrix_persistent_frontend','test_affine_matrix_persistent_lifecycle','test_proof_premise_lease_lifecycle','test_affine_matrix_projection','test_default_affine_search']
def link(target,newobj=None,newname=None):
 args=shlex.split((build/'tests/CMakeFiles'/f'{target}.dir/link.txt').read_text());after_output=False
 for i,arg in enumerate(args):
  if after_output:args[i]=str(out/(newname or target));after_output=False;continue
  if arg=='-o':after_output=True;continue
  if arg.endswith(('.o','.a','.so')):
   resolved=Path(arg) if Path(arg).is_absolute() else build/'tests'/arg
   if resolved.name=='affine_matrix.cpp.o':args[i]=str(obj)
   elif newobj is not None and resolved.name==target+'.cpp.o':args[i]=str(newobj)
   else:
    resolved=resolved.resolve();args[i]=str(resolved)
    if arg.endswith('.o'):reused_objects[str(resolved)]=hashlib.sha256(resolved.read_bytes()).hexdigest()
 run(args)
for target in targets:link(target)
memobj=out/'test_affine_matrix_sorted_extension_memory.cpp.o'
run([compiler,*compileflags,'-c',str(src/'tests/test_affine_matrix_sorted_extension_memory.cpp'),'-o',str(memobj)])
link('test_affine_matrix_sorted_extension',memobj,'test_affine_matrix_sorted_extension_memory')
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
bind={'kind':kind,'predecessor_tree_sha256':oldbinding['source_tree_sha256'],'source_tree_sha256':json.loads((new/'source-bindings.json').read_text())['source_tree_sha256'],'unchanged_headers_and_other_sources_verified':True,'replaced_object':'affine_matrix.cpp.o','new_object_sha256':sha(obj),'reused_objects':reused_objects,'compiler':compiler,'flags':compileflags,'commands':commands,'binaries':{p.name:sha(p) for p in out.glob('test_*') if not p.name.endswith('.o')}}
(out/'build-bindings.json').write_text(json.dumps(bind,indent=2)+'\n')
print('coherent supplement built',kind,len(bind['binaries']))
