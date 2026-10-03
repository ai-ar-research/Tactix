"""Read retained comparison records only; never rerun a solver or collector."""
import csv,hashlib,json
from pathlib import Path
root=Path(__file__).resolve().parents[3];run=Path(__file__).resolve().parent
csvpath=root/'report-to-andrew/20261003T164617Z-tierd-portfolio-misses/portfolio-misses.csv'
attrpath=root/'benchmarks/runs/20261003-winner-integration-attribution/attribution.csv'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(csvpath)=='b2444220c04a4e1a05bbe3d21c4bc73d2e119bad625fb31a9a3ac7d773c0b4f6'
attrs={r['rank']:r for r in csv.DictReader(attrpath.open())};bindings={str(p.relative_to(root)):sha(p) for p in [csvpath,attrpath,Path(__file__)]};records=[]
for row in csv.DictReader(csvpath.open()):
 if row['verdict']!='UNSAT' or row['checked_solvers']!='lymphosat':continue
 a=attrs[row['rank']];arms={}
 for arm in ['tactix','lymphosat','satsuma-kissat','kissat-sc2024','ae-kissat-mab']:
  p=root/row[arm+'_source_record'];bindings[str(p.relative_to(root))]=sha(p);d=json.loads(p.read_text())
  assert d['instance']==row['instance'] and d['verdict']==row[arm+'_verdict']
  pc=d.get('proof_check') or {};solve=d['solve']
  if arm in ['tactix','lymphosat']:
   assert d['verdict']=='UNSAT' and solve['exit_code']==20 and not solve['timed_out']
   assert float(row[arm+'_solve_seconds'])==solve['wall_seconds']
   assert int(a[arm+'_certificate_bytes'])==d['certificate_bytes']
  if arm=='lymphosat':assert pc['result']=='verified' and pc['pipeline']=='pbp'
  else:assert pc.get('result')!='verified'
  stages=[{k:s.get(k) for k in ['name','result','timed_out','wall_seconds','timeout_seconds']} for s in pc.get('stages',[])]
  arms[arm]={'source':str(p.relative_to(root)),'solve':solve,'verdict':d['verdict'],'certificate_bytes':d.get('certificate_bytes'),'proof_result':pc.get('result'),'proof_seconds':pc.get('wall_seconds'),'pipeline':pc.get('pipeline'),'stages':stages}
  if arm=='tactix':
   assert pc['timed_out'];arms[arm]['outside_recorded_stages_seconds']=round(pc['wall_seconds']-sum(s['wall_seconds'] for s in stages),3)
   arms[arm]['structural_statistics']=[s for s in d.get('solver_statistics',[]) if s.startswith(('c sr ','c conggates','c parity','c count-pb'))]
 assert arms['lymphosat']['certificate_bytes']>arms['tactix']['certificate_bytes']
 records.append({'rank':int(row['rank']),'benchmark':row['benchmark'],'instance':row['instance'],'historical_handler':a['historical_lymphosat_handler'],'attribution_limit':a['historical_attribution_limit'],'latest_matchers':json.loads(a['latest_lymphosat_matcher_messages']),'arms':arms})
assert [r['rank'] for r in records]==[226,291,511,541,744,1129]
result={'scope':'Unique checked UNSAT among four reference arms within the fixed 44 misses; not all Tier D','records':records,'all_tactix_reported_unsat':True,'all_tactix_checker_timeout':True,'all_lymphosat_proofs_larger_bytes':True,'causal_attribution_established':False}
(run/'result.json').write_text(json.dumps(result,indent=2)+'\n')
(run/'manifest.json').write_text(json.dumps({'bindings':bindings,'result_sha256':sha(run/'result.json')},indent=2)+'\n')
for r in records:
 t=r['arms']['tactix'];l=r['arms']['lymphosat']
 print(r['rank'],t['solve']['wall_seconds'],l['solve']['wall_seconds'],t['certificate_bytes'],l['certificate_bytes'],t['outside_recorded_stages_seconds'])
