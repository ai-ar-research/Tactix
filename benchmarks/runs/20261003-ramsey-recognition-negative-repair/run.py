from pathlib import Path
import hashlib,json,subprocess,os
root=Path(__file__).resolve().parents[3];run=Path(__file__).resolve().parent
assert os.environ['SLURM_JOB_ID']
m=json.loads((run/'manifest.json').read_text())
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for p,h in m['bindings'].items():assert sha(root/p)==h,p
rows=[]
for i in range(3):
 src=root/m['parent']/('case-'+str(i))/'original.cnf'
 lines=src.read_text().splitlines();header=next(l for l in lines if l.startswith('p '));variables=int(header.split()[2])
 original=[list(map(int,l.split()[:-1])) for l in lines if l and not l.startswith(('p','c'))]
 for kind in ('missing','duplicate','sign'):
  clauses=[r[:] for r in original]
  if kind=='missing':clauses.pop()
  if kind=='duplicate':clauses[0]=clauses[1][:]
  if kind=='sign':clauses[0][0]=-clauses[0][0]
  work=run/(str(i)+'-'+kind);work.mkdir()
  cnf=work/'input.cnf';prefix=work/'output'
  cnf.write_text('p cnf '+str(variables)+' '+str(len(clauses))+'\n'+''.join(' '.join(map(str,r))+' 0\n' for r in clauses))
  before=sha(cnf)
  command=[str(root/m['binary']),str(prefix),str(cnf)]
  p=subprocess.run(command,capture_output=True,text=True,timeout=300)
  (work/'stdout').write_text(p.stdout);(work/'stderr').write_text(p.stderr)
  assert sha(cnf)==before and prefix.with_suffix('.cnf').read_bytes()==cnf.read_bytes()
  assert p.returncode==2 and not prefix.with_suffix('.dsr').exists(),(i,kind,p.returncode)
  rows.append({'case':i,'corruption':kind,'command':command,'input_sha256':before,'input_preserved':True,'copied_input_equal':True,'exit':p.returncode,'proof_absent':True})
for p,h in m['bindings'].items():assert sha(root/p)==h,p
(run/'result.json').write_text(json.dumps({'job':os.environ['SLURM_JOB_ID'],'scope':'Corrected admission controls only; does not rerun positive certificates','controls':rows},indent=2)+'\n')
print('Passed',len(rows),'intended corrupted-input admission controls')
