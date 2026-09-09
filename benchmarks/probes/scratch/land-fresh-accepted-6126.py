#!/usr/bin/env python3
"""Land the fresh reevaluation scaffold, tested classifier and frozen snapshots."""
import hashlib
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[3]
target=root/'benchmarks/reports/2026-09-08-fresh-accepted-landing.json'
assert not target.exists()
assert not subprocess.run(['git','diff','--cached','--name-only'],cwd=root,capture_output=True,text=True,check=True).stdout
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as stream:
        for block in iter(lambda:stream.read(8<<20),b''):h.update(block)
    return h.hexdigest()
owned=set()
def retain(p):
    assert p.is_file() and p.stat().st_size<5<<20,p
    owned.add(str(p.relative_to(root)))
audit=root/'benchmarks/reports/2026-09-08-fresh-accepted-validation.json'
a=json.loads(audit.read_text())
for rel,digest in a['hashes'].items():assert sha(root/rel)==digest,rel
for name in ('PROGRESS.md','tools/bench/checked_comparison.py','tools/bench/test_checked_comparison.py',
    'benchmarks/reports/2026-09-08-fresh-accepted-reevaluation.md',
    'benchmarks/reports/2026-09-08-fresh-accepted-validation.json',
    'benchmarks/reports/2026-09-08-publication-gates-6124.json'):retain(root/name)
for p in (root/'benchmarks/reports/2026-09-08-fresh-rank50-6123').iterdir():retain(p)
for n in range(6121,6127):
    for p in (root/'benchmarks/probes/scratch').glob('*'+str(n)+'.py'):retain(p)
c=root/'benchmarks/runs/20260908-6121-fresh-accepted-ranks1-50'
for name in ('manifest.json','launch.json','matrix.tsv','array.sbatch'):retain(c/name)
for p in (c/'pins').glob('*.py'):retain(p)
for rel,digest in a['hashes'].items():
    p=root/rel
    if p.parent.name in ('results','provisional') and any(f'-{n}-' in str(p) for n in (6108,6114,6115,6121)):
        retain(p)
# No live log or trace is staged; only atomically published audited records.
report=root/'report-to-andrew/20260908-2245-fresh-accepted-reevaluation'
log=(report/'report.log').read_text()
assert 'Overfull' not in log and 'Output written on report.pdf (2 pages,' in log
assert (report/'report.pdf').read_bytes().startswith(b'%PDF-')
target.write_text(json.dumps(dict(hashes={rel:sha(root/rel) for rel in sorted(owned)},
    andrew_report={str((report/name).relative_to(root)):sha(report/name) for name in ('report.tex','report.pdf')},
    queue_at_audit=a['queue'],
    outcome='Fresh accepted-production/current-Kissat paired reevaluation1676920 running. All50 retained;no unresolved-case aggregate. 33 benchmarktests,sixdocchecks pass. Prior6107solver gates preserved;notpromoted.',
    cleanup='No manual deletion. Audited final normal records assert original-certificate/derivative cleanup;active scratch/pins/logs untouched. No live traces staged or fuzz-owner artifacts accessed.',
    next_action='Finish exact1676920 andclassifyinvalidities/solve losses beforefinaltable. PrioritizeactualOOM/excessmemory,Rank4,Rank32,portableconvergence;preserveisolatedgeneralclosures and proof-rule/strategyboundary.',
    limitations='Pendingmodels/proofs andfuzzblockacceptance,notindependentdevelopment. No fixedreference replacement,no denied transfer/pushretry.'),indent=2)+'\n')
retain(target)
subprocess.run(['git','add','-f','--',*sorted(owned)],cwd=root,check=True)
subprocess.run(['git','diff','--cached','--check'],cwd=root,check=True)
subprocess.run(['git','commit','-m','Launch fresh accepted Rank 1-50 reevaluation and classify results before scoring'],cwd=root,check=True)
assert not subprocess.run(['git','status','--porcelain','--',*sorted(owned)],cwd=root,capture_output=True,text=True,check=True).stdout
print('PASS:',len(owned),'ownedpaths landed;no live traces,deletion orpush')
