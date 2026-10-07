from pathlib import Path
import subprocess,json,sys,hashlib
r=Path(__file__).resolve().parent.parent;n=r/'build/persistent-xor-memory-fixed-cumulative-20261007';kind=sys.argv[1];b=n/(kind+'-replacement');binding=json.loads((b/'build-bindings.json').read_text());results=[]
for name,h in binding['binaries'].items():
 exe=b/name;assert hashlib.sha256(exe.read_bytes()).hexdigest()==h
 with (b/(name+'.log')).open('w') as log:
  code=subprocess.run([str(exe)],cwd=b,stdout=log,stderr=subprocess.STDOUT).returncode
 results.append({'test':name,'exit_code':code});assert code==0,(name,code)
for script,driver in [('affine_matrix_persistent_lifecycle_proof_model.sh','test_affine_matrix_persistent_lifecycle'),('default_affine_search_proof_model.sh','test_default_affine_search')]:
 with (b/(script+'.log')).open('w') as log:
  code=subprocess.run([str(n/'source/tests'/script),str(b/driver),str(n/'source')],cwd=b,stdout=log,stderr=subprocess.STDOUT).returncode
 results.append({'test':script,'exit_code':code});assert code==0,(script,code)
(b/'check-results.json').write_text(json.dumps({'kind':kind,'checks':results,'all_passed':True},indent=2)+'\n');print(kind,len(results),'fixed native and independent proof/model checks PASS')
