from pathlib import Path
import hashlib,json,subprocess,sys,tarfile,io,shutil,resource
root=Path(__file__).resolve().parent.parent
base=root/'build/persistent-xor-coherent-qualification-20261007'
names=['test_affine_matrix_dirty_rows','test_default_affine_search','test_affine_matrix_persistent_lifecycle','test_affine_matrix_persistent_frontend','test_proof_premise_lease_lifecycle','test_affine_matrix_sorted_extension','test_affine_matrix_sorted_extension_budget','test_affine_matrix_sorted_extension_memory','test_affine_matrix_incremental_capture','test_affine_matrix_projection','test_sr_proof_encoding','test_population_observer','test_affine_matrix_transaction','test_affine_matrix_search','test_affine_matrix_live','test_affine_matrix','test_api_c','test_flat_payload_replay','test_affine_basis_action','test_affine_matrix_actual_geometry','test_random_controller_affine_resource']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
mode=sys.argv[1]
if mode=='freeze':
 base.mkdir(exist_ok=False);source=base/'source';source.mkdir()
 commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()
 archive=subprocess.check_output(['git','archive','HEAD'],cwd=root)
 with tarfile.open(fileobj=io.BytesIO(archive)) as t:t.extractall(source,filter='data')
 changed=subprocess.check_output(['git','diff','--name-only','HEAD'],cwd=root,text=True).splitlines()
 untracked=subprocess.check_output(['git','ls-files','--others','--exclude-standard','src','tests'],cwd=root,text=True).splitlines()
 overlays=sorted(set(changed+untracked))
 for rel in overlays:
  p=root/rel
  if p.is_file():(source/rel).parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,source/rel)
 for rel in ['benchmarks/reports/2026-09-14-orbitopal-certified-claude-fuzz/terminal-report.md']:
  dest=source/rel;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(root/rel,dest)
 hashes={str(p.relative_to(source)):sha(p) for p in sorted(source.rglob('*')) if p.is_file()}
 digest=hashlib.sha256(''.join(f'{k} {v}\n' for k,v in sorted(hashes.items())).encode()).hexdigest()
 (base/'source-bindings.json').write_text(json.dumps({'base_commit':commit,'overlays':overlays,'source_tree_sha256':digest,'source_file_hashes':hashes},indent=2)+'\n')
 print('Frozen',digest,flush=True)
else:
 kind=sys.argv[2];source=base/'source';build=base/kind
 binding=json.loads((base/'source-bindings.json').read_text())
 for rel,h in binding['source_file_hashes'].items():assert sha(source/rel)==h,rel
 if mode=='build':
  flags=['-DCMAKE_BUILD_TYPE=Release','-DTACTIX_ENABLE_PROOF_SHADOW=OFF'] if kind=='release' else ['-DCMAKE_BUILD_TYPE=Debug','-DTACTIX_ENABLE_PROOF_SHADOW=ON']
  if kind=='sanitize':flags+=['-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer','-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer','-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined']
  subprocess.run(['cmake','-S',str(source),'-B',str(build),*flags],check=True)
  build_names=[] if kind=='release' else names
  subprocess.run(['cmake','--build',str(build),'--parallel','6','--target',*build_names,'tactix'],check=True)
  (build/'qualification-bindings.json').write_text(json.dumps({'source_tree_sha256':binding['source_tree_sha256'],'binaries':{n:sha(build/'tests'/n) for n in build_names},'tactix_sha256':sha(build/'tactix')},indent=2)+'\n')
 elif mode=='check':
  resource.setrlimit(resource.RLIMIT_CORE,(0,0))
  out=base/(kind+'-checks');out.mkdir(exist_ok=False);results=[]
  compiled=json.loads((build/'qualification-bindings.json').read_text())
  for n,h in compiled['binaries'].items():
   exe=build/'tests'/n;assert sha(exe)==h
   with (out/(n+'.log')).open('w') as log:r=subprocess.run([str(exe)],cwd=out,stdout=log,stderr=subprocess.STDOUT,timeout=120)
   results.append({'test':n,'exit_code':r.returncode});print(n,r.returncode,flush=True)
   if r.returncode:break
  if all(r['exit_code']==0 for r in results):
   for script,driver in [('affine_matrix_persistent_lifecycle_proof_model.sh','test_affine_matrix_persistent_lifecycle'),('default_affine_search_proof_model.sh','test_default_affine_search'),('affine_matrix_actual_geometry_proof_model.sh','test_affine_matrix_actual_geometry'),('random_controller_affine_resource_proof_model.sh','test_random_controller_affine_resource')]:
    with (out/(script+'.log')).open('w') as log:r=subprocess.run(['bash',str(source/'tests'/script),str(build/'tests'/driver),str(root)],cwd=out,stdout=log,stderr=subprocess.STDOUT,timeout=180)
    results.append({'test':script,'exit_code':r.returncode});print(script,r.returncode,flush=True)
  (out/'results.json').write_text(json.dumps({'source_tree_sha256':binding['source_tree_sha256'],'kind':kind,'checks':results,'all_passed':len(results)==len(names)+4 and all(r['exit_code']==0 for r in results)},indent=2)+'\n')
  assert len(results)==len(names)+4 and all(r['exit_code']==0 for r in results)
