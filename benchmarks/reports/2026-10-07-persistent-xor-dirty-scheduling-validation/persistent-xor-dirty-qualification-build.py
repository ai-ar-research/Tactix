from pathlib import Path
import subprocess,shlex,json,hashlib,sys
r=Path(__file__).resolve().parent.parent;old=r/'build/persistent-xor-dirty-cumulative-20261007';new=r/'build/persistent-xor-dirty-qualification-20261007';kind=sys.argv[1];b=old/kind;o=new/(kind+'-replacement');o.mkdir(exist_ok=False);src=new/'source'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
ob=json.loads((old/'source-bindings.json').read_text());nb=json.loads((new/'source-bindings.json').read_text())
changes=['src/rules/semantic_extension/affine_matrix.cpp','tests/test_affine_matrix_dirty_rows.cpp','tests/test_default_affine_search.cpp']
for p,h in ob['source_file_hashes'].items():
 if p not in changes:assert sha(src/p)==h,p
for p,h in nb['source_file_hashes'].items():assert sha(src/p)==h,p
compiler=next(x.split('=',1)[1] for x in (b/'CMakeCache.txt').read_text().splitlines() if x.startswith('CMAKE_CXX_COMPILER:FILEPATH='))
def flags(path):
 result={}
 for line in path.read_text().splitlines():
  if ' = ' in line:
   k,v=line.split(' = ',1);result[k]=shlex.split(v)
 return result
commands=[];objects={};reused={}
def run(a):commands.append(a);subprocess.run(a,check=True)
for p in changes:
 target=Path(p).stem
 f=flags(b/'CMakeFiles/tactix_lib.dir/flags.make') if p.startswith('src/') else flags(b/'tests/CMakeFiles'/(target+'.dir')/'flags.make')
 obj=o/(Path(p).name+'.o');run([compiler,*f['CXX_DEFINES'],*f['CXX_INCLUDES'],*f['CXX_FLAGS'],'-c',str(src/p),'-o',str(obj)]);objects[Path(p).name+'.o']=obj
names=['test_affine_matrix_dirty_rows','test_default_affine_search','test_affine_matrix_persistent_lifecycle','test_affine_matrix_persistent_frontend','test_proof_premise_lease_lifecycle','test_affine_matrix_sorted_extension','test_affine_matrix_sorted_extension_budget','test_affine_matrix_sorted_extension_memory','test_affine_matrix_incremental_capture','test_affine_matrix_projection','test_sr_proof_encoding','test_population_observer','test_affine_matrix_transaction','test_affine_matrix_search','test_affine_matrix_live','test_affine_matrix','test_api_c','test_flat_payload_replay','test_affine_basis_action']
for name in names:
 a=shlex.split((b/'tests/CMakeFiles'/(name+'.dir')/'link.txt').read_text());output=False
 for i,x in enumerate(a):
  if output:a[i]=str(o/name);output=False;continue
  if x=='-o':output=True;continue
  if x.endswith(('.o','.a','.so')):
   f=(Path(x) if Path(x).is_absolute() else b/'tests'/x).resolve()
   if f.name in objects:a[i]=str(objects[f.name])
   else:a[i]=str(f);reused[str(f)]=sha(f)
 run(a)
result={'kind':kind,'predecessor_tree_sha256':ob['source_tree_sha256'],'source_tree_sha256':nb['source_tree_sha256'],'unchanged_headers_and_other_sources_verified':True,'replacement_objects':{n:sha(p) for n,p in objects.items()},'reused_objects':reused,'commands':commands,'compiler':compiler,'binaries':{n:sha(o/n) for n in names}}
(o/'build-bindings.json').write_text(json.dumps(result,indent=2)+'\n');print(kind,'19 coherent corrected dirty/scheduling binaries built')
