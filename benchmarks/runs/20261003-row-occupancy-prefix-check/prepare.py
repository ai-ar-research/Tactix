from pathlib import Path
import json,lzma,hashlib
root=Path(__file__).resolve().parents[3];run=Path(__file__).resolve().parent
prior=root/'benchmarks/runs/20261003-row-occupancy-extension';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for name,h in json.loads((prior/'SHA256.json').read_text()).items():assert sha(prior/name)==h,name
source=root/'benchmarks/cache/tierD24-26/d8225a58e2ca7c2fb93c533f406c02d5-abw-K-dwt__234.mtx-w50.cnf.xz'
raw=lzma.open(source,'rb').read();assert hashlib.sha256(raw).hexdigest()==json.loads((prior/'result.json').read_text())['input_sha256']
lines=raw.decode().splitlines();header=next(s for s in lines if s.startswith('p cnf '));variables,count=map(int,header.split()[2:]);body=[s for s in lines if s and not s.startswith(('c','p'))]
records=json.loads((prior/'proposed-definitions.json').read_text());a=variables+len(records)+1;b=a+1
gadget=[[a,b],[a,-b],[-a,b],[-a,-b]]
assert sum(s.split().count('0') for s in body)==count
(run/'audit.cnf').write_text(f'p cnf {b} {count+4}\n'+'\n'.join(body)+'\n'+''.join(' '.join(map(str,c))+' 0\n' for c in gadget))
lrat=[];dsr=[];logical=count+4
for record in records:
 ids=[]
 for index,clause in enumerate(record['clauses']):
  logical+=1;hints=[] if index<3 else [-i for i in ids]
  lrat.append([logical,*clause,0,*hints,0]);ids.append(logical);dsr.append(clause)
unit=logical+1;lrat.extend([[unit,a,0,count+1,count+2,0],[unit+1,0,unit,count+3,count+4,0]]);dsr.extend([[a],[]])
(run/'all-addition.lrat').write_text(''.join(' '.join(map(str,line))+'\n' for line in lrat));(run/'audit.dsr').write_text(''.join(' '.join(map(str,c))+(' ' if c else '')+'0\n' for c in dsr))
(run/'negative.lrat').write_text(f'{count+5} 0 0\n')
checkers=root/'benchmarks/runs/20261003-ramsey-degree-dsr-feasibility/checkers'
files=[source,prior/'proposed-definitions.json',prior/'SHA256.json',run/'prepare.py',run/'run.py',run/'job.sh',run/'audit.cnf',run/'all-addition.lrat',run/'audit.dsr',run/'negative.lrat',checkers/'drat-trim',checkers/'cake_lpr',root/'tools/bench/proof_oracle.py']
(run/'manifest.json').write_text(json.dumps({'input_sha256':hashlib.sha256(raw).hexdigest(),'original_clauses':count,'definition_additions':len(dsr)-2,'gadget_clause_ids':list(range(count+1,count+5)),'bindings':{str(p.relative_to(root)):sha(p) for p in files},'scope':'Every-addition certificate validation of conservative definitions over actual target CNF; no solve or proof of row-cover units'},indent=2)+'\n')
