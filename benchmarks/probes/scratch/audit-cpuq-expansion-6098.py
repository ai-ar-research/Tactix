#!/usr/bin/env python3
"""Fresh normal-campaign audit bound to checked-results selection and closure."""
from pathlib import Path
source=Path(__file__).with_name('audit-rank37-heap-6089.py')
code=source.read_text()
for old,new in {
 '2026-09-08-rank37-heap-ancestry-validation.json':'2026-09-08-cpuq-expansion-validation.json',
 '2026-09-08-rank37-heap-ancestry.md':'2026-09-08-cpuq-expansion.md',
 "'compare-macro-heaps-6094')":"'compare-macro-heaps-6094','analyze-cpuq-expansion-6097')",
}.items():
    assert old in code,old;code=code.replace(old,new)
anchor="extra = '''"
assert code.count(anchor)==1
added='''
selection=root/'benchmarks/reports/2026-09-08-cpuq-expansion'
p=selection/'summary.json';s=json.loads(p.read_text());bind(p)
for rel,digest in s['hashes'].items():assert sha(root/rel)==digest,rel
assert len(s['proposed_additions'])==20 and s['compact_unsat_count']==12 and s['strict_unsat_count']==4
assert Counter(r['verdict'] for r in s['proposed_additions'][:10])==dict(UNSAT=7,SAT=3)
for width in (50,60):bind(selection/f'proposed-rank1-{width}.tsv')
p=root/'benchmarks/reports/2026-09-08-rank37-macro-heaps-completion.json';bind(p)
s=json.loads(p.read_text())
for rel,digest in s['hashes'].items():assert sha(root/rel)==digest,rel
'''
code=code.replace(anchor,anchor+added)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
