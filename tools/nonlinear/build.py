#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys,tarfile,time
r=Path(sys.argv[1]).resolve();root=r.parents[2];m=json.loads((r/'config.json').read_text());pin=root/m['build_pin']
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1<<20),b''):h.update(b)
 return h.hexdigest()
assert sha(pin/'source.tar')==m['source_archive_sha256']
for name,digest in json.loads((r/'contract.json').read_text()).items():assert sha(r/'pins'/name)==digest
compiler='/cm/local/apps/gcc/13.1.0/bin/c++';env=dict(os.environ,CC='/cm/local/apps/gcc/13.1.0/bin/gcc',CXX=compiler)
library=subprocess.check_output([compiler,'-print-file-name=libstdc++.so']).decode().strip();env['LD_LIBRARY_PATH']=str(Path(library).parent)
stages=[]
def call(name,args):
 start=time.monotonic()
 with (pin/(name+'.log')).open('wb') as f:code=subprocess.run(list(map(str,args)),env=env,stdout=f,stderr=subprocess.STDOUT).returncode
 stages.append(dict(stage=name,command=list(map(str,args)),exit_code=code,seconds=time.monotonic()-start));(pin/'stages.json').write_text(json.dumps(stages,indent=2)+'\n')
 if code:raise SystemExit(code)
source=pin/'baseline-source';source.mkdir()
with tarfile.open(pin/'source.tar') as f:f.extractall(source,filter='data')
call('baseline-configure',['cmake','-S',source,'-B',pin/'baseline','-DCMAKE_BUILD_TYPE=Release','-DTACTIX_BUILD_TESTS=OFF'])
call('baseline-build',['cmake','--build',pin/'baseline','-j16','--target','tactix'])
candidate_source=pin/'candidate-source';shutil.copytree(source,candidate_source)
call('install',['python3.12',candidate_source/'tools/nonlinear/install_probe.py',candidate_source])
# Bind every installed file and verify that the research installer changes
# only its explicit headers, tests and the three policy/frontend seams.
changed=[]
for p in candidate_source.rglob('*'):
 if p.is_file():
  old=source/p.relative_to(candidate_source)
  if not old.exists() or sha(old)!=sha(p):changed.append(str(p.relative_to(candidate_source)))
assert set(changed)=={'src/diagnostics/shared_anf.hpp','src/diagnostics/shared_nonlinear_probe.hpp','tests/test_shared_anf.cpp','tests/test_shared_nonlinear_probe.cpp','src/policies/default_policy.cpp','src/main.cpp'},changed
(pin/'installed-files.json').write_text(json.dumps({name:sha(candidate_source/name) for name in changed},indent=2)+'\n')
build=pin/'candidate'
call('candidate-configure',['cmake','-S',candidate_source,'-B',build,'-DCMAKE_BUILD_TYPE=Release'])
tests=['shared_anf','shared_nonlinear_probe','extended_resolution','rup_consequence','semantic_variable_advice']
call('candidate-build',['cmake','--build',build,'-j16','--target','tactix']+['test_'+name for name in tests])
call('candidate-tests',['ctest','--test-dir',build,'--output-on-failure','-R','^('+'|'.join(tests)+')$'])
# The CLI must reject proof requests before opening a file in a research arm.
cnf=pin/'guard.cnf';cnf.write_text('p cnf 1 1\n1 0\n');proof=pin/'must-not-exist.pbp'
refusal=subprocess.run([build/'tactix',cnf,proof],env=dict(env,TACTIX_NONLINEAR_ARM='definitions'),capture_output=True)
assert refusal.returncode==1 and b'forbids proof output' in refusal.stderr and not proof.exists()
(pin/'proof-refusal.json').write_text(json.dumps(dict(exit_code=refusal.returncode,stderr=refusal.stderr.decode(),proof_created=proof.exists()),indent=2)+'\n')
# Check the unchanged baseline against the probe-disabled binary on fixed CNFs.
for text in ['p cnf 1 1\n1 0\n','p cnf 1 2\n1 0\n-1 0\n','p cnf 3 3\n1 2 0\n-1 3 0\n-2 -3 0\n']:
 cnf.write_text(text);records=[]
 for binary in [pin/'baseline/tactix',build/'tactix']:
  result=subprocess.run([binary,cnf],env=dict(env,TACTIX_NONLINEAR_ARM='baseline'),capture_output=True)
  records.append((result.returncode,[line for line in result.stdout.splitlines() if line.startswith((b's ',b'v '))]))
 assert records[0]==records[1],records
for name,path in [('baseline',pin/'baseline/tactix'),('candidate',build/'tactix')]:shutil.copy2(path,r/'pins'/name)
ready=dict(config_sha256=sha(r/'config.json'),corpus_sha256=sha(r/'corpus.json'),source_archive_sha256=sha(pin/'source.tar'),binaries={name:sha(r/'pins'/name) for name in ['baseline','candidate']},tests=tests,baseline_probe_disabled_fixture_comparison=True,proof_request_rejected=True,installed_files_sha256=sha(pin/'installed-files.json'))
(r/'READY.json').write_text(json.dumps(ready,indent=2)+'\n');print(json.dumps(ready))
