#!/usr/bin/env python3
"""Successor audit; never rerun or rewrite earlier published checkpoints."""
from pathlib import Path
source=Path(__file__).with_name('audit-congruence-runtime-rank46-6679.py')
code=source.read_text()
changes={
    "target=root/'benchmarks/reports/2026-09-09-congruence-runtime-rank46-audit-6679.json'":
        "target=root/'benchmarks/reports/2026-09-09-rank46-watch-history-audit-6691.json'",
    "prior_path=root/'benchmarks/reports/2026-09-09-congruence-empty-gates-audit-6646.json'":
        "prior_path=root/'benchmarks/reports/2026-09-09-congruence-runtime-rank46-audit-6679.json'",
    "owned={'PROGRESS.md','tools/bench/certificate_prefix.py','tools/bench/test_certificate_prefix.py',\n       'tools/bench/watch_observer.py','tools/bench/test_watch_observer.py',\n       'benchmarks/reports/2026-09-09-congruence-runtime-and-rank46.md'}":
        "owned={'PROGRESS.md','tools/bench/watch_history.py','tools/bench/test_watch_history.py',\n       'benchmarks/reports/2026-09-09-rank46-watch-history.md'}",
    '6668<=int(match[1])<=6683':'6684<=int(match[1])<=6695',
    "    ('20260909-6669-rank46-current-prefix','summary-6671.json'),\n    ('20260909-6673-rank46-long-prefix','summary-6675.json'),\n    ('20260909-6676-rank46-search-window','summary-6678.json'),\n    ('20260909-6681-rank46-conflict-watch','summary-6683.json')":
        "    ('20260909-6684-rank46-watch-history','summary-6686.json'),\n    ('20260909-6688-rank46-earlier-window','summary-6690.json')",
    "    evidence[str((c/summary).relative_to(root))]=sha(c/summary)":
        "    evidence[str((c/summary).relative_to(root))]=sha(c/summary)\n    p=c/'localization-6687.json'\n    if p.exists():\n        own(p); bindings(json.loads(p.read_text())['hashes']); evidence[str(p.relative_to(root))]=sha(p)",
    'report-to-andrew/20260909-1940-congruence-runtime-rank46':'report-to-andrew/20260909-2005-rank46-watch-history',
    'Candidate6654 remains experimental/unpromoted/unfuzzed; no solver source edit this follow-up. Rank49 timing closes, memory does not; full50 still1.138354554 times fixedKissat with pending checks. Rank46 selected conflict follows reversed watch order; first movement/reconstruction cause remains open. No FlatLists change, fixed-reference refresh, fuzz transfer/artifact inspection or denied push retry. Initial report compile exposed path-line overfull boxes, corrected before publication.':
        'Diagnostic-only follow-up, source6654 and all references unchanged. Rank46 watch history has177292 identical portable rows before first observed pose change108138; earlier full search window refines next action. No whole-formula equality or repair acceptance inferred. Rank49 timing remains closed, full50 goal1.01 and memory remain unmet; source fuzz unavailable and no promotion, FlatLists change, external transfer or push retry. Pending validators wrote no summary; successful writers remain frozen.'}
for before,after in changes.items():
    assert code.count(before)==1,before
    code=code.replace(before,after)
anchor='checks={}'; assert code.count(anchor)==1
code=code.replace(anchor,"""c=root/'benchmarks/runs/20260909-6693-rank46-reason-watch'
m=json.loads((c/'manifest.json').read_text())
for rel,digest in m['hashes'].items(): assert sha(c/rel)==digest,rel
bindings(m['dependencies'])
for filename in ('manifest.json','launch.json','run.py','array.sbatch'): own(c/filename)
account(c,json.loads((c/'launch.json').read_text()),2)
records={}
for p in (c/'results').glob('*.json'):
    own(p); r=json.loads(p.read_text()); assert r['exit_code']==0 and r['scratch_deleted']
    assert r['binary_sha256']==m['hashes']['pins/'+r['arm']]
    for rel,digest in r['trace_sha256'].items(): assert sha(c/'traces'/rel)==digest
    records[r['arm']]=dict(exit_code=r['exit_code'],scratch_deleted=r['scratch_deleted'])
p=c/'summary-6695.json'
if p.exists():
    own(p); bindings(json.loads(p.read_text())['hashes']); evidence[str(p.relative_to(root))]=sha(p)
evidence['reason_watch_checkpoint']=dict(results=records,replay_validated=p.exists())
"""+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
