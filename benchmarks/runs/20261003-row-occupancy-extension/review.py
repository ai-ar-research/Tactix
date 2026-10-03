from pathlib import Path
import json,hashlib,itertools,subprocess,lzma,sys
root=Path(__file__).resolve().parents[3];run=Path(__file__).resolve().parent
base=root/'benchmarks/runs/20261003-graph-certified-gate-recovery'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for name,h in json.loads((base/'SHA256.json').read_text()).items():assert sha(base/name)==h
summary=json.loads((base/'summary.json').read_text());source=root/summary['input'];assert hashlib.sha256(lzma.open(source,'rb').read()).hexdigest()==summary['input_sha256']
intervals=json.loads((base/'intervals.json').read_text());rows=max(x['row'] for x in intervals)+1;columns=max(x['last'] for x in intervals)+1
relations=json.loads((base/'interval-relations.json').read_text());assert rows==columns==relations['complete_row_at_most_one']
with lzma.open(source,'rt') as f:
 header=next(line for line in f if line.startswith('p cnf '));extent=int(header.split()[2])
records=[];definitions=[]
for row in range(rows):
 choices={j:[] for j in range(columns)}
 for item in intervals:
  if item['row']==row:choices[item['first']].append(item)
 # Minimum number of already-certified contiguous blocks in an exact partition.
 best={columns:[]}
 for first in reversed(range(columns)):
  candidates=[ [item]+best[item['last']+1] for item in choices[first] if item['last']+1 in best]
  if candidates:best[first]=min(candidates,key=lambda items:(len(items),tuple(x['output'] for x in items)))
 assert 0 in best;blocks=best[0];assert len(blocks)==3
 assert [j for item in blocks for j in range(item['first'],item['last']+1)]==list(range(columns))
 # Absence literals a_i mean no primary selection in their certified blocks.
 # Define fresh p iff OR_i NOT a_i. No p unit or row-cover clause is asserted.
 helper=extent+row+1;inputs=[x['output'] for x in blocks]
 clauses=[[helper,a] for a in inputs]+[[-helper,*[-a for a in inputs]]]
 definitions+=clauses
 records.append({'action':'row-occupancy-review','kind':'conservative-extension','row':row,'auxiliary':helper,'meaning':'some original primary selection in this row','boolean_form':'p iff OR(not absence_i)','certified_blocks':blocks,'clauses':clauses,'proof':'positive-pivot fresh clauses, then negative-pivot tautological resolvents','reconstruction':'project p; unique extension p=OR(not absence_i)','phase_advice':'positive is a branch choice, not an asserted unit','lifetime':'proposed live definition; not installed'})
(run/'proposed-definitions.json').write_text(json.dumps(records,indent=2)+'\n')
(run/'proposed-prefix.dsr').write_text(''.join(' '.join(map(str,c))+' 0\n' for c in definitions))
# Universal local check on the three abstract inputs, without a closing gadget.
clauses=[(6,1),(6,2),(6,3),(-6,-1,-2,-3)];models=0
for abc in itertools.product([False,True],repeat=3):
 extensions=[]
 for p in [False,True]:
  val={1:abc[0],2:abc[1],3:abc[2],6:p}
  if all(any(val[abs(l)]==(l>0) for l in c) for c in clauses):extensions.append(p)
 assert extensions==[not all(abc)];models+=1
# All-addition LRAT checks every definition. The disjoint closing gadget is
# initially unit-free and is never referenced by a definition's RAT hints.
cnf=run/'micro.cnf';cnf.write_text('p cnf 5 4\n4 5 0\n4 -5 0\n-4 5 0\n-4 -5 0\n')
lrat=run/'micro.lrat';lrat.write_text('5 6 1 0 0\n6 6 2 0 0\n7 6 3 0 0\n8 -6 -1 -2 -3 0 -5 -6 -7 0\n9 4 0 1 2 0\n10 0 9 3 4 0\n')
proof=run/'micro.dsr';proof.write_text('6 1 0\n6 2 0\n6 3 0\n-6 -1 -2 -3 0\n4 0\n0\n')
checkers=root/'benchmarks/runs/20261003-ramsey-degree-dsr-feasibility/checkers'
sys.path.insert(0,str(root/'tools/bench'));import proof_oracle as oracle
for name,command,kind in [('all-addition',[str(checkers/'cake_lpr'),str(cnf),str(lrat)],'cake_lpr'),('forward',[str(checkers/'drat-trim'),str(cnf),str(proof),'-f'],'drat-trim')]:
 p=subprocess.run(command,capture_output=True,text=True,timeout=30);(run/(name+'.stdout')).write_text(p.stdout);(run/(name+'.stderr')).write_text(p.stderr)
 assert oracle.proof_check_status(kind,p.returncode,p.stdout,p.stderr)=='verified',(name,p.stdout,p.stderr)
# Adding p as a unit has no justification. Its RAT resolvent is not RUP.
bad=run/'invalid.drat';bad.write_text(proof.read_text().replace('4 0\n0\n','6 0\n4 0\n0\n'))
p=subprocess.run([str(checkers/'drat-trim'),str(cnf),str(bad),'-f'],capture_output=True,text=True,timeout=30)
(run/'negative.stdout').write_text(p.stdout);(run/'negative.stderr').write_text(p.stderr)
assert oracle.proof_check_status('drat-trim',p.returncode,p.stdout,p.stderr)=='rejected'
result={'scope':'Static certified-block reuse and universally checked local definition; no target solver experiment or production installation','rows':rows,'columns':columns,'original_extent':extent,'new_variables':rows,'definition_clauses':len(definitions),'definition_literals':sum(map(len,definitions)),'prefix_bytes':(run/'proposed-prefix.dsr').stat().st_size,'old_assignments_with_unique_extension':models,'all_addition_verified':True,'forward_verified':True,'unjustified_unit_rejected':True,'input_sha256':summary['input_sha256'],'checkers':{str(checkers/name):sha(checkers/name) for name in ['drat-trim','cake_lpr']}}
(run/'result.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2),flush=True)
(run/'SHA256.json').write_text(json.dumps({p.name:sha(p) for p in sorted(run.iterdir()) if p.is_file() and p.name!='SHA256.json'},indent=2)+'\n')
