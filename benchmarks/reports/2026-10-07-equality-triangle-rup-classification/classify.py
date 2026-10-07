#!/usr/bin/env python3
"""Checker-only classification; never invokes solver or mutates original."""
from pathlib import Path
import hashlib,json,os,subprocess,time
D=Path(__file__).resolve().parent;ROOT=D.parents[3]
M=json.loads((D/'manifest.json').read_text())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for name,h in M['bindings'].items():assert sha(ROOT/name)==h,name
assert os.environ.get('SLURM_JOB_ID')
O=D/'results';O.mkdir(exist_ok=False)
A=ROOT/M['original'];S=A/'on/results'
b=(S/'direct.binary.lsr').read_bytes();i=0;live={}
# Canonical original checker CNF uses one clause per line.
for line in (S/'checker.cnf').read_text().splitlines():
 if line and line[0] not in 'cp':
  c=list(map(int,line.split()));assert c[-1]==0;live[len(live)+1]=c[:-1]
def integer():
 global i
 n=0;s=0
 while True:
  c=b[i];i+=1;n|=(c&127)<<s;s+=7
  if c<128:return -(n>>1) if n&1 else n>>1
rows=[];triangles=[[-1423,-1425,1427],[-1423,-1427,1425],[-1425,-1427,1423]]
while i<len(b):
 k=b[i];i+=1;assert k in (3,4);idx=integer();parts=[]
 for _ in range(2 if k==3 else 1):
  x=[]
  while True:
   n=integer()
   if not n:break
   x.append(n)
  parts.append(x)
 if idx>=9034:
  rows.append({'id':idx,'literals':parts[0],'hints':parts[1]});continue
 if k==4:
  for deleted in parts[0]:assert deleted in live;del live[deleted]
 else:
  c=parts[0]
  # LSR witness starts at the second occurrence of the pivot.
  end=next((j for j in range(1,len(c)) if c[j]==c[0]),len(c))
  live[idx]=c[:end]
assert len(rows)==3 and all(any(h<0 for h in r['hints']) for r in rows)
assert [sorted(r['literals']) for r in rows]==[sorted(c) for c in triangles]
C=O/'live-prefix.cnf';extent=max(abs(l) for c in live.values() for l in c)
C.write_text('p cnf %d %d\n'%(extent,len(live))+''.join(' '.join(map(str,c))+' 0\n' for c in live.values()))
lines=(S/'transcript').read_text().splitlines();boundary=int(next(x for x in lines if x.startswith('offline-prefix-bytes ')).split()[1])
P=O/'bridged-suffix.dsr';P.write_bytes((S/'proof.dsr').read_bytes()[boundary:])
T=O/'direct-suffix.dsr';T.write_text(''.join(' '.join(map(str,c))+' 0\n' for c in triangles))
deadline=time.monotonic()+600;stages=[]
def run(name,cmd):
 with (O/(name+'.log')).open('wb') as f:r=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT,timeout=max(1,deadline-time.monotonic()))
 t=(O/(name+'.log')).read_text().splitlines();stages.append({'name':name,'command':cmd,'exit':r.returncode,'log_sha256':sha(O/(name+'.log'))});return r.returncode,t
trim=M['trim'];sr=M['sr']
rc,t=run('direct-full-independent',[sr,str(S/'checker.cnf'),str(S/'direct.binary.lsr'),'c']);assert rc==0 and 's VALID' in t
rc,t=run('bridged-rup-only',[trim,'-f','-c','-U',str(C),str(P),str(O/'bridged.binary.lsr')]);assert rc==0 and 's VALID' in t
rc,t=run('bridged-independent',[sr,str(C),str(O/'bridged.binary.lsr'),'c']);assert rc==0 and 's VALID' in t
rc,t=run('direct-rup-only',[trim,'-f','-c','-U',str(C),str(T),str(O/'direct-rup.binary.lsr')]);assert rc!=0 and 's VALID' not in t
assert any('not RUP' in x for x in t)
(O/'result.json').write_text(json.dumps({'status':'CLASSIFIED','original_terminal_failed_preserved':True,'direct_full_prefix_independently_valid':True,'direct_theorem_uses_negative_RAT_hints':rows,'direct_suffix_not_RUP':True,'bridged_suffix_RUP_only_and_independently_valid':True,'no_solver_run':True,'no_CDCL_consumption_claim':True,'stages':stages,'derived_files':{x.name:sha(x) for x in [C,P,T]}},indent=2)+'\n')
