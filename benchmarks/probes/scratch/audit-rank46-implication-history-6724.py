#!/usr/bin/env python3
"""Seal earlier clause-history localization and queue-state controls."""
from pathlib import Path
source=Path(__file__).with_name('audit-congruence-runtime-rank46-6679.py')
code=source.read_text()
changes={
    "target=root/'benchmarks/reports/2026-09-09-congruence-runtime-rank46-audit-6679.json'":
        "target=root/'benchmarks/reports/2026-09-09-rank46-implication-history-audit-6724.json'",
    "prior_path=root/'benchmarks/reports/2026-09-09-congruence-empty-gates-audit-6646.json'":
        "prior_path=root/'benchmarks/reports/2026-09-09-rank46-reason-history-audit-6700.json'",
    "owned={'PROGRESS.md','tools/bench/certificate_prefix.py','tools/bench/test_certificate_prefix.py',\n       'tools/bench/watch_observer.py','tools/bench/test_watch_observer.py',\n       'benchmarks/reports/2026-09-09-congruence-runtime-and-rank46.md'}":
        "owned={'PROGRESS.md','tools/bench/queue_bump_trace.py','tools/bench/test_queue_bump_trace.py',\n       'benchmarks/reports/2026-09-09-rank46-implication-history.md'}",
    '6668<=int(match[1])<=6683':'6708<=int(match[1])<=6725',
    "    ('20260909-6669-rank46-current-prefix','summary-6671.json'),\n    ('20260909-6673-rank46-long-prefix','summary-6675.json'),\n    ('20260909-6676-rank46-search-window','summary-6678.json'),\n    ('20260909-6681-rank46-conflict-watch','summary-6683.json')":
        "    ('20260909-6705-rank46-propagation-split','summary-6707.json'),\n    ('20260909-6708-rank46-implication-watch','summary-6710.json'),\n    ('20260909-6711-rank46-implication-history','summary-6713.json'),\n    ('20260909-6714-rank46-first-swap','summary-6716.json'),\n    ('20260909-6718-rank46-queue-bumps','summary-6720.json')",
    "    evidence[str((c/summary).relative_to(root))]=sha(c/summary)":
        "    evidence[str((c/summary).relative_to(root))]=sha(c/summary)\n    p=c/'localization-6717.json'\n    if p.exists():\n        own(p); bindings(json.loads(p.read_text())['hashes']); evidence[str(p.relative_to(root))]=sha(p)",
    'report-to-andrew/20260909-1940-congruence-runtime-rank46':'report-to-andrew/20260909-2055-rank46-implication-history',
    'Candidate6654 remains experimental/unpromoted/unfuzzed; no solver source edit this follow-up. Rank49 timing closes, memory does not; full50 still1.138354554 times fixedKissat with pending checks. Rank46 selected conflict follows reversed watch order; first movement/reconstruction cause remains open. No FlatLists change, fixed-reference refresh, fuzz transfer/artifact inspection or denied push retry. Initial report compile exposed path-line overfull boxes, corrected before publication.':
        '6654 and native pins unchanged; diagnostic-only increment with five new queue parser tests. Immediate implication swap107104 is explained by unit-watch order, but earlier clause history73636/76396 predates the first branchswap78797. Queue pre-move links/stamps already reversed; no sort defect inferred.6721 follows earlier all-scans. Full50/runtime/memory/fuzz remain open. No fixed-reference refresh, FlatLists change, fuzz artifact/transfer, promotion or denied push retry. Initial report compile had two overfull text lines, corrected before publication.'}
for before,after in changes.items():
    assert code.count(before)==1,before
    code=code.replace(before,after)
anchor='checks={}'; assert code.count(anchor)==1
code=code.replace(anchor,"""c=root/'benchmarks/runs/20260909-6721-rank46-earlier-allscan'
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
p=c/'summary-6723.json'
if p.exists():
    own(p); bindings(json.loads(p.read_text())['hashes']); evidence[str(p.relative_to(root))]=sha(p)
evidence['6721_checkpoint']=dict(results=records,replay_validated=p.exists())
"""+anchor)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
