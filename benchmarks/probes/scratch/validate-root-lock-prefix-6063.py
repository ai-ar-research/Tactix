#!/usr/bin/env python3
"""Compare all decisions and full Reduce state against immutable native traces."""
from pathlib import Path
import hashlib
import importlib.util
import itertools
import json
root=Path(__file__).resolve().parents[3]
c=root/'benchmarks/runs/20260908-6060-root-lock-rank32-prefix'
source=Path(__file__).with_name('compare-forward-admission-rank32-5839.py')
code=source.read_text().replace('20260908-5837-forward-admission-rank32-prefix','20260908-6060-root-lock-rank32-prefix')
code=code.replace("row['conflicts'] == '179463'","row['conflicts'] == '271079'")
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
sp=importlib.util.spec_from_file_location('history',Path(__file__).with_name('validate-retention-history-6044.py'))
h=importlib.util.module_from_spec(sp);sp.loader.exec_module(h)
m=json.loads((c/'manifest.json').read_text())
native=root/m['native_reduce_history']['path']
assert h.sha(native/'summary.json')==m['native_reduce_history']['summary_sha256']
r=json.loads((native/'results/kissat.json').read_text())
assert h.sha(native/'traces/kissat.reduce.tsv')==r['trace_sha256']['kissat.reduce.tsv']
rows=[]
for a,b in itertools.zip_longest(h.reductions(c/'traces/candidate.reduce.tsv'),h.reductions(native/'traces/kissat.reduce.tsv')):
    if a is None or b is None:
        rows.append(dict(context_mismatch=True,candidate=None if a is None else a[0],kissat=None if b is None else b[0]));continue
    fields=('reduction','conflict','mode','tier1','tier2')
    if any(a[0][k]!=b[0][k] for k in fields):
        rows.append(dict(context_mismatch=True,candidate=a[0],kissat=b[0]));continue
    rows.append(h.comparison(a,b))
assert rows
different=[r for r in rows if r.get('context_mismatch') or not r['exact_clause_metadata']]
out=dict(all_reductions_match=not different, reductions=len(rows),first_difference=different[0] if different else None,
         comparisons=rows,decision_summary_sha256=h.sha(c/'summary.json'),native_summary_sha256=h.sha(native/'summary.json'),
         scope='Full clause multiplicity,metadata,eligibility,ranks and victim flags at each matched Reduce boundary; no runtime or final proof claim.')
target=c/'reduce-validation.json';assert not target.exists();target.write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({k:v for k,v in out.items() if k!='comparisons'},indent=2))
