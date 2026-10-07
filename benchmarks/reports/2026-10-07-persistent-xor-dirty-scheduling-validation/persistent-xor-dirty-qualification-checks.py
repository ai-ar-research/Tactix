from pathlib import Path
import subprocess,json,sys,hashlib,resource
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
r=Path(__file__).resolve().parent.parent;d=r/'build/persistent-xor-dirty-qualification-20261007';kind=sys.argv[1];b=d/(kind+'-replacement');o=d/(kind+'-checks');o.mkdir(exist_ok=False);binding=json.loads((b/'build-bindings.json').read_text());results=[]
for name,h in binding['binaries'].items():
 exe=b/name;assert hashlib.sha256(exe.read_bytes()).hexdigest()==h
 with (o/(name+'.log')).open('w') as log:p=subprocess.run([str(exe)],cwd=o,stdout=log,stderr=subprocess.STDOUT,timeout=60)
 results.append({'test':name,'exit_code':p.returncode});assert p.returncode==0,(name,p.returncode)
for script,driver in [('affine_matrix_persistent_lifecycle_proof_model.sh','test_affine_matrix_persistent_lifecycle'),('default_affine_search_proof_model.sh','test_default_affine_search')]:
 with (o/(script+'.log')).open('w') as log:p=subprocess.run([str(d/'source/tests'/script),str(b/driver),str(d/'source')],cwd=o,stdout=log,stderr=subprocess.STDOUT,timeout=120)
 results.append({'test':script,'exit_code':p.returncode});assert p.returncode==0,(script,p.returncode)
(o/'results.json').write_text(json.dumps({'kind':kind,'source_tree_sha256':binding['source_tree_sha256'],'checks':results,'all_passed':True},indent=2)+'\n');print(kind,len(results),'coherent dirty/scheduling/API/replay andindependent proof/model checks PASS')
