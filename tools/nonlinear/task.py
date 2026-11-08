#!/usr/bin/env python3
"""One instance/repetition, all four arms on the same CPU, all costs charged."""
from pathlib import Path
import hashlib,json,lzma,os,signal,socket,subprocess,sys,time
r=Path(sys.argv[1]).resolve();root=r.parents[2];sys.path.insert(0,str(r/'pins'));import modelcheck
m=json.loads((r/'config.json').read_text());ready=json.loads((r/'READY.json').read_text())
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1<<20),b''):h.update(b)
 return h.hexdigest()
assert sha(r/'config.json')==ready['config_sha256'] and sha(r/'corpus.json')==ready['corpus_sha256']
for name,digest in json.loads((r/'contract.json').read_text()).items():assert sha(r/'pins'/name)==digest
for name,digest in ready['binaries'].items():assert sha(r/'pins'/name)==digest
index=int(sys.argv[2]);entry_id,rep=divmod(index,m['repetitions']);entry=json.loads((r/'corpus.json').read_text())['entries'][entry_id]
compressed=root/entry['cache_path'];assert sha(compressed)==entry['compressed_sha256']
w=r/'logs'/f'{index:03}';w.mkdir();scratch=r/'scratch'/f'{index:03}';scratch.mkdir();cnf=scratch/'input.cnf'
t=time.monotonic()
with lzma.open(compressed,'rb') as src,cnf.open('wb') as dst:
 while b:=src.read(1<<20):dst.write(b)
prepare=time.monotonic()-t;input_sha=sha(cnf)
cpu=min(os.sched_getaffinity(0));os.sched_setaffinity(0,{cpu})
base={k:v for k,v in os.environ.items() if not k.startswith(('TACTIX_','SAT_'))};base['TMPDIR']=str(scratch)
library=subprocess.check_output(['/cm/local/apps/gcc/13.1.0/bin/c++','-print-file-name=libstdc++.so']).decode().strip();base['LD_LIBRARY_PATH']=str(Path(library).parent)
shift=(entry_id+rep)%4;order=m['arms'][shift:]+m['arms'][:shift]
record=dict(entry=entry,repetition=rep,cpu=cpu,node=socket.gethostname(),job=os.environ.get('SLURM_JOB_ID'),order=order,config_sha256=sha(r/'config.json'),ready_sha256=sha(r/'READY.json'),contract_sha256=sha(r/'contract.json'),input_sha256=input_sha,input_bytes=cnf.stat().st_size,common_decompression_seconds=prepare,arms={},complete=False)
result_path=r/'results'/f'{index:03}.json'
def save():
 tmp=result_path.with_suffix('.partial');tmp.write_text(json.dumps(record,indent=2)+'\n');tmp.replace(result_path)
save()
for arm in order:
 env=dict(base)
 if arm!='baseline':env['TACTIX_NONLINEAR_ARM']=arm
 binary=r/'pins'/('baseline' if arm=='baseline' else 'candidate');stdout=w/(arm+'.out');stderr=w/(arm+'.err')
 start=time.monotonic();timeout=False;term_time=None
 with stdout.open('wb') as out,stderr.open('wb') as err:
  proc=subprocess.Popen([binary,cnf],env=env,stdout=out,stderr=err,start_new_session=True)
  while True:
   pid,status,usage=os.wait4(proc.pid,os.WNOHANG)
   if pid:break
   now=time.monotonic()
   if now-start>=m['seconds'] and not timeout:
    timeout=True;term_time=now;os.killpg(proc.pid,signal.SIGTERM)
   if term_time is not None and now-term_time>=2:
    os.killpg(proc.pid,signal.SIGKILL);pid,status,usage=os.wait4(proc.pid,0);break
   time.sleep(.01)
 elapsed=time.monotonic()-start;code=os.waitstatus_to_exitcode(status);lines=stdout.read_text(errors='replace').splitlines()
 verdict='timeout' if timeout else 'SAT' if code==10 and 's SATISFIABLE' in lines else 'UNSAT-unchecked' if code==20 and 's UNSATISFIABLE' in lines else 'unexpected-result'
 model_ok=None;model_seconds=0;message='not applicable'
 if verdict=='SAT':
  t=time.monotonic();model_ok,message=modelcheck.check_files(cnf,stdout);model_seconds=time.monotonic()-t
  if not model_ok:verdict='invalid-model'
 nonlinear=[line for line in lines if line.startswith('c nonlinear ')];stat_prefixes=['c conflicts','c decisions','c propagate','c eliminate','c factor','c congruence','c sweep','c timing','c parsing']
 record['arms'][arm]=dict(command=[str(binary),str(cnf)],environment={k:v for k,v in env.items() if k.startswith('TACTIX_')},exit_code=code,status=verdict,wall_seconds=elapsed,cpu_seconds=usage.ru_utime+usage.ru_stime,peak_rss_kib=usage.ru_maxrss,par2=elapsed if verdict in ['SAT','UNSAT-unchecked'] else 2*m['seconds'],model_valid=model_ok,model_validation_seconds=model_seconds,model_message=message,stdout_sha256=sha(stdout),stderr_sha256=sha(stderr),nonlinear=nonlinear,stats=[line for line in lines if any(line.startswith(prefix) for prefix in stat_prefixes)])
 save()
record['complete']=True;cnf.unlink();scratch.rmdir();save()
if any(a['status'] in ['unexpected-result','invalid-model'] for a in record['arms'].values()):raise SystemExit(1)
