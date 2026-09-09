#!/usr/bin/env python3
"""Fresh checkpoint validation, named normal-job audit and immutable bindings."""
import datetime as dt
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
source=Path(__file__).with_name('audit-rank50-publication-6118.py')
code=source.read_text()
changes={
 '2026-09-08-rank50-publication-validation.json':'2026-09-08-fresh-accepted-validation.json',
 '2026-09-08-rank50-publication.md':'2026-09-08-fresh-accepted-reevaluation.md',
 '5968, 5970, 6059, 6061, 6108, 6114, 6115)':'5968, 5970, 6059, 6061, 6108, 6114, 6115, 6121)',
 'number in (6059, 6061, 6108, 6114, 6115)':'number in (6059, 6061, 6108, 6114, 6115, 6121)',
 "jobs = '1673402,1676727,1676783,1676803,1676804'":"jobs = '1673402,1676783,1676803,1676804,1676920'",
 "'-j', '1676727,1676781,1676783,1676803,1676804'":"'-j', '1676781,1676783,1676803,1676804,1676920'",
 'Userapproved Rank1-50target;fixedKissat7239.010s preservesold40. Starting6049=1.216724524x,50expected.6107generalpublicationrepair99/99Release twice;BC6114/full506115 submitted. No speedup orpromotion claimed.':
 'Fresh accepted-production/current-Kissat reevaluation6121/job1676920 submitted100tasks. Interim6123retainsall50rows,no partialaggregate. Newclassificationtests pass;Rank4bothcheckedUNSAT. Independent6107fullsolver ratio1.182462519x,BCno regression;checks/fuzzpending,no promotion.',
}
# Replace the destination strings in the wrapper's transformation table, not
# its old6067 anchors. This leaves the frozen wrappers untouched.
for old,new in changes.items():
    assert old in code,old;code=code.replace(old,new)
anchor="extra='''"
assert code.count(anchor)==1
extra='''
for name in ('tools/bench/checked_comparison.py','tools/bench/test_checked_comparison.py',
             'benchmarks/reports/2026-09-08-publication-gates-6124.json'):bind(root/name)
snapshot=root/'benchmarks/reports/2026-09-08-fresh-rank50-6123'
for p in snapshot.iterdir():bind(p)
s=json.loads((snapshot/'summary.json').read_text())
assert s['ranks']==50 and s['ratio'] is None and len(s['pairs'])==50
for rel,digest in s['record_hashes'].items():assert sha(Path(rel))==digest,rel
c=run_dir(6121);m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['protected_campaign_hashes'].items():assert sha(root/rel)==digest,rel
'''
code=code.replace(anchor,anchor+extra)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
